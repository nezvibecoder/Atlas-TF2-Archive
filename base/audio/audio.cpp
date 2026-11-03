#include "audio/audio.hpp"

#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "runtime/memory.hpp"
#include "runtime/thread.hpp"

#define FAR
#define NEAR
#include <audioclient.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>
#undef FAR
#undef NEAR

ATLAS_NO_DESTROY audio_device g_audio{ };

inline constexpr i64 AUDIO_BUFFER_DURATION{ 400000 };
inline constexpr u32 AUDIO_WAIT{ 10 };
inline constexpr u64 AUDIO_RETRY_DELAY{ 1000 };
inline constexpr u32 AUDIO_MIN_RAMP{ 10 };

static void ATLAS_STDCALL audio_thread( void *parameter )
{
    ATLAS_UNUSED_PARAM( parameter );

    g_audio.run( );
}

static u32 read_u32( const u8 *at )
{
    u32 value{ };
    atl::memcpy( &value, at, sizeof( value ) );
    return value;
}

static u16 read_u16( const u8 *at )
{
    u16 value{ };
    atl::memcpy( &value, at, sizeof( value ) );
    return value;
}

bool parse_wav( audio_sound_t &sound, const u8 *wav, usize size )
{
    constexpr u32 riff_tag{ 0x46464952 };
    constexpr u32 wave_tag{ 0x45564157 };
    constexpr u32 format_tag{ 0x20746D66 };
    constexpr u32 data_tag{ 0x61746164 };
    constexpr u16 pcm_format{ 1 };

    if ( size < 12 || read_u32( wav ) != riff_tag || read_u32( wav + 8 ) != wave_tag )
    {
        return false;
    }

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
            if ( format != pcm_format || bits != 16 || rate != AUDIO_SOUND_RATE || channels < 1 || channels > AUDIO_CHANNELS )
            {
                return false;
            }

            sound = audio_sound_t{ reinterpret_cast< const i16 * >( body ), length / ( channels * 2u ), channels };
            return true;
        }

        at += 8 + length + ( length & 1 );
    }

    return false;
}

bool audio_device::startup( )
{
    if ( !I( LoadLibraryExA )( ATLAS_XORSTR( "ole32.dll" ).get( ), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 ) ||
         !I( LoadLibraryExA )( ATLAS_XORSTR( "winhttp.dll" ).get( ), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 ) )
    {
        return false;
    }

    for ( voice_t &voice : this->m_voices )
    {
        voice.m_event = I( CreateEventW )( nullptr, FALSE, FALSE, nullptr );

        if ( !voice.m_event )
        {
            return false;
        }
    }

    if ( !atl::create_hidden_thread( audio_thread, nullptr ) )
    {
        return false;
    }

    while ( !this->m_ready.load( ) )
    {
        I( Sleep )( 1 );
    }

    return this->m_ready.load( ) > 0;
}

void audio_device::run( )
{
    if ( FAILED( I( CoInitializeEx )( nullptr, COINIT_MULTITHREADED ) ) ||
         FAILED( I( CoCreateInstance )( __uuidof( MMDeviceEnumerator ), nullptr, CLSCTX_ALL,
                                        __uuidof( IMMDeviceEnumerator ),
                                        reinterpret_cast< void ** >( &this->m_devices ) ) ) )
    {
        this->m_ready.store( -1 );
        return;
    }

    this->m_ready.store( 1 );

    for ( ;; )
    {
        void *events[ AUDIO_VOICE_COUNT ]{ };
        u32   count{ };

        for ( const voice_t &voice : this->m_voices )
        {
            if ( voice.m_running )
            {
                events[ count++ ] = voice.m_event;
            }
        }

        if ( count )
        {
            I( WaitForMultipleObjects )( count, events, FALSE, AUDIO_WAIT );
        } else
        {
            I( Sleep )( AUDIO_WAIT );
        }

        for ( voice_t &voice : this->m_voices )
        {
            this->service( voice );
        }
    }
}

usize audio_device::open_voice( u32 sample_rate, audio_render_fn render, void *user, f32 volume )
{
    atl::scoped_lock guard{ this->m_lock };

    return this->open_locked( sample_rate, render, user, volume );
}

