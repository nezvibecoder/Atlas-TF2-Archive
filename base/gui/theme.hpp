#pragma once

#include "runtime/obfuscation.hpp"
#include "util/typedefs.hpp"

// I think this is better for organization, after all we don't need to go after calls through the code
// We can just find what we want here and cross references and change the values easier.

inline constexpr usize MENU_LABEL_LENGTH{ 64 };
inline constexpr usize MENU_TOOLTIP_LENGTH{ 256 }; // Setting this to 512 crashes the game. IDK why.

// icons
inline constexpr const char *ICON_USER{ "!" };
inline constexpr const char *ICON_CROSSHAIR{ "\"" };
inline constexpr const char *ICON_EYE{ "#" };
inline constexpr const char *ICON_WRENCH{ "$" };
inline constexpr const char *ICON_SETTINGS{ "%" };
inline constexpr const char *ICON_CODE{ "&" };
inline constexpr const char *ICON_SAVE{ "*" };
inline constexpr const char *ICON_CHEVRON_DOWN{ "+" };
inline constexpr const char *ICON_SEARCH{ "," };
inline constexpr const char *ICON_CHEVRON_UP{ "-" };
inline constexpr const char *ICON_CROSS{ "." };
inline constexpr const char *ICON_SPARKLES{ "/" };
inline constexpr const char *ICON_ZAP{ "0" };
inline constexpr const char *ICON_DOLLAR{ "1" };
inline constexpr const char *ICON_BAN{ "2" };
inline constexpr const char *ICON_FLAME{ "3" };
inline constexpr const char *ICON_PLAYERS{ "4" };
inline constexpr const char *ICON_LOADER{ "5" };
inline constexpr const char *ICON_TRIANGLE_ALERT{ "6" };
inline constexpr const char *ICON_RULER{ "7" };
inline constexpr const char *ICON_RADIO{ "8" };
inline constexpr const char *ICON_MUSIC{ "9" };

inline const char *MENU_FONT( )
{
    return ATLAS_XORSTR_STATIC( "Atlas Compact" );
}
inline const char *MENU_FONT_BOLD( )
{
    return ATLAS_XORSTR_STATIC( "Atlas Compact Bd" );
}
inline const char *MENU_FONT_TINY( )
{
    return ATLAS_XORSTR_STATIC( "Atlas Compact Sm" );
}
inline const char *MENU_FONT_MONO( )
{
    return ATLAS_XORSTR_STATIC( "Atlas Mono" );
}

inline constexpr i32 MENU_ICON_SIZE{ 13 };
inline constexpr i32 MENU_ICON_SIZE_LARGE{ 16 };

inline constexpr i32 TINY_FONT_INK_TOP{ 1 };
inline constexpr i32 TINY_FONT_INK_HEIGHT{ 7 };

inline constexpr i32 LABEL_FONT_INK_TOP{ 1 };
inline constexpr i32 LABEL_FONT_INK_HEIGHT{ 8 };

inline constexpr i32 MENU_BASE_WIDTH{ 560 };
inline constexpr i32 MENU_BASE_HEIGHT{ 400 };
inline constexpr i32 MENU_SCREEN_MARGIN{ 48 };
inline constexpr i32 MENU_HEADER_HEIGHT{ 23 };
inline constexpr i32 MENU_HEADER_PADDING{ 7 };
inline constexpr i32 MENU_HEADER_GAP{ 6 };
inline constexpr i32 MENU_LOGO_SIZE{ 14 };
inline constexpr i32 MENU_DECORATION_SIZE{ 16 };

inline constexpr i32 MENU_RAIL_WIDTH{ 32 };
inline constexpr i32 MENU_RAIL_ITEM_HEIGHT{ 31 };
inline constexpr i32 MENU_RAIL_TOP_PADDING{ 3 };
inline constexpr i32 MENU_RAIL_FOOT_HEIGHT{ 31 };
inline constexpr i32 MENU_SELECTION_BAR{ 2 };

inline constexpr i32 MENU_SEARCH_WIDTH{ 96 };
inline constexpr i32 MENU_SEARCH_HEIGHT{ 15 };
inline constexpr i32 MENU_SEARCH_MARGIN{ 8 };

