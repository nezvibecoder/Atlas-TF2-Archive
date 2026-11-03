#include "audio/stream.hpp"

#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "runtime/assert.hpp"
#include "runtime/memory.hpp"
#include "runtime/obfuscation.hpp"
#include "runtime/thread.hpp"
#include "util/format.hpp"
#include "util/string.hpp"

#define FAR
#include <winhttp.h>
#undef FAR

inline constexpr u32   STREAM_SECONDS{ 5 };
inline constexpr u32   STREAM_FULL_WAIT{ 20 };
inline constexpr usize MP3_WINDOW{ 16 * 1024 };
inline constexpr usize VORBIS_MEMORY{ 512 * 1024 };
inline constexpr usize OGG_PAGE_HEADER{ 27 };

static bool content_is( const wchar_t *type, const wchar_t *prefix )
{
    const usize size{ str::length( prefix ) };
    return str::length( type ) >= size && str::equals_ignore_case( type, size, prefix, size );
}

static bool starts_ogg_stream( const u8 *data, usize size )
{
    return size >= OGG_PAGE_HEADER && data[ 0 ] == 'O' && data[ 1 ] == 'g' && data[ 2 ] == 'g' && data[ 3 ] == 'S' &&
           data[ 4 ] == 0 && ( data[ 5 ] & 2 ) != 0;
}

static usize next_ogg_stream( const u8 *data, usize size )
{
    for ( usize at{ 1 }; at < size; ++at )
    {
        if ( starts_ogg_stream( data + at, size - at ) )
        {
            return at;
        }
    }

    return size;
}

static void fft( f32 *real, f32 *imag, usize size )
{
    for ( usize index{ 1 }, swapped{ }; index < size; ++index )
    {
        usize bit{ size >> 1 };

        for ( ; swapped & bit; bit >>= 1 )
        {
            swapped ^= bit;
        }

        swapped ^= bit;

        if ( index < swapped )
        {
            const f32 real_value{ real[ index ] };
            const f32 imag_value{ imag[ index ] };

            real[ index ]   = real[ swapped ];
            imag[ index ]   = imag[ swapped ];
            real[ swapped ] = real_value;
            imag[ swapped ] = imag_value;
        }
    }

    for ( usize length{ 2 }; length <= size; length <<= 1 )
    {
        const f32   angle{ -2.0f * PI / static_cast< f32 >( length ) };
        const usize half{ length / 2 };

        for ( usize start{ }; start < size; start += length )
        {
            for ( usize step{ }; step < half; ++step )
            {
                const f32   twiddle_real{ math::cos( angle * static_cast< f32 >( step ) ) };
                const f32   twiddle_imag{ math::sin( angle * static_cast< f32 >( step ) ) };
                const usize top{ start + step };
                const usize bottom{ top + half };
                const f32   product_real{ real[ bottom ] * twiddle_real - imag[ bottom ] * twiddle_imag };
                const f32   product_imag{ real[ bottom ] * twiddle_imag + imag[ bottom ] * twiddle_real };

                real[ bottom ] = real[ top ] - product_real;
                imag[ bottom ] = imag[ top ] - product_imag;
                real[ top ] += product_real;
                imag[ top ] += product_imag;
            }
        }
    }
}

audio_stream *audio_stream::open( const char *url, audio_stream_error_t &error )
{
    auto *stream{ new audio_stream{ } };

    error = stream->connect( url );

    while ( error == audio_stream_error_t::none && !stream->m_ring )
    {
        error = stream->receive( );
    }

    if ( error == audio_stream_error_t::none )
    {
        stream->m_references.store( 2 );

        if ( !atl::create_hidden_thread( reader, stream ) )
        {
            stream->m_references.store( 1 );
            error = audio_stream_error_t::thread;
        }
    }

    if ( error != audio_stream_error_t::none )
    {
        stream->drop( );
        return nullptr;
    }

    return stream;
}

void audio_stream::release( )
{
    if ( this->m_voice != AUDIO_NO_VOICE )
    {
        g_audio.close_voice( this->m_voice );
    }

    this->m_closing.store( true );
    this->drop( );
}

void audio_stream::play( )
{
    this->m_voice = g_audio.open_voice( this->m_rate, render_voice, this, this->m_volume );

    Assert( this->m_voice != AUDIO_NO_VOICE );
}

void audio_stream::pause( bool paused )
{
    if ( this->m_voice != AUDIO_NO_VOICE )
    {
        g_audio.pause_voice( this->m_voice, paused );
    }
}