usize audio_device::open_locked( u32 sample_rate, audio_render_fn render, void *user, f32 volume )
{
    for ( usize index{ }; index < AUDIO_VOICE_COUNT; ++index )
    {
        voice_t &voice{ this->m_voices[ index ] };

        if ( voice.m_slot != slot_t::free )
        {
            continue;
        }

        voice.m_slot   = slot_t::open;
        voice.m_render = render;
        voice.m_user   = user;
        voice.m_rate   = sample_rate;
        voice.m_paused = false;
        voice.m_target = volume;
        voice.m_ramp   = 0;
        ++voice.m_serial;

        return index;
    }

    return AUDIO_NO_VOICE;
}

void audio_device::close_voice( usize voice )
{
    atl::scoped_lock guard{ this->m_lock };

    this->m_voices[ voice ].m_slot = slot_t::closing;
}

void audio_device::pause_voice( usize voice, bool paused )
{
    atl::scoped_lock guard{ this->m_lock };

    this->m_voices[ voice ].m_paused = paused;
}

void audio_device::slide_voice( usize voice, f32 volume, u32 milliseconds )
{
    atl::scoped_lock guard{ this->m_lock };

    voice_t &target{ this->m_voices[ voice ] };
    target.m_target = volume;
    target.m_ramp   = milliseconds;
    ++target.m_serial;
}

void audio_device::play( const audio_sound_t &sound, f32 volume )
{
    atl::scoped_lock guard{ this->m_lock };

    if ( this->m_sound_voice == AUDIO_NO_VOICE )
    {
        this->m_sound_voice = this->open_locked( AUDIO_SOUND_RATE, render_sounds, this, 1.0f );
    }

    for ( playing_t &playing : this->m_playing )
    {
        if ( !playing.m_sound )
        {
            playing = playing_t{ &sound, 0, volume };
            return;
        }
    }
}

bool audio_device::open_client( voice_t &voice )
{
    IMMDevice *device{ };

    if ( FAILED( this->m_devices->GetDefaultAudioEndpoint( eRender, eConsole, &device ) ) )
    {
        return false;
    }

    const HRESULT activated{ device->Activate( __uuidof( IAudioClient ), CLSCTX_ALL, nullptr,
                                               reinterpret_cast< void ** >( &voice.m_client ) ) };
    device->Release( );

    if ( FAILED( activated ) )
    {
        voice.m_client = nullptr;
        return false;
    }

    WAVEFORMATEX format{ };
    format.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels       = AUDIO_CHANNELS;
    format.nSamplesPerSec  = voice.m_rate;
    format.wBitsPerSample  = sizeof( f32 ) * 8;
    format.nBlockAlign     = AUDIO_CHANNELS * sizeof( f32 );
    format.nAvgBytesPerSec = voice.m_rate * format.nBlockAlign;

    constexpr DWORD flags{ AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                           AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY };

    if ( FAILED( voice.m_client->Initialize( AUDCLNT_SHAREMODE_SHARED, flags, AUDIO_BUFFER_DURATION, 0, &format, nullptr ) ) ||
         FAILED( voice.m_client->SetEventHandle( voice.m_event ) ) ||
         FAILED( voice.m_client->GetBufferSize( &voice.m_buffer_frames ) ) ||
         FAILED( voice.m_client->GetService( __uuidof( IAudioRenderClient ), reinterpret_cast< void ** >( &voice.m_output ) ) ) )
    {
        this->release_client( voice );
        return false;
    }

    return true;
}

void audio_device::release_client( voice_t &voice )
{
    if ( voice.m_output )
    {
        voice.m_output->Release( );
        voice.m_output = nullptr;
    }

    if ( voice.m_client )
    {
        voice.m_client->Stop( );
        voice.m_client->Release( );
        voice.m_client = nullptr;
    }

    voice.m_running = false;
}