inline constexpr i32 MENU_SUBNAV_WIDTH{ 92 };
inline constexpr i32 MENU_SUBNAV_ITEM_HEIGHT{ 20 };
inline constexpr i32 MENU_SUBNAV_TOP_PADDING{ 3 };
inline constexpr i32 MENU_SUBNAV_INDENT{ 10 };

inline constexpr i32 MENU_FOOTER_HEIGHT{ MENU_HEADER_HEIGHT };

inline constexpr i32 MENU_RAIL_FOOT_OFFSET{ MENU_BASE_HEIGHT - MENU_HEADER_HEIGHT - MENU_FOOTER_HEIGHT -
                                            MENU_RAIL_TOP_PADDING - MENU_RAIL_FOOT_HEIGHT };
inline constexpr i32 MENU_FOOTER_PADDING{ MENU_HEADER_PADDING };
inline constexpr i32 MENU_FOOTER_PADDING_RIGHT{ MENU_SEARCH_MARGIN };
inline constexpr i32 MENU_FOOTER_DOT{ 5 };

inline constexpr i32 MENU_CONTENT_PADDING{ 7 };
inline constexpr i32 MENU_CONTENT_PADDING_RIGHT{ 6 };
inline constexpr i32 MENU_COLUMN_GAP{ 7 };
inline constexpr i32 MENU_COLUMN_WIDTH{ 208 };

inline constexpr i32 PANEL_DOCK_GAP{ 6 };
inline constexpr i32 PANEL_HEADER_HEIGHT{ MENU_HEADER_HEIGHT };
inline constexpr i32 PANEL_DOCK_PADDING{ MENU_CONTENT_PADDING };

inline constexpr i32 CARD_TITLE_HEIGHT{ 18 }; // 1px frame + 17px bar
inline constexpr i32 CARD_PADDING_X{ 7 };
inline constexpr i32 CARD_PADDING_TOP{ 4 };    // 1px rule + 3px
inline constexpr i32 CARD_PADDING_BOTTOM{ 6 }; // 5px + 1px frame
inline constexpr i32 CARD_BLOCK_GAP{ 0 };      // rows sit flush, blocks carry their own padding

inline constexpr i32 WIDGET_ROW_HEIGHT{ 17 };
inline constexpr i32 WIDGET_ROW_BLEED{ 4 }; // hover strip runs past the card padding
inline constexpr i32 WIDGET_CHECKBOX_SIZE{ 9 };
inline constexpr i32 WIDGET_CHECKBOX_GAP{ 6 };
inline constexpr i32 WIDGET_LABEL_HEIGHT{ 14 };
inline constexpr i32 WIDGET_LABEL_GAP{ 0 };
inline constexpr i32 WIDGET_LABEL_INDENT{ 15 }; // checkbox + gap
inline constexpr i32 WIDGET_BLOCK_PADDING_TOP{ 1 };
inline constexpr i32 WIDGET_BLOCK_PADDING_BOTTOM{ 4 };
inline constexpr i32 WIDGET_WELL_HEIGHT{ 18 }; // 16px well + 1px frame either side
inline constexpr i32 WIDGET_WELL_PADDING{ 5 };
inline constexpr i32 WIDGET_KEYBIND_HEIGHT{ 17 };
inline constexpr i32 WIDGET_CHIP_HEIGHT{ 15 };
inline constexpr i32 WIDGET_CHIP_PADDING{ 4 };
inline constexpr i32 WIDGET_KEYBIND_MIN_WIDTH{ 30 };
inline constexpr i32 WIDGET_BIND_POPUP_WIDTH{ 60 };
inline constexpr i32 WIDGET_BIND_MODE_COUNT{ 3 };
inline constexpr i32 WIDGET_SLIDER_HEIGHT{ 5 }; // 3px track + 1px frame either side
inline constexpr i32 WIDGET_SEGMENT_HEIGHT{ 15 };
inline constexpr i32 WIDGET_SEGMENT_GAP{ 4 };
inline constexpr i32 WIDGET_MARK_SIZE{ 3 };
inline constexpr i32 WIDGET_LIST_ROW_HEIGHT{ 15 };
inline constexpr i32 WIDGET_LIST_PADDING{ 5 };
inline constexpr i32 WIDGET_LIST_MARGIN_TOP{ 2 };
inline constexpr i32 WIDGET_LIST_MARGIN_BOTTOM{ 4 };
inline constexpr i32 WIDGET_BUTTON_HEIGHT{ 17 };
inline constexpr i32 WIDGET_BUTTON_WIDTH{ 40 };
inline constexpr i32 WIDGET_BUTTON_GAP{ 4 };
inline constexpr i32 COMBO_POPUP_GAP{ 0 }; // the list hangs straight off the well
// hard ceiling, not a window: the popup has no scroll offset, so anything past this row is drawn
// nowhere and hit-tested nowhere. it has to stay >= the longest list any combo box is built with
inline constexpr i32 COMBO_POPUP_MAX_ROWS{ 16 };

