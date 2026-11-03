#pragma once

#include "audio/audio.hpp"
#include "audio/codecs.hpp"
#include "runtime/atomic.hpp"
#include "runtime/mutex.hpp"

inline constexpr usize AUDIO_SPECTRUM_BINS{ 128 };
inline constexpr usize AUDIO_TITLE_CAPACITY{ 96 };

enum class audio_stream_state_t : u8
{
    playing,
    stalled,
    stopped,
};

enum class audio_stream_error_t : u8
{
    none,
    url,
    connect,
    status,
    format,
    thread,
};

class audio_stream
{
public:
    static audio_stream *open( const char *url, audio_stream_error_t &error );

    void release( );

    void play( );
    void pause( bool paused );
    void slide( f32 volume, u32 milliseconds );

    audio_stream_state_t state( ) const;
    i32                  buffer_percent( ) const;
    bool                 connected( ) const;
    void                 title( char *out, usize capacity ) const;
    void                 spectrum( f32 ( &bins )[ AUDIO_SPECTRUM_BINS ] ) const;

private:
    enum class codec_t : u8
    {
        mp3,
        vorbis,
    };

    static constexpr usize ICY_META_CAPACITY{ 255 * 16 };
    static constexpr usize HISTORY_SIZE{ AUDIO_SPECTRUM_BINS * 2 };

    audio_stream_error_t connect( const char *url );
    audio_stream_error_t receive( );
    audio_stream_error_t filter( const u8 *data, u32 size );
    audio_stream_error_t decode( const u8 *data, u32 size );
    audio_stream_error_t decode_mp3( );
    audio_stream_error_t decode_vorbis( );

    bool push( const f32 *left, const f32 *right, usize stride, u32 frames, u32 rate );
    void render( f32 *out, u32 frames );
    void read_icy_title( );
    void read_vorbis_title( );
    void set_title( const char *title );
    void close( );
    void drop( );

    static void ATLAS_STDCALL reader( void *parameter );
    static void               render_voice( void *user, f32 *out, u32 frames );

    atl::atomic< i32 >  m_references{ 1 };
    atl::atomic< bool > m_closing{ };
    atl::atomic< bool > m_ended{ };
    atl::atomic< bool > m_stalled{ };

    void *m_session{ };
    void *m_connection{ };
    void *m_request{ };

    codec_t m_codec{ };
    u32     m_metaint{ };
    u32     m_audio_left{ };
    u32     m_meta_left{ };
    u32     m_meta_size{ };
    char    m_meta[ ICY_META_CAPACITY + 1 ]{ };

    u8    m_input[ 64 * 1024 ]{ };
    usize m_input_size{ };
    usize m_input_used{ };

    mp3dec_t    m_mp3{ };
    stb_vorbis *m_vorbis{ };
    char       *m_vorbis_memory{ };

    f32               *m_ring{ };
    u32                m_rate{ };
    u32                m_capacity{ };
    atl::atomic< u32 > m_written{ };
    atl::atomic< u32 > m_read{ };

    usize m_voice{ AUDIO_NO_VOICE };
    f32   m_volume{ };

    mutable atl::mutex m_title_lock{ };
    char               m_title[ AUDIO_TITLE_CAPACITY ]{ };

    mutable atl::mutex m_history_lock{ };
    f32                m_history[ HISTORY_SIZE ]{ };
    usize              m_history_at{ };
};
