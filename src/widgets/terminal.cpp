#include "tg/widgets/terminal.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/pty.h"
#include "tg/widgets/menu.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace tg
{
    // ------------------------------------------------------------ helpers

    // Display width of a codepoint in cells (a compact wcwidth): 0 combining, 2 wide (CJK, emoji), else 1.
    static int CellWidth( uint32_t C )
    {
        if ( C < 0x300 ) return 1;
        if ( C >= 0x1F1E6 && C <= 0x1F1FF ) return 2; // regional indicator: the flag's two letters share 2 cells
        if ( ( C >= 0x300 && C <= 0x36F ) || ( C >= 0x200B && C <= 0x200F ) || ( C >= 0xFE00 && C <= 0xFE0F ) || C == 0x20E3 ||
             ( C >= 0x1F3FB && C <= 0x1F3FF ) || ( C >= 0xE0020 && C <= 0xE007F ) )
            return 0;
        if ( ( C >= 0x1100 && C <= 0x115F ) || ( C >= 0x2E80 && C <= 0xA4CF && C != 0x303F ) || ( C >= 0xAC00 && C <= 0xD7A3 ) ||
             ( C >= 0xF900 && C <= 0xFAFF ) || ( C >= 0xFE30 && C <= 0xFE4F ) || ( C >= 0xFF00 && C <= 0xFF60 ) || ( C >= 0xFFE0 && C <= 0xFFE6 ) ||
             ( C >= 0x1F300 && C <= 0x1F64F ) || ( C >= 0x1F680 && C <= 0x1F6FF ) || ( C >= 0x1F900 && C <= 0x1F9FF ) ||
             ( C >= 0x1FA70 && C <= 0x1FAFF ) || ( C >= 0x20000 && C <= 0x3FFFD ) || C == 0x231A || C == 0x231B || C == 0x23F0 || C == 0x23F3 ||
             C == 0x2705 || C == 0x274C || C == 0x2753 || C == 0x2757 || C == 0x2B50 || C == 0x2B55 || ( C >= 0x25FD && C <= 0x25FE ) )
            return 2;
        return 1;
    }

    // DEC special graphics (ESC ( 0): the line-drawing set used by curses, dialog, tmux...
    static uint32_t DecGraphics( uint32_t C )
    {
        static const uint32_t Map[] = { 0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0, 0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C,
                                        0x2514, 0x253C, 0x23BA, 0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C, 0x2502, 0x2264,
                                        0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7 };
        return C >= 0x60 && C <= 0x7E ? Map[ C - 0x60 ] : C;
    }

    static void AppendUtf8( std::string& S, uint32_t C )
    {
        if ( C < 0x80 ) S += char( C );
        else if ( C < 0x800 ) { S += char( 0xC0 | C >> 6 ); S += char( 0x80 | ( C & 0x3F ) ); }
        else if ( C < 0x10000 ) { S += char( 0xE0 | C >> 12 ); S += char( 0x80 | ( ( C >> 6 ) & 0x3F ) ); S += char( 0x80 | ( C & 0x3F ) ); }
        else { S += char( 0xF0 | C >> 18 ); S += char( 0x80 | ( ( C >> 12 ) & 0x3F ) ); S += char( 0x80 | ( ( C >> 6 ) & 0x3F ) ); S += char( 0x80 | ( C & 0x3F ) ); }
    }

    static constexpr uint32_t ColIndex = 0x1000000, ColRgb = 0x2000000, SeqBit = 0x80000000u;

    // Emoji sequences are interned process-wide: a cell stays 16 bytes, the table only grows with
    // distinct sequences (a few hundred at most in practice).
    static std::vector<std::string>& SeqTable() { static std::vector<std::string> T; return T; }
    static uint32_t Intern( const std::string& S )
    {
        static std::unordered_map<std::string, uint32_t> Ids;
        auto It = Ids.find( S );
        if ( It != Ids.end() ) return It->second;
        SeqTable().push_back( S );
        uint32_t Id = SeqBit | uint32_t( SeqTable().size() - 1 );
        Ids.emplace( S, Id );
        return Id;
    }

    const std::string& Terminal::SequenceText( uint32_t Cp )
    {
        static const std::string Empty;
        size_t I = Cp & ~SeqBit;
        return ( Cp & SeqBit ) && I < SeqTable().size() ? SeqTable()[ I ] : Empty;
    }

    std::string Terminal::CellText( const TermCell& C )
    {
        if ( C.Cp & SeqBit ) return SequenceText( C.Cp );
        std::string S;
        AppendUtf8( S, C.Cp ? C.Cp : ' ' );
        return S;
    }

    static bool IsJoiner( uint32_t C ) // continues the previous character
    {
        return C == 0x200D || C == 0xFE0F || C == 0x20E3 || ( C >= 0x1F3FB && C <= 0x1F3FF ) || ( C >= 0xE0020 && C <= 0xE007F );
    }

    // Glyph for a cell: a shaped emoji sequence (HarfBuzz builds), else its first codepoint.
    static const Glyph& CellGlyph( const Font& F, uint32_t Cp )
    {
        if ( !( Cp & SeqBit ) ) return F.Find( Cp );
        const std::string& S = Terminal::SequenceText( Cp );
        if ( const Glyph* G = F.FindSequence( S ) ) return *G;
        size_t i = 0;
        return F.Find( S.empty() ? uint32_t( '?' ) : DecodeUtf8( S, i ) );
    }

    // ------------------------------------------------------------ setup

    Terminal::Terminal()
    {
        Interactive = true;
        Focusable = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
        Screen.assign( size_t( H ), Line( size_t( W ) ) );
        AltBuf = Screen;
        Tabs.assign( size_t( W ), false );
        for ( int i = 8; i < W; i += 8 ) Tabs[ size_t( i ) ] = true;
        Bottom = H - 1;
        ContextMenu = [ this ]( Menu& M ) {
            M.Item( "Copy", [ this ] { Context* C = GetContext(); if ( C && C->SetClipboard && HasSel ) C->SetClipboard( SelectedText() ); }, "Ctrl+Shift+C" ).Enabled = HasSel;
            M.Item( "Paste", [ this ] { Context* C = GetContext(); if ( C && C->GetClipboard ) Paste( C->GetClipboard() ); }, "Ctrl+Shift+V" );
            M.Separator();
            M.Item( "Clear", [ this ] { Clear(); } );
        };
    }

    Terminal::~Terminal()
    {
        Alive.reset();
        Kill();
    }

    TermCell Terminal::Blank() const
    {
        TermCell C;
        C.Bg = Pen.Bg; // erase with the current background (xterm "BCE")
        return C;
    }

    void Terminal::Reset()
    {
        Pen = TermCell{};
        Cx = Cy = 0;
        WrapPending = Alt = AppCursor = Bracketed = Origin = false;
        CursorVisible = AutoWrap = true;
        Top = 0;
        Bottom = H - 1;
        G0Lines = G1Lines = UseG1 = false;
        State = St::Ground;
        for ( auto& L : Screen ) EraseCells( L, 0, W );
        Redraw();
    }

    void Terminal::Clear()
    {
        Scrollback.clear();
        for ( auto& L : Screen ) EraseCells( L, 0, W );
        Cx = Cy = 0;
        View = 0;
        HasSel = false;
        if ( Proc ) Send( "\x0c" ); // let a shell redraw its prompt
        Redraw();
    }

    void Terminal::UpdateMetrics()
    {
        const Font& F = Style.TextFont ? *Style.TextFont : *GetTheme().Mono;
        float S = GetContext() ? GetContext()->GetScale() : 1.f;
        CellW = std::max( 1.f, std::round( F.Find( 'M' ).Advance * S ) / S ); // whole device pixels: no seams
        CellH = std::max( 1.f, std::round( F.LineHeight * S ) / S );
    }

    Vec2 Terminal::Measure( float Avail )
    {
        UpdateMetrics();
        return { std::min( Avail, 80 * CellW + Style.Padding.Horizontal() ), 24 * CellH + Style.Padding.Vertical() };
    }

    void Terminal::Arrange( Rect R )
    {
        Bounds = R;
        UpdateMetrics();
        Rect In = Inner();
        Resize( std::max( 2, int( In.w / CellW ) ), std::max( 1, int( In.h / CellH ) ) );
    }

    void Terminal::Resize( int C, int R )
    {
        if ( C == W && R == H ) return;
        auto Fit = [ & ]( std::vector<Line>& Buf, bool Main ) {
            for ( auto& L : Buf ) L.resize( size_t( C ), TermCell{} );
            if ( R < int( Buf.size() ) )
            {
                // shrinking: keep the cursor's line on screen by pushing the top into the scrollback
                int Drop = Main ? std::max( 0, Cy - ( R - 1 ) ) : 0;
                for ( int i = 0; i < Drop; i++ ) { Scrollback.push_back( Buf.front() ); Buf.erase( Buf.begin() ); }
                Buf.resize( size_t( R ) );
                if ( Main ) Cy -= Drop;
            }
            else
                while ( int( Buf.size() ) < R ) Buf.push_back( Line( size_t( C ) ) );
        };
        Fit( Screen, !Alt );
        Fit( AltBuf, Alt );
        while ( Scrollback.size() > ScrollbackLimit ) Scrollback.pop_front();
        W = C;
        H = R;
        Cx = std::clamp( Cx, 0, W - 1 );
        Cy = std::clamp( Cy, 0, H - 1 );
        Top = 0;
        Bottom = H - 1;
        WrapPending = false;
        Tabs.assign( size_t( W ), false );
        for ( int i = 8; i < W; i += 8 ) Tabs[ size_t( i ) ] = true;
        if ( Proc ) Proc->Resize( W, H );
        if ( OnResize ) OnResize( W, H );
        Redraw();
    }

    const TermCell& Terminal::Cell( int C, int R ) const
    {
        static const TermCell Empty;
        if ( R < 0 || R >= H || C < 0 || C >= W ) return Empty;
        return Screen[ size_t( R ) ][ size_t( C ) ];
    }

    std::string Terminal::RowText( int R ) const
    {
        std::string S;
        if ( R < 0 || R >= H ) return S;
        for ( auto& C : Screen[ size_t( R ) ] )
            if ( C.Width ) S += CellText( C );
        while ( !S.empty() && S.back() == ' ' ) S.pop_back();
        return S;
    }

    // ------------------------------------------------------------ output side: the parser

    void Terminal::Write( std::string_view B )
    {
        for ( unsigned char Ch : B )
        {
            switch ( State )
            {
            case St::Ground:
                if ( Ch == 0x1B ) { State = St::Esc; Utf8Need = 0; break; }
                if ( Ch < 0x20 ) { Utf8Need = 0; Control( Ch ); break; }
                if ( Ch == 0x7F ) break;
                if ( Ch < 0x80 ) { Utf8Need = 0; Put( Ch ); break; }
                if ( Ch >= 0xC0 )
                {
                    Utf8Need = Ch >= 0xF0 ? 3 : Ch >= 0xE0 ? 2 : 1;
                    Utf8Cp = Ch & ( 0x3F >> Utf8Need );
                    break;
                }
                if ( Utf8Need == 0 ) { Put( 0xFFFD ); break; }
                Utf8Cp = ( Utf8Cp << 6 ) | ( Ch & 0x3F );
                if ( --Utf8Need == 0 ) Put( Utf8Cp );
                break;
            case St::Esc:
                Seq.clear();
                if ( Ch == '[' ) State = St::Csi;
                else if ( Ch == ']' ) State = St::Osc;
                else if ( Ch == '(' || Ch == ')' ) { CharsetSlot = char( Ch ); State = St::Charset; }
                else if ( Ch == 'P' || Ch == 'X' || Ch == '^' || Ch == '_' ) State = St::Ignore; // DCS / SOS / PM / APC
                else { State = St::Ground; EscDispatch( char( Ch ) ); }
                break;
            case St::Csi:
                if ( Ch >= 0x20 && Ch <= 0x3F ) { if ( Seq.size() < 256 ) Seq += char( Ch ); }
                else if ( Ch >= 0x40 && Ch <= 0x7E ) { State = St::Ground; CsiDispatch( char( Ch ) ); }
                else if ( Ch == 0x1B ) State = St::Esc;
                else if ( Ch < 0x20 ) Control( Ch ); // C0 controls execute even mid-sequence
                break;
            case St::Osc:
                if ( Ch == 0x07 ) { State = St::Ground; OscDispatch(); }
                else if ( Ch == 0x1B ) State = St::OscEsc;
                else if ( Seq.size() < 4096 ) Seq += char( Ch );
                break;
            case St::OscEsc:
                State = St::Ground;
                OscDispatch(); // ESC \ (or anything) ends the string
                break;
            case St::Charset:
                ( CharsetSlot == '(' ? G0Lines : G1Lines ) = Ch == '0';
                State = St::Ground;
                break;
            case St::Ignore:
                if ( Ch == 0x07 ) State = St::Ground;
                else if ( Ch == 0x1B ) State = St::OscEsc, Seq.clear();
                break;
            }
        }
        Redraw();
    }

    void Terminal::EraseCells( Line& L, int From, int To )
    {
        From = std::max( 0, From );
        To = std::min( int( L.size() ), To );
        for ( int i = From; i < To; i++ ) L[ size_t( i ) ] = Blank();
    }

    void Terminal::Put( uint32_t Cp )
    {
        if ( UseG1 ? G1Lines : G0Lines ) Cp = DecGraphics( Cp );
        // emoji sequences: selectors, skin tones, keycaps, ZWJ + emoji, the 2nd flag letter join the previous cell
        if ( LastY >= 0 && LastY < H && LastX < W )
        {
            TermCell& P = Screen[ size_t( LastY ) ][ size_t( LastX ) ];
            bool PrevRegional = !( P.Cp & SeqBit ) && P.Cp >= 0x1F1E6 && P.Cp <= 0x1F1FF;
            const std::string Prev = CellText( P );
            bool AfterZwj = Prev.size() >= 3 && Prev.compare( Prev.size() - 3, 3, "\xE2\x80\x8D" ) == 0;
            if ( P.Width && ( IsJoiner( Cp ) || AfterZwj || ( PrevRegional && Cp >= 0x1F1E6 && Cp <= 0x1F1FF ) ) )
            {
                std::string S = Prev;
                AppendUtf8( S, Cp );
                P.Cp = Intern( S );
                if ( PrevRegional && !IsJoiner( Cp ) ) LastY = -1; // a flag is exactly two letters
                return;
            }
        }
        int Wd = CellWidth( Cp );
        if ( Wd == 0 ) return; // combining marks / stray joiners: dropped
        if ( WrapPending && AutoWrap ) { Cx = 0; LineFeed(); }
        WrapPending = false;
        if ( Wd == 2 && Cx == W - 1 )
        {
            if ( !AutoWrap ) return;
            EraseCells( Screen[ size_t( Cy ) ], Cx, Cx + 1 );
            Cx = 0;
            LineFeed();
        }
        Line& L = Screen[ size_t( Cy ) ];
        auto Unhalf = [ & ]( int X ) { // overwriting half of a wide char clears the other half
            if ( X < 0 || X >= W ) return;
            if ( L[ size_t( X ) ].Width == 2 && X + 1 < W ) L[ size_t( X + 1 ) ] = Blank();
            if ( L[ size_t( X ) ].Width == 0 && X > 0 ) L[ size_t( X - 1 ) ] = Blank();
        };
        Unhalf( Cx );
        TermCell C = Pen;
        C.Cp = Cp;
        C.Width = uint8_t( Wd );
        L[ size_t( Cx ) ] = C;
        LastX = Cx;
        LastY = Cy;
        if ( Wd == 2 )
        {
            Unhalf( Cx + 1 );
            TermCell R = Pen;
            R.Cp = 0;
            R.Width = 0;
            L[ size_t( Cx + 1 ) ] = R;
        }
        Cx += Wd;
        if ( Cx >= W ) { Cx = W - 1; WrapPending = true; }
    }

    void Terminal::Control( uint8_t C )
    {
        LastY = -1;
        switch ( C )
        {
        case 0x07: if ( OnBell ) OnBell(); break;
        case 0x08: if ( Cx > 0 ) Cx--; WrapPending = false; break;
        case 0x09:
            WrapPending = false;
            do Cx++; while ( Cx < W - 1 && !Tabs[ size_t( Cx ) ] );
            Cx = std::min( Cx, W - 1 );
            break;
        case 0x0A: case 0x0B: case 0x0C: LineFeed(); break;
        case 0x0D: Cx = 0; WrapPending = false; break;
        case 0x0E: UseG1 = true; break;
        case 0x0F: UseG1 = false; break;
        default: break;
        }
    }

    void Terminal::LineFeed()
    {
        WrapPending = false;
        if ( Cy == Bottom ) ScrollUp( Top, Bottom, 1 );
        else if ( Cy < H - 1 ) Cy++;
    }

    void Terminal::ScrollUp( int T, int B, int N )
    {
        N = std::clamp( N, 0, B - T + 1 );
        for ( int i = 0; i < N; i++ )
        {
            // lines scrolled off the top of the full main screen go to the scrollback
            if ( T == 0 && !Alt )
            {
                Scrollback.push_back( Screen[ size_t( T ) ] );
                if ( Scrollback.size() > ScrollbackLimit ) Scrollback.pop_front();
                else if ( View > 0 ) View++; // keep the user's view still while they read history
            }
            Line L = std::move( Screen[ size_t( T ) ] );
            Screen.erase( Screen.begin() + T );
            EraseCells( L, 0, W );
            Screen.insert( Screen.begin() + B, std::move( L ) );
        }
    }

    void Terminal::ScrollDown( int T, int B, int N )
    {
        N = std::clamp( N, 0, B - T + 1 );
        for ( int i = 0; i < N; i++ )
        {
            Line L = std::move( Screen[ size_t( B ) ] );
            Screen.erase( Screen.begin() + B );
            EraseCells( L, 0, W );
            Screen.insert( Screen.begin() + T, std::move( L ) );
        }
    }

    void Terminal::EscDispatch( char C )
    {
        LastY = -1;
        switch ( C )
        {
        case '7': SavedX = Cx; SavedY = Cy; SavedPen = Pen; SavedG0 = G0Lines; break;
        case '8': Cx = SavedX; Cy = SavedY; Pen = SavedPen; G0Lines = SavedG0; WrapPending = false; break;
        case 'D': LineFeed(); break;
        case 'E': Cx = 0; LineFeed(); break;
        case 'M':
            WrapPending = false;
            if ( Cy == Top ) ScrollDown( Top, Bottom, 1 );
            else if ( Cy > 0 ) Cy--;
            break;
        case 'H': if ( Cx < W ) Tabs[ size_t( Cx ) ] = true; break;
        case 'c': Reset(); break;
        default: break; // '=' '>' keypad modes etc.
        }
    }

    void Terminal::Reply( std::string_view S ) { Send( S ); }

    void Terminal::CsiDispatch( char F )
    {
        LastY = -1;
        char Priv = 0;
        std::string_view S = Seq;
        if ( !S.empty() && ( S[ 0 ] == '?' || S[ 0 ] == '>' || S[ 0 ] == '=' || S[ 0 ] == '<' ) ) { Priv = S[ 0 ]; S.remove_prefix( 1 ); }
        bool Inter = S.find_first_of( " !\"#$%&'()*+,-./" ) != std::string_view::npos;
        std::vector<int> P;
        {
            int V = -1;
            for ( char Ch : S )
            {
                if ( Ch >= '0' && Ch <= '9' ) V = ( V < 0 ? 0 : V ) * 10 + ( Ch - '0' ), V = std::min( V, 99999 );
                else if ( Ch == ';' || Ch == ':' ) { P.push_back( V ); V = -1; }
            }
            P.push_back( V );
        }
        auto Arg = [ & ]( size_t i, int Def ) { return i < P.size() && P[ i ] > 0 ? P[ i ] : Def; };
        if ( Inter ) return; // e.g. "CSI 2 q" (cursor style): not supported, harmless
        int N = Arg( 0, 1 );
        Line& L = Screen[ size_t( Cy ) ];
        if ( F != 'm' ) WrapPending = false;

        switch ( F )
        {
        case 'A': Cy = std::max( Cy >= Top ? Top : 0, Cy - N ); break;
        case 'B': case 'e': Cy = std::min( Cy <= Bottom ? Bottom : H - 1, Cy + N ); break;
        case 'C': case 'a': Cx = std::min( W - 1, Cx + N ); break;
        case 'D': Cx = std::max( 0, Cx - N ); break;
        case 'E': Cy = std::min( H - 1, Cy + N ); Cx = 0; break;
        case 'F': Cy = std::max( 0, Cy - N ); Cx = 0; break;
        case 'G': case '`': Cx = std::clamp( N - 1, 0, W - 1 ); break;
        case 'd': Cy = std::clamp( N - 1 + ( Origin ? Top : 0 ), 0, H - 1 ); break;
        case 'H': case 'f':
            Cy = std::clamp( Arg( 0, 1 ) - 1 + ( Origin ? Top : 0 ), 0, H - 1 );
            Cx = std::clamp( Arg( 1, 1 ) - 1, 0, W - 1 );
            break;
        case 'J':
        {
            int M = P[ 0 ] < 0 ? 0 : P[ 0 ];
            if ( M == 0 ) { EraseCells( L, Cx, W ); for ( int y = Cy + 1; y < H; y++ ) EraseCells( Screen[ size_t( y ) ], 0, W ); }
            else if ( M == 1 ) { EraseCells( L, 0, Cx + 1 ); for ( int y = 0; y < Cy; y++ ) EraseCells( Screen[ size_t( y ) ], 0, W ); }
            else if ( M == 2 ) for ( auto& Ln : Screen ) EraseCells( Ln, 0, W );
            else if ( M == 3 ) { Scrollback.clear(); View = 0; }
            break;
        }
        case 'K':
        {
            int M = P[ 0 ] < 0 ? 0 : P[ 0 ];
            if ( M == 0 ) EraseCells( L, Cx, W );
            else if ( M == 1 ) EraseCells( L, 0, Cx + 1 );
            else EraseCells( L, 0, W );
            break;
        }
        case 'L': if ( Cy >= Top && Cy <= Bottom ) { ScrollDown( Cy, Bottom, N ); Cx = 0; } break;
        case 'M': if ( Cy >= Top && Cy <= Bottom ) { bool A = Alt; Alt = true; ScrollUp( Cy, Bottom, N ); Alt = A; Cx = 0; } break; // never into scrollback
        case '@':
            N = std::min( N, W - Cx );
            L.insert( L.begin() + Cx, size_t( N ), Blank() );
            L.resize( size_t( W ) );
            break;
        case 'P':
            N = std::min( N, W - Cx );
            L.erase( L.begin() + Cx, L.begin() + Cx + N );
            L.resize( size_t( W ), Blank() );
            break;
        case 'X': EraseCells( L, Cx, Cx + N ); break;
        case 'S': if ( !Priv ) { bool A = Alt; Alt = true; ScrollUp( Top, Bottom, N ); Alt = A; } break;
        case 'T': if ( !Priv ) ScrollDown( Top, Bottom, N ); break;
        case 'm': if ( !Priv ) Sgr( P ); break;
        case 'r':
        {
            int T = Arg( 0, 1 ) - 1, B = Arg( 1, H ) - 1;
            if ( T < B && B < H ) { Top = T; Bottom = B; }
            else { Top = 0; Bottom = H - 1; }
            Cx = 0;
            Cy = Origin ? Top : 0;
            break;
        }
        case 's': if ( !Priv ) { SavedX = Cx; SavedY = Cy; SavedPen = Pen; } break;
        case 'u': if ( !Priv ) { Cx = SavedX; Cy = SavedY; Pen = SavedPen; } break;
        case 'h':
        case 'l':
        {
            bool On = F == 'h';
            if ( Priv != '?' ) break; // ANSI modes (insert, newline): not supported
            for ( int M : P )
            {
                switch ( M )
                {
                case 1: AppCursor = On; break;
                case 6: Origin = On; Cx = 0; Cy = On ? Top : 0; break;
                case 7: AutoWrap = On; break;
                case 25: CursorVisible = On; break;
                case 2004: Bracketed = On; break;
                case 47: case 1047: case 1049:
                    if ( On == Alt ) break;
                    if ( M == 1049 && On ) { SavedX = Cx; SavedY = Cy; SavedPen = Pen; }
                    std::swap( Screen, AltBuf );
                    Alt = On;
                    if ( On ) for ( auto& Ln : Screen ) EraseCells( Ln, 0, W );
                    if ( M == 1049 && !On ) { Cx = SavedX; Cy = SavedY; Pen = SavedPen; }
                    Top = 0;
                    Bottom = H - 1;
                    View = 0;
                    break;
                default: break; // mouse reporting (1000/1002/1006), focus events...: not supported
                }
            }
            break;
        }
        case 'n':
            if ( P[ 0 ] == 5 ) Reply( "\x1b[0n" );
            else if ( P[ 0 ] == 6 )
            {
                char Buf[ 32 ];
                std::snprintf( Buf, sizeof( Buf ), "\x1b[%d;%dR", Cy + 1 - ( Origin ? Top : 0 ), Cx + 1 );
                Reply( Buf );
            }
            break;
        case 'c':
            if ( Priv == '>' ) Reply( "\x1b[>0;10;1c" );
            else if ( !Priv ) Reply( "\x1b[?1;2c" ); // "VT100 with advanced video"
            break;
        case 'g':
            if ( P[ 0 ] <= 0 ) { if ( Cx < W ) Tabs[ size_t( Cx ) ] = false; }
            else if ( P[ 0 ] == 3 ) std::fill( Tabs.begin(), Tabs.end(), false );
            break;
        default: break;
        }
    }

    void Terminal::Sgr( const std::vector<int>& P0 )
    {
        // re-split so both "38;2;r;g;b" and "38:2::r:g:b" work: the raw Seq tells us where colons were
        std::vector<std::vector<int>> Groups;
        {
            std::vector<int> G;
            int V = -1;
            for ( char Ch : Seq )
            {
                if ( Ch >= '0' && Ch <= '9' ) V = ( V < 0 ? 0 : V ) * 10 + ( Ch - '0' ), V = std::min( V, 99999 );
                else if ( Ch == ':' ) { G.push_back( V ); V = -1; }
                else if ( Ch == ';' ) { G.push_back( V ); Groups.push_back( G ); G.clear(); V = -1; }
            }
            G.push_back( V );
            Groups.push_back( G );
        }
        (void)P0;
        auto Color256 = [ & ]( size_t& i, const std::vector<int>& G, bool Fg ) {
            // extended color: either the rest of this colon group, or the following ';' params
            std::vector<int> A;
            if ( G.size() > 1 ) A.assign( G.begin() + 1, G.end() );
            else
                for ( size_t k = i + 1; k < Groups.size() && A.size() < 4; k++ ) A.push_back( Groups[ k ][ 0 ] );
            uint32_t C = 0;
            size_t Used = 0;
            if ( !A.empty() && A[ 0 ] == 5 && A.size() >= 2 ) { C = ColIndex | uint32_t( std::clamp( A[ 1 ], 0, 255 ) ); Used = 2; }
            else if ( !A.empty() && A[ 0 ] == 2 )
            {
                size_t O = ( G.size() > 1 && A.size() >= 5 ) ? 2 : 1; // colon form may carry a color-space id
                if ( A.size() >= O + 3 )
                {
                    auto Ch = [ & ]( size_t k ) { return uint32_t( std::clamp( A[ k ] < 0 ? 0 : A[ k ], 0, 255 ) ); };
                    C = ColRgb | Ch( O ) << 16 | Ch( O + 1 ) << 8 | Ch( O + 2 );
                    Used = O + 3;
                }
            }
            if ( G.size() <= 1 ) i += Used;
            if ( C ) ( Fg ? Pen.Fg : Pen.Bg ) = C;
        };
        for ( size_t i = 0; i < Groups.size(); i++ )
        {
            const auto& G = Groups[ i ];
            int V = G[ 0 ] < 0 ? 0 : G[ 0 ];
            if ( V == 0 ) { Pen.Fg = Pen.Bg = 0; Pen.Attr = 0; }
            else if ( V == 1 ) Pen.Attr |= AttrBold;
            else if ( V == 2 ) Pen.Attr |= AttrDim;
            else if ( V == 3 ) Pen.Attr |= AttrItalic;
            else if ( V == 4 ) { if ( G.size() > 1 && G[ 1 ] == 0 ) Pen.Attr &= ~AttrUnderline; else Pen.Attr |= AttrUnderline; }
            else if ( V == 7 ) Pen.Attr |= AttrInverse;
            else if ( V == 9 ) Pen.Attr |= AttrStrike;
            else if ( V == 21 ) Pen.Attr |= AttrUnderline;
            else if ( V == 22 ) Pen.Attr &= ~( AttrBold | AttrDim );
            else if ( V == 23 ) Pen.Attr &= ~AttrItalic;
            else if ( V == 24 ) Pen.Attr &= ~AttrUnderline;
            else if ( V == 27 ) Pen.Attr &= ~AttrInverse;
            else if ( V == 29 ) Pen.Attr &= ~AttrStrike;
            else if ( V >= 30 && V <= 37 ) Pen.Fg = ColIndex | uint32_t( V - 30 );
            else if ( V == 38 ) Color256( i, G, true );
            else if ( V == 39 ) Pen.Fg = 0;
            else if ( V >= 40 && V <= 47 ) Pen.Bg = ColIndex | uint32_t( V - 40 );
            else if ( V == 48 ) Color256( i, G, false );
            else if ( V == 49 ) Pen.Bg = 0;
            else if ( V >= 90 && V <= 97 ) Pen.Fg = ColIndex | uint32_t( V - 90 + 8 );
            else if ( V >= 100 && V <= 107 ) Pen.Bg = ColIndex | uint32_t( V - 100 + 8 );
        }
    }

    void Terminal::OscDispatch()
    {
        size_t Semi = Seq.find( ';' );
        if ( Semi == std::string::npos ) return;
        int Code = std::atoi( Seq.substr( 0, Semi ).c_str() );
        if ( Code == 0 || Code == 2 )
        {
            Title = Seq.substr( Semi + 1 );
            if ( OnTitle ) OnTitle( Title );
        }
        Seq.clear();
    }

    // ------------------------------------------------------------ input side

    void Terminal::Send( std::string_view B )
    {
        if ( Proc && Proc->Running() ) Proc->Write( B );
        else if ( OnInput ) OnInput( B );
    }

    void Terminal::Paste( std::string T )
    {
        std::string S;
        for ( size_t i = 0; i < T.size(); i++ )
        {
            if ( T[ i ] == '\r' && i + 1 < T.size() && T[ i + 1 ] == '\n' ) continue;
            S += T[ i ] == '\n' ? '\r' : T[ i ];
        }
        if ( Bracketed ) S = "\x1b[200~" + S + "\x1b[201~"; // the shell won't run pasted lines one by one
        View = 0;
        Send( S );
    }

    void Terminal::ScrollView( int Lines )
    {
        View = std::clamp( View + Lines, 0, int( Scrollback.size() ) );
        Redraw();
    }

    bool Terminal::Spawn( const std::string& Command, const std::string& Cwd )
    {
        Context* C = GetContext();
        if ( !C || !Pty::Supported() ) return false;
        Kill();
        Proc = std::make_unique<Pty>();
        std::weak_ptr<int> Weak = Alive;
        PtyOptions O;
        O.Command = Command;
        O.Cwd = Cwd;
        O.Cols = W;
        O.Rows = H;
        bool Ok = Proc->Start( O,
            [ C, Weak, this ]( std::string Out ) { C->Post( [ Weak, this, Out = std::move( Out ) ] { if ( Weak.lock() ) Write( Out ); } ); },
            [ C, Weak, this ]( int Code ) { C->Post( [ Weak, this, Code ] { if ( Weak.lock() && OnExit ) OnExit( Code ); } ); } );
        if ( !Ok ) Proc.reset();
        return Ok;
    }

    bool Terminal::IsRunning() const { return Proc && Proc->Running(); }

    void Terminal::Kill()
    {
        if ( Proc ) { Proc->Kill(); Proc.reset(); }
    }

    // ------------------------------------------------------------ selection

    const Terminal::Line& Terminal::AbsLine( long N ) const
    {
        static const Line Empty;
        if ( N < 0 ) return Empty;
        if ( N < long( Scrollback.size() ) ) return Scrollback[ size_t( N ) ];
        N -= long( Scrollback.size() );
        return N < H ? Screen[ size_t( N ) ] : Empty;
    }

    void Terminal::CellAt( Vec2 P, long& Ln, int& Col ) const
    {
        Rect In = Inner();
        Col = std::clamp( int( std::floor( ( P.x - In.x ) / CellW + 0.5f ) ), 0, W ); // between cells: caret-like
        int Row = std::clamp( int( std::floor( ( P.y - In.y ) / CellH ) ), 0, H - 1 );
        Ln = FirstVisible() + Row;
    }

    bool Terminal::InSel( long Ln, int Col ) const
    {
        if ( !HasSel ) return false;
        long AL = SelA_Line, BL = SelB_Line;
        int AC = SelA_Col, BC = SelB_Col;
        if ( BL < AL || ( BL == AL && BC < AC ) ) { std::swap( AL, BL ); std::swap( AC, BC ); }
        if ( Ln < AL || Ln > BL ) return false;
        if ( Ln == AL && Col < AC ) return false;
        if ( Ln == BL && Col >= BC ) return false;
        return true;
    }

    std::string Terminal::SelectedText() const
    {
        std::string Out;
        if ( !HasSel ) return Out;
        long AL = SelA_Line, BL = SelB_Line;
        int AC = SelA_Col, BC = SelB_Col;
        if ( BL < AL || ( BL == AL && BC < AC ) ) { std::swap( AL, BL ); std::swap( AC, BC ); }
        for ( long L = AL; L <= BL; L++ )
        {
            const Line& Ln = AbsLine( L );
            std::string Row;
            int From = L == AL ? AC : 0, To = L == BL ? BC : int( Ln.size() );
            for ( int c = From; c < To && c < int( Ln.size() ); c++ )
                if ( Ln[ size_t( c ) ].Width ) Row += CellText( Ln[ size_t( c ) ] );
            while ( !Row.empty() && Row.back() == ' ' ) Row.pop_back();
            Out += Row;
            if ( L != BL ) Out += '\n';
        }
        return Out;
    }

    // ------------------------------------------------------------ keys -> bytes

    bool Terminal::OnEvent( const Event& E )
    {
        Context* C = GetContext();
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            CellAt( E.Pos, SelA_Line, SelA_Col );
            SelB_Line = SelA_Line;
            SelB_Col = SelA_Col;
            Selecting = true;
            HasSel = false;
            Redraw();
            return true;
        case EventType::MouseMove:
            if ( !Selecting || !IsPressed() ) return false;
            CellAt( E.Pos, SelB_Line, SelB_Col );
            HasSel = SelA_Line != SelB_Line || SelA_Col != SelB_Col;
            Redraw();
            return true;
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left ) return false;
            Selecting = false;
            return true;
        case EventType::MouseWheel:
            if ( Alt ) // full-screen apps (less, man, vim): the wheel scrolls them, like other terminals
            {
                std::string K;
                for ( int i = 0; i < 3; i++ ) K += E.Wheel.y > 0 ? ( AppCursor ? "\x1bOA" : "\x1b[A" ) : ( AppCursor ? "\x1bOB" : "\x1b[B" );
                Send( K );
                return true;
            }
            ScrollView( int( E.Wheel.y * 3 ) );
            return true;
        case EventType::Text:
        {
            std::string S;
            AppendUtf8( S, E.Codepoint );
            View = 0;
            HasSel = false;
            Send( S );
            return true;
        }
        case EventType::KeyDown:
            break;
        default:
            return false;
        }

        const bool Shift = E.Mods & ModShift, Ctrl = E.Mods & ModCtrl, AltK = E.Mods & ModAlt;
        // terminal-standard clipboard keys
        if ( Ctrl && Shift && E.KeyCode == Key::C ) { if ( C && C->SetClipboard && HasSel ) C->SetClipboard( SelectedText() ); return true; }
        if ( Ctrl && Shift && E.KeyCode == Key::V ) { if ( C && C->GetClipboard ) Paste( C->GetClipboard() ); return true; }
        if ( Ctrl && !Shift && E.KeyCode == Key::C && HasSel ) // like Windows Terminal: copy if there's a selection, else ^C
        {
            if ( C && C->SetClipboard ) C->SetClipboard( SelectedText() );
            HasSel = false;
            Redraw();
            return true;
        }
        if ( Shift && E.KeyCode == Key::PageUp ) { ScrollView( H - 1 ); return true; }
        if ( Shift && E.KeyCode == Key::PageDown ) { ScrollView( -( H - 1 ) ); return true; }

        int Mod = 1 + ( Shift ? 1 : 0 ) + ( AltK ? 2 : 0 ) + ( Ctrl ? 4 : 0 );
        auto Csi = [ & ]( char Final ) -> std::string {
            if ( Mod > 1 ) { char B[ 32 ]; std::snprintf( B, sizeof( B ), "\x1b[1;%d%c", Mod, Final ); return B; }
            return AppCursor ? std::string( "\x1bO" ) + Final : std::string( "\x1b[" ) + Final;
        };
        auto Tilde = [ & ]( int N ) {
            char B[ 32 ];
            if ( Mod > 1 ) std::snprintf( B, sizeof( B ), "\x1b[%d;%d~", N, Mod );
            else std::snprintf( B, sizeof( B ), "\x1b[%d~", N );
            return std::string( B );
        };
        std::string Out;
        switch ( E.KeyCode )
        {
        case Key::Enter: case Key::KpEnter: Out = "\r"; break;
        case Key::Backspace: Out = Ctrl ? "\x08" : "\x7f"; break;
        case Key::Tab: Out = Shift ? "\x1b[Z" : "\t"; break;
        case Key::Escape: Out = "\x1b"; break;
        case Key::Up: Out = Csi( 'A' ); break;
        case Key::Down: Out = Csi( 'B' ); break;
        case Key::Right: Out = Csi( 'C' ); break;
        case Key::Left: Out = Csi( 'D' ); break;
        case Key::Home: Out = Csi( 'H' ); break;
        case Key::End: Out = Csi( 'F' ); break;
        case Key::Insert: Out = Tilde( 2 ); break;
        case Key::Delete: Out = Tilde( 3 ); break;
        case Key::PageUp: Out = Tilde( 5 ); break;
        case Key::PageDown: Out = Tilde( 6 ); break;
        case Key::F1: Out = "\x1bOP"; break;
        case Key::F2: Out = "\x1bOQ"; break;
        case Key::F3: Out = "\x1bOR"; break;
        case Key::F4: Out = "\x1bOS"; break;
        case Key::F5: Out = Tilde( 15 ); break;
        case Key::F6: Out = Tilde( 17 ); break;
        case Key::F7: Out = Tilde( 18 ); break;
        case Key::F8: Out = Tilde( 19 ); break;
        case Key::F9: Out = Tilde( 20 ); break;
        case Key::F10: Out = Tilde( 21 ); break;
        case Key::F11: Out = Tilde( 23 ); break;
        case Key::F12: Out = Tilde( 24 ); break;
        default:
            if ( E.KeyCode >= Key::A && E.KeyCode <= Key::Z && ( Ctrl || AltK ) )
            {
                char L = char( 'a' + ( uint16_t( E.KeyCode ) - uint16_t( Key::A ) ) );
                if ( Ctrl ) Out = std::string( 1, char( L - 'a' + 1 ) );           // ^A .. ^Z
                else Out = std::string( "\x1b" ) + char( Shift ? L - 32 : L );     // Alt+x = ESC x
            }
            break;
        }
        if ( Out.empty() ) return false;
        View = 0;
        HasSel = false;
        Send( Out );
        return true;
    }

    // ------------------------------------------------------------ drawing

    Color Terminal::Resolve( uint32_t C, bool Fg, uint8_t Attr ) const
    {
        if ( C == 0 ) return Fg ? Style.Foreground : Style.Background;
        if ( C & ColRgb ) return Color( uint8_t( C >> 16 ), uint8_t( C >> 8 ), uint8_t( C ) );
        int I = int( C & 0xFF );
        if ( I < 16 )
        {
            if ( Fg && Style.BoldIsBright && ( Attr & AttrBold ) && I < 8 ) I += 8;
            return Style.Palette[ I ];
        }
        if ( I < 232 )
        {
            static const uint8_t L[] = { 0, 95, 135, 175, 215, 255 };
            I -= 16;
            return Color( L[ I / 36 ], L[ ( I / 6 ) % 6 ], L[ I % 6 ] );
        }
        uint8_t G = uint8_t( 8 + ( I - 232 ) * 10 );
        return Color( G, G, G );
    }

    // Box drawing, block elements and braille as geometry: lines meet exactly between cells.
    bool Terminal::DrawSpecial( DrawList& D, uint32_t Cp, Rect R, Color C ) const
    {
        const float S = D.Scale;
        auto Snap = [ S ]( float V ) { return std::round( V * S ) / S; };
        if ( Cp >= 0x2580 && Cp <= 0x259F ) // blocks
        {
            float X = R.x, Y = R.y, Wd = R.w, Ht = R.h;
            auto Fill = [ & ]( float X0, float Y0, float X1, float Y1, float A = 1 ) {
                D.RectFilled( { Snap( X + Wd * X0 ), Snap( Y + Ht * Y0 ), Snap( X + Wd * X1 ) - Snap( X + Wd * X0 ), Snap( Y + Ht * Y1 ) - Snap( Y + Ht * Y0 ) }, C.Fade( A ) );
            };
            if ( Cp == 0x2580 ) Fill( 0, 0, 1, 0.5f );
            else if ( Cp >= 0x2581 && Cp <= 0x2588 ) Fill( 0, 1 - ( Cp - 0x2580 ) / 8.f, 1, 1 );
            else if ( Cp >= 0x2589 && Cp <= 0x258F ) Fill( 0, 0, ( 0x2590 - Cp ) / 8.f, 1 );
            else if ( Cp == 0x2590 ) Fill( 0.5f, 0, 1, 1 );
            else if ( Cp >= 0x2591 && Cp <= 0x2593 ) Fill( 0, 0, 1, 1, ( Cp - 0x2590 ) * 0.25f );
            else if ( Cp == 0x2594 ) Fill( 0, 0, 1, 0.125f );
            else if ( Cp == 0x2595 ) Fill( 0.875f, 0, 1, 1 );
            else
            {
                // quadrants: bit 1 UL, 2 UR, 4 LL, 8 LR
                static const uint8_t Q[] = { 4, 8, 1, 1 | 4 | 8, 1 | 8, 1 | 2 | 4, 1 | 2 | 8, 2, 2 | 4, 2 | 4 | 8 };
                uint8_t M = Q[ Cp - 0x2596 ];
                if ( M & 1 ) Fill( 0, 0, 0.5f, 0.5f );
                if ( M & 2 ) Fill( 0.5f, 0, 1, 0.5f );
                if ( M & 4 ) Fill( 0, 0.5f, 0.5f, 1 );
                if ( M & 8 ) Fill( 0.5f, 0.5f, 1, 1 );
            }
            return true;
        }
        if ( Cp >= 0x2800 && Cp <= 0x28FF ) // braille: 2x4 dots
        {
            static const int Bx[] = { 0, 0, 0, 1, 1, 1, 0, 1 }, By[] = { 0, 1, 2, 0, 1, 2, 3, 3 };
            float Dw = std::max( 1.f / S, Snap( R.w * 0.22f ) );
            for ( int b = 0; b < 8; b++ )
                if ( ( Cp - 0x2800 ) & ( 1u << b ) )
                {
                    float Cx_ = R.x + R.w * ( 0.28f + 0.44f * Bx[ b ] ), Cy_ = R.y + R.h * ( 0.14f + 0.24f * By[ b ] );
                    D.RectFilled( { Snap( Cx_ - Dw * 0.5f ), Snap( Cy_ - Dw * 0.5f ), Dw, Dw }, C );
                }
            return true;
        }
        if ( Cp < 0x2500 || Cp > 0x257F ) return false;

        // box drawing: weight per arm (up, right, down, left): 1 light, 2 heavy, 3 double
        struct Arms { uint8_t U, R, D, L; };
        Arms A{ 0, 0, 0, 0 };
        auto Set = [ & ]( uint8_t U, uint8_t Rr, uint8_t Dd, uint8_t L ) { A = { U, Rr, Dd, L }; };
        switch ( Cp )
        {
        case 0x2500: case 0x2504: case 0x2508: case 0x254C: Set( 0, 1, 0, 1 ); break;
        case 0x2501: case 0x2505: case 0x2509: case 0x254D: Set( 0, 2, 0, 2 ); break;
        case 0x2502: case 0x2506: case 0x250A: case 0x254E: Set( 1, 0, 1, 0 ); break;
        case 0x2503: case 0x2507: case 0x250B: case 0x254F: Set( 2, 0, 2, 0 ); break;
        case 0x250C: Set( 0, 1, 1, 0 ); break;  case 0x250F: Set( 0, 2, 2, 0 ); break;
        case 0x2510: Set( 0, 0, 1, 1 ); break;  case 0x2513: Set( 0, 0, 2, 2 ); break;
        case 0x2514: Set( 1, 1, 0, 0 ); break;  case 0x2517: Set( 2, 2, 0, 0 ); break;
        case 0x2518: Set( 1, 0, 0, 1 ); break;  case 0x251B: Set( 2, 0, 0, 2 ); break;
        case 0x251C: Set( 1, 1, 1, 0 ); break;  case 0x2523: Set( 2, 2, 2, 0 ); break;
        case 0x2524: Set( 1, 0, 1, 1 ); break;  case 0x252B: Set( 2, 0, 2, 2 ); break;
        case 0x252C: Set( 0, 1, 1, 1 ); break;  case 0x2533: Set( 0, 2, 2, 2 ); break;
        case 0x2534: Set( 1, 1, 0, 1 ); break;  case 0x253B: Set( 2, 2, 0, 2 ); break;
        case 0x253C: Set( 1, 1, 1, 1 ); break;  case 0x254B: Set( 2, 2, 2, 2 ); break;
        case 0x2550: Set( 0, 3, 0, 3 ); break;  case 0x2551: Set( 3, 0, 3, 0 ); break;
        case 0x2554: Set( 0, 3, 3, 0 ); break;  case 0x2557: Set( 0, 0, 3, 3 ); break;
        case 0x255A: Set( 3, 3, 0, 0 ); break;  case 0x255D: Set( 3, 0, 0, 3 ); break;
        case 0x2560: Set( 3, 3, 3, 0 ); break;  case 0x2563: Set( 3, 0, 3, 3 ); break;
        case 0x2566: Set( 0, 3, 3, 3 ); break;  case 0x2569: Set( 3, 3, 0, 3 ); break;
        case 0x256C: Set( 3, 3, 3, 3 ); break;
        case 0x2574: Set( 0, 0, 0, 1 ); break;  case 0x2575: Set( 1, 0, 0, 0 ); break;
        case 0x2576: Set( 0, 1, 0, 0 ); break;  case 0x2577: Set( 0, 0, 1, 0 ); break;
        case 0x2578: Set( 0, 0, 0, 2 ); break;  case 0x2579: Set( 2, 0, 0, 0 ); break;
        case 0x257A: Set( 0, 2, 0, 0 ); break;  case 0x257B: Set( 0, 0, 2, 0 ); break;
        case 0x256D: case 0x256E: case 0x256F: case 0x2570:
        {
            // rounded corners: a quarter arc between the two edge midpoints
            float Lw = std::max( 1.f / S, Snap( R.w * 0.11f ) );
            Vec2 M = { Snap( R.x + R.w * 0.5f - Lw * 0.5f ) + Lw * 0.5f, Snap( R.y + R.h * 0.5f - Lw * 0.5f ) + Lw * 0.5f };
            float Rad = std::min( R.w, R.h ) * 0.5f;
            bool Right = Cp == 0x256D || Cp == 0x2570, Down = Cp == 0x256D || Cp == 0x256E;
            Vec2 Ctr = { M.x + ( Right ? Rad : -Rad ), M.y + ( Down ? Rad : -Rad ) };
            Vec2 Pts[ 10 ];
            float A0 = Right ? ( Down ? 3.14159f : 1.5708f ) : ( Down ? 4.71239f : 0.f );
            for ( int i = 0; i < 10; i++ ) { float T = A0 + 1.5708f * i / 9; Pts[ i ] = { Ctr.x + std::cos( T ) * Rad, Ctr.y + std::sin( T ) * Rad }; }
            D.Polyline( Pts, 10, C, Lw );
            // extend straight to the cell edges so neighbours connect
            float Ex = Right ? R.Right() : R.x, Ey = Down ? R.Bottom() : R.y;
            D.RectFilled( { std::min( Ctr.x, Ex ), Snap( M.y - Lw * 0.5f ), std::fabs( Ex - Ctr.x ), Lw }, C );
            D.RectFilled( { Snap( M.x - Lw * 0.5f ), std::min( Ctr.y, Ey ), Lw, std::fabs( Ey - Ctr.y ) }, C );
            return true;
        }
        default: return false; // anything else: the font's glyph
        }
        float Light = std::max( 1.f / S, Snap( R.w * 0.11f ) ), Heavy = Light * 2;
        Vec2 M = { Snap( R.x + R.w * 0.5f ), Snap( R.y + R.h * 0.5f ) };
        const float Dd = Light; // half the gap between the two strokes of a double line
        auto HBar = [ & ]( float X0, float X1, float Yc, float T ) { D.RectFilled( { std::min( X0, X1 ), Snap( Yc - T * 0.5f ), std::fabs( X1 - X0 ), T }, C ); };
        auto VBar = [ & ]( float Y0, float Y1, float Xc, float T ) { D.RectFilled( { Snap( Xc - T * 0.5f ), std::min( Y0, Y1 ), T, std::fabs( Y1 - Y0 ) }, C ); };
        bool AnyV = A.U || A.D, AnyH = A.L || A.R;
        float Hw = std::max( A.U, A.D ) == 2 ? Heavy : std::max( A.U, A.D ) == 3 ? Dd + Light : Light; // how far horizontals reach into the joint
        float Vw = std::max( A.L, A.R ) == 2 ? Heavy : std::max( A.L, A.R ) == 3 ? Dd + Light : Light;
        // single / heavy arms: from the edge to just past the centre
        if ( A.L && A.L != 3 ) HBar( R.x, M.x + Hw * 0.5f, M.y, A.L == 2 ? Heavy : Light );
        if ( A.R && A.R != 3 ) HBar( M.x - Hw * 0.5f, R.Right(), M.y, A.R == 2 ? Heavy : Light );
        if ( A.U && A.U != 3 ) VBar( R.y, M.y + Vw * 0.5f, M.x, A.U == 2 ? Heavy : Light );
        if ( A.D && A.D != 3 ) VBar( M.y - Vw * 0.5f, R.Bottom(), M.x, A.D == 2 ? Heavy : Light );
        // double arms: each stroke stops at the near line if a perpendicular arm is on its side,
        // otherwise runs to the far line, so corners and tees nest properly (╔ ╦ ╬)
        const float H2 = Light * 0.5f;
        for ( int Side = -1; Side <= 1; Side += 2 )
        {
            bool VSide = Side < 0 ? A.U : A.D, HSide = Side < 0 ? A.L : A.R;
            float Yb = M.y + Side * Dd, Xb = M.x + Side * Dd;
            if ( A.L == 3 ) HBar( R.x, VSide ? M.x - Dd + H2 : AnyV ? M.x + Dd + H2 : M.x + H2, Yb, Light );
            if ( A.R == 3 ) HBar( VSide ? M.x + Dd - H2 : AnyV ? M.x - Dd - H2 : M.x - H2, R.Right(), Yb, Light );
            if ( A.U == 3 ) VBar( R.y, HSide ? M.y - Dd + H2 : AnyH ? M.y + Dd + H2 : M.y + H2, Xb, Light );
            if ( A.D == 3 ) VBar( HSide ? M.y + Dd - H2 : AnyH ? M.y - Dd - H2 : M.y - H2, R.Bottom(), Xb, Light );
        }
        return true;
    }

    void Terminal::Draw( DrawList& D )
    {
        const TerminalStyle& S = Style;
        const Font& Reg = S.TextFont ? *S.TextFont : *GetTheme().Mono;
        const Font& Bold = S.BoldFont ? *S.BoldFont : ( GetTheme().MonoBold ? *GetTheme().MonoBold : Reg );
        D.RectFilled( Bounds, S.Background, S.Radius );
        Rect In = Inner();
        D.PushClip( Bounds.Shrink( 1 ) );
        const long First = FirstVisible();
        const bool Focused = IsFocused();

        for ( int r = 0; r < H; r++ )
        {
            const Line& L = AbsLine( First + r );
            float Y = In.y + r * CellH;
            int N = std::min( W, int( L.size() ) );
            // backgrounds (merged runs) + selection
            for ( int x = 0; x < N; )
            {
                const TermCell& Ce = L[ size_t( x ) ];
                bool Inv = Ce.Attr & AttrInverse;
                uint32_t BgC = Inv ? ( Ce.Fg ? Ce.Fg : 0xFFFFFFFF ) : Ce.Bg;
                int x2 = x + 1;
                while ( x2 < N )
                {
                    const TermCell& Nx = L[ size_t( x2 ) ];
                    uint32_t Nb = ( Nx.Attr & AttrInverse ) ? ( Nx.Fg ? Nx.Fg : 0xFFFFFFFF ) : Nx.Bg;
                    if ( Nb != BgC ) break;
                    x2++;
                }
                if ( BgC )
                {
                    Color Bc = BgC == 0xFFFFFFFF ? S.Foreground : Resolve( BgC, false, Ce.Attr );
                    D.RectFilled( { In.x + x * CellW, Y, ( x2 - x ) * CellW, CellH }, Bc );
                }
                x = x2;
            }
            // glyphs
            for ( int x = 0; x < N; x++ )
            {
                const TermCell& Ce = L[ size_t( x ) ];
                if ( !Ce.Width ) continue;
                bool Inv = Ce.Attr & AttrInverse;
                Color Fg = Inv ? ( Ce.Bg ? Resolve( Ce.Bg, false, Ce.Attr ) : S.Background ) : Resolve( Ce.Fg, true, Ce.Attr );
                if ( Ce.Attr & AttrDim ) Fg = Color::Lerp( Fg, S.Background, 0.4f );
                Rect R = { In.x + x * CellW, Y, CellW * Ce.Width, CellH };
                if ( Ce.Cp > ' ' && !DrawSpecial( D, Ce.Cp, R, Fg ) )
                {
                    const Font& F = Ce.Attr & AttrBold ? Bold : Reg;
                    const Glyph& G = CellGlyph( F, Ce.Cp );
                    float Off = Ce.Width == 2 ? ( R.w - G.Advance ) * 0.5f : 0; // centre wide glyphs in their 2 cells
                    D.Glyph( F, G, { R.x + Off, Y }, Fg );
                }
                if ( Ce.Attr & AttrUnderline ) D.RectFilled( { R.x, Y + Reg.Ascent + 2, R.w, 1 }, Fg );
                if ( Ce.Attr & AttrStrike ) D.RectFilled( { R.x, Y + CellH * 0.55f, R.w, 1 }, Fg );
            }
            if ( HasSel )
                for ( int x = 0; x < W; x++ )
                    if ( InSel( First + r, x ) ) D.RectFilled( { In.x + x * CellW, Y, CellW, CellH }, S.Selection );
        }

        // cursor (only when looking at the live screen)
        if ( CursorVisible && View == 0 )
        {
            Rect Cr = { In.x + Cx * CellW, In.y + Cy * CellH, CellW, CellH };
            const TermCell& Ce = Screen[ size_t( Cy ) ][ size_t( Cx ) ];
            if ( Focused )
            {
                D.RectFilled( Cr, S.Cursor );
                if ( Ce.Cp > ' ' && Ce.Width )
                {
                    const Font& F = Ce.Attr & AttrBold ? Bold : Reg;
                    D.Glyph( F, CellGlyph( F, Ce.Cp ), { Cr.x, Cr.y }, S.Background );
                }
            }
            else D.RectStroke( Cr, S.Cursor, 0, 1 );
        }
        D.PopClip();

        // scrollback position
        if ( !Scrollback.empty() && View > 0 )
        {
            float Total = float( Scrollback.size() + size_t( H ) ), Track = Bounds.h - 8;
            float Th = std::max( 16.f, Track * H / Total ), T = 1 - float( View ) / float( Scrollback.size() );
            D.RectFilled( { Bounds.Right() - 6, Bounds.y + 4 + ( Track - Th ) * T, 3, Th }, GetTheme()[ Tok::TextDim ], 1.5f );
        }
        if ( Focused ) D.RectStroke( Bounds, GetTheme()[ Tok::Accent ].Fade( 0.5f ), S.Radius );
    }
}
