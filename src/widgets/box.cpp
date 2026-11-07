#include "tg/widgets/box.h"
#include "tg/draw.h"

namespace tg
{
    BoxStyle Box::Card()
    {
        BoxStyle S;
        S.Fill = Tok::Surface;
        S.Stroke = Tok::Border;
        S.Radius = 10;
        return S;
    }

    BoxStyle Box::Panel()
    {
        BoxStyle S;
        S.Fill = Tok::Surface;
        return S;
    }

    void Box::Draw( DrawList& D )
    {
        if ( Style.Shadow ) D.Shadow( Bounds, Style.Radius, Color( 0, 0, 0, 90 ), 10, { 0, 3 } );
        D.RectFilled( Bounds, Style.Fill.Resolve(), Style.Radius );
        D.RectStroke( Bounds, Style.Stroke.Resolve(), Style.Radius, Style.StrokeWidth );
        DrawChildren( D );
    }

    Box& AddRow( Widget& Parent, float Gap, Insets Padding ) { return Parent.Add<Box>( Dir::Row, Gap, Padding ); }
    Box& AddColumn( Widget& Parent, float Gap, Insets Padding ) { return Parent.Add<Box>( Dir::Column, Gap, Padding ); }

    Box& AddSpacer( Widget& Parent, float Weight )
    {
        auto& B = Parent.Add<Box>();
        B.Layout.Width = Grow( Weight );
        B.Layout.Height = Grow( Weight );
        return B;
    }

    Box& AddDivider( Widget& Parent, bool Vertical )
    {
        auto& B = Parent.Add<Box>();
        B.Style.Fill = Tok::Border;
        if ( Vertical ) B.Layout.Width = Px( 1 );
        else B.Layout.Height = Px( 1 );
        return B;
    }
}