void audio_device::service( voice_t &voice )
{
    slot_t slot{ };
    bool   paused{ };

    {
        atl::scoped_lock guard{ this->m_lock };

        slot   = voice.m_slot;
        paused = voice.m_paused;
    }

    if ( slot == slot_t::free )
    {
        return;
    }

    if ( slot == slot_t::closing )
    {
        this->release_client( voice );
        voice.m_gain       = 0.0f;
        voice.m_steps_left = 0;

        atl::scoped_lock guard{ this->m_lock };
        voice.m_slot = slot_t::free;
        return;
    }

    if ( !voice.m_client )
    {
        const u64 now{ I( GetTickCount64 )( ) };

        if ( now < voice.m_retry_at )
        {
            return;
        }

        if ( !this->open_client( voice ) )
        {
            voice.m_retry_at = now + AUDIO_RETRY_DELAY;
            return;
        }
    }

    if ( paused )
    {
        if ( voice.m_running )
        {
            voice.m_client->Stop( );
            voice.m_running = false;
        }

        return;
    }

    u32 padding{ };
    if ( FAILED( voice.m_client->GetCurrentPadding( &padding ) ) )
    {
        this->release_client( voice );
        return;
    }

    if ( const u32 frames{ voice.m_buffer_frames - padding } )
    {
        BYTE *buffer{ };
        if ( FAILED( voice.m_output->GetBuffer( frames, &buffer ) ) )
        {
            this->release_client( voice );
            return;
        }

        auto *out{ reinterpret_cast< f32 * >( buffer ) };

        {
            atl::scoped_lock guard{ this->m_lock };

            if ( voice.m_slot != slot_t::open )
            {
                atl::memset( out, 0, frames * AUDIO_CHANNELS * sizeof( f32 ) );
            } else
            {
                voice.m_render( voice.m_user, out, frames );
            }

            if ( voice.m_serial != voice.m_applied )
            {
                voice.m_applied    = voice.m_serial;
                voice.m_steps_left = math::bigger( voice.m_ramp, AUDIO_MIN_RAMP ) * voice.m_rate / 1000;
                voice.m_step       = ( voice.m_target - voice.m_gain ) / static_cast< f32 >( voice.m_steps_left );
                voice.m_goal       = voice.m_target;
            }
        }

        this->ramp( voice, out, frames );
        voice.m_output->ReleaseBuffer( frames, 0 );
    }

    if ( !voice.m_running )
    {
        voice.m_client->Start( );
        voice.m_running = true;
    }
}

void audio_device::ramp( voice_t &voice, f32 *out, u32 frames )
{
    for ( u32 frame{ }; frame < frames; ++frame )
    {
        if ( voice.m_steps_left )
        {
            voice.m_gain = --voice.m_steps_left ? voice.m_gain + voice.m_step : voice.m_goal;
        }

        out[ frame * AUDIO_CHANNELS ] *= voice.m_gain;
        out[ frame * AUDIO_CHANNELS + 1 ] *= voice.m_gain;
    }
}

void audio_device::render_sounds( void *user, f32 *out, u32 frames )
{
    auto *device{ static_cast< audio_device * >( user ) };

    atl::memset( out, 0, frames * AUDIO_CHANNELS * sizeof( f32 ) );

    for ( playing_t &playing : device->m_playing )
    {
        if ( !playing.m_sound )
        {
            continue;
        }

        const audio_sound_t &sound{ *playing.m_sound };
        const u32            count{ math::smaller( frames, sound.m_frames - playing.m_at ) };
        const f32            scale{ playing.m_volume / 32768.0f };

        for ( u32 frame{ }; frame < count; ++frame )
        {
            const i16 *sample{ sound.m_samples + ( playing.m_at + frame ) * sound.m_channels };
            const f32  left{ static_cast< f32 >( sample[ 0 ] ) * scale };

            out[ frame * AUDIO_CHANNELS ] += left;
            out[ frame * AUDIO_CHANNELS + 1 ] += sound.m_channels > 1 ? static_cast< f32 >( sample[ 1 ] ) * scale : left;
        }

        playing.m_at += count;

        if ( playing.m_at >= sound.m_frames )
        {
            playing.m_sound = nullptr;
        }
    }
}
