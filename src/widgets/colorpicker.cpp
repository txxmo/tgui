#include "tg/widgets/colorpicker.h"
#include <memory>
#include "tg/widgets/choice.h"
#include "tg/widgets/container.h"
#include "tg/binds.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/box.h"
#include "tg/widgets/popup.h"
#include "tg/widgets/textinput.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    // ------------------------------------------------------------ color math

    void ToHSV( Color C, float& H, float& S, float& V )
    {
        float R = C.r / 255.f, G = C.g / 255.f, B = C.b / 255.f;
        float Mx = std::max( { R, G, B } ), Mn = std::min( { R, G, B } ), Dl = Mx - Mn;
        V = Mx;
        S = Mx > 0 ? Dl / Mx : 0;
        if ( Dl <= 0 ) { H = 0; return; }
        if ( Mx == R ) H = std::fmod( ( G - B ) / Dl + 6.f, 6.f );
        else if ( Mx == G ) H = ( B - R ) / Dl + 2.f;
        else H = ( R - G ) / Dl + 4.f;
        H /= 6.f;
    }

    Color FromHSV( float H, float S, float V, uint8_t A )
    {
        H = std::fmod( H, 1.f ) * 6.f;
        if ( H < 0 ) H += 6.f;
        int I = int( H );
        float F = H - I, P = V * ( 1 - S ), Q = V * ( 1 - S * F ), T = V * ( 1 - S * ( 1 - F ) );
        float R, G, B;
        switch ( I % 6 )
        {
        case 0: R = V; G = T; B = P; break;
        case 1: R = Q; G = V; B = P; break;
        case 2: R = P; G = V; B = T; break;
        case 3: R = P; G = Q; B = V; break;
        case 4: R = T; G = P; B = V; break;
        default: R = V; G = P; B = Q; break;
        }
        auto U = []( float x ) { return uint8_t( std::lround( Clamp( x, 0, 1 ) * 255 ) ); };
        return { U( R ), U( G ), U( B ), A };
    }

    std::string ToHex( Color C, bool Alpha )
    {
        char Buf[ 16 ];
        if ( Alpha ) std::snprintf( Buf, sizeof( Buf ), "#%02X%02X%02X%02X", C.r, C.g, C.b, C.a );
        else std::snprintf( Buf, sizeof( Buf ), "#%02X%02X%02X", C.r, C.g, C.b );
        return Buf;
    }

    bool ParseHex( const std::string& In, Color& Out )
    {
        std::string S = In;
        S.erase( std::remove_if( S.begin(), S.end(), []( char c ) { return c == '#' || c == ' '; } ), S.end() );
        auto Hx = []( char c ) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        for ( char c : S ) if ( Hx( c ) < 0 ) return false;
        auto Byte = [ & ]( size_t i ) { return uint8_t( Hx( S[ i ] ) * 16 + Hx( S[ i + 1 ] ) ); };
        if ( S.size() == 3 ) { Out = { uint8_t( Hx( S[ 0 ] ) * 17 ), uint8_t( Hx( S[ 1 ] ) * 17 ), uint8_t( Hx( S[ 2 ] ) * 17 ), 255 }; return true; }
        if ( S.size() == 6 ) { Out = { Byte( 0 ), Byte( 2 ), Byte( 4 ), 255 }; return true; }
        if ( S.size() == 8 ) { Out = { Byte( 0 ), Byte( 2 ), Byte( 4 ), Byte( 6 ) }; return true; }
        return false;
    }

    // Checkerboard behind translucent colors. With a radius it follows the rounded corners exactly
    // (no square checker corners poking out of a rounded swatch).
    void DrawChecker( DrawList& D, Rect R, float Cell, float Radius )
    {
        D.RectFilled( R, Color( 200, 200, 200 ), Radius );
        const Color Dark( 150, 150, 150 );
        for ( float y = R.y, Row = 0; y < R.Bottom(); y += Cell, Row++ )
            for ( float x = R.x + ( int( Row ) % 2 ) * Cell; x < R.Right(); x += Cell * 2 )
            {
                Rect C = Rect{ x, y, Cell, Cell }.Intersect( R );
                if ( C.Empty() ) continue;
                bool NearCorner = Radius > 0 && ( C.x < R.x + Radius || C.Right() > R.Right() - Radius ) && ( C.y < R.y + Radius || C.Bottom() > R.Bottom() - Radius );
                if ( !NearCorner ) { D.RectFilled( C, Dark ); continue; }
                Vec2 Q[ 4 ] = { C.Pos(), { C.Right(), C.y }, C.Max(), { C.x, C.Bottom() } };
                D.ConvexFillInRounded( Q, 4, Dark, R, Radius );
            }
    }

    // ------------------------------------------------------------ HsvArea: the square + bars, handles dragging

    class HsvArea : public Widget
    {
    public:
        explicit HsvArea( ColorPanel& P ) : Panel( P ) { Interactive = true; }

        Vec2 Measure( float ) override
        {
            const auto& S = Panel.Style;
            float Bars = Panel.ShowAlpha ? 2 : 1;
            return { S.SquareSize + ( S.Gap + S.BarWidth ) * Bars, S.SquareSize };
        }

        Rect Square() const { return { Bounds.x, Bounds.y, Panel.Style.SquareSize, Bounds.h }; }
        Rect HueBar() const { auto& S = Panel.Style; return { Bounds.x + S.SquareSize + S.Gap, Bounds.y, S.BarWidth, Bounds.h }; }
        Rect AlphaBar() const { auto& S = Panel.Style; return { Bounds.x + S.SquareSize + S.Gap * 2 + S.BarWidth, Bounds.y, S.BarWidth, Bounds.h }; }

        void Draw( DrawList& D ) override
        {
            const auto& S = Panel.Style;
            Color Cur = Panel.GetColor();
            Color Hue = FromHSV( Panel.H, 1, 1 );
            Color Stroke = S.Stroke.Resolve();

            // 1) saturation/value square: white -> hue horizontally, then transparent -> black vertically
            Rect Sq = Square();
            D.RectMultiColor( Sq, Color( 255, 255, 255 ), Hue, Hue, Color( 255, 255, 255 ) );
            D.RectMultiColor( Sq, Color( 0, 0, 0, 0 ), Color( 0, 0, 0, 0 ), Color( 0, 0, 0 ), Color( 0, 0, 0 ) );
            D.RectStroke( Sq, Stroke );
            Vec2 Knob = { Sq.x + Panel.S * Sq.w, Sq.y + ( 1 - Panel.V ) * Sq.h };
            D.CircleStroke( Knob, 7, Color( 0, 0, 0, 120 ), 1 );
            D.CircleStroke( Knob, 6, Color( 255, 255, 255 ), 2 );

            // 2) hue bar: 6 gradient segments
            Rect Hb = HueBar();
            for ( int i = 0; i < 6; i++ )
            {
                Color A = FromHSV( i / 6.f, 1, 1 ), B = FromHSV( ( i + 1 ) / 6.f, 1, 1 );
                D.RectMultiColor( { Hb.x, Hb.y + Hb.h * i / 6.f, Hb.w, Hb.h / 6.f }, A, A, B, B );
            }
            D.RectStroke( Hb, Stroke );
            Marker( D, Hb, Panel.H );

            // 3) alpha bar over a checkerboard
            if ( Panel.ShowAlpha )
            {
                Rect Ab = AlphaBar();
                DrawChecker( D, Ab, 4 );
                Color Opaque = Cur.WithAlpha( 255 );
                D.RectMultiColor( Ab, Opaque, Opaque, Opaque.WithAlpha( 0 ), Opaque.WithAlpha( 0 ) );
                D.RectStroke( Ab, Stroke );
                Marker( D, Ab, 1 - Cur.a / 255.f );
            }
        }

        bool OnEvent( const Event& E ) override
        {
            if ( E.Type == EventType::MouseDown && E.Button == MouseButton::Left )
            {
                Target = Square().Contains( E.Pos ) ? 1 : HueBar().Shrink( -3 ).Contains( E.Pos ) ? 2
                       : Panel.ShowAlpha && AlphaBar().Shrink( -3 ).Contains( E.Pos ) ? 3 : 0;
                Apply( E.Pos );
                return true;
            }
            if ( E.Type == EventType::MouseMove && IsPressed() ) { Apply( E.Pos ); return true; }
            if ( E.Type == EventType::MouseUp ) { Target = 0; return true; }
            return false;
        }

    private:
        ColorPanel& Panel;
        int Target = 0; // 1 square, 2 hue, 3 alpha

        static void Marker( DrawList& D, Rect Bar, float T )
        {
            float Y = Bar.y + Clamp( T, 0, 1 ) * Bar.h;
            Rect M = { Bar.x - 2, Y - 2.5f, Bar.w + 4, 5 };
            D.RectFilled( M, Color( 255, 255, 255 ), 2 );
            D.RectStroke( M, Color( 0, 0, 0, 140 ), 2 );
        }

        void Apply( Vec2 P )
        {
            Color C = Panel.GetColor();
            if ( Target == 1 )
            {
                Rect Sq = Square();
                Panel.SetFromHSV( Panel.H, Clamp( ( P.x - Sq.x ) / Sq.w, 0, 1 ), 1 - Clamp( ( P.y - Sq.y ) / Sq.h, 0, 1 ), C.a );
            }
            else if ( Target == 2 )
            {
                Rect Hb = HueBar();
                Panel.SetFromHSV( Clamp( ( P.y - Hb.y ) / Hb.h, 0, 0.9999f ), Panel.S, Panel.V, C.a );
            }
            else if ( Target == 3 )
            {
                Rect Ab = AlphaBar();
                Panel.SetFromHSV( Panel.H, Panel.S, Panel.V, uint8_t( std::lround( ( 1 - Clamp( ( P.y - Ab.y ) / Ab.h, 0, 1 ) ) * 255 ) ) );
            }
        }
    };

    // Old vs new color preview.
    class Swatch : public Widget
    {
    public:
        explicit Swatch( ColorPanel& P ) : Panel( P ) { Layout.Width = Px( 44 ); Layout.Height = Px( 30 ); }
        void Draw( DrawList& D ) override
        {
            DrawChecker( D, Bounds, 5 );
            D.RectFilled( Bounds, Panel.GetColor() );
            D.RectStroke( Bounds, GetTheme()[ Tok::BorderStrong ] );
        }
        ColorPanel& Panel;
    };

    // ------------------------------------------------------------ ColorPanel

    ColorPanel::ColorPanel( Color* B, bool A ) : Bind( B ), ShowAlpha( A )
    {
        Layout.Direction = Dir::Column;
        Layout.Gap = 10;
        Add<HsvArea>( *this );
        auto& Row = Add<Widget>();
        Row.Layout.Direction = Dir::Row;
        Row.Layout.Gap = 8;
        Row.Layout.CrossAlign = Align::Center;
        Row.Add<Swatch>( *this );
        Hex = &Row.Add<TextInput>( "#RRGGBB" );
        Hex->Layout.Width = Grow();
        Hex->OnChange = [ this ]( const std::string& T ) {
            Color C;
            if ( ParseHex( T, C ) && C != GetColor() )
            {
                if ( !this->ShowAlpha ) C.a = 255;
                ToHSV( C, H, S, V );
                Emit( C );
            }
        };
        SetColor( GetColor() );
    }

    void ColorPanel::SetColor( Color C )
    {
        ToHSV( C, H, S, V );
        Emit( C );
        Hex->SetText( ToHex( C, ShowAlpha && C.a != 255 ) );
    }

    void ColorPanel::SetFromHSV( float NH, float NS, float NV, uint8_t A )
    {
        H = NH; S = NS; V = NV;
        Color C = FromHSV( H, S, V, A );
        Emit( C );
        if ( !Hex->IsFocused() ) Hex->SetText( ToHex( C, ShowAlpha && A != 255 ) );
    }

    void ColorPanel::Emit( Color C )
    {
        bool Changed = C != GetColor();
        ( Bind ? *Bind : Own ) = C;
        Last = C;
        if ( Changed && OnChange ) OnChange( C );
        Redraw();
    }

    void ColorPanel::Draw( DrawList& D )
    {
        // Someone changed *Bind behind our back: follow it (but don't lose hue on greys).
        if ( Bind && *Bind != Last )
        {
            Last = *Bind;
            ToHSV( *Bind, H, S, V );
            if ( !Hex->IsFocused() ) Hex->SetText( ToHex( *Bind, ShowAlpha && Bind->a != 255 ) );
        }
        DrawChildren( D );
    }

    // ------------------------------------------------------------ ColorPicker (field + popup)

    ColorPicker::ColorPicker( Color* B, bool A ) : Bind( B ), ShowAlpha( A ) { Interactive = true; }

    ColorPicker::~ColorPicker()
    {
        if ( Open ) { Open->OnClose = nullptr; Open->Close(); }
    }

    void ColorPicker::SetColor( Color C )
    {
        if ( C == GetColor() ) return;
        ( Bind ? *Bind : Own ) = C;
        if ( OnChange ) OnChange( C );
        Redraw();
    }

    Vec2 ColorPicker::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        float W = Style.SwatchSize + Style.Padding.Horizontal();
        if ( ShowHex ) W += 8 + std::ceil( F.Width( "#DDDDDDDD" ) );
        return { W, std::max( Style.SwatchSize, F.LineHeight ) + Style.Padding.Vertical() };
    }

    void ColorPicker::Draw( DrawList& D )
    {
        const ColorPickerStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Animate( HoverT, IsHovered() ? 1.f : 0.f );
        Color C = GetColor();

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.RectFilled( Bounds, Color( 255, 255, 255, 10 ).Fade( HoverT ), S.Radius );
        D.RectStroke( Bounds, ( Open ? S.StrokeOpen : S.Stroke ).Resolve(), S.Radius );

        Rect In = Bounds.Shrink( S.Padding );
        Rect Sw = { In.x, std::floor( In.y + ( In.h - S.SwatchSize ) * 0.5f ), S.SwatchSize, S.SwatchSize };
        if ( C.a < 255 ) DrawChecker( D, Sw, 4.5f, 4 );
        D.RectFilled( Sw, C, 4 );
        D.RectStroke( Sw, Color( 0, 0, 0, 60 ), 4 );
        if ( ShowHex )
            D.Text( F, { Sw.Right() + 8, std::floor( In.y + ( In.h - F.LineHeight ) * 0.5f ) }, ToHex( C, ShowAlpha && C.a != 255 ), S.TextColor.Resolve() );
    }

    bool ColorPicker::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type != EventType::MouseUp ) return false;
        if ( !IsHovered() ) return true;
        if ( Open ) { Open->Close(); return true; }

        Popup& P = OpenPopup( *this, Bounds );
        P.MinWidth = 0;
        P.OpenTime = 0.26f; // a bigger panel: a slower, softer slide reads smoother
        P.CloseTime = 0.16f;
        P.Layout.Padding = Insets( 12 );
        Open = &P;
        P.OnClose = [ this ] { Open = nullptr; Redraw(); };
        auto& Panel = P.Add<ColorPanel>( nullptr, ShowAlpha );
        Panel.SetColor( GetColor() );
        Panel.OnChange = [ this ]( Color C ) { SetColor( C ); };
        return true;
    }

    // ---- key binds: a color
    BindValue ColorPicker::BindGet() const { Color C = GetColor(); return { { double( C.r ), double( C.g ), double( C.b ), double( C.a ) } }; }
    void ColorPicker::BindSet( const BindValue& V )
    {
        if ( V.V.size() >= 4 && BindHas( V ) ) SetColor( Color( uint8_t( V.V[ 0 ] ), uint8_t( V.V[ 1 ] ), uint8_t( V.V[ 2 ] ), uint8_t( V.V[ 3 ] ) ) );
    }
    void ColorPicker::BindEditor( Container& Into, BindValue& V )
    {
        if ( V.V.size() < 4 ) V = BindGet();
        auto C = std::make_shared<Color>( Color( uint8_t( V.V[ 0 ] ), uint8_t( V.V[ 1 ] ), uint8_t( V.V[ 2 ] ), uint8_t( V.V[ 3 ] ) ) );
        auto& P = Into.ColorPicker( "Color", C.get(), ShowAlpha );
        P.OnChange = [ C, &V ]( Color X ) { V.V = { double( X.r ), double( X.g ), double( X.b ), double( X.a ) }; };
    }
    std::string ColorPicker::BindText( const BindValue& V ) const
    {
        if ( V.V.size() < 4 ) return "";
        char B[ 16 ];
        std::snprintf( B, sizeof( B ), "#%02X%02X%02X", unsigned( V.V[ 0 ] ), unsigned( V.V[ 1 ] ), unsigned( V.V[ 2 ] ) );
        return B;
    }
}
