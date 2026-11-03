#include "tg/widgets/label.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <cmath>

namespace tg
{
    Label::Label( std::string T, bool W ) : Wrap( W ), Text( std::move( T ) ) {}

    const Font& Label::GetFont() const { return Style.TextFont ? *Style.TextFont : *GetTheme().Body; }

    void Label::SetText( std::string T )
    {
        if ( T == Text ) return;
        // Same width? Then only a redraw is needed (common for ticking numbers in a dashboard).
        bool SameSize = !Wrap && GetFont().Width( T ) == GetFont().Width( Text );
        Text = std::move( T );
        if ( SameSize ) Redraw();
        else Invalidate();
    }

    Vec2 Label::Measure( float Avail )
    {
        const Insets& P = Layout.Padding;
        Vec2 S = GetFont().Measure( Text, Wrap ? std::fmax( 1.f, Avail - P.Horizontal() ) : 0 );
        return { std::ceil( S.x ) + P.Horizontal(), S.y + P.Vertical() };
    }

    void Label::Draw( DrawList& D )
    {
        const Font& F = GetFont();
        Rect In = Bounds.Shrink( Layout.Padding );
        Vec2 Sz = F.Measure( Text, Wrap ? In.w : 0 );
        float X = In.x, Y = In.y;
        if ( Style.HAlign == Align::Center ) X += std::floor( ( In.w - Sz.x ) * 0.5f );
        if ( Style.HAlign == Align::End ) X += In.w - Sz.x;
        if ( Style.VAlign == Align::Center ) Y += std::floor( ( In.h - Sz.y ) * 0.5f );
        if ( Style.VAlign == Align::End ) Y += In.h - Sz.y;
        D.Text( F, { X, Y }, Text, Style.TextColor.Resolve().Fade( Enabled ? 1.f : 0.5f ), Wrap ? In.w : 0 );
    }
}