void audio_stream::slide( f32 volume, u32 milliseconds )
{
    this->m_volume = volume;

    if ( this->m_voice != AUDIO_NO_VOICE )
    {
        g_audio.slide_voice( this->m_voice, volume, milliseconds );
    }
}

audio_stream_state_t audio_stream::state( ) const
{
    if ( this->m_ended.load( ) && this->m_written.load( ) == this->m_read.load( ) )
    {
        return audio_stream_state_t::stopped;
    }

    return this->m_stalled.load( ) ? audio_stream_state_t::stalled : audio_stream_state_t::playing;
}

i32 audio_stream::buffer_percent( ) const
{
    return static_cast< i32 >( ( this->m_written.load( ) - this->m_read.load( ) ) * 100u / this->m_capacity );
}

bool audio_stream::connected( ) const
{
    return !this->m_ended.load( );
}

void audio_stream::title( char *out, usize capacity ) const
{
    atl::scoped_lock guard{ this->m_title_lock };

    str::copy( out, capacity, static_cast< const char * >( this->m_title ) );
}

void audio_stream::spectrum( f32 ( &bins )[ AUDIO_SPECTRUM_BINS ] ) const
{
    f32 real[ HISTORY_SIZE ]{ };
    f32 imag[ HISTORY_SIZE ]{ };

    {
        atl::scoped_lock guard{ this->m_history_lock };

        for ( usize index{ }; index < HISTORY_SIZE; ++index )
        {
            real[ index ] = this->m_history[ ( this->m_history_at + index ) % HISTORY_SIZE ];
        }
    }

    for ( usize index{ }; index < HISTORY_SIZE; ++index )
    {
        real[ index ] *= 0.5f - 0.5f * math::cos( 2.0f * PI * static_cast< f32 >( index ) / static_cast< f32 >( HISTORY_SIZE ) );
    }

    fft( real, imag, HISTORY_SIZE );

    for ( usize bin{ }; bin < AUDIO_SPECTRUM_BINS; ++bin )
    {
        bins[ bin ] = math::sqrt( real[ bin ] * real[ bin ] + imag[ bin ] * imag[ bin ] ) * ( 4.0f / static_cast< f32 >( HISTORY_SIZE ) );
    }
}

audio_stream_error_t audio_stream::connect( const char *url )
{
    wchar_t     wide[ 512 ]{ };
    const usize length{ str::length( url ) };

    if ( length >= sizeof( wide ) / sizeof( wide[ 0 ] ) )
    {
        return audio_stream_error_t::url;
    }

    for ( usize index{ }; index < length; ++index )
    {
        wide[ index ] = static_cast< wchar_t >( static_cast< u8 >( url[ index ] ) );
    }

    URL_COMPONENTS parts{ };
    parts.dwStructSize      = sizeof( parts );
    parts.dwHostNameLength  = static_cast< DWORD >( -1 );
    parts.dwUrlPathLength   = static_cast< DWORD >( -1 );
    parts.dwExtraInfoLength = static_cast< DWORD >( -1 );

    wchar_t host[ 256 ]{ };

    if ( !I( WinHttpCrackUrl )( wide, 0, 0, &parts ) ||
         !str::copy( host, sizeof( host ) / sizeof( host[ 0 ] ), parts.lpszHostName, parts.dwHostNameLength ) )
    {
        return audio_stream_error_t::url;
    }

    this->m_session = I( WinHttpOpen )( ATLAS_XORWSTR_STATIC( L"VLC/3.0.20 LibVLC/3.0.20" ), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );

    if ( !this->m_session )
    {
        return audio_stream_error_t::connect;
    }

    I( WinHttpSetTimeouts )( this->m_session, 5000, 5000, 5000, 10000 );

    this->m_connection = I( WinHttpConnect )( this->m_session, host, parts.nPort, 0 );

    if ( !this->m_connection )
    {
        return audio_stream_error_t::connect;
    }

    this->m_request = I( WinHttpOpenRequest )( this->m_connection, ATLAS_XORWSTR_STATIC( L"GET" ), parts.lpszUrlPath, nullptr,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0 );

    if ( !this->m_request ||
         !I( WinHttpSendRequest )( this->m_request, ATLAS_XORWSTR_STATIC( L"Icy-MetaData: 1" ), static_cast< DWORD >( -1 ),
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0 ) ||
         !I( WinHttpReceiveResponse )( this->m_request, nullptr ) )
    {
        return audio_stream_error_t::connect;
    }

    DWORD status{ };
    DWORD size{ sizeof( status ) };

    if ( !I( WinHttpQueryHeaders )( this->m_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX ) ||
         status != 200 )
    {
        return audio_stream_error_t::status;
    }

    wchar_t type[ 64 ]{ };
    size = sizeof( type ) - sizeof( type[ 0 ] );

    I( WinHttpQueryHeaders )( this->m_request, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, type, &size,
                              WINHTTP_NO_HEADER_INDEX );

    if ( content_is( type, ATLAS_XORWSTR_STATIC( L"audio/mpeg" ) ) )
    {
        this->m_codec = codec_t::mp3;
    } else if ( content_is( type, ATLAS_XORWSTR_STATIC( L"application/ogg" ) ) ||
                content_is( type, ATLAS_XORWSTR_STATIC( L"audio/ogg" ) ) )
    {
        this->m_codec         = codec_t::vorbis;
        this->m_vorbis_memory = static_cast< char * >( atl::malloc( VORBIS_MEMORY ) );

        if ( !this->m_vorbis_memory )
        {
            return audio_stream_error_t::format;
        }
    } else
    {
        return audio_stream_error_t::format;
    }

    DWORD metaint{ };
    size = sizeof( metaint );

    if ( I( WinHttpQueryHeaders )( this->m_request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_NUMBER,
                                   ATLAS_XORWSTR_STATIC( L"icy-metaint" ), &metaint, &size, WINHTTP_NO_HEADER_INDEX ) )
    {
        this->m_metaint    = metaint;
        this->m_audio_left = metaint;
    }

    return audio_stream_error_t::none;
}

