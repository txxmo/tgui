#include "tg/widgets/splitter.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Splitter::Splitter( Dir D, float R ) : Direction( D == Dir::Column ? Dir::Column : Dir::Row ), Ratio( R )
    {
        Interactive = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
        A = &Add<Container>( Dir::Column, 10 );
        B = &Add<Container>( Dir::Column, 10 );
    }

    Vec2 Splitter::Measure( float Avail )
    {
        if ( Direction == Dir::Row )
        {
            float W1 = ( Avail - Style.Handle ) * Ratio, W2 = Avail - Style.Handle - W1;
            Vec2 S1 = A->MeasureCached( W1 ), S2 = B->MeasureCached( W2 );
            return { S1.x + S2.x + Style.Handle, std::max( S1.y, S2.y ) };
        }
        Vec2 S1 = A->MeasureCached( Avail ), S2 = B->MeasureCached( Avail );
        return { std::max( S1.x, S2.x ), S1.y + S2.y + Style.Handle };
    }

    void Splitter::Arrange( Rect R )
    {
        Bounds = R;
        const bool Row = Direction == Dir::Row;
        float Total = ( Row ? R.w : R.h ) - Style.Handle;
        float First = Clamp( std::round( Total * Ratio ), std::min( Style.MinFirst, Total ), std::max( Total - Style.MinSecond, std::min( Style.MinFirst, Total ) ) );
        if ( Row )
        {
            A->Arrange( { R.x, R.y, First, R.h } );
            HandleR = { R.x + First, R.y, Style.Handle, R.h };
            B->Arrange( { HandleR.Right(), R.y, R.Right() - HandleR.Right(), R.h } );
        }
        else
        {
            A->Arrange( { R.x, R.y, R.w, First } );
            HandleR = { R.x, R.y + First, R.w, Style.Handle };
            B->Arrange( { R.x, HandleR.Bottom(), R.w, R.Bottom() - HandleR.Bottom() } );
        }
    }

    void Splitter::Draw( DrawList& D )
    {
        D.PushClip( A->Bounds );
        A->Draw( D );
        D.PopClip();
        D.PushClip( B->Bounds );
        B->Draw( D );
        D.PopClip();
        Color L = ( Dragging || Hot ? Style.LineHot : Style.Line ).Resolve();
        Vec2 C = HandleR.Center();
        if ( Direction == Dir::Row ) D.RectFilled( { std::floor( C.x ), HandleR.y, Dragging || Hot ? 2.f : 1.f, HandleR.h }, L );
        else D.RectFilled( { HandleR.x, std::floor( C.y ), HandleR.w, Dragging || Hot ? 2.f : 1.f }, L );
    }

    Widget* Splitter::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( HandleR.Shrink( -2 ).Contains( P ) ) return this;
        if ( Widget* H = A->HitTest( P ) ) return H;
        return B->HitTest( P );
    }

    bool Splitter::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseEnter: Hot = true; Redraw(); return false;
        case EventType::MouseLeave: Hot = false; Redraw(); return false;
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            Dragging = true;
            return true;
        case EventType::MouseMove:
        {
            if ( !Dragging ) return false;
            const bool Row = Direction == Dir::Row;
            float Total = ( Row ? Bounds.w : Bounds.h ) - Style.Handle;
            float Pos = ( Row ? E.Pos.x - Bounds.x : E.Pos.y - Bounds.y ) - Style.Handle * 0.5f;
            Ratio = Total > 0 ? Clamp( Pos / Total, 0, 1 ) : 0.5f;
            Arrange( Bounds ); // only our subtree moves
            Redraw();
            return true;
        }
        case EventType::MouseUp:
            Dragging = false;
            Redraw();
            return E.Button == MouseButton::Left;
        default:
            return false;
        }
    }
}
