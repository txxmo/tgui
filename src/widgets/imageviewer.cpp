#include "tg/widgets/imageviewer.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/colorpicker.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    ImageViewer::ImageViewer( TextureId T, int Wd, int Ht ) : Tex( T ), W( Wd ), H( Ht )
    {
        Interactive = true;
        Focusable = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
    }

    void ImageViewer::SetSource( TextureId T, int Wd, int Ht, bool Keep )
    {
        bool SizeChanged = Wd != W || Ht != H;
        Tex = T;
        W = Wd;
        H = Ht;
        if ( !Keep || SizeChanged ) Fitted = true;
        Redraw();
    }

    void ImageViewer::Fit()
    {
        if ( W <= 0 || H <= 0 || Bounds.w <= 0 ) return;
        Z = std::clamp( std::min( Bounds.w / W, Bounds.h / H ), MinZoom, MaxZoom );
        Pan = { ( Bounds.w - W * Z ) * 0.5f, ( Bounds.h - H * Z ) * 0.5f };
        Fitted = true;
        BadgeT = 1.5f;
        Redraw();
    }

    void ImageViewer::Actual()
    {
        Z = 1;
        Pan = { ( Bounds.w - W ) * 0.5f, ( Bounds.h - H ) * 0.5f };
        Fitted = false;
        BadgeT = 1.5f;
        Redraw();
    }

    void ImageViewer::ZoomAt( float Nz, Vec2 A )
    {
        Nz = std::clamp( Nz, MinZoom, MaxZoom );
        Vec2 Local = { A.x - Bounds.x, A.y - Bounds.y };
        // keep the image point under the anchor still
        Vec2 Img = { ( Local.x - Pan.x ) / Z, ( Local.y - Pan.y ) / Z };
        Z = Nz;
        Pan = { Local.x - Img.x * Z, Local.y - Img.y * Z };
        Fitted = false;
        BadgeT = 1.5f;
        Redraw();
    }

    Vec2 ImageViewer::ImageToScreen( Vec2 P ) const { return { Bounds.x + ShownPan.x + P.x * ShownZ, Bounds.y + ShownPan.y + P.y * ShownZ }; }
    Vec2 ImageViewer::ScreenToImage( Vec2 P ) const { return { ( P.x - Bounds.x - ShownPan.x ) / ShownZ, ( P.y - Bounds.y - ShownPan.y ) / ShownZ }; }

    void ImageViewer::Arrange( Rect R )
    {
        bool First = Bounds.w <= 0;
        Bounds = R;
        if ( Fitted ) { Fit(); if ( First ) { ShownZ = Z; ShownPan = Pan; } }
    }

    void ImageViewer::Draw( DrawList& D )
    {
        Context* C = GetContext();
        // glide zoom / pan (dragging stays 1:1)
        float K = Dragging ? 1.f : C ? 1 - std::exp( -18 * C->DeltaTime() ) : 1.f;
        ShownZ += ( Z - ShownZ ) * K;
        ShownPan = ShownPan + ( Pan - ShownPan ) * K;
        if ( C && ( std::fabs( Z - ShownZ ) > 1e-4f || std::fabs( Pan.x - ShownPan.x ) > 0.05f || std::fabs( Pan.y - ShownPan.y ) > 0.05f ) ) C->RequestFrame();
        else { ShownZ = Z; ShownPan = Pan; }

        D.RectFilled( Bounds, Fill.Resolve(), 6 );
        D.PushClip( Bounds );
        Rect Img = { Bounds.x + ShownPan.x, Bounds.y + ShownPan.y, W * ShownZ, H * ShownZ };
        if ( Checker ) DrawChecker( D, Img.Intersect( Bounds ), 8 );
        if ( Tex ) D.Image( Tex, Img );
        D.PopClip();
        if ( ShowZoom && BadgeT > 0 && C )
        {
            BadgeT -= C->DeltaTime();
            C->RequestFrame();
            char B[ 16 ];
            std::snprintf( B, sizeof( B ), "%.0f%%", Z * 100 );
            const Font& F = *GetTheme().Small;
            float Bw = F.Width( B ) + 14;
            Rect Br = { Bounds.Right() - Bw - 8, Bounds.y + 8, Bw, 22 };
            float A = std::min( 1.f, BadgeT );
            D.RectFilled( Br, Color( 0, 0, 0, uint8_t( 150 * A ) ), 6 );
            D.Text( F, { Br.x + 7, std::floor( Br.y + ( Br.h - F.LineHeight ) * 0.5f ) }, B, Color( 255, 255, 255, uint8_t( 255 * A ) ) );
        }
    }

    bool ImageViewer::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseWheel: ZoomAt( Z * std::pow( 1.2f, E.Wheel.y ), E.Pos ); return true;
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            if ( E.Clicks == 2 ) { if ( Fitted ) Actual(); else Fit(); return true; }
            Dragging = true;
            DragFrom = E.Pos;
            PanFrom = Pan;
            return true;
        case EventType::MouseMove:
            if ( !Dragging ) return false;
            Pan = { PanFrom.x + E.Pos.x - DragFrom.x, PanFrom.y + E.Pos.y - DragFrom.y };
            Fitted = false;
            Redraw();
            return true;
        case EventType::MouseUp: Dragging = false; return E.Button == MouseButton::Left;
        case EventType::KeyDown:
        {
            Vec2 Mid = Bounds.Center();
            if ( E.KeyCode == Key::Equal || E.KeyCode == Key::KpAdd ) { ZoomAt( Z * 1.25f, Mid ); return true; }
            if ( E.KeyCode == Key::Minus || E.KeyCode == Key::KpSubtract ) { ZoomAt( Z / 1.25f, Mid ); return true; }
            if ( E.KeyCode == Key::Num0 || E.KeyCode == Key::Kp0 ) { Fit(); return true; }
            if ( E.KeyCode == Key::Num1 ) { Actual(); return true; }
            return false;
        }
        default: return false;
        }
    }
}