audio_stream_error_t audio_stream::receive( )
{
    u8    chunk[ 4096 ];
    DWORD available{ };
    DWORD received{ };

    if ( !I( WinHttpQueryDataAvailable )( this->m_request, &available ) || !available ||
         !I( WinHttpReadData )( this->m_request, chunk, math::smaller< DWORD >( available, sizeof( chunk ) ), &received ) ||
         !received )
    {
        return audio_stream_error_t::connect;
    }

    return this->filter( chunk, static_cast< u32 >( received ) );
}

audio_stream_error_t audio_stream::filter( const u8 *data, u32 size )
{
    if ( !this->m_metaint )
    {
        return this->decode( data, size );
    }

    while ( size )
    {
        if ( this->m_audio_left )
        {
            const u32 take{ math::smaller( size, this->m_audio_left ) };

            if ( const audio_stream_error_t error{ this->decode( data, take ) }; error != audio_stream_error_t::none )
            {
                return error;
            }

            this->m_audio_left -= take;
            data += take;
            size -= take;
            continue;
        }

        if ( !this->m_meta_left )
        {
            this->m_meta_left = static_cast< u32 >( *data ) * 16;
            this->m_meta_size = 0;
            ++data;
            --size;

            if ( !this->m_meta_left )
            {
                this->m_audio_left = this->m_metaint;
            }

            continue;
        }

        const u32 take{ math::smaller( size, this->m_meta_left ) };

        atl::memcpy( this->m_meta + this->m_meta_size, data, take );
        this->m_meta_size += take;
        this->m_meta_left -= take;
        data += take;
        size -= take;

        if ( !this->m_meta_left )
        {
            this->m_meta[ this->m_meta_size ] = '\0';
            this->m_audio_left                = this->m_metaint;
            this->read_icy_title( );
        }
    }

    return audio_stream_error_t::none;
}

audio_stream_error_t audio_stream::decode( const u8 *data, u32 size )
{
    while ( size )
    {
        if ( this->m_input_used )
        {
            this->m_input_size -= this->m_input_used;
            memmove( this->m_input, this->m_input + this->m_input_used, this->m_input_size );
            this->m_input_used = 0;
        }

        const usize take{ math::smaller< usize >( size, sizeof( this->m_input ) - this->m_input_size ) };

        if ( !take )
        {
            return audio_stream_error_t::format;
        }

        atl::memcpy( this->m_input + this->m_input_size, data, take );
        this->m_input_size += take;
        data += take;
        size -= static_cast< u32 >( take );

        const audio_stream_error_t error{ this->m_codec == codec_t::mp3 ? this->decode_mp3( ) : this->decode_vorbis( ) };

        if ( error != audio_stream_error_t::none )
        {
            return error;
        }
    }

    return audio_stream_error_t::none;
}

