#include "base.hpp"

#include "features/soundboard.hpp"

#include "audio/codecs.hpp"
#include "gui/gui.hpp"
#include "gui/notifications.hpp"
#include "runtime/thread.hpp"
#include "steam.hpp"

ATLAS_NO_DESTROY soundboard g_soundboard{ };

inline constexpr u32   SOUNDBOARD_RATE{ 24000 };
inline constexpr usize SOUNDBOARD_SAMPLE_LIMIT{ SOUNDBOARD_RATE * 600 };
inline constexpr usize SOUNDBOARD_FILE_LIMIT{ 64u * 1024u * 1024u };
inline constexpr usize SOUNDBOARD_VORBIS_MEMORY{ 512 * 1024 };
inline constexpr u64   SOUNDBOARD_MAX_READ{ SOUNDBOARD_RATE / 4 };
inline constexpr i32   SOUNDBOARD_HEADER_SIZE{ 14 };
inline constexpr i32   SOUNDBOARD_PACKET_OVERHEAD{ SOUNDBOARD_HEADER_SIZE + 4 };
inline constexpr u8    VOICE_PAYLOAD_SAMPLE_RATE{ 0x0B };
inline constexpr u8    VOICE_PAYLOAD_OPUS_PLC{ 0x06 };

inline constexpr u16 VOICE_ENCODER_INIT{ 1 };
inline constexpr u16 VOICE_ENCODER_COMPRESS{ 4 };
inline constexpr u16 VOICE_ENCODER_RESET{ 6 };
inline constexpr i32 VOICE_ENCODER_QUALITY{ 5 };

struct resampler_t
{
    atl::vector< i16 > *m_out{ };
    f64                 m_step{ };
    f64                 m_position{ };
    f32                 m_previous{ };
    bool                m_primed{ };

    void set_rate( u32 rate )
    {
        this->m_step = static_cast< f64 >( rate ) / SOUNDBOARD_RATE;
    }

    bool push( f32 sample )
    {
        if ( !this->m_primed )
        {
            this->m_previous = sample;
            this->m_primed   = true;
            return true;
        }

        for ( ; this->m_position < 1.0; this->m_position += this->m_step )
        {
            const f32 value{ this->m_previous + ( sample - this->m_previous ) * static_cast< f32 >( this->m_position ) };

            if ( this->m_out->size( ) >= SOUNDBOARD_SAMPLE_LIMIT ||
                 !this->m_out->push_back( static_cast< i16 >( math::clamp( value, -1.0f, 1.0f ) * 32767.0f ) ) )
            {
                return false;
            }
        }

        this->m_position -= 1.0;
        this->m_previous = sample;
        return true;
    }
};

static u32 ATLAS_STDCALL soundboard_thread( void *parameter )
{
    g_soundboard.decode( static_cast< i32 >( reinterpret_cast< usize >( parameter ) ) );
    return 0;
}

ATLAS_FORCE_INLINE u16 read_u16( const u8 *data )
{
    return static_cast< u16 >( data[ 0 ] | data[ 1 ] << 8 );
}

ATLAS_FORCE_INLINE u32 read_u32( const u8 *data )
{
    return static_cast< u32 >( data[ 0 ] | data[ 1 ] << 8 | data[ 2 ] << 16 | data[ 3 ] << 24 );
}

ATLAS_FORCE_INLINE void write_u16( u8 *data, u16 value )
{
    data[ 0 ] = static_cast< u8 >( value );
    data[ 1 ] = static_cast< u8 >( value >> 8 );
}

ATLAS_FORCE_INLINE u32 voice_crc32( const u8 *data, usize size )
{
    u32 crc{ 0xFFFFFFFFu };

    for ( usize index{ }; index < size; ++index )
    {
        crc ^= data[ index ];

        for ( i32 bit{ }; bit < 8; ++bit )
        {
            crc = ( crc >> 1 ) ^ ( 0xEDB88320u & ( 0u - ( crc & 1u ) ) );
        }
    }

    return ~crc;
}

