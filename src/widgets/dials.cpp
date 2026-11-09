#include "tg/widgets/dials.h"
#include "tg/binds.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/container.h"
#include "tg/widgets/slider.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace tg
{
    static constexpr float A0 = 2.356194f, Sweep = 4.712389f; // 135° start, 270° travel

    Knob::Knob( std::string L, float* B, float Mn, float Mx, float St ) : Label( std::move( L ) ), Bind( B ), Min( Mn ), Max( Mx ), Step( St )
    {
        Interactive = true;
        Focusable = true;
        Default = GetValue();
    }

    void Knob::SetValue( float V )
    {
        V = std::clamp( V, std::min( Min, Max ), std::max( Min, Max ) );
        if ( Step > 0 ) V = Min + std::round( ( V - Min ) / Step ) * Step;
        if ( V == GetValue() ) return;
        ( Bind ? *Bind : Own ) = V;
        if ( OnChange ) OnChange( V );
        Redraw();
    }

    Vec2 Knob::Measure( float ) { return { std::max( Size, GetTheme().Small->Width( Label ) ), Size + GetTheme().Small->LineHeight + 4 }; }

    void Knob::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        Animate( HoverT, IsHovered() || IsPressed() ? 1.f : 0.f );
        float Tv = Max != Min ? std::clamp( ( GetValue() - Min ) / ( Max - Min ), 0.f, 1.f ) : 0;
        if ( Shown < 0 ) Shown = Tv;
        float t = Animate( Shown, Tv, IsPressed() ? 30.f : 16.f );
        Vec2 C = { Bounds.Center().x, Bounds.y + Size * 0.5f };
        float R = Size * 0.5f - 4;
        auto Arc = [ & ]( float From, float To, Color Col, float W ) {
            if ( To - From < 0.001f ) return;
            D.Path.clear();
            D.PathArc( C, R, A0 + Sweep * From, A0 + Sweep * To );
            std::vector<Vec2> P = D.Path;
            D.Polyline( P.data(), int( P.size() ), Col, W );
        };
        Arc( 0, 1, Track.Resolve(), 4 );
        Arc( 0, t, Fill.Resolve(), 4 );
        D.CircleFilled( C, R - 7, Color::Lerp( T[ Tok::Raised ], T[ Tok::SurfaceAlt ], 0.4f ) );
        D.CircleStroke( C, R - 7, T[ Tok::BorderStrong ].Fade( 0.6f + 0.4f * HoverT ) );
        float A = A0 + Sweep * t;
        D.Line( C + Vec2( std::cos( A ), std::sin( A ) ) * ( R - 16 ), C + Vec2( std::cos( A ), std::sin( A ) ) * ( R - 9 ), Fill.Resolve(), 2.5f );
        char B[ 32 ];
        std::snprintf( B, sizeof( B ), Format.c_str(), GetValue() );
        const Font& F = *T.Small;
        D.Text( F, { std::floor( C.x - F.Width( B ) * 0.5f ), std::floor( C.y - F.LineHeight * 0.5f ) }, B, Text.Resolve() );
        D.Text( F, { std::floor( C.x - F.Width( Label ) * 0.5f ), Bounds.y + Size + 2 }, Label, T[ Tok::TextDim ] );
    }

    bool Knob::OnEvent( const Event& E )
    {
        float Range = Max - Min, St = Step > 0 ? Step : Range / 100;
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            if ( E.Clicks == 2 ) { SetValue( Default ); return true; }
            DragY = E.Pos.y;
            DragV = GetValue();
            return true;
        case EventType::MouseMove:
            if ( !IsPressed() ) return false;
            // 200 px of travel = the whole range; Shift for fine control
            SetValue( DragV + ( DragY - E.Pos.y ) / ( ( E.Mods & ModShift ) ? 1000.f : 200.f ) * Range );
            return true;
        case EventType::MouseUp: return E.Button == MouseButton::Left;
        case EventType::MouseWheel: SetValue( GetValue() + E.Wheel.y * St ); return true;
        case EventType::KeyDown:
            if ( E.KeyCode == Key::Up || E.KeyCode == Key::Right ) { SetValue( GetValue() + St * ( ( E.Mods & ModShift ) ? 10 : 1 ) ); return true; }
            if ( E.KeyCode == Key::Down || E.KeyCode == Key::Left ) { SetValue( GetValue() - St * ( ( E.Mods & ModShift ) ? 10 : 1 ) ); return true; }
            return false;
        default: return false;
        }
    }

    void Knob::BindSet( const BindValue& V ) { if ( BindHas( V ) ) SetValue( float( V.V[ 0 ] ) ); }
    void Knob::BindEditor( Container& Into, BindValue& V )
    {
        auto F = std::make_shared<float>( BindHas( V ) ? float( V.V[ 0 ] ) : GetValue() );
        V.V = { *F };
        auto& S = Into.Slider( "Value", F.get(), Min, Max, Step );
        auto Show = S.OnChange;
        S.OnChange = [ F, Show, &V ]( float X ) { if ( Show ) Show( X ); V.V = { X }; };
    }
    std::string Knob::BindText( const BindValue& V ) const { return BindHas( V ) ? BindNumber( V.V[ 0 ], Step ) : ""; }

    // ------------------------------------------------------------ XYPad

    XYPad::XYPad( std::string L, Vec2* B, Vec2 Mn, Vec2 Mx ) : Label( std::move( L ) ), Bind( B ), Min( Mn ), Max( Mx )
    {
        Interactive = true;
        Focusable = true;
        Own = { ( Mn.x + Mx.x ) * 0.5f, ( Mn.y + Mx.y ) * 0.5f };
    }

    void XYPad::SetValue( Vec2 V )
    {
        V.x = std::clamp( V.x, std::min( Min.x, Max.x ), std::max( Min.x, Max.x ) );
        V.y = std::clamp( V.y, std::min( Min.y, Max.y ), std::max( Min.y, Max.y ) );
        if ( V == GetValue() ) return;
        ( Bind ? *Bind : Own ) = V;
        if ( OnChange ) OnChange( V );
        Redraw();
    }

    Rect XYPad::Pad() const { return { Bounds.x, Bounds.y + GetTheme().Small->LineHeight + 4, Size, Size }; }

    Vec2 XYPad::Measure( float ) { return { Size, Size + GetTheme().Small->LineHeight + 4 }; }

    void XYPad::SetFromMouse( Vec2 P )
    {
        Rect R = Pad();
        float Tx = std::clamp( ( P.x - R.x ) / R.w, 0.f, 1.f ), Ty = std::clamp( ( P.y - R.y ) / R.h, 0.f, 1.f );
        SetValue( { Min.x + ( Max.x - Min.x ) * Tx, Max.y + ( Min.y - Max.y ) * Ty } ); // y up
    }

    void XYPad::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& F = *T.Small;
        Rect R = Pad();
        Vec2 V = GetValue();
        char B[ 48 ];
        std::snprintf( B, sizeof( B ), "%.2f, %.2f", V.x, V.y );
        bool Fits = F.Width( Label ) + F.Width( B ) + 8 <= R.w;
        if ( Fits || !( IsHovered() || IsPressed() ) ) D.Text( F, { Bounds.x, Bounds.y }, Label, T[ Tok::TextDim ] );
        if ( Fits || IsHovered() || IsPressed() ) D.Text( F, { R.Right() - F.Width( B ), Bounds.y }, B, T[ Tok::Text ] ); // small pads: value while hovered
        D.RectFilled( R, Fill.Resolve(), 8 );
        for ( int i = 1; i < GridLines; i++ )
        {
            float K = float( i ) / GridLines;
            D.RectFilled( { R.x + R.w * K, R.y + 4, 1, R.h - 8 }, Grid.Resolve().Fade( i * 2 == GridLines ? 1 : 0.5f ) );
            D.RectFilled( { R.x + 4, R.y + R.h * K, R.w - 8, 1 }, Grid.Resolve().Fade( i * 2 == GridLines ? 1 : 0.5f ) );
        }
        float Tx = Max.x != Min.x ? ( V.x - Min.x ) / ( Max.x - Min.x ) : 0.5f, Ty = Max.y != Min.y ? ( Max.y - V.y ) / ( Max.y - Min.y ) : 0.5f;
        Vec2 P = { R.x + R.w * Tx, R.y + R.h * Ty };
        Color Dc = Dot.Resolve();
        D.RectFilled( { P.x, R.y, 1, R.h }, Dc.Fade( 0.35f ) );
        D.RectFilled( { R.x, P.y, R.w, 1 }, Dc.Fade( 0.35f ) );
        D.CircleFilled( P, IsPressed() ? 8 : 7, Dc.Fade( 0.3f ) );
        D.CircleFilled( P, 5, Dc );
        D.RectStroke( R, T[ Tok::Border ], 8 );
    }

    bool XYPad::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left || !Pad().Contains( E.Pos ) ) return false;
            SetFromMouse( E.Pos );
            return true;
        case EventType::MouseMove: if ( !IsPressed() ) return false; SetFromMouse( E.Pos ); return true;
        case EventType::MouseUp: return E.Button == MouseButton::Left;
        case EventType::KeyDown:
        {
            Vec2 V = GetValue(), St = { ( Max.x - Min.x ) / 50, ( Max.y - Min.y ) / 50 };
            if ( E.Mods & ModShift ) St = St * 5;
            if ( E.KeyCode == Key::Left ) V.x -= St.x;
            else if ( E.KeyCode == Key::Right ) V.x += St.x;
            else if ( E.KeyCode == Key::Up ) V.y += St.y;
            else if ( E.KeyCode == Key::Down ) V.y -= St.y;
            else return false;
            SetValue( V );
            return true;
        }
        default: return false;
        }
    }

    void XYPad::BindSet( const BindValue& V ) { if ( V.V.size() >= 2 && BindHas( V ) ) SetValue( { float( V.V[ 0 ] ), float( V.V[ 1 ] ) } ); }
    void XYPad::BindEditor( Container& Into, BindValue& V )
    {
        if ( V.V.size() < 2 ) V = BindGet();
        auto P = std::make_shared<Vec2>( Vec2{ float( V.V[ 0 ] ), float( V.V[ 1 ] ) } );
        auto& X = Into.Add<XYPad>( "Point", P.get(), Min, Max );
        X.OnChange = [ P, &V ]( Vec2 Q ) { V.V = { Q.x, Q.y }; };
    }
    std::string XYPad::BindText( const BindValue& V ) const
    {
        if ( V.V.size() < 2 ) return "";
        char B[ 48 ];
        std::snprintf( B, sizeof( B ), "%.2f, %.2f", V.V[ 0 ], V.V[ 1 ] );
        return B;
    }
}
