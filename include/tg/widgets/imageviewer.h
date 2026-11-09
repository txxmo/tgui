// ImageViewer — zoom and pan an image (screenshots, screen-share frames, maps, charts).
// Wheel zooms at the cursor, drag pans, double-click toggles fit / 100%, + / - / 0 keys.
//
//   auto& V = W.Add<tg::ImageViewer>( Tex, 1920, 1080 );
#pragma once
#include "../theme.h"
#include "../widget.h"

namespace tg
{
    class ImageViewer : public Widget
    {
    public:
        ImageViewer( TextureId Tex = 0, int Width = 0, int Height = 0 );
        void SetSource( TextureId Tex, int Width, int Height, bool KeepView = true );
        float MinZoom = 0.05f, MaxZoom = 32;
        bool Checker = true;      // checkerboard behind transparent images
        bool ShowZoom = true;     // "125%" badge
        Paint Fill = Tok::SurfaceAlt;

        float Zoom() const { return Z; }
        void Fit();               // whole image visible
        void Actual();            // 100%, centred
        void ZoomAt( float NewZoom, Vec2 Anchor );
        Vec2 ImageToScreen( Vec2 Px ) const;
        Vec2 ScreenToImage( Vec2 P ) const;

        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        TextureId Tex;
        int W, H;
        float Z = 1, ShownZ = 1;
        Vec2 Pan, ShownPan; // image top-left relative to the widget, in UI units
        bool Fitted = true, Dragging = false;
        Vec2 DragFrom, PanFrom;
        float BadgeT = 0;
    };
}