ATLAS_FORCE_INLINE usize sound_folder( wchar_t ( &path )[ SOUNDBOARD_PATH_CAPACITY ] )
{
    u32 length{ I( GetModuleFileNameW )( nullptr, path, SOUNDBOARD_PATH_CAPACITY - SOUNDBOARD_FILE_LENGTH - 16 ) };

    if ( !length || length >= SOUNDBOARD_PATH_CAPACITY - SOUNDBOARD_FILE_LENGTH - 16 )
    {
        return 0;
    }

    while ( length && path[ length - 1 ] != L'\\' )
    {
        --length;
    }

    path[ length ] = L'\0';
    str::append( path, SOUNDBOARD_PATH_CAPACITY, ATLAS_XORWSTR( L"atlas-sounds" ).get( ) );
    I( CreateDirectoryW )( path, nullptr );

    length           = str::length( path );
    path[ length++ ] = L'\\';
    path[ length ]   = L'\0';
    return length;
}

ATLAS_FORCE_INLINE bool is_sound_file( const wchar_t *name, usize length )
{
    if ( length < 5 || name[ length - 4 ] != L'.' )
    {
        return false;
    }

    wchar_t extension[ 4 ]{ };
    for ( usize index{ }; index < 3; ++index )
    {
        extension[ index ] = str::to_lower( name[ length - 3 + index ] );
    }

    return str::equals( extension, ATLAS_XORWSTR( L"wav" ).get( ) ) || str::equals( extension, ATLAS_XORWSTR( L"mp3" ).get( ) ) ||
           str::equals( extension, ATLAS_XORWSTR( L"ogg" ).get( ) );
}

ATLAS_FORCE_INLINE bool read_sound( const wchar_t *path, atl::vector< u8 > &bytes )
{
    void *file{ I( CreateFileW )( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ) };
    if ( file == INVALID_HANDLE_VALUE )
    {
        return false;
    }

    LARGE_INTEGER size{ };
    DWORD         read{ };
    const bool    ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart > 0 && size.QuadPart <= static_cast< i64 >( SOUNDBOARD_FILE_LIMIT ) &&
                      bytes.resize( static_cast< usize >( size.QuadPart ) ) &&
                      I( ReadFile )( file, bytes.data( ), static_cast< DWORD >( size.QuadPart ), &read, nullptr ) && read == size.QuadPart };

    I( CloseHandle )( file );
    return ok;
}

ATLAS_FORCE_INLINE bool decode_wav( const atl::vector< u8 > &bytes, resampler_t &resampler )
{
    constexpr u32 format_tag{ 0x20746D66 };
    constexpr u32 data_tag{ 0x61746164 };
    constexpr u16 pcm_format{ 1 };
    constexpr u16 extensible_format{ 0xFFFE };

    const u8   *wav{ bytes.data( ) };
    const usize size{ bytes.size( ) };

    u16 format{ };
    u16 channels{ };
    u16 bits{ };
    u32 rate{ };

    for ( usize at{ 12 }; at + 8 <= size; )
    {
        const u32 tag{ read_u32( wav + at ) };
        const u32 length{ read_u32( wav + at + 4 ) };
        const u8 *body{ wav + at + 8 };

        if ( length > size - at - 8 )
        {
            return false;
        }

        if ( tag == format_tag && length >= 16 )
        {
            format   = read_u16( body );
            channels = read_u16( body + 2 );
            rate     = read_u32( body + 4 );
            bits     = read_u16( body + 14 );
        } else if ( tag == data_tag )
        {
            if ( ( format != pcm_format && format != extensible_format ) || bits != 16 || !channels || !rate )
            {
                return false;
            }

            resampler.set_rate( rate );

            const usize frames{ length / ( 2u * channels ) };
            for ( usize frame{ }; frame < frames; ++frame )
            {
                f32 sum{ };
                for ( usize channel{ }; channel < channels; ++channel )
                {
                    sum += static_cast< i16 >( read_u16( body + ( frame * channels + channel ) * 2 ) ) / 32768.0f;
                }

                if ( !resampler.push( sum / channels ) )
                {
                    break;
                }
            }

            return true;
        }

        at += 8 + length + ( length & 1 );
    }

    return false;
}

