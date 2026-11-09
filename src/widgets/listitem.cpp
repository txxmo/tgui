#include "tg/widgets/listitem.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    void DrawCheckMark( DrawList& D, Rect Box, Color C, float Thickness, float T )
    {
        if ( T <= 0.01f ) return;
        Vec2 A = { Box.x + Box.w * 0.26f, Box.y + Box.h * 0.53f };
        Vec2 B = { Box.x + Box.w * 0.43f, Box.y + Box.h * 0.69f };
        Vec2 E = { Box.x + Box.w * 0.75f, Box.y + Box.h * 0.33f };
        auto L = []( Vec2 P, Vec2 Q, float t ) { return P + ( Q - P ) * t; };
        Vec2 Pts[ 3 ] = { A, L( A, B, std::min( 1.f, T / 0.4f ) ), L( B, E, std::max( 0.f, ( T - 0.4f ) / 0.6f ) ) };
        D.Polyline( Pts, T < 0.4f ? 2 : 3, C, Thickness );
    }

    ListItemStyle& ListItem::Defaults()
    {
        static ListItemStyle S;
        return S;
    }

    ListItem::ListItem( std::string T ) : Text( std::move( T ) ), Style( Defaults() )
    {
        Interactive = true; KeyActivates = true; // Enter on it while highlighted with the arrow keys
    }

    Vec2 ListItem::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        float Extra = ( ShowCheck ? Style.CheckSize + 8 : 0 ) + ( ShowMark ? Style.CheckSize + 8 : 0 );
        return { std::ceil( F.Width( Text ) ) + Extra + Style.Padding.Horizontal(), F.LineHeight + Style.Padding.Vertical() };
    }

    void ListItem::Draw( DrawList& D )
    {
        const ListItemStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Animate( HoverT, Interactive && IsHovered() ? 1.f : 0.f, 22.f );
        Animate( SelT, Selected ? 1.f : 0.f, 22.f );

        // background: selected tint, then hover tint on top
        if ( !ShowCheck ) D.RectFilled( Bounds, S.SelectedFill.Resolve().Fade( SelT ), S.Radius );
        D.RectFilled( Bounds, S.HoverFill.Resolve().Fade( HoverT ), S.Radius );

        Rect In = Bounds.Shrink( S.Padding );
        float X = In.x;
        float Cy = In.y + In.h * 0.5f;

        // multi-select: a mini checkbox on the left
        if ( ShowCheck )
        {
            Rect Box = { X, std::floor( Cy - S.CheckSize * 0.5f ), S.CheckSize, S.CheckSize };
            D.RectFilled( Box, GetTheme()[ Tok::SurfaceAlt ], 4 );
            D.RectStroke( Box, GetTheme()[ Tok::BorderStrong ], 4 );
            D.RectFilled( Box, S.Check.Resolve().Fade( SelT ), 4 );
            DrawCheckMark( D, Box, GetTheme()[ Tok::TextOnAccent ], 1.8f, SelT );
            X += S.CheckSize + 8;
        }

        Color Tc = Color::Lerp( S.TextColor.Resolve(), S.SelectedText.Resolve(), SelT );
        D.Text( F, { X, std::floor( Cy - F.LineHeight * 0.5f ) }, Text, Tc.Fade( Enabled ? 1 : 0.5f ) );

        // single-select dropdown: tick on the right of the chosen row
        if ( ShowMark && SelT > 0.01f )
        {
            Rect Box = { In.Right() - S.CheckSize, std::floor( Cy - S.CheckSize * 0.5f ), S.CheckSize, S.CheckSize };
            DrawCheckMark( D, Box, S.Check.Resolve().Fade( SelT ), 1.8f );
        }
    }

    bool ListItem::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() && OnClick ) { auto Fn = OnClick; Fn(); } // copy: the handler may delete us
            return !BubbleRelease;
        }
        return false;
    }
}
