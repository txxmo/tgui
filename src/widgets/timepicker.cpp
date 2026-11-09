#include "tg/widgets/timepicker.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/container.h"
#include "tg/widgets/popup.h"
#include "tg/widgets/virtuallist.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    std::string Time::ToString( bool Sec, bool H24 ) const
    {
        char B[ 24 ];
        int Hh = H24 ? H : ( H % 12 == 0 ? 12 : H % 12 );
        if ( Sec ) std::snprintf( B, sizeof( B ), H24 ? "%02d:%02d:%02d" : "%d:%02d:%02d", Hh, M, S );
        else std::snprintf( B, sizeof( B ), H24 ? "%02d:%02d" : "%d:%02d", Hh, M );
        std::string Out = B;
        if ( !H24 ) Out += H < 12 ? " AM" : " PM";
        return Out;
    }

    TimePicker::TimePicker( tg::Time* B ) : Bind( B ) { Interactive = true; Focusable = true; KeyActivates = true; }

    TimePicker::~TimePicker()
    {
        if ( Open ) { Open->OnClose = nullptr; Open->Close(); }
    }

    void TimePicker::SetTime( tg::Time T )
    {
        T.H = ( T.H % 24 + 24 ) % 24;
        T.M = ( T.M % 60 + 60 ) % 60;
        T.S = ( T.S % 60 + 60 ) % 60;
        if ( T == GetTime() ) return;
        ( Bind ? *Bind : Own ) = T;
        if ( OnChange ) OnChange( T );
        Redraw();
    }

    void TimePicker::BindSet( const BindValue& V )
    {
        if ( V.V.size() >= 2 ) SetTime( { int( V.V[ 0 ] ), int( V.V[ 1 ] ), V.V.size() > 2 ? int( V.V[ 2 ] ) : 0 } );
    }

    Vec2 TimePicker::Measure( float Avail )
    {
        const Font& F = *GetTheme().Body;
        return { std::min( Avail, std::ceil( F.Width( Seconds ? "00:00:00 PM" : "00:00 PM" ) ) + 44 ), F.LineHeight + 16 };
    }

    int TimePicker::Part( Vec2 P ) const
    {
        const Font& F = *GetTheme().Body;
        float X = P.x - Bounds.x - 10, Two = F.Width( "00:" );
        return std::clamp( int( X / Two ), 0, Seconds ? 2 : 1 );
    }

    void TimePicker::Bump( int Pt, int D )
    {
        tg::Time T = GetTime();
        if ( Pt == 0 ) T.H += D;
        else if ( Pt == 1 ) T.M += D;
        else T.S += D;
        SetTime( T );
    }

    void TimePicker::OpenList()
    {
        if ( Open ) return;
        Popup& P = OpenPopup( *this, Bounds );
        P.MinWidth = 0;
        P.Layout.Padding = Insets( 6 );
        P.Layout.Direction = Dir::Row;
        P.Layout.Gap = 4;
        Open = &P;
        P.OnClose = [ this ] { Open = nullptr; Redraw(); };
        auto Column = [ & ]( int Count, int Step, int Cur, std::function<void( int )> Pick ) {
            auto& L = P.Add<VirtualList>( size_t( ( Count + Step - 1 ) / Step ), 28 );
            L.Layout.Width = Px( 56 );
            L.Layout.Height = Px( 28 * 7 );
            L.TextOf = [ Step ]( size_t i ) { char B[ 8 ]; std::snprintf( B, sizeof( B ), "%02d", int( i ) * Step ); return std::string( B ); };
            L.Selected = Cur / Step;
            L.OnClick = [ Pick, Step ]( size_t i ) { Pick( int( i ) * Step ); };
            L.Arrange( { 0, 0, 56, 28 * 7 } );
            L.ScrollTo( size_t( std::max( 0, Cur / Step + 3 ) ) );
        };
        tg::Time T = GetTime();
        Column( 24, 1, T.H, [ this ]( int V ) { tg::Time N = GetTime(); N.H = V; SetTime( N ); } );
        Column( 60, std::max( 1, MinuteStep ), T.M, [ this ]( int V ) { tg::Time N = GetTime(); N.M = V; SetTime( N ); } );
        if ( Seconds ) Column( 60, 1, T.S, [ this ]( int V ) { tg::Time N = GetTime(); N.S = V; SetTime( N ); } );
    }

    void TimePicker::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& F = *T.Body;
        D.RectFilled( Bounds, T[ Tok::SurfaceAlt ], 6 );
        D.RectStroke( Bounds, Open ? T[ Tok::Accent ] : T[ Tok::Border ], 6 );
        float Y = std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f );
        if ( IsHovered() && !Open ) // highlight the part the wheel / arrows will change
        {
            Context* C = GetContext();
            int Pt = C ? Part( C->MousePos() ) : 0;
            float Two = F.Width( "00:" );
            D.RectFilled( { Bounds.x + 8 + Pt * Two, Bounds.y + 5, F.Width( "00" ) + 4, Bounds.h - 10 }, T[ Tok::Raised ], 4 );
        }
        D.Text( F, { Bounds.x + 10, Y }, GetTime().ToString( Seconds, H24 ), T[ Tok::Text ] );
        // clock glyph
        Vec2 C = { Bounds.Right() - 18, Bounds.Center().y };
        D.CircleStroke( C, 6.5f, T[ Tok::TextDim ], 1.3f );
        D.Line( C, C + Vec2( 0, -4 ), T[ Tok::TextDim ], 1.3f );
        D.Line( C, C + Vec2( 3, 1.5f ), T[ Tok::TextDim ], 1.3f );
    }

    bool TimePicker::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseWheel: Bump( Part( E.Pos ), E.Wheel.y > 0 ? 1 : -1 ); return true;
        case EventType::MouseMove: Redraw(); return false;
        case EventType::MouseDown: return E.Button == MouseButton::Left;
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left ) return false;
            if ( IsHovered() ) { if ( Open ) Open->Close(); else OpenList(); }
            return true;
        case EventType::KeyDown:
            if ( E.KeyCode == Key::Up ) { Bump( ( E.Mods & ModShift ) ? 0 : 1, 1 ); return true; }
            if ( E.KeyCode == Key::Down ) { Bump( ( E.Mods & ModShift ) ? 0 : 1, -1 ); return true; }
            return false;
        default: return false;
        }
    }
}