ATLAS_FORCE_INLINE bool decode_mp3( const atl::vector< u8 > &bytes, resampler_t &resampler )
{
    mp3dec_t dec{ };
    f32      pcm[ MINIMP3_MAX_SAMPLES_PER_FRAME ]{ };
    usize    used{ };
    bool     rated{ };

    mp3dec_init( &dec );

    if ( bytes.size( ) >= 10 && bytes[ 0 ] == 'I' && bytes[ 1 ] == 'D' && bytes[ 2 ] == '3' )
    {
        used = 10 + ( ( bytes[ 6 ] & 0x7F ) << 21 | ( bytes[ 7 ] & 0x7F ) << 14 | ( bytes[ 8 ] & 0x7F ) << 7 | ( bytes[ 9 ] & 0x7F ) );
        used += ( bytes[ 5 ] & 0x10 ) ? 10 : 0;
    }

    while ( used < bytes.size( ) )
    {
        mp3dec_frame_info_t info{ };
        const i32           samples{ mp3dec_decode_frame( &dec, bytes.data( ) + used, static_cast< int >( bytes.size( ) - used ), pcm, &info ) };

        if ( !info.frame_bytes )
        {
            break;
        }

        used += static_cast< usize >( info.frame_bytes );

        if ( !samples )
        {
            continue;
        }

        if ( !rated )
        {
            resampler.set_rate( static_cast< u32 >( info.hz ) );
            rated = true;
        }

        for ( i32 frame{ }; frame < samples; ++frame )
        {
            const f32 sample{ info.channels > 1 ? ( pcm[ frame * 2 ] + pcm[ frame * 2 + 1 ] ) * 0.5f : pcm[ frame ] };

            if ( !resampler.push( sample ) )
            {
                return true;
            }
        }
    }

    return rated;
}

ATLAS_FORCE_INLINE bool decode_vorbis( const atl::vector< u8 > &bytes, resampler_t &resampler )
{
    char *memory{ static_cast< char * >( atl::malloc( SOUNDBOARD_VORBIS_MEMORY ) ) };
    if ( !memory )
    {
        return false;
    }

    const stb_vorbis_alloc alloc{ memory, static_cast< int >( SOUNDBOARD_VORBIS_MEMORY ) };
    int                    used{ };
    int                    error{ };
    stb_vorbis            *vorbis{ stb_vorbis_open_pushdata( bytes.data( ), static_cast< int >( bytes.size( ) ), &used, &error, &alloc ) };

    if ( vorbis )
    {
        resampler.set_rate( stb_vorbis_get_info( vorbis ).sample_rate );

        for ( usize offset{ static_cast< usize >( used ) }; offset < bytes.size( ); )
        {
            int     channels{ };
            int     samples{ };
            float **output{ };

            used = stb_vorbis_decode_frame_pushdata( vorbis, bytes.data( ) + offset, static_cast< int >( bytes.size( ) - offset ), &channels, &output, &samples );

            if ( !used && !samples )
            {
                break;
            }

            offset += static_cast< usize >( used );

            bool full{ };
            for ( int frame{ }; frame < samples && !full; ++frame )
            {
                full = !resampler.push( channels > 1 ? ( output[ 0 ][ frame ] + output[ 1 ][ frame ] ) * 0.5f : output[ 0 ][ frame ] );
            }

            if ( full )
            {
                break;
            }
        }
    }

    atl::free( memory );
    return vorbis != nullptr;
}

ATLAS_FORCE_INLINE bool decode_sound( const atl::vector< u8 > &bytes, atl::vector< i16 > &samples )
{
    resampler_t resampler{ &samples };

    if ( bytes.size( ) >= 12 && read_u32( bytes.data( ) ) == 0x46464952 && read_u32( bytes.data( ) + 8 ) == 0x45564157 )
    {
        return decode_wav( bytes, resampler );
    }

    if ( bytes.size( ) >= 4 && read_u32( bytes.data( ) ) == 0x5367674F )
    {
        return decode_vorbis( bytes, resampler );
    }

    return decode_mp3( bytes, resampler );
}

