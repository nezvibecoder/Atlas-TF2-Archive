#pragma once

#include "link.hpp"

#include "runtime/array.hpp"
#include "runtime/atomic.hpp"
#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"

inline constexpr usize SOUNDBOARD_CAPACITY{ 64 };
inline constexpr usize SOUNDBOARD_NAME_LENGTH{ 63 };
inline constexpr usize SOUNDBOARD_FILE_LENGTH{ 260 };
inline constexpr usize SOUNDBOARD_PATH_CAPACITY{ 1024 };

class soundboard
{
public:
    void refresh( );
    void play( usize index );
    void stop( );
    void on_frame( );
    void on_level_shutdown( );
    void on_voice_status( i32 entity, bool talking );
    void decode( i32 generation );
    i32  on_compressed_voice( u8 *out, i32 size, i32 written );

    bool               busy( ) const;
    const char *const *labels( ) const;
    usize              count( ) const;
    usize             *selected( );

private:
    struct sound_t
    {
        wchar_t                                m_file[ SOUNDBOARD_FILE_LENGTH ]{ };
        fixed_string< SOUNDBOARD_NAME_LENGTH > m_name{ };
    };

    bool pressed( );
    bool prepare_encoder( );

    atl::array< sound_t, SOUNDBOARD_CAPACITY >      m_sounds{ };
    atl::array< const char *, SOUNDBOARD_CAPACITY > m_labels{ };
    usize                                           m_count{ };
    usize                                           m_selected{ };
    bool                                            m_key_held{ };
    bool                                            m_recording{ };
    void                                           *m_encoder{ };

    atl::mutex                             m_lock{ };
    i32                                    m_generation{ };
    wchar_t                                m_request[ SOUNDBOARD_PATH_CAPACITY ]{ };
    fixed_string< SOUNDBOARD_NAME_LENGTH > m_request_name{ };
    atl::vector< i16 >                     m_samples{ };
    usize                                  m_cursor{ };
    u64                                    m_last_read{ };

    atl::atomic< bool > m_failed{ };
    atl::atomic< bool > m_decoding{ };
    atl::atomic< bool > m_active{ };
};

extern soundboard g_soundboard;
