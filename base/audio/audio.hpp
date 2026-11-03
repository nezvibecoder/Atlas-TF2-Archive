#pragma once

#include "runtime/atomic.hpp"
#include "runtime/mutex.hpp"
#include "util/typedefs.hpp"

struct IAudioClient;
struct IAudioRenderClient;
struct IMMDeviceEnumerator;

inline constexpr u32   AUDIO_CHANNELS{ 2 };
inline constexpr u32   AUDIO_SOUND_RATE{ 48000 };
inline constexpr usize AUDIO_VOICE_COUNT{ 4 };
inline constexpr usize AUDIO_SOUND_COUNT{ 8 };
inline constexpr usize AUDIO_NO_VOICE{ AUDIO_VOICE_COUNT };

using audio_render_fn = void ( * )( void *user, f32 *out, u32 frames );

struct audio_sound_t
{
    const i16 *m_samples{ };
    u32        m_frames{ };
    u32        m_channels{ };
};

bool parse_wav( audio_sound_t &sound, const u8 *wav, usize size );

class audio_device
{
public:
    bool startup( );
    void run( );

    usize open_voice( u32 sample_rate, audio_render_fn render, void *user, f32 volume );
    void  close_voice( usize voice );
    void  pause_voice( usize voice, bool paused );
    void  slide_voice( usize voice, f32 volume, u32 milliseconds );

    void play( const audio_sound_t &sound, f32 volume );

private:
    enum class slot_t : u8
    {
        free,
        open,
        closing,
    };

    struct voice_t
    {
        slot_t          m_slot{ slot_t::free };
        audio_render_fn m_render{ };
        void           *m_user{ };
        u32             m_rate{ };
        bool            m_paused{ };
        f32             m_target{ };
        u32             m_ramp{ };
        u32             m_serial{ };

        IAudioClient       *m_client{ };
        IAudioRenderClient *m_output{ };
        void               *m_event{ };
        u32                 m_buffer_frames{ };
        bool                m_running{ };
        u64                 m_retry_at{ };
        u32                 m_applied{ };
        f32                 m_gain{ };
        f32                 m_goal{ };
        f32                 m_step{ };
        u32                 m_steps_left{ };
    };

    struct playing_t
    {
        const audio_sound_t *m_sound{ };
        u32                  m_at{ };
        f32                  m_volume{ };
    };

    usize open_locked( u32 sample_rate, audio_render_fn render, void *user, f32 volume );
    bool  open_client( voice_t &voice );
    void  release_client( voice_t &voice );
    void  service( voice_t &voice );
    void  ramp( voice_t &voice, f32 *out, u32 frames );

    static void render_sounds( void *user, f32 *out, u32 frames );

    IMMDeviceEnumerator *m_devices{ };
    atl::atomic< i32 >   m_ready{ };

    atl::mutex m_lock{ };
    voice_t    m_voices[ AUDIO_VOICE_COUNT ]{ };
    playing_t  m_playing[ AUDIO_SOUND_COUNT ]{ };
    usize      m_sound_voice{ AUDIO_NO_VOICE };
};

extern audio_device g_audio;