void soundboard::refresh( )
{
    this->m_count = 0;

    wchar_t pattern[ SOUNDBOARD_PATH_CAPACITY ]{ };
    if ( !sound_folder( pattern ) )
    {
        return;
    }

    str::append( pattern, SOUNDBOARD_PATH_CAPACITY, ATLAS_XORWSTR( L"*" ).get( ) );

    WIN32_FIND_DATAW found{ };
    void            *search{ I( FindFirstFileW )( pattern, &found ) };

    if ( search == INVALID_HANDLE_VALUE )
    {
        return;
    }

    do
    {
        const usize length{ str::length( found.cFileName ) };
        if ( ( found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) || length >= SOUNDBOARD_FILE_LENGTH || !is_sound_file( found.cFileName, length ) )
        {
            continue;
        }

        char name[ SOUNDBOARD_NAME_LENGTH + 1 ]{ };
        atl::to_ansi( found.cFileName, name, sizeof( name ) );

        usize slot{ this->m_count };
        while ( slot && str::compare_ignore_case( this->m_sounds[ slot - 1 ].m_name.c_str( ), name ) > 0 )
        {
            this->m_sounds[ slot ] = this->m_sounds[ slot - 1 ];
            --slot;
        }

        str::copy( this->m_sounds[ slot ].m_file, SOUNDBOARD_FILE_LENGTH, found.cFileName );
        this->m_sounds[ slot ].m_name.assign( string_view{ name } );
        ++this->m_count;
    } while ( this->m_count < SOUNDBOARD_CAPACITY && I( FindNextFileW )( search, &found ) );

    I( FindClose )( search );

    for ( usize index{ }; index < this->m_count; ++index )
    {
        this->m_labels[ index ] = this->m_sounds[ index ].m_name.c_str( );
    }
}

void soundboard::play( usize index )
{
    if ( index >= this->m_count || !this->prepare_encoder( ) )
    {
        return;
    }

    wchar_t path[ SOUNDBOARD_PATH_CAPACITY ]{ };
    if ( !sound_folder( path ) )
    {
        return;
    }

    str::append( path, SOUNDBOARD_PATH_CAPACITY, this->m_sounds[ index ].m_file );

    i32 generation{ };

    {
        atl::scoped_lock guard{ this->m_lock };

        str::copy( this->m_request, SOUNDBOARD_PATH_CAPACITY, path );
        this->m_request_name = this->m_sounds[ index ].m_name;
        generation           = ++this->m_generation;

        this->m_active.store( false );
        this->m_decoding.store( true );
    }

    if ( !atl::create_hidden_thread( reinterpret_cast< atl::thread_entry_fn >( soundboard_thread ),
                                     reinterpret_cast< void * >( static_cast< usize >( generation ) ) ) )
    {
        this->m_decoding.store( false );
        push_notification( notify_kind_t::error, ICON_MUSIC, ATLAS_XORSTR_STATIC( "Soundboard" ),
                           ATLAS_XORSTR_STATIC( "Could not start the decoder thread" ) );
    }
}

void soundboard::stop( )
{
    atl::scoped_lock guard{ this->m_lock };

    ++this->m_generation;
    this->m_active.store( false );
    this->m_decoding.store( false );
    this->m_samples.reset( );
}

void soundboard::decode( i32 generation )
{
    wchar_t path[ SOUNDBOARD_PATH_CAPACITY ]{ };

    {
        atl::scoped_lock guard{ this->m_lock };

        if ( this->m_generation != generation )
        {
            return;
        }

        str::copy( path, SOUNDBOARD_PATH_CAPACITY, this->m_request );
    }

    atl::vector< u8 >  bytes{ };
    atl::vector< i16 > samples{ };
    const bool         decoded{ read_sound( path, bytes ) && decode_sound( bytes, samples ) && !samples.empty( ) };

    atl::scoped_lock guard{ this->m_lock };

    if ( this->m_generation != generation )
    {
        return;
    }

    this->m_decoding.store( false );

    if ( !decoded )
    {
        this->m_failed.store( true );
        return;
    }

    this->m_samples.swap( samples );
    this->m_cursor    = 0;
    this->m_last_read = 0;
    this->m_active.store( true );
}

void soundboard::on_frame( )
{
    if ( this->pressed( ) )
    {
        if ( this->busy( ) )
        {
            this->stop( );
        } else
        {
            this->play( this->m_selected );
        }
    }

    if ( this->m_failed.exchange( false ) )
    {
        atl::scoped_lock guard{ this->m_lock };
        push_notification( notify_kind_t::error, ICON_MUSIC, ATLAS_XORSTR_STATIC( "Could not decode" ), this->m_request_name.c_str( ) );
    }

    const bool wanted{ this->m_active.load( ) };
    if ( wanted != this->m_recording )
    {
        g_tf2.m_engine->ClientCmd_Unrestricted( wanted ? ATLAS_XORSTR( "+voicerecord" ).get( ) : ATLAS_XORSTR( "-voicerecord" ).get( ) );
        this->m_recording = wanted;
    }
}

void soundboard::on_level_shutdown( )
{
    this->stop( );
}

void soundboard::on_voice_status( i32 entity, bool talking )
{
    if ( entity != -1 || talking || !this->m_recording )
    {
        return;
    }

    this->stop( );
}