audio_stream_error_t audio_stream::decode_mp3( )
{
    f32 pcm[ MINIMP3_MAX_SAMPLES_PER_FRAME ];

    while ( this->m_input_size - this->m_input_used >= MP3_WINDOW )
    {
        mp3dec_frame_info_t info{ };
        const i32           samples{ mp3dec_decode_frame( &this->m_mp3, this->m_input + this->m_input_used,
                                                          static_cast< int >( this->m_input_size - this->m_input_used ), pcm, &info ) };

        if ( !info.frame_bytes )
        {
            return audio_stream_error_t::format;
        }

        this->m_input_used += static_cast< usize >( info.frame_bytes );

        if ( samples && !this->push( pcm, pcm + ( info.channels > 1 ? 1 : 0 ), static_cast< usize >( info.channels ),
                                     static_cast< u32 >( samples ), static_cast< u32 >( info.hz ) ) )
        {
            return audio_stream_error_t::connect;
        }
    }

    return audio_stream_error_t::none;
}

audio_stream_error_t audio_stream::decode_vorbis( )
{
    for ( ;; )
    {
        const u8   *data{ this->m_input + this->m_input_used };
        const usize pending{ this->m_input_size - this->m_input_used };

        if ( this->m_vorbis && starts_ogg_stream( data, pending ) )
        {
            this->m_vorbis = nullptr;
        }

        if ( !this->m_vorbis )
        {
            const stb_vorbis_alloc memory{ this->m_vorbis_memory, static_cast< int >( VORBIS_MEMORY ) };
            int                    used{ };
            int                    error{ };

            this->m_vorbis = stb_vorbis_open_pushdata( data, static_cast< int >( pending ), &used, &error, &memory );

            if ( !this->m_vorbis )
            {
                return error == VORBIS_need_more_data && this->m_input_size < sizeof( this->m_input )
                           ? audio_stream_error_t::none
                           : audio_stream_error_t::format;
            }

            this->m_input_used += static_cast< usize >( used );
            this->read_vorbis_title( );
            continue;
        }

        const usize limit{ next_ogg_stream( data, pending ) };
        int         channels{ };
        int         samples{ };
        float     **output{ };
        const int   used{ stb_vorbis_decode_frame_pushdata( this->m_vorbis, data, static_cast< int >( limit ), &channels, &output, &samples ) };

        if ( !used && !samples )
        {
            if ( limit == pending )
            {
                return audio_stream_error_t::none;
            }

            this->m_input_used += limit;
            continue;
        }

        this->m_input_used += static_cast< usize >( used );

        if ( samples &&
             !this->push( output[ 0 ], output[ channels > 1 ? 1 : 0 ], 1, static_cast< u32 >( samples ),
                          stb_vorbis_get_info( this->m_vorbis ).sample_rate ) )
        {
            return audio_stream_error_t::connect;
        }
    }
}

bool audio_stream::push( const f32 *left, const f32 *right, usize stride, u32 frames, u32 rate )
{
    if ( !this->m_ring )
    {
        u32 capacity{ 1 };
        while ( capacity < rate * STREAM_SECONDS )
        {
            capacity <<= 1;
        }

        this->m_ring = static_cast< f32 * >( atl::malloc( capacity * AUDIO_CHANNELS * sizeof( f32 ) ) );

        if ( !this->m_ring )
        {
            return false;
        }

        this->m_rate     = rate;
        this->m_capacity = capacity;
    }

    const u32 written{ this->m_written.load( ) };

    while ( written + frames - this->m_read.load( ) > this->m_capacity )
    {
        if ( this->m_closing.load( ) )
        {
            return false;
        }

        I( Sleep )( STREAM_FULL_WAIT );
    }

    for ( u32 frame{ }; frame < frames; ++frame )
    {
        f32 *slot{ this->m_ring + ( ( written + frame ) & ( this->m_capacity - 1 ) ) * AUDIO_CHANNELS };

        slot[ 0 ] = left[ frame * stride ];
        slot[ 1 ] = right[ frame * stride ];
    }

    this->m_written.store( written + frames );
    return true;
}

