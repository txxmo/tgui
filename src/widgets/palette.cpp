#include "tg/widgets/palette.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/container.h"
#include "tg/widgets/textinput.h"
#include "tg/widgets/virtuallist.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace tg
{
    int FuzzyScore( const std::string& P, const std::string& T, std::vector<int>* Matched )
    {
        if ( Matched ) Matched->clear();
        if ( P.empty() ) return 0;
        int Score = 0, Run = 0;
        size_t Ti = 0;
        int Last = -2;
        for ( char Pc : P )
        {
            if ( Pc == ' ' ) continue; // spaces in the query are just separators
            char Lp = char( std::tolower( (unsigned char)Pc ) );
            bool Found = false;
            for ( ; Ti < T.size(); Ti++ )
                if ( std::tolower( (unsigned char)T[ Ti ] ) == Lp )
                {
                    bool WordStart = Ti == 0 || T[ Ti - 1 ] == ' ' || T[ Ti - 1 ] == '-' || T[ Ti - 1 ] == '_' || T[ Ti - 1 ] == '/' ||
                                     ( std::isupper( (unsigned char)T[ Ti ] ) && std::islower( (unsigned char)T[ Ti - 1 ] ) );
                    Run = int( Ti ) == Last + 1 ? Run + 1 : 0;
                    Score += 1 + ( WordStart ? 8 : 0 ) + Run * 4 + ( Ti == 0 ? 6 : 0 );
                    if ( Matched ) Matched->push_back( int( Ti ) );
                    Last = int( Ti );
                    Ti++;
                    Found = true;
                    break;
                }
            if ( !Found ) return -1;
        }
        return Score * 100 / int( 10 + T.size() ) + Score; // prefer shorter names on ties
    }

    // the search field: Escape closes the palette (a plain TextInput would just give up focus)
    struct PaletteInput : TextInput
    {
        CommandPalette* Owner;
        PaletteInput( std::string Ph, CommandPalette* O ) : TextInput( std::move( Ph ) ), Owner( O ) {}
        bool OnEvent( const Event& E ) override
        {
            if ( E.Type == EventType::KeyDown && E.KeyCode == Key::Escape ) { Owner->Close(); return true; }
            return TextInput::OnEvent( E );
        }
    };

    CommandPalette::CommandPalette( std::vector<Command> C, std::string Ph ) : Cmds( std::move( C ) )
    {
        Interactive = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
        auto& Cd = Add<Container>( Dir::Column, 6, Insets( 8 ) );
        Card = &Cd;
        Input = &Cd.Add<PaletteInput>( std::move( Ph ), this );
        Input->OnChange = [ this ]( const std::string& Q ) { Filter( Q ); };
        Input->OnSubmit = [ this ]( const std::string& ) { RunSelected(); };
        List = &Cd.Add<VirtualList>( 0, 36 );
        List->Layout.Height = Fit();
        List->Focusable = false; // the input keeps the keyboard; Up / Down come to us
        List->OnActivate = [ this ]( size_t ) { RunSelected(); };
        List->OnClick = [ this ]( size_t ) { RunSelected(); }; // a click runs it
        List->DrawRow = [ this ]( DrawList& D, size_t i, Rect R, bool, bool Sel ) {
            const Theme& T = GetTheme();
            const Font& F = *T.Body;
            const Font& S = *T.Small;
            const Hit& H = Shown[ i ];
            const Command& Cm = Cmds[ H.Cmd ];
            float X = R.x + 10, Y = std::floor( R.y + ( R.h - F.LineHeight ) * 0.5f );
            if ( Sel ) D.RectFilled( { R.x, R.y + 6, 3, R.h - 12 }, T[ Tok::Accent ], 1.5f );
            if ( !Cm.Icon.empty() ) { D.Text( F, { X, Y }, Cm.Icon, T[ Sel ? Tok::Accent : Tok::TextDim ] ); }
            X += 24;
            // the name with the matched letters in the accent color
            size_t Ci = 0;
            for ( size_t b = 0; b < Cm.Name.size(); )
            {
                size_t e = b;
                bool M = Ci < H.Chars.size() && H.Chars[ Ci ] == int( b );
                while ( e < Cm.Name.size() && ( Ci < H.Chars.size() && H.Chars[ Ci ] == int( e ) ) == M )
                {
                    if ( M ) Ci++;
                    e++;
                }
                std::string Part = Cm.Name.substr( b, e - b );
                D.Text( M ? *T.Bold : F, { X, Y }, Part, M ? T[ Tok::Accent ] : T[ Tok::Text ] );
                X += ( M ? *T.Bold : F ).Width( Part );
                b = e;
            }
            float Rx = R.Right() - 12;
            if ( !Cm.Shortcut.empty() )
            {
                float W = S.Width( Cm.Shortcut ) + 12;
                Rect K = { Rx - W, R.y + ( R.h - 20 ) * 0.5f, W, 20 };
                D.RectFilled( K, T[ Tok::Raised ], 4 );
                D.Text( S, { K.x + 6, std::floor( K.y + ( K.h - S.LineHeight ) * 0.5f ) }, Cm.Shortcut, T[ Tok::TextDim ] );
                Rx = K.x - 10;
            }
            if ( !Cm.Group.empty() ) D.Text( S, { Rx - S.Width( Cm.Group ), std::floor( R.y + ( R.h - S.LineHeight ) * 0.5f ) }, Cm.Group, T[ Tok::TextDim ] );
        };
        Filter( "" );
    }

    void CommandPalette::Filter( const std::string& Q )
    {
        Shown.clear();
        for ( size_t i = 0; i < Cmds.size(); i++ )
        {
            Hit H{ i, 0, {} };
            H.Score = FuzzyScore( Q, Cmds[ i ].Name, &H.Chars );
            if ( H.Score < 0 && !Cmds[ i ].Keywords.empty() )
            {
                int K = FuzzyScore( Q, Cmds[ i ].Keywords );
                if ( K >= 0 ) { H.Score = K / 2; H.Chars.clear(); }
            }
            if ( H.Score >= 0 ) Shown.push_back( std::move( H ) );
        }
        if ( !Q.empty() ) std::stable_sort( Shown.begin(), Shown.end(), []( const Hit& A, const Hit& B ) { return A.Score > B.Score; } );
        List->SetCount( Shown.size() );
        List->Selected = Shown.empty() ? -1 : 0;
        List->ScrollTo( 0 );
        List->Layout.Height = Px( 36.f * std::min<size_t>( Shown.size(), size_t( MaxRows ) ) );
        Invalidate();
    }

    void CommandPalette::RunSelected()
    {
        if ( List->Selected < 0 || size_t( List->Selected ) >= Shown.size() ) return;
        auto Fn = Cmds[ Shown[ size_t( List->Selected ) ].Cmd ].Run;
        Close();
        if ( Fn ) Fn();
    }

    void CommandPalette::Close()
    {
        if ( Closed ) return;
        Closed = true;
        Visible = false;
        if ( Context* C = GetContext() )
        {
            if ( C->Focused() == Input ) C->SetFocus( nullptr );
            C->Post( [ C, this ] { C->Overlay().Remove( this ); } );
            C->InvalidateLayout();
        }
    }

    void CommandPalette::Arrange( Rect R )
    {
        Bounds = R;
        float W = std::min( Width, R.w - 32 );
        float H = Card->MeasureCached( W ).y;
        Card->Arrange( { std::floor( R.x + ( R.w - W ) * 0.5f ), std::floor( R.y + R.h * 0.14f ), W, H } );
    }

    void CommandPalette::Draw( DrawList& D )
    {
        Animate( OpenT, 1.f, 16.f );
        size_t V0 = D.Vtx.size(), C0 = D.Cmds.size();
        D.RectFilled( Bounds, Color( 0, 0, 0, 110 ) );
        Rect Cr = Card->Bounds;
        D.Shadow( Cr, 12, Color( 0, 0, 0, 160 ), 18, { 0, 8 } );
        D.RectFilled( Cr, GetTheme()[ Tok::Surface ], 12 );
        D.RectStroke( Cr, GetTheme()[ Tok::BorderStrong ], 12 );
        DrawChildren( D );
        if ( Shown.empty() )
        {
            const Font& S = *GetTheme().Small;
            D.Text( S, { Cr.x + 16, Cr.Bottom() - 4 }, "No matching commands", GetTheme()[ Tok::TextDim ] );
        }
        if ( OpenT < 1 )
        {
            float E = EaseOutCubic( OpenT );
            D.TransformFrom( V0, C0, { Cr.Center().x, Cr.y }, 0.97f + 0.03f * E, 0, { 0, ( 1 - E ) * -10 } );
            D.FadeFrom( V0, E );
        }
    }

    bool CommandPalette::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::KeyDown )
        {
            int N = int( Shown.size() ), S = List->Selected;
            switch ( E.KeyCode )
            {
            case Key::Down: if ( N ) List->Selected = ( S + 1 ) % N; break;
            case Key::Up: if ( N ) List->Selected = ( S - 1 + N ) % N; break;
            case Key::PageDown: if ( N ) List->Selected = std::min( N - 1, S + MaxRows ); break;
            case Key::PageUp: if ( N ) List->Selected = std::max( 0, S - MaxRows ); break;
            case Key::Escape: Close(); return true;
            default: return false;
            }
            if ( List->Selected >= 0 ) List->ScrollTo( size_t( List->Selected ) );
            Redraw();
            return true;
        }
        if ( E.Type == EventType::MouseDown && !Card->Bounds.Contains( E.Pos ) ) { Close(); return true; }
        return E.Type == EventType::MouseDown || E.Type == EventType::MouseUp;
    }

    Widget* CommandPalette::HitTest( Vec2 P )
    {
        if ( !Visible ) return nullptr;
        if ( Widget* H = Card->HitTest( P ) ) return H;
        return Bounds.Contains( P ) ? this : nullptr; // modal
    }

    CommandPalette& OpenPalette( Context& Ctx, const CommandList& L, std::string Ph )
    {
        auto& P = Ctx.Overlay().Add<CommandPalette>( L.Items, std::move( Ph ) );
        Ctx.InvalidateLayout();
        Ctx.SetFocus( P.Children()[ 0 ]->Children()[ 0 ].get() ); // the search field
        return P;
    }
}