inline constexpr i32 PICKER_SWATCH_WIDTH{ 20 };
inline constexpr i32 PICKER_SWATCH_HEIGHT{ 9 };
inline constexpr i32 PICKER_POPUP_GAP{ 3 }; // swatch bottom -> popup top
inline constexpr i32 PICKER_POPUP_PADDING{ 6 };
inline constexpr i32 PICKER_FIELD_SIZE{ 88 };
inline constexpr i32 PICKER_BAR_WIDTH{ 8 };
inline constexpr i32 PICKER_BAR_GAP{ 6 };
inline constexpr i32 PICKER_ROW_GAP{ 6 }; // field bottom -> footer top
inline constexpr i32 PICKER_FOOTER_HEIGHT{ 16 };
inline constexpr i32 PICKER_PREVIEW_WIDTH{ 20 };
inline constexpr i32 PICKER_HEX_PADDING{ 5 };
inline constexpr i32 PICKER_CHECKER_SIZE{ 3 };
inline constexpr i32 PICKER_CURSOR_RADIUS{ 4 };
inline constexpr i32 PICKER_KNOB_HEIGHT{ 4 };
inline constexpr i32 PICKER_KNOB_BLEED{ 2 };
inline constexpr i32 PICKER_POPUP_RISE{ 5 };

namespace gui
{
    struct theme_t
    {
        color_t m_accent{ 239, 75, 72 };
        color_t m_accent_alt{ 222, 82, 79 };
        color_t m_checkbox_top{ 239, 75, 72 };
        color_t m_checkbox_top_hover{ 243, 99, 95 };
        color_t m_slider_drag{ 243, 99, 95 };
        color_t m_input_focus_border{ 176, 74, 74 };
        color_t m_selected_background{ 25, 26, 30 };
        color_t m_row_selected_background{ 29, 30, 34 };

        color_t m_shadow{ 0, 0, 0, 72 };
        bool    m_blur_background{ false };
        bool    m_snap_indicators{ false };
        bool    m_dock_panels{ true };
        usize   m_particles{ };
        bool    m_sounds{ true };
        float   m_sound_volume{ 1.0f };

        void set_accent( color_t accent );
    };

} // namespace gui

extern gui::theme_t g_theme;

// color pallete
inline constexpr color_t MENU_BACKGROUND{ 23, 24, 27, 236 };
inline constexpr color_t MENU_HEADER_BACKGROUND{ 18, 19, 22, 242 };
inline constexpr color_t MENU_RAIL_BACKGROUND{ 14, 15, 18, 242 };
inline constexpr color_t MENU_SUBNAV_BACKGROUND{ 19, 20, 23, 242 };
inline constexpr color_t MENU_OUTER_BORDER{ 5, 5, 6 };
inline constexpr color_t MENU_INNER_BORDER{ 5, 5, 6 };
inline constexpr color_t MENU_SECTION_SEPARATOR{ 5, 5, 6 };
inline constexpr color_t MENU_SUBNAV_SEPARATOR{ 5, 5, 6 };
inline constexpr color_t MENU_FOOTER_TEXT{ 77, 78, 83 };
inline constexpr color_t MENU_FOOTER_TEXT_DIM{ 77, 78, 83 };
inline constexpr color_t MENU_TEXT{ 232, 233, 236 };
inline constexpr color_t MENU_TEXT_MUTED{ 150, 151, 156 };
inline constexpr color_t MENU_TEXT_DIM{ 94, 95, 99 };
inline constexpr color_t MENU_TEXT_FAINT{ 77, 78, 83 };
inline constexpr color_t MENU_LOGO_GLYPH{ 242, 243, 245 };
inline constexpr color_t MENU_WORDMARK{ 242, 243, 245 };

