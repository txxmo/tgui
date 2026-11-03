#include "tg/widgets/popup.h"
#include "tg/context.h"
#include "tg/draw.h"

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
        Closed = true;
        Visible = false;
        auto Cb = std::move( OnClose );
        OnClose = nullptr;
        if ( Cb ) Cb();
        if ( Context* C = GetContext() )
        {
            // Defer the delete: we may be inside one of our own children's event handlers.
            C->Post( [ C, this ] { C->Overlay().Remove( this ); } );
            C->InvalidateLayout();
        }
    }

    void Popup::Draw( DrawList& D )
    {
        if ( Style.Shadow ) D.Shadow( Bounds, Style.Radius, Color( 0, 0, 0, 110 ), 12, { 0, 4 } );
        D.RectFilled( Bounds, Style.Fill.Resolve(), Style.Radius );
        D.RectStroke( Bounds, Style.Stroke.Resolve(), Style.Radius );
        D.PushClip( Bounds.Shrink( 1 ) );
        DrawChildren( D );
        D.PopClip();
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
