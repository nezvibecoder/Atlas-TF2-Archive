#include "audio/audio.hpp"

#include "gui.hpp"
#include "sound_data.hpp"
#include "theme.hpp"

namespace gui
{
    inline constexpr usize SOUND_VARIANTS{ 3 };

    struct sound_source_t
    {
        const u8 *m_wav{ };
        usize     m_size{ };
    };

    struct sound_set_t
    {
        audio_sound_t m_variants[ SOUND_VARIANTS ]{ };
        usize         m_count{ };
        usize         m_next{ };
    };

    static constexpr sound_source_t SOUND_SOURCES[ SOUND_COUNT ][ SOUND_VARIANTS ]{
        { { sounds::HOVER_0, sizeof( sounds::HOVER_0 ) }, { sounds::HOVER_1, sizeof( sounds::HOVER_1 ) }, { sounds::HOVER_2, sizeof( sounds::HOVER_2 ) } },
        { { sounds::CLICK_0, sizeof( sounds::CLICK_0 ) }, { sounds::CLICK_1, sizeof( sounds::CLICK_1 ) }, { sounds::CLICK_2, sizeof( sounds::CLICK_2 ) } },
        { { sounds::SLIDER_0, sizeof( sounds::SLIDER_0 ) }, { sounds::SLIDER_1, sizeof( sounds::SLIDER_1 ) }, { sounds::SLIDER_2, sizeof( sounds::SLIDER_2 ) } },
        { { sounds::TOGGLE_ON, sizeof( sounds::TOGGLE_ON ) } },
        { { sounds::TOGGLE_OFF, sizeof( sounds::TOGGLE_OFF ) } },
        { { sounds::TAB, sizeof( sounds::TAB ) } },
        { { sounds::POPUP_OPEN, sizeof( sounds::POPUP_OPEN ) } },
        { { sounds::POPUP_CLOSE, sizeof( sounds::POPUP_CLOSE ) } },
        { { sounds::MENU_OPEN, sizeof( sounds::MENU_OPEN ) } },
        { { sounds::MENU_CLOSE, sizeof( sounds::MENU_CLOSE ) } },
        { { sounds::NOTIFY, sizeof( sounds::NOTIFY ) } },
        { { sounds::SUCCESS, sizeof( sounds::SUCCESS ) } },
        { { sounds::WARN, sizeof( sounds::WARN ) } },
    };

    static sound_set_t s_sound_sets[ SOUND_COUNT ]{ };
    static u64         s_slider_free_at{ };

    bool load_sounds( )
    {
        for ( usize sound{ }; sound < SOUND_COUNT; ++sound )
        {
            sound_set_t &set{ s_sound_sets[ sound ] };

            for ( const sound_source_t &source : SOUND_SOURCES[ sound ] )
            {
                if ( !source.m_wav )
                {
                    break;
                }

                if ( !parse_wav( set.m_variants[ set.m_count++ ], source.m_wav, source.m_size ) )
                {
                    return false;
                }
            }
        }

        return true;
    }

    void play_sound( sound_t sound )
    {
        if ( !g_theme.m_sounds )
        {
            return;
        }

        sound_set_t         &set{ s_sound_sets[ sound ] };
        const audio_sound_t &clip{ set.m_variants[ set.m_next ] };

        if ( sound == SOUND_SLIDER )
        {
            const u64 now{ clock( ) };

            if ( now < s_slider_free_at )
            {
                return;
            }

            s_slider_free_at = now + clip.m_frames * 1000u / AUDIO_SOUND_RATE;
        }

        set.m_next = ( set.m_next + 1 ) % set.m_count;
        g_audio.play( clip, g_theme.m_sound_volume );
    }
} // namespace gui