inline constexpr color_t RAIL_IDLE_COLOR{ 94, 95, 99 };
inline constexpr color_t RAIL_HOVER_COLOR{ 230, 231, 234 };
inline constexpr color_t RAIL_HOVER_BACKGROUND{ 23, 24, 28, 130 };
inline constexpr color_t RAIL_ACTIVE_BACKGROUND{ 23, 24, 28 };
inline constexpr color_t RAIL_FOOT_COLOR{ 63, 64, 69 };

inline constexpr color_t SUBNAV_IDLE_COLOR{ 117, 118, 123 };
inline constexpr color_t SUBNAV_HOVER_COLOR{ 216, 217, 221 };
inline constexpr color_t SUBNAV_ACTIVE_COLOR{ 230, 231, 234 };
inline constexpr color_t SUBNAV_HOVER_BACKGROUND{ 23, 24, 28, 130 };
inline constexpr color_t SUBNAV_ACTIVE_BACKGROUND{ 23, 24, 28 };

inline constexpr color_t CARD_BACKGROUND{ 25, 26, 30 };
inline constexpr color_t CARD_TITLE_BACKGROUND{ 30, 31, 36 };
inline constexpr color_t CARD_BORDER{ 44, 45, 50 };
inline constexpr color_t CARD_RULE{ 44, 45, 50 };
inline constexpr color_t CARD_TITLE_TEXT{ 223, 224, 227 };

inline constexpr color_t CONTROL_BACKGROUND{ 16, 17, 20 };
inline constexpr color_t CONTROL_BACKGROUND_HOVER{ 22, 23, 27 };
inline constexpr color_t CONTROL_BORDER{ 43, 44, 49 };
inline constexpr color_t CONTROL_BORDER_HOVER{ 61, 62, 68 };
inline constexpr color_t CONTROL_LABEL{ 150, 151, 156 };
inline constexpr color_t CONTROL_VALUE{ 207, 208, 212 };
inline constexpr color_t STATUS_WARNING{ 226, 170, 74 };

inline constexpr color_t CHECKBOX_BOX_IDLE{ 20, 21, 24 };
inline constexpr color_t CHECKBOX_BORDER_IDLE{ 53, 54, 59 };
inline constexpr color_t CHECKBOX_TEXT_IDLE{ 150, 151, 156 };
inline constexpr color_t CHECKBOX_TEXT_HOVER{ 198, 199, 203 };
inline constexpr color_t CHECKBOX_TEXT_ACTIVE{ 232, 233, 236 };
inline constexpr color_t CHECKBOX_MARK_COLOR{ 245, 246, 247 };
inline constexpr color_t CHECKBOX_KEY_COLOR{ 94, 95, 99 };

inline constexpr color_t SWATCH_BORDER{ 59, 60, 65 };

inline constexpr color_t SLIDER_BORDER{ 38, 39, 43 };
inline constexpr color_t SLIDER_VALUE_COLOR{ 236, 238, 240 };
inline constexpr color_t SLIDER_NOTCH_COLOR{ 245, 246, 247 };

inline constexpr color_t ROW_HOVER_BACKGROUND{ 32, 33, 39 };

inline constexpr color_t LIST_ROW_BACKGROUND{ 34, 35, 42 };
inline constexpr color_t LIST_META_COLOR{ 106, 107, 112 };
inline constexpr color_t LIST_TEXT_IDLE{ 154, 155, 160 };
inline constexpr color_t LIST_TEXT_ACTIVE{ 240, 241, 243 };

