#include "tg/widgets/extras.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <cmath>

namespace tg
{
    void Spinner::Draw( DrawList& D )
    {
        Context* C = GetContext();
        double T = C ? C->Time() : 0;
        Vec2 Ctr = Bounds.Center();
        float R = std::min( Bounds.w, Bounds.h ) * 0.5f - Thickness * 0.5f;
        D.CircleStroke( Ctr, R + Thickness * 0.5f, GetTheme()[ Tok::Raised ], Thickness );
        // a 100-degree arc chasing around
        float A0 = float( std::fmod( T * Speed, 1.0 ) * 6.2831853 ), Len = 1.75f;
        Vec2 P[ 20 ];
        for ( int i = 0; i < 20; i++ )
        {
            float A = A0 + Len * i / 19.f;
            P[ i ] = { Ctr.x + std::cos( A ) * R, Ctr.y + std::sin( A ) * R };
        }
        D.Polyline( P, 20, Color.Resolve(), Thickness );
        if ( C && Visible ) C->RequestFrame(); // animated while on screen
    }

    Vec2 Badge::Measure( float )
    {
        const Font& F = *GetTheme().Small;
        return { std::ceil( F.Width( Text ) ) + 16, F.LineHeight + 6 };
    }

    void Badge::Draw( DrawList& D )
    {
        const Font& F = *GetTheme().Small;
        Color Fc = Fill.Resolve();
        if ( Soft )
        {
            D.RectFilled( Bounds, Fc.WithAlpha( 40 ), Bounds.h * 0.5f );
            D.RectStroke( Bounds, Fc.WithAlpha( 90 ), Bounds.h * 0.5f );
        }
        else D.RectFilled( Bounds, Fc, Bounds.h * 0.5f );
        float W = F.Width( Text );
        D.Text( F, { std::floor( Bounds.x + ( Bounds.w - W ) * 0.5f ), std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) }, Text,
                Soft ? Color::Lerp( Fc, GetTheme()[ Tok::Text ], 0.25f ) : TextColor.Resolve() );
    }

    Vec2 Link::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        return { std::ceil( F.Width( Text ) ), F.LineHeight };
    }

    void Link::Draw( DrawList& D )
    {
        const Font& F = *GetTheme().Body;
        Color C = TextColor.Resolve();
        if ( IsHovered() ) C = Color::Lerp( C, GetTheme()[ Tok::Text ], 0.3f );
        float Y = std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f );
        D.Text( F, { Bounds.x, Y }, Text, C );
        if ( IsHovered() ) D.RectFilled( { Bounds.x, Y + F.Ascent + 2, F.Width( Text ), 1 }, C );
    }

    bool Link::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::MouseEnter || E.Type == EventType::MouseLeave ) { Redraw(); return false; }
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp ) { if ( IsHovered() && OnClick ) { auto Fn = OnClick; Fn(); } return true; }
        return false;
    }
}