void audio_stream::render( f32 *out, u32 frames )
{
    const u32 read{ this->m_read.load( ) };
    u32       available{ this->m_written.load( ) - read };

    if ( this->m_stalled.load( ) )
    {
        if ( available < this->m_rate && !this->m_ended.load( ) )
        {
            available = 0;
        } else
        {
            this->m_stalled.store( false );
        }
    }

    const u32 count{ math::smaller( available, frames ) };

    for ( u32 frame{ }; frame < count; ++frame )
    {
        const f32 *slot{ this->m_ring + ( ( read + frame ) & ( this->m_capacity - 1 ) ) * AUDIO_CHANNELS };

        out[ frame * AUDIO_CHANNELS ]     = slot[ 0 ];
        out[ frame * AUDIO_CHANNELS + 1 ] = slot[ 1 ];
    }

    atl::memset( out + count * AUDIO_CHANNELS, 0, ( frames - count ) * AUDIO_CHANNELS * sizeof( f32 ) );
    this->m_read.store( read + count );

    if ( count < frames && !this->m_ended.load( ) )
    {
        this->m_stalled.store( true );
    }

    atl::scoped_lock guard{ this->m_history_lock };

    for ( u32 frame{ frames > HISTORY_SIZE ? frames - static_cast< u32 >( HISTORY_SIZE ) : 0 }; frame < frames; ++frame )
    {
        this->m_history[ this->m_history_at ] = ( out[ frame * AUDIO_CHANNELS ] + out[ frame * AUDIO_CHANNELS + 1 ] ) * 0.5f;
        this->m_history_at                    = ( this->m_history_at + 1 ) % HISTORY_SIZE;
    }
}

void audio_stream::read_icy_title( )
{
    const char *key{ ATLAS_XORSTR_STATIC( "StreamTitle='" ) };
    const char *end{ ATLAS_XORSTR_STATIC( "';" ) };
    const char *meta{ this->m_meta };
    const usize size{ str::length( meta ) };
    const usize key_size{ str::length( key ) };
    const usize start{ str::find( meta, size, key, key_size ) };

    if ( start == str::npos )
    {
        this->set_title( "" );
        return;
    }

    const usize from{ start + key_size };
    const usize to{ str::find( meta, size, end, str::length( end ), from ) };
    char        title[ AUDIO_TITLE_CAPACITY ]{ };

    str::copy( title, sizeof( title ), meta + from, ( to == str::npos ? size : to ) - from );
    this->set_title( title );
}

void audio_stream::read_vorbis_title( )
{
    const stb_vorbis_comment comments{ stb_vorbis_get_comment( this->m_vorbis ) };
    const char              *artist_key{ ATLAS_XORSTR_STATIC( "ARTIST=" ) };
    const char              *title_key{ ATLAS_XORSTR_STATIC( "TITLE=" ) };
    const usize              artist_size{ str::length( artist_key ) };
    const usize              title_size{ str::length( title_key ) };

    char artist[ AUDIO_TITLE_CAPACITY ]{ };
    char title[ AUDIO_TITLE_CAPACITY ]{ };

    for ( int index{ }; index < comments.comment_list_length; ++index )
    {
        const char *comment{ comments.comment_list[ index ] };
        const usize size{ str::length( comment ) };

        if ( size > artist_size && str::compare_ignore_case( comment, artist_size, artist_key, artist_size ) == 0 )
        {
            str::copy( artist, sizeof( artist ), comment + artist_size, size - artist_size );
        } else if ( size > title_size && str::compare_ignore_case( comment, title_size, title_key, title_size ) == 0 )
        {
            str::copy( title, sizeof( title ), comment + title_size, size - title_size );
        }
    }

    if ( !title[ 0 ] )
    {
        this->set_title( artist );
        return;
    }

    if ( !artist[ 0 ] )
    {
        this->set_title( title );
        return;
    }

    char line[ AUDIO_TITLE_CAPACITY ]{ };
    str::format_to( line, sizeof( line ), ATLAS_XORSTR( "{} - {}" ).get( ), artist, title );
    this->set_title( line );
}

void audio_stream::set_title( const char *title )
{
    atl::scoped_lock guard{ this->m_title_lock };

    str::copy( this->m_title, sizeof( this->m_title ), title );
}

void audio_stream::close( )
{
    void **handles[]{ &this->m_request, &this->m_connection, &this->m_session };

    for ( void **handle : handles )
    {
        if ( *handle )
        {
            I( WinHttpCloseHandle )( *handle );
            *handle = nullptr;
        }
    }
}

void audio_stream::drop( )
{
    if ( this->m_references.fetch_add( -1 ) != 1 )
    {
        return;
    }

    this->close( );
    atl::free( this->m_ring );
    atl::free( this->m_vorbis_memory );

    delete this;
}

void ATLAS_STDCALL audio_stream::reader( void *parameter )
{
    auto *stream{ static_cast< audio_stream * >( parameter ) };

    while ( !stream->m_closing.load( ) && stream->receive( ) == audio_stream_error_t::none )
    {
    }

    stream->close( );
    stream->m_ended.store( true );
    stream->drop( );
}

void audio_stream::render_voice( void *user, f32 *out, u32 frames )
{
    static_cast< audio_stream * >( user )->render( out, frames );
}