inline constexpr color_t POPUP_BACKGROUND{ 14, 15, 18 };
inline constexpr color_t POPUP_ROW_HOVER{ 28, 29, 33 };
inline constexpr color_t POPUP_ROW_SELECTED{ 29, 30, 34 };

inline constexpr color_t BUTTON_BACKGROUND{ 32, 33, 38 };
inline constexpr color_t BUTTON_BACKGROUND_HOVER{ 40, 41, 48 };
inline constexpr color_t BUTTON_BACKGROUND_PRESS{ 22, 23, 27 };
inline constexpr color_t BUTTON_BORDER{ 48, 49, 54 };
inline constexpr color_t BUTTON_TEXT{ 201, 202, 207 };
inline constexpr color_t BUTTON_TEXT_HOVER{ 240, 241, 243 };
inline constexpr color_t BUTTON_TEXT_PRESS{ 245, 246, 247 };

inline constexpr color_t PANEL_BACKGROUND{ 23, 24, 27, 220 };
inline constexpr color_t PANEL_HEADER_BACKGROUND{ 18, 19, 22, 236 };
inline constexpr color_t PANEL_INNER_BORDER{ 5, 5, 6 };

inline constexpr color_t CHIP_IDLE_BACKGROUND{ 16, 17, 20 };
inline constexpr color_t CHIP_HOVER_BACKGROUND{ 26, 27, 32 };
inline constexpr color_t CHIP_IDLE_TEXT{ 150, 151, 156 };
inline constexpr color_t CHIP_ACTIVE_TEXT{ 182, 183, 188 };

inline constexpr color_t SEARCH_BACKGROUND{ 14, 15, 18 };
inline constexpr color_t SEARCH_BORDER{ 43, 44, 49 };

inline constexpr i32     MENU_SHADOW_SPREAD{ 26 };
inline constexpr i32     MENU_SHADOW_DROP{ 6 };
inline constexpr color_t MENU_SHADOW{ 0, 0, 0, 72 };

inline constexpr color_t PICKER_POPUP_BACKGROUND{ 14, 15, 18, 252 };
inline constexpr color_t PICKER_HEX_BACKGROUND{ 16, 17, 20 };
inline constexpr color_t PICKER_CHECKER_LIGHT{ 62, 63, 67 };
inline constexpr color_t PICKER_CHECKER_DARK{ 38, 39, 42 };
inline constexpr color_t PICKER_CURSOR_COLOR{ 245, 246, 247 };
inline constexpr color_t PICKER_CURSOR_SHADOW{ 12, 13, 15, 200 };

inline constexpr i32     SCROLLBAR_WIDTH{ 2 };
inline constexpr i32     SCROLLBAR_INSET{ 1 };
inline constexpr color_t SCROLLBAR_TRACK{ 16, 17, 20 };
inline constexpr color_t SCROLLBAR_THUMB{ 74, 75, 80 };
inline constexpr i32     SCROLL_WHEEL_STEP{ 17 };
inline constexpr u32     SCROLL_GLIDE_DURATION{ 110 };
inline constexpr i32     SCROLL_SHADOW_DEPTH{ 16 };
inline constexpr i32     SCROLL_SHADOW_DROP{ 4 }; // clears the title rule before it ramps in
inline constexpr color_t SCROLL_SHADOW{ 8, 9, 11, 190 };

inline constexpr u32     TOOLTIP_FADE_IN_DURATION{ 150 };
inline constexpr u32     TOOLTIP_FADE_OUT_DURATION{ 110 };
inline constexpr i32     TOOLTIP_PADDING_X{ 5 };
inline constexpr i32     TOOLTIP_HEIGHT{ 17 };
inline constexpr i32     TOOLTIP_LINE_ADVANCE{ 14 };
inline constexpr i32     TOOLTIP_OFFSET_X{ 11 };
inline constexpr i32     TOOLTIP_OFFSET_Y{ 14 };
inline constexpr color_t TOOLTIP_BACKGROUND{ 14, 15, 18, 248 };
inline constexpr color_t TOOLTIP_BORDER{ 43, 44, 49 };
inline constexpr color_t TOOLTIP_TEXT{ 201, 202, 207 };

