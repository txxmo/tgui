#include "tg/widgets/popup.h"
#include "tg/context.h"
#include "tg/draw.h"
#include <algorithm>

namespace tg
{
    Popup::Popup() : Box( Dir::Column, 0, Insets( 4 ) )
    {
        Style.Fill = Tok::Surface;
        Style.Stroke = Tok::BorderStrong;
        Style.Radius = 8;
        Style.Shadow = true;
        Interactive = true; // swallow clicks on its padding
    }

    Popup::~Popup()
    {
        if ( !Closed && OnClose ) OnClose();
    }

    void Popup::Close()
    {
        if ( Closed ) return;
        Closed = true; // no more input; it fades out, then removes itself
        auto Cb = std::move( OnClose );
        OnClose = nullptr;
        if ( Cb ) Cb();
        Context* C = GetContext();
        if ( !C ) return;
        if ( !Animated || OpenT <= 0 ) Finish();
        C->RequestFrame();
    }

    void Popup::Finish()
    {
        Visible = false;
        if ( Removing ) return;
        Removing = true;
        if ( Context* C = GetContext() )
        {
            // Defer the delete: we may be inside one of our own children's event handlers.
            C->Post( [ C, this ] { C->Overlay().Remove( this ); } );
            C->InvalidateLayout();
        }
    }

    void Popup::Draw( DrawList& D )
    {
        Context* C = GetContext();
        float Dt = C ? C->DeltaTime() : 1.f;
        if ( !Animated ) OpenT = Closed ? 0.f : 1.f;
        else if ( !Closed ) OpenT = std::min( 1.f, OpenT + Dt / std::max( 0.01f, OpenTime ) );
        else OpenT = std::max( 0.f, OpenT - Dt / std::max( 0.01f, CloseTime ) );
        if ( Closed && OpenT <= 0 ) { Finish(); return; }
        if ( C && ( Closed || OpenT < 1 ) ) C->RequestFrame();

        // opening: unrolls from the anchor (down, or up when it opened above), slides 8px and fades in
        bool Below = Bounds.y >= Anchor.y + Anchor.h * 0.5f;
        float E = Closed ? EaseInOutCubic( OpenT ) : EaseOutCubic( OpenT );
        size_t V0 = D.Vtx.size(), C0 = D.Cmds.size();
        float Pad = 24, RevealH = ( Bounds.h + Pad * 2 ) * ( Closed ? 1.f : 0.25f + 0.75f * E );
        Rect Reveal = Below ? Rect{ Bounds.x - Pad, Bounds.y - Pad, Bounds.w + Pad * 2, RevealH }
                            : Rect{ Bounds.x - Pad, Bounds.Bottom() + Pad - RevealH, Bounds.w + Pad * 2, RevealH };
        D.PushClip( Reveal );
        if ( Style.Shadow ) D.Shadow( Bounds, Style.Radius, Color( 0, 0, 0, 110 ), 12, { 0, 4 } );
        D.RectFilled( Bounds, Style.Fill.Resolve(), Style.Radius );
        D.RectStroke( Bounds, Style.Stroke.Resolve(), Style.Radius );
        D.PushClip( Bounds.Shrink( 1 ) );
        DrawChildren( D );
        D.PopClip();
        D.PopClip();
        if ( E < 1 )
        {
            Vec2 Pivot = { Bounds.Center().x, Below ? Bounds.y : Bounds.Bottom() };
            D.TransformFrom( V0, C0, Pivot, 0.97f + 0.03f * E, 0, { 0, ( 1 - E ) * ( Below ? -8.f : 8.f ) } );
            D.FadeFrom( V0, E );
        }
    }

    Widget* Popup::HitTest( Vec2 P )
    {
        if ( Closed ) return nullptr;
        return Widget::HitTest( P );
    }

    Popup& OpenPopup( Widget& Owner, Rect Anchor )
    {
        Context* C = Owner.GetContext();
        auto& P = C->Overlay().Add<Popup>();
        P.Owner = &Owner;
        P.Anchor = Anchor;
        P.MinWidth = Anchor.w;
        return P;
    }
}
