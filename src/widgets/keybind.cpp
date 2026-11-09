#include "tg/widgets/keybind.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/listitem.h"
#include "tg/widgets/popup.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    const char* BindModeName( BindMode M )
    {
        switch ( M )
        {
        case BindMode::Hold: return "hold";
        case BindMode::Toggle: return "toggle";
        default: return "always";
        }
    }

    KeybindStyle& Keybind::Defaults()
    {
        static KeybindStyle S;
        return S;
    }

    Keybind::Keybind( std::string L, Key* B ) : Text( std::move( L ) ), Bind( B ), Style( Defaults() )
    {
        Interactive = true; Focusable = true; KeyActivates = true; // Tab to it, Enter / Space clicks
        Focusable = true;
    }

    Keybind::~Keybind()
    {
        if ( Menu ) { Menu->OnClose = nullptr; Menu->Close(); }
        if ( Listening ) if ( Context* C = GetContext() ) C->ReleaseCapture( this );
    }

    void Keybind::Rearm()
    {
        if ( Context* C = GetContext() ) ToggleBase = C->KeyPressCount( GetKey() );
    }

    void Keybind::SetKey( Key K )
    {
        if ( K == GetKey() ) return;
        ( Bind ? *Bind : Own ) = K;
        Rearm();
        if ( OnChange ) OnChange( K );
        Invalidate(); // the box width may change
    }

    void Keybind::SetMode( BindMode M )
    {
        ( ModeBind ? *ModeBind : OwnMode ) = M;
        Rearm();
        Redraw();
    }

    bool Keybind::IsActive() const
    {
        Key K = GetKey();
        if ( GetMode() == BindMode::Always ) return true;
        if ( K == Key::None ) return false;
        Context* C = GetContext();
        if ( !C ) return false;
        if ( GetMode() == BindMode::Hold ) return C->IsKeyDown( K );
        return ( C->KeyPressCount( K ) - ToggleBase ) % 2 == 1;
    }

    void Keybind::StartListening()
    {
        if ( Listening ) return;
        Listening = true;
        if ( Context* C = GetContext() ) C->Capture( this );
        Redraw();
    }

    void Keybind::StopListening()
    {
        if ( !Listening ) return;
        Listening = false;
        if ( Context* C = GetContext() ) C->ReleaseCapture( this );
        Redraw();
    }

    std::string Keybind::BoxText() const
    {
        if ( Listening ) return "...";
        std::string S = GetKey() == Key::None ? "NONE" : KeyName( GetKey() );
        if ( ShowMode && GetKey() != Key::None ) S += std::string( "  \xC2\xB7 " ) + BindModeName( GetMode() );
        return S;
    }

    Rect Keybind::BoxRect() const
    {
        const Font& F = *GetTheme().Small;
        float W = std::max( Style.MinBoxWidth, std::ceil( F.Width( BoxText() ) ) + 20 );
        return { Bounds.Right() - W, std::floor( Bounds.y + ( Bounds.h - Style.BoxHeight ) * 0.5f ), W, Style.BoxHeight };
    }

    Vec2 Keybind::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        const Font& Sf = *GetTheme().Small;
        float Box = std::max( Style.MinBoxWidth, std::ceil( Sf.Width( BoxText() ) ) + 20 );
        float Lbl = Text.empty() ? 0 : std::ceil( F.Width( Text ) ) + Style.LabelGap;
        return { Lbl + Box, std::max( Style.BoxHeight, F.LineHeight ) };
    }

    void Keybind::Draw( DrawList& D )
    {
        const KeybindStyle& S = Style;
        const Font& F = *GetTheme().Body;
        const Font& Sf = *GetTheme().Small;
        Animate( HoverT, IsHovered() ? 1.f : 0.f );
        Animate( ListenT, Listening ? 1.f : 0.f, 20.f );

        // label on the left
        if ( !Text.empty() )
            D.Text( F, { Bounds.x, std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) }, Text, S.Label.Resolve().Fade( Enabled ? 1 : 0.5f ) );

        // key box on the right
        Rect Box = BoxRect();
        D.RectFilled( Box, S.Fill.Resolve(), S.Radius );
        D.RectFilled( Box, Color( 255, 255, 255, 12 ).Fade( HoverT ), S.Radius );
        D.RectStroke( Box, Color::Lerp( S.Stroke.Resolve(), S.StrokeListening.Resolve(), ListenT ), S.Radius );

        std::string T = BoxText();
        Color Tc = Listening ? S.StrokeListening.Resolve() : GetKey() == Key::None ? S.NoneText.Resolve() : S.KeyText.Resolve();
        if ( Listening && GetContext() )
        {
            // gentle pulse while waiting for a key
            double Ph = std::fmod( GetContext()->Time(), 1.2 ) / 1.2;
            Tc = Tc.Fade( 0.55f + 0.45f * float( std::sin( Ph * 6.2831853 ) * 0.5 + 0.5 ) );
            GetContext()->RequestFrameIn( 1.0 / 30 );
        }
        float W = Sf.Width( T );
        D.Text( Sf, { std::floor( Box.x + ( Box.w - W ) * 0.5f ), std::floor( Box.y + ( Box.h - Sf.LineHeight ) * 0.5f ) }, T, Tc );
    }

    void Keybind::OpenModeMenu()
    {
        if ( Menu ) return;
        Rect Box = BoxRect();
        Popup& P = OpenPopup( *this, Box );
        P.MinWidth = std::max( Box.w, 110.f );
        Menu = &P;
        P.OnClose = [ this ] { Menu = nullptr; Redraw(); };
        const BindMode Modes[] = { BindMode::Hold, BindMode::Toggle, BindMode::Always };
        for ( BindMode M : Modes )
        {
            auto& Row = P.Add<ListItem>( BindModeName( M ) );
            Row.ShowMark = true;
            Row.Selected = GetMode() == M;
            Row.OnClick = [ this, M ] { SetMode( M ); if ( Menu ) Menu->Close(); };
        }
    }

    bool Keybind::OnEvent( const Event& E )
    {
        if ( Listening )
        {
            // While listening we have input capture: the next key or button is the bind.
            if ( E.Type == EventType::KeyDown && !E.Repeat )
            {
                SetKey( E.KeyCode == Key::Escape ? Key::None : E.KeyCode ); // Escape = unbind, like tgui
                StopListening();
                return true;
            }
            if ( E.Type == EventType::MouseDown )
            {
                SetKey( MouseKey( E.Button ) );
                StopListening();
                return true;
            }
            if ( E.Type == EventType::FocusOut ) { StopListening(); return false; }
            return true;
        }

        switch ( E.Type )
        {
        case EventType::MouseDown:
            return E.Button == MouseButton::Left || E.Button == MouseButton::Right;
        case EventType::MouseUp:
            if ( !IsHovered() ) return true;
            if ( E.Button == MouseButton::Left ) { StartListening(); return true; }
            if ( E.Button == MouseButton::Right && AllowModes ) { OpenModeMenu(); return true; }
            return false;
        case EventType::KeyDown:
            // keyboard: focused + Enter/Space starts listening
            if ( ( E.KeyCode == Key::Enter || E.KeyCode == Key::Space ) && IsFocused() ) { StartListening(); return true; }
            return false;
        default:
            return false;
        }
    }
}