i32 soundboard::on_compressed_voice( u8 *out, i32 size, i32 written )
{
    if ( !this->m_active.load( ) )
    {
        return written;
    }

    atl::scoped_lock guard{ this->m_lock };

    if ( !this->m_active.load( ) || size <= SOUNDBOARD_PACKET_OVERHEAD )
    {
        return 0;
    }

    const u64 now{ I( GetTickCount64 )( ) };
    if ( !this->m_last_read )
    {
        this->m_last_read = now;
    }

    const u64 due{ math::smaller( ( now - this->m_last_read ) * ( SOUNDBOARD_RATE / 1000 ), SOUNDBOARD_MAX_READ ) };
    const f32 gain{ static_cast< f32 >( g_cfg.m_soundboard.m_volume ) / 100.0f };

    this->m_last_read = now;

    i16  pcm[ SOUNDBOARD_MAX_READ ]{ };
    i32  count{ };
    bool last{ };

    while ( static_cast< u64 >( count ) < due )
    {
        if ( this->m_cursor >= this->m_samples.size( ) )
        {
            if ( !g_cfg.m_soundboard.m_loop )
            {
                this->m_active.store( false );
                last = true;
                break;
            }

            this->m_cursor = 0;
        }

        pcm[ count++ ] = static_cast< i16 >( math::clamp( this->m_samples[ this->m_cursor++ ] * gain, -32768.0f, 32767.0f ) );
    }

    const i32 frames{ util::call_virtual< i32( ATLAS_FASTCALL * )( void *, const i16 *, i32, u8 *, i32, bool ) >( this->m_encoder, VOICE_ENCODER_COMPRESS )(
        this->m_encoder, pcm, count, out + SOUNDBOARD_HEADER_SIZE, size - SOUNDBOARD_PACKET_OVERHEAD, last ) };

    if ( frames <= 0 )
    {
        return 0;
    }

    const u64 steam_id{ STEAMID64_INDIVIDUAL | g_steam.local_account_id( ) };
    atl::memcpy( out, &steam_id, sizeof( steam_id ) );

    out[ 8 ] = VOICE_PAYLOAD_SAMPLE_RATE;
    write_u16( out + 9, static_cast< u16 >( SOUNDBOARD_RATE ) );
    out[ 11 ] = VOICE_PAYLOAD_OPUS_PLC;
    write_u16( out + 12, static_cast< u16 >( frames ) );

    const i32 length{ SOUNDBOARD_HEADER_SIZE + frames };
    const u32 crc{ voice_crc32( out, static_cast< usize >( length ) ) };
    atl::memcpy( out + length, &crc, sizeof( crc ) );

    return length + 4;
}

bool soundboard::prepare_encoder( )
{
    if ( !this->m_encoder )
    {
        this->m_encoder = g_tf2.m_VoiceEncoder_OPUS_create.is_null( ) ? nullptr : g_tf2.m_VoiceEncoder_OPUS_create.call_as< void * >( );

        if ( !this->m_encoder ||
             !util::call_virtual< bool( ATLAS_FASTCALL * )( void *, i32, i32 ) >( this->m_encoder, VOICE_ENCODER_INIT )(
                 this->m_encoder, VOICE_ENCODER_QUALITY, static_cast< i32 >( SOUNDBOARD_RATE ) ) )
        {
            this->m_encoder = nullptr;
            push_notification( notify_kind_t::error, ICON_MUSIC, ATLAS_XORSTR_STATIC( "Soundboard" ),
                               ATLAS_XORSTR_STATIC( "Steam's voice encoder was not found" ) );
            return false;
        }
    }

    util::call_virtual< void( ATLAS_FASTCALL * )( void * ) >( this->m_encoder, VOICE_ENCODER_RESET )( this->m_encoder );
    return true;
}

bool soundboard::busy( ) const
{
    return this->m_active.load( ) || this->m_decoding.load( );
}

const char *const *soundboard::labels( ) const
{
    return this->m_labels.data( );
}

usize soundboard::count( ) const
{
    return this->m_count;
}

usize *soundboard::selected( )
{
    return &this->m_selected;
}

bool soundboard::pressed( )
{
    const bool active{ gui::bind_active( g_cfg.m_soundboard.m_key ) };
    const bool edge{ active && !this->m_key_held };

    this->m_key_held = active;
    return edge;
}
