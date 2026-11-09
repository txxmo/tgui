#include "tg/widgets/number.h"
#include <memory>
#include "tg/widgets/choice.h"
#include "tg/widgets/container.h"
#include "tg/binds.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/textinput.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tg
{
    NumberInput::NumberInput( float* B, float Mn, float Mx, float St, const char* Fmt ) : Bind( B ), Min( Mn ), Max( Mx ), Step( St ), Format( Fmt ? Fmt : "%g" )
    {
        Interactive = true;
        Own = Mn;
        Text = &Add<TextInput>();
        Text->Style.Fill = Color( 0, 0, 0, 0 );   // the NumberInput draws the frame
        Text->Style.Stroke = Color( 0, 0, 0, 0 );
        Text->Style.StrokeFocused = Color( 0, 0, 0, 0 );
        Text->OnSubmit = [ this ]( const std::string& ) { Commit(); };
        Text->OnBlur = [ this ] { Commit(); };
        Shown = GetValue();
        Text->SetText( Formatted( Shown ) );
    }

    std::string NumberInput::Formatted( float V ) const
    {
        char B[ 64 ];
        std::snprintf( B, sizeof( B ), Format.c_str(), double( V ) );
        return B;
    }

    void NumberInput::SetValue( float V )
    {
        V = Clamp( V, std::min( Min, Max ), std::max( Min, Max ) );
        if ( Step > 0 ) V = Min + std::round( ( V - Min ) / Step ) * Step;
        if ( V != GetValue() )
        {
            ( Bind ? *Bind : Own ) = V;
            if ( OnChange ) OnChange( V );
        }
        Shown = GetValue();
        Text->SetText( Formatted( Shown ) );
        Redraw();
    }

    void NumberInput::StepBy( float N ) { SetValue( GetValue() + N * ( Step > 0 ? Step : 1 ) ); }

    void NumberInput::Commit()
    {
        const std::string& S = Text->GetText();
        char* End = nullptr;
        double V = std::strtod( S.c_str(), &End );
        if ( End != S.c_str() ) SetValue( float( V ) );
        else SetValue( GetValue() ); // not a number: restore
    }

    Vec2 NumberInput::Measure( float Avail )
    {
        float H = Text->MeasureCached( 100 ).y;
        return { std::min( Avail, 140.f ), H };
    }

    void NumberInput::Arrange( Rect R )
    {
        Bounds = R;
        float Bw = Style.ButtonWidth;
        MinusR = { R.Right() - Bw * 2, R.y, Bw, R.h };
        PlusR = { R.Right() - Bw, R.y, Bw, R.h };
        Text->Arrange( { R.x, R.y, R.w - Bw * 2, R.h } );
    }

    void NumberInput::Draw( DrawList& D )
    {
        const NumberStyle& S = Style;
        // follow external changes to *Bind while not editing
        if ( GetValue() != Shown && !Text->IsFocused() ) { Shown = GetValue(); Text->SetText( Formatted( Shown ) ); }

        bool Foc = Text->IsFocused();
        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        DrawChildren( D );
        D.RectFilled( { MinusR.x, Bounds.y + 6, 1, Bounds.h - 12 }, S.Stroke.Resolve() );
        D.RectFilled( { PlusR.x, Bounds.y + 6, 1, Bounds.h - 12 }, S.Stroke.Resolve() );
        for ( int B : { -1, 1 } )
        {
            Rect R = B < 0 ? MinusR : PlusR;
            bool Hot = HoverBtn == B && IsHovered();
            if ( PressBtn == B && IsPressed() ) D.RectFilled( R.Shrink( 3 ), Color( 255, 255, 255, 14 ), 4 );
            Color G = ( Hot ? S.GlyphHover : S.Glyph ).Resolve();
            Vec2 C = R.Center();
            D.RectFilled( { C.x - 5, C.y - 0.75f, 10, 1.5f }, G );
            if ( B > 0 ) D.RectFilled( { C.x - 0.75f, C.y - 5, 1.5f, 10 }, G );
        }
        D.RectStroke( Bounds, ( Foc ? S.StrokeFocused : S.Stroke ).Resolve(), S.Radius );
    }

    Widget* NumberInput::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( MinusR.Contains( P ) || PlusR.Contains( P ) ) return this;
        if ( Text->HitTest( P ) ) return Text;
        return this;
    }

    bool NumberInput::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            int H = MinusR.Contains( E.Pos ) ? -1 : PlusR.Contains( E.Pos ) ? 1 : 0;
            if ( H != HoverBtn ) { HoverBtn = H; Redraw(); }
            return false;
        }
        case EventType::MouseLeave: HoverBtn = 0; Redraw(); return false;
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            PressBtn = MinusR.Contains( E.Pos ) ? -1 : PlusR.Contains( E.Pos ) ? 1 : 0;
            return true;
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left ) return false;
            if ( PressBtn != 0 && ( PressBtn < 0 ? MinusR : PlusR ).Contains( E.Pos ) ) { Commit(); StepBy( float( PressBtn ) ); }
            PressBtn = 0;
            return true;
        case EventType::KeyDown: // bubbled up from the text field
            if ( E.KeyCode == Key::Up || E.KeyCode == Key::Down )
            {
                Commit();
                StepBy( ( E.KeyCode == Key::Up ? 1.f : -1.f ) * ( E.Mods & ModShift ? 10.f : 1.f ) );
                return true;
            }
            return false;
        case EventType::MouseWheel:
            if ( !Text->IsFocused() ) return false; // don't hijack page scrolling
            Commit();
            StepBy( E.Wheel.y > 0 ? 1.f : -1.f );
            return true;
        default:
            return false;
        }
    }

    // ---- key binds: a number
    BindValue NumberInput::BindGet() const { return { { GetValue() } }; }
    void NumberInput::BindSet( const BindValue& V ) { if ( BindHas( V ) ) SetValue( float( V.V[ 0 ] ) ); }
    void NumberInput::BindEditor( Container& Into, BindValue& V )
    {
        auto F = std::make_shared<float>( BindHas( V ) ? float( V.V[ 0 ] ) : GetValue() );
        V.V = { *F };
        auto& S = Into.Number( "Value", F.get(), Min, Max, Step, Format.c_str() );
        auto Show = S.OnChange;
        S.OnChange = [ F, Show, &V ]( float X ) { if ( Show ) Show( X ); V.V = { X }; };
    }
    std::string NumberInput::BindText( const BindValue& V ) const { return BindHas( V ) ? BindNumber( V.V[ 0 ], Step ) : ""; }
}
