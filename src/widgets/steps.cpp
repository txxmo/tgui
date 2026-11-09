#include "tg/widgets/steps.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/button.h"
#include "tg/widgets/collapsible.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace tg
{
    // ------------------------------------------------------------ Pagination

    Pagination::Pagination( int* P, int N ) : Bind( P ), PageCount( N ) { Interactive = true; Focusable = true; }

    void Pagination::SetPage( int P )
    {
        P = std::clamp( P, 0, std::max( 0, PageCount - 1 ) );
        if ( P == GetPage() ) return;
        ( Bind ? *Bind : Own ) = P;
        if ( OnChange ) OnChange( P );
        Redraw();
    }

    void Pagination::Build()
    {
        Cells.clear();
        const float S = 30, G = 4;
        float X = Bounds.x;
        auto Add = [ & ]( int P ) { Cells.push_back( { P, { X, Bounds.y, S, S } } ); X += S + G; };
        int Cur = GetPage(), N = PageCount;
        Add( -1 );
        std::vector<int> Show;
        for ( int i = 0; i < N; i++ )
            if ( i == 0 || i == N - 1 || std::abs( i - Cur ) <= Neighbours ) Show.push_back( i );
        for ( size_t k = 0; k < Show.size(); k++ )
        {
            if ( k && Show[ k ] - Show[ k - 1 ] > 1 ) Add( -10 );
            Add( Show[ k ] );
        }
        Add( -2 );
    }

    Vec2 Pagination::Measure( float ) { return { ( 30 + 4 ) * float( std::min( PageCount, 5 + Neighbours * 2 ) + 2 ), 30 }; }

    void Pagination::Draw( DrawList& D )
    {
        Build();
        const Theme& T = GetTheme();
        const Font& F = *T.Body;
        int Cur = GetPage();
        for ( size_t i = 0; i < Cells.size(); i++ )
        {
            auto& C = Cells[ i ];
            bool Hv = int( i ) == Hover && IsHovered(), On = C.Page == Cur;
            bool Off = ( C.Page == -1 && Cur == 0 ) || ( C.Page == -2 && Cur >= PageCount - 1 );
            if ( On ) D.RectFilled( C.R, T[ Tok::Accent ], 6 );
            else if ( Hv && C.Page != -10 && !Off ) D.RectFilled( C.R, T[ Tok::Raised ], 6 );
            std::string S = C.Page == -1 ? "\xE2\x80\xB9" : C.Page == -2 ? "\xE2\x80\xBA" : C.Page == -10 ? "\xE2\x80\xA6" : std::to_string( C.Page + 1 );
            Color Tc = On ? T[ Tok::TextOnAccent ] : Off || C.Page == -10 ? T[ Tok::TextDim ] : T[ Tok::Text ];
            D.Text( F, { std::floor( C.R.Center().x - F.Width( S ) * 0.5f ), std::floor( C.R.Center().y - F.LineHeight * 0.5f ) }, S, Tc );
        }
    }

    bool Pagination::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            int H = -3;
            for ( size_t i = 0; i < Cells.size(); i++ )
                if ( Cells[ i ].R.Contains( E.Pos ) ) H = int( i );
            if ( H != Hover ) { Hover = H; Redraw(); }
            return false;
        }
        case EventType::MouseDown: return E.Button == MouseButton::Left;
        case EventType::MouseUp:
            for ( auto& C : Cells )
                if ( C.R.Contains( E.Pos ) )
                {
                    if ( C.Page == -1 ) SetPage( GetPage() - 1 );
                    else if ( C.Page == -2 ) SetPage( GetPage() + 1 );
                    else if ( C.Page >= 0 ) SetPage( C.Page );
                }
            return true;
        case EventType::KeyDown:
            if ( E.KeyCode == Key::Left ) { SetPage( GetPage() - 1 ); return true; }
            if ( E.KeyCode == Key::Right ) { SetPage( GetPage() + 1 ); return true; }
            if ( E.KeyCode == Key::Home ) { SetPage( 0 ); return true; }
            if ( E.KeyCode == Key::End ) { SetPage( PageCount - 1 ); return true; }
            return false;
        default: return false;
        }
    }

    // ------------------------------------------------------------ Stepper

    Stepper::Stepper( std::vector<std::string> S, int* C ) : Steps( std::move( S ) ), Bind( C ) { Interactive = true; Layout.Width = Grow(); }

    void Stepper::SetStep( int S )
    {
        S = std::clamp( S, 0, std::max( 0, int( Steps.size() ) - 1 ) );
        if ( S == GetStep() ) return;
        ( Bind ? *Bind : Own ) = S;
        if ( OnChange ) OnChange( S );
        Redraw();
    }

    Vec2 Stepper::Measure( float Avail ) { return { std::min( Avail, 120.f * Steps.size() ), 30 + GetTheme().Small->LineHeight + 6 }; }

    Rect Stepper::StepRect( size_t i ) const
    {
        float W = Bounds.w / std::max<size_t>( 1, Steps.size() );
        return { Bounds.x + W * i, Bounds.y, W, Bounds.h };
    }

    void Stepper::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& S = *T.Small;
        const Font& B = *T.Bold;
        int Cur = GetStep();
        if ( Shown < 0 ) Shown = float( Cur );
        float P = Animate( Shown, float( Cur ), 12.f );
        const float R = 13;
        for ( size_t i = 0; i < Steps.size(); i++ )
        {
            Rect Sr = StepRect( i );
            Vec2 C = { Sr.Center().x, Bounds.y + R + 2 };
            if ( i + 1 < Steps.size() ) // connector, filling as you advance
            {
                Vec2 N = { StepRect( i + 1 ).Center().x, C.y };
                float L0 = C.x + R + 6, L1 = N.x - R - 6;
                D.RectFilled( { L0, C.y - 1, L1 - L0, 2 }, T[ Tok::Border ] );
                float Fill = std::clamp( P - float( i ), 0.f, 1.f );
                D.RectFilled( { L0, C.y - 1, ( L1 - L0 ) * Fill, 2 }, T[ Tok::Accent ] );
            }
            bool Done = int( i ) < Cur, Now = int( i ) == Cur;
            if ( Done || Now ) D.CircleFilled( C, R, T[ Tok::Accent ] );
            else D.CircleStroke( C, R, T[ Tok::BorderStrong ], 1.5f );
            if ( Done )
            {
                Vec2 Ck[ 3 ] = { { C.x - 5, C.y }, { C.x - 1.5f, C.y + 3.5f }, { C.x + 5, C.y - 4 } };
                D.Polyline( Ck, 3, T[ Tok::TextOnAccent ], 2 );
            }
            else
            {
                std::string N = std::to_string( i + 1 );
                D.Text( B, { std::floor( C.x - B.Width( N ) * 0.5f ), std::floor( C.y - B.LineHeight * 0.5f ) }, N, Now ? T[ Tok::TextOnAccent ] : T[ Tok::TextDim ] );
            }
            const std::string& L = Steps[ i ];
            D.Text( S, { std::floor( C.x - S.Width( L ) * 0.5f ), C.y + R + 4 }, L, Now ? T[ Tok::Text ] : T[ Tok::TextDim ] );
        }
    }

    bool Stepper::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::MouseDown && E.Button == MouseButton::Left ) return AllowJumpBack;
        if ( E.Type == EventType::MouseUp && E.Button == MouseButton::Left && AllowJumpBack )
        {
            for ( size_t i = 0; i < Steps.size(); i++ )
                if ( StepRect( i ).Contains( E.Pos ) && int( i ) < GetStep() ) SetStep( int( i ) );
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------ Wizard

    Wizard::Wizard() : Container( Dir::Column, 14 )
    {
        Bar = &Add<tg::Stepper>( std::vector<std::string>{} );
        Bar->OnChange = [ this ]( int S ) { Cur = S; Sync(); };
        Pages = &Add<Container>( Dir::Stack, 0 );
        Pages->Layout.Height = Grow();
        auto& Row = Add<Container>( Dir::Row, 8 );
        Row.Layout.MainAlign = Align::End;
        BackB = &Row.Add<tg::Button>( "Back", [ this ] { Back(); } );
        NextB = &Row.Add<tg::Button>( "Next", [ this ] { Next(); } );
        NextB->Style = Button::Primary();
    }

    Container& Wizard::Step( std::string Title )
    {
        Titles.push_back( Title );
        Bar->Steps = Titles;
        auto& P = Pages->Add<Container>( Dir::Column, 10 );
        Steps.push_back( &P );
        Sync();
        return P;
    }

    void Wizard::Go( int S )
    {
        Cur = std::clamp( S, 0, std::max( 0, int( Steps.size() ) - 1 ) );
        Sync();
        if ( OnStepChange ) OnStepChange( Cur );
    }

    void Wizard::Next()
    {
        if ( CanAdvance && !CanAdvance( Cur ) ) return;
        if ( Cur + 1 >= int( Steps.size() ) ) { if ( OnFinish ) OnFinish(); return; }
        Go( Cur + 1 );
    }

    void Wizard::Back() { if ( Cur > 0 ) Go( Cur - 1 ); }

    void Wizard::Sync()
    {
        for ( size_t i = 0; i < Steps.size(); i++ ) Steps[ i ]->Visible = int( i ) == Cur;
        Bar->SetStep( Cur );
        BackB->Enabled = Cur > 0;
        NextB->Text = Cur + 1 >= int( Steps.size() ) ? "Finish" : "Next";
        Invalidate();
    }

    // ------------------------------------------------------------ Accordion

    Accordion::Accordion() : Container( Dir::Column, 6 ) {}

    tg::Collapsible& Accordion::Section( std::string Title, bool Open )
    {
        auto& C = Add<tg::Collapsible>( std::move( Title ) );
        Sections.push_back( &C );
        tg::Collapsible* Self = &C;
        C.OnToggle = [ this, Self ]( bool O ) {
            if ( !O || !Exclusive ) return;
            for ( auto* S : Sections )
                if ( S != Self && S->IsOpen() ) S->SetOpen( false );
        };
        if ( Open ) C.SetOpen( true );
        return C;
    }
}
