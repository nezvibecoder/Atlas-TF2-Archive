#include "gdi.hpp"

namespace render::gdi
{
    HDC api::GetDC( void *window ) const
    {
        if ( !window || !I( GetDC ) )
        {
            return nullptr;
        }

        return I( GetDC )( ( HWND )window );
    }

    void api::ReleaseDC( void *window, HDC context ) const
    {
        if ( window && context && I( ReleaseDC ) )
        {
            I( ReleaseDC )( ( HWND )window, context );
        }
    }

    bool api::AddFontMemResourceEx( const u8 *data, u32 size, void **font, u32 *font_count ) const
    {
        if ( !data || !size || !font || !font_count || !I( AddFontMemResourceEx ) )
        {
            return false;
        }

        DWORD count{ };
        void *handle{ I( AddFontMemResourceEx )( const_cast< u8 * >( data ), size, nullptr, &count ) };

        if ( !handle || !count )
        {
            return false;
        }

        *font       = handle;
        *font_count = count;
        return true;
    }

    void api::RemoveFontMemResourceEx( void *font ) const
    {
        if ( font && I( RemoveFontMemResourceEx ) )
        {
            I( RemoveFontMemResourceEx )( font );
        }
    }

    HGDIOBJ api::SelectObject( HDC context, HGDIOBJ value ) const
    {
        if ( !context || !value || !I( SelectObject ) )
        {
            return nullptr;
        }

        return I( SelectObject )( context, value );
    }

    HGDIOBJ api::CreateFontA( i32 height, i32 weight, const char *face, u32 quality ) const
    {
        if ( !face || !I( CreateFontA ) )
        {
            return nullptr;
        }

        return I( CreateFontA )( height, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, quality, DEFAULT_PITCH | FF_DONTCARE, face );
    }

    void api::DeleteObject( HGDIOBJ value ) const
    {
        if ( value && I( DeleteObject ) )
        {
            I( DeleteObject )( value );
        }
    }

    bool api::GetTextExtentPoint32A( HDC context, const char *text, i32 size, SIZE *extent ) const
    {
        if ( !context || !text || size <= 0 || !extent || !I( GetTextExtentPoint32A ) )
        {
            return false;
        }

        return I( GetTextExtentPoint32A )( context, text, size, extent ) != 0;
    }

    u32 api::GetGlyphOutlineA( HDC context, u32 code, u32 format, GLYPHMETRICS *metrics,
                               u32 buffer_size, void *buffer, const MAT2 *matrix ) const
    {
        if ( !context || !metrics || !matrix || !I( GetGlyphOutlineA ) )
        {
            return 0xFFFFFFFFu;
        }

        return I( GetGlyphOutlineA )( context, code, format, metrics, buffer_size, buffer, matrix );
    }

    bool api::CreateBitmap( u32 width, u32 height, bitmap_t *bitmap ) const
    {
        BITMAPINFO info{ };
        info.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
        info.bmiHeader.biWidth       = static_cast< LONG >( width );
        info.bmiHeader.biHeight      = -static_cast< LONG >( height );
        info.bmiHeader.biPlanes      = 1;
        info.bmiHeader.biBitCount    = 32;
        info.bmiHeader.biCompression = BI_RGB;

        *bitmap           = bitmap_t{ };
        bitmap->m_context = I( CreateCompatibleDC )( nullptr );

        if ( !bitmap->m_context )
        {
            return false;
        }

        void *bits{ };
        bitmap->m_bitmap = I( CreateDIBSection )( bitmap->m_context, &info, DIB_RGB_COLORS, &bits, nullptr, 0 );

        if ( !bitmap->m_bitmap )
        {
            this->DeleteBitmap( bitmap );
            return false;
        }

        bitmap->m_previous = this->SelectObject( bitmap->m_context, bitmap->m_bitmap );
        bitmap->m_pixels   = static_cast< u32 * >( bits );
        bitmap->m_width    = width;
        bitmap->m_height   = height;
        return true;
    }

    void api::DeleteBitmap( bitmap_t *bitmap ) const
    {
        if ( bitmap->m_previous )
        {
            this->SelectObject( bitmap->m_context, bitmap->m_previous );
        }

        this->DeleteObject( bitmap->m_bitmap );

        if ( bitmap->m_context )
        {
            I( DeleteDC )( bitmap->m_context );
        }

        *bitmap = bitmap_t{ };
    }

    bool api::Line( HDC context, point_t from, point_t to, color_t value ) const
    {
        if ( !context || !this->Ready( ) )
        {
            return false;
        }

        HGDIOBJ pen{ I( CreatePen )( PS_SOLID, 1, value.gdi( ) ) };

        if ( !pen )
        {
            return false;
        }

        HGDIOBJ previous{ this->SelectObject( context, pen ) };
        bool    result{ I( MoveToEx )( context, from.x, from.y, nullptr ) != 0 &&
                     I( LineTo )( context, to.x, to.y ) != 0 };

        if ( previous )
        {
            this->SelectObject( context, previous );
        }

        this->DeleteObject( pen );
        return result;
    }

    bool api::Box( HDC context, rect_t bounds, color_t value ) const
    {
        if ( !context || !this->Ready( ) )
        {
            return false;
        }

        HGDIOBJ pen{ I( CreatePen )( PS_SOLID, 1, value.gdi( ) ) };

        if ( !pen )
        {
            return false;
        }

        HGDIOBJ previous{ this->SelectObject( context, pen ) };
        bool    result{ I( MoveToEx )( context, bounds.left, bounds.top, nullptr ) != 0 &&
                     I( LineTo )( context, bounds.right, bounds.top ) != 0 &&
                     I( LineTo )( context, bounds.right, bounds.bottom ) != 0 &&
                     I( LineTo )( context, bounds.left, bounds.bottom ) != 0 &&
                     I( LineTo )( context, bounds.left, bounds.top ) != 0 };

        if ( previous )
        {
            this->SelectObject( context, previous );
        }

        this->DeleteObject( pen );
        return result;
    }

    bool api::Fill( HDC context, rect_t bounds, color_t value ) const
    {
        if ( !context || !this->Ready( ) )
        {
            return false;
        }

        HGDIOBJ brush{ I( CreateSolidBrush )( value.gdi( ) ) };

        if ( !brush )
        {
            return false;
        }

        HGDIOBJ previous{ this->SelectObject( context, brush ) };
        bool    result{ I( PatBlt )( context, bounds.left, bounds.top, bounds.right - bounds.left,
                                  bounds.bottom - bounds.top, PATCOPY ) != 0 };

        if ( previous )
        {
            this->SelectObject( context, previous );
        }

        this->DeleteObject( brush );
        return result;
    }

    bool api::TextOutA( HDC context, point_t position, color_t value, const char *text ) const
    {
        if ( !context || !text || !this->Ready( ) )
        {
            return false;
        }

        I( SetBkMode )( context, TRANSPARENT );
        I( SetTextColor )( context, value.gdi( ) );

        return I( TextOutA )( context, position.x, position.y, text,
                              static_cast< i32 >( str::length( text ) ) ) != 0;
    }
} // namespace render::gdi