inline constexpr color_t INPUT_BACKGROUND_FOCUS{ 20, 21, 25 };
inline constexpr color_t INPUT_TEXT{ 232, 233, 236 };
inline constexpr i32     INPUT_RAIL_HEIGHT{ 1 };
inline constexpr i32     INPUT_PADDING_X{ 5 };
inline constexpr i32     INPUT_ICON_GAP{ 4 };
inline constexpr usize   MENU_TEXT_LENGTH{ 48 };

inline constexpr i32     EDITOR_LINE_HEIGHT{ 14 };
inline constexpr i32     EDITOR_PADDING{ 6 };
inline constexpr i32     EDITOR_TAB_WIDTH{ 4 };
inline constexpr i32     EDITOR_WHEEL_LINES{ 3 };
inline constexpr u32     EDITOR_DOUBLE_CLICK{ 400 };
inline constexpr color_t EDITOR_BACKGROUND{ 16, 17, 20 };
inline constexpr color_t EDITOR_BORDER{ 43, 44, 49 };
inline constexpr color_t EDITOR_GUTTER{ 19, 20, 24 };
inline constexpr color_t EDITOR_GUTTER_TEXT{ 77, 78, 83 };
inline constexpr color_t EDITOR_CURRENT_LINE{ 23, 24, 29 };
inline constexpr color_t EDITOR_SELECTION{ 52, 64, 98 };
inline constexpr color_t EDITOR_ERROR_LINE{ 86, 30, 32 };
inline constexpr color_t EDITOR_CARET{ 232, 233, 236 };
inline constexpr color_t EDITOR_TEXT{ 212, 213, 217 };
inline constexpr color_t EDITOR_KEYWORD{ 198, 120, 221 };
inline constexpr color_t EDITOR_LIBRARY{ 97, 175, 239 };
inline constexpr color_t EDITOR_STRING{ 152, 195, 121 };
inline constexpr color_t EDITOR_NUMBER{ 209, 154, 102 };
inline constexpr color_t EDITOR_COMMENT{ 98, 104, 116 };
inline constexpr color_t CONSOLE_INFO{ 150, 151, 156 };
inline constexpr color_t CONSOLE_ERROR{ 239, 108, 104 };

// animation timers
inline constexpr u32 WIDGET_REVEAL_DURATION{ 220 };
inline constexpr u8  WIDGET_REVEAL_SPLIT{ 150 };
inline constexpr u32 WIDGET_TOGGLE_DURATION{ 140 };
inline constexpr u32 WIDGET_HOVER_DURATION{ 100 };
inline constexpr u32 COMBO_EXPAND_DURATION{ 170 };
inline constexpr u32 COMBO_COLLAPSE_DURATION{ 130 };
inline constexpr u32 PICKER_EXPAND_DURATION{ 160 };
inline constexpr u32 PICKER_COLLAPSE_DURATION{ 110 };

inline constexpr u32   INPUT_FOCUS_DURATION{ 190 };
inline constexpr u32   INPUT_PLACEHOLDER_DURATION{ 130 };
inline constexpr u32   INPUT_CARET_PERIOD{ 1150 };
inline constexpr u32   INPUT_CARET_SOLID{ 420 };
inline constexpr float INPUT_CARET_GLIDE{ 62.0f };

inline constexpr u32 CONTENT_ENTER_DURATION{ 220 };
inline constexpr i32 CONTENT_ENTER_RISE{ 8 };
inline constexpr i32 CONTENT_CARD_STAGGER{ 90 };
inline constexpr i32 CONTENT_CARD_FADE{ 130 };
inline constexpr u32 STRIP_ACTIVE_DURATION{ 200 };
inline constexpr u32 STRIP_HOVER_DURATION{ 130 };
inline constexpr u32 ANIMATION_MAX_DELTA{ 50 };
inline constexpr u32 BACKGROUND_FADE_DURATION{ 260 };
