#pragma once

#include "libs/lazy_importer.hpp"
#include "util/string.hpp"

namespace render::gdi
{
#if defined( ATLAS_PROVIDE_OWN_WINDEFS )
    using HDC     = void *;
    using HGDIOBJ = void *;

    struct SIZE
    {
        i32 cx{ };
        i32 cy{ };
    };

    struct FIXED
    {
        u16 fract{ };
        i16 value{ };
    };

    struct MAT2
    {
        FIXED eM11{ };
        FIXED eM12{ };
        FIXED eM21{ };
        FIXED eM22{ };
    };

    struct GLYPHMETRICS
    {
        u32     gmBlackBoxX{ };
        u32     gmBlackBoxY{ };
        point_t gmptGlyphOrigin{ };
        i16     gmCellIncX{ };
        i16     gmCellIncY{ };
    };

    inline constexpr i32 PS_SOLID{ 0 };
    inline constexpr i32 TRANSPARENT{ 1 };
    inline constexpr i32 FW_DONTCARE{ 0 };
    inline constexpr i32 FW_LIGHT{ 300 };
    inline constexpr i32 FW_NORMAL{ 400 };
    inline constexpr i32 FW_MEDIUM{ 500 };
    inline constexpr i32 FW_SEMIBOLD{ 600 };
    inline constexpr i32 FW_BOLD{ 700 };
    inline constexpr u32 DEFAULT_CHARSET{ 1 };
    inline constexpr u32 OUT_DEFAULT_PRECIS{ 0 };
    inline constexpr u32 CLIP_DEFAULT_PRECIS{ 0 };
    inline constexpr u32 NONANTIALIASED_QUALITY{ 3 };
    inline constexpr u32 PROOF_QUALITY{ 2 };
    inline constexpr u32 ANTIALIASED_QUALITY{ 4 };
    inline constexpr u32 DEFAULT_PITCH{ 0 };
    inline constexpr u32 FF_DONTCARE{ 0 };
    inline constexpr u32 PATCOPY{ 0x00F00021 };
    inline constexpr u32 GGO_BITMAP{ 1 };
    inline constexpr u32 GGO_GRAY8_BITMAP{ 6 };
#endif

    // A 32bpp top-down DIB selected into a memory DC; GDI draws into m_pixels directly.
    struct bitmap_t
    {
        HDC     m_context{ };
        HGDIOBJ m_bitmap{ };
        HGDIOBJ m_previous{ };
        u32    *m_pixels{ };
        u32     m_width{ };
        u32     m_height{ };
    };

    class api
    {
    public:
        bool Ready( ) const
        {
            return I( CreatePen ) && I( CreateSolidBrush ) && I( DeleteObject ) && I( LineTo ) &&
                   I( MoveToEx ) && I( PatBlt ) && I( SelectObject ) && I( SetBkMode ) &&
                   I( SetTextColor ) && I( TextOutA ) && I( CreateFontA ) && I( GetTextExtentPoint32A );
        }

        bool WindowReady( ) const
        {
            return this->Ready( ) && I( GetDC ) && I( ReleaseDC );
        }

        bool FontReady( ) const
        {
            return this->Ready( ) && I( AddFontMemResourceEx ) && I( RemoveFontMemResourceEx );
        }

        bool GlyphReady( ) const
        {
            return this->Ready( ) && I( GetGlyphOutlineA ) && I( CreateCompatibleDC ) && I( CreateDIBSection ) &&
                   I( DeleteDC ) && I( GdiFlush );
        }

        HDC     GetDC( void *window ) const;
        void    ReleaseDC( void *window, HDC context ) const;
        bool    AddFontMemResourceEx( const u8 *data, u32 size, void **font, u32 *font_count ) const;
        void    RemoveFontMemResourceEx( void *font ) const;
        HGDIOBJ CreateFontA( i32 height, i32 weight, const char *face,
                             u32 quality = PROOF_QUALITY ) const;
        HGDIOBJ SelectObject( HDC context, HGDIOBJ value ) const;
        void    DeleteObject( HGDIOBJ value ) const;
        bool    GetTextExtentPoint32A( HDC context, const char *text, i32 size, SIZE *extent ) const;
        u32     GetGlyphOutlineA( HDC context, u32 code, u32 format, GLYPHMETRICS *metrics, u32 buffer_size,
                                  void *buffer, const MAT2 *matrix ) const;
        bool    CreateBitmap( u32 width, u32 height, bitmap_t *bitmap ) const;
        void    DeleteBitmap( bitmap_t *bitmap ) const;

        bool Line( HDC context, point_t from, point_t to, color_t value ) const;
        bool Box( HDC context, rect_t bounds, color_t value ) const;
        bool Fill( HDC context, rect_t bounds, color_t value ) const;
        bool TextOutA( HDC context, point_t position, color_t value, const char *text ) const;
    };
} // namespace render::gdi
