// Terminal — a VT/xterm terminal emulator widget.
//
//   auto& T = W.Terminal();
//   T.Spawn();                          // the user's shell in a pseudo-terminal (bash/zsh, cmd.exe on Windows)
//   T.Spawn( "ssh root@10.0.4.21" );    // or any command
//
//   // ...or drive it yourself (an SSH library, a websocket, logs):
//   T.Write( "\x1b[32mready\x1b[0m\r\n" );          // output to display
//   T.OnInput = []( std::string_view Keys ) { ... }; // what the user typed, as terminal byte sequences
//
// Supports 16/256/truecolor, bold/dim/italic/underline/inverse/strike, cursor movement & erase, scroll
// regions, insert/delete lines/chars, alternate screen (vim, htop, less), autowrap, tabs, DEC line drawing,
// wide characters (CJK, emoji), bracketed paste, cursor-position / device-attribute reports, window title.
// Box drawing, block elements and braille are drawn as shapes so they join seamlessly between cells.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tg
{
    class Font;
    class Pty;

    struct TermCell
    {
        uint32_t Cp = ' ';
        uint32_t Fg = 0, Bg = 0; // packed: 0 = default; 0x1000000 | index (0..255); 0x2000000 | 0xRRGGBB
        uint8_t Attr = 0;        // TermAttr bits
        uint8_t Width = 1;       // 2 = wide char, 0 = the right half of a wide char
    };

    enum TermAttr : uint8_t { AttrBold = 1, AttrDim = 2, AttrItalic = 4, AttrUnderline = 8, AttrInverse = 16, AttrStrike = 32 };

    struct TerminalStyle
    {
        Font* TextFont = nullptr;     // nullptr = Theme().Mono
        Font* BoldFont = nullptr;     // nullptr = Theme().MonoBold
        Color Background = Color::Hex( 0x0B0D11 );
        Color Foreground = Color::Hex( 0xD7DAE0 );
        Color Cursor = Color::Hex( 0x6EB7D4 );
        Color Selection = Color( 110, 183, 212, 90 );
        Color Palette[ 16 ] = {
            Color::Hex( 0x1D2027 ), Color::Hex( 0xE5534B ), Color::Hex( 0x3FB97A ), Color::Hex( 0xE3A33B ),
            Color::Hex( 0x4F8FE0 ), Color::Hex( 0xB277D6 ), Color::Hex( 0x4DB6C8 ), Color::Hex( 0xC7CBD3 ),
            Color::Hex( 0x5C6370 ), Color::Hex( 0xFF6B63 ), Color::Hex( 0x5AD490 ), Color::Hex( 0xF5C25B ),
            Color::Hex( 0x6EA8F5 ), Color::Hex( 0xCB92EC ), Color::Hex( 0x6ED6E6 ), Color::Hex( 0xFFFFFF ) };
        Insets Padding = { 8, 6 };
        float Radius = 8;
        bool BoldIsBright = false;    // old-school: bold also brightens colors 0-7
    };

    class Terminal : public Widget
    {
    public:
        Terminal();
        ~Terminal() override;

        TerminalStyle Style;
        size_t ScrollbackLimit = 5000;

        // ---- data in / out
        void Write( std::string_view Bytes );                        // UI thread (Post from other threads)
        std::function<void( std::string_view )> OnInput;             // keys / paste / replies, as bytes
        std::function<void( int Cols, int Rows )> OnResize;
        std::function<void( const std::string& )> OnTitle;
        std::function<void()> OnBell;

        // ---- run a process (pseudo-terminal)
        bool Spawn( const std::string& Command = {}, const std::string& Cwd = {} );
        bool IsRunning() const;
        void Kill();
        std::function<void( int ExitCode )> OnExit;

        // ---- state
        int Cols() const { return W; }
        int Rows() const { return H; }
        int CursorCol() const { return Cx; }
        int CursorRow() const { return Cy; }
        const TermCell& Cell( int Col, int Row ) const;  // on the visible screen (not scrollback)
        std::string RowText( int Row ) const;             // trailing spaces trimmed
        std::string Title;
        bool AltScreen() const { return Alt; }
        size_t ScrollbackLines() const { return Scrollback.size(); }
        void Clear();                                     // screen + scrollback
        void Reset();
        std::string SelectedText() const;
        void Paste( std::string Text );
        void ScrollView( int Lines );                     // + = back into history
        int ViewOffset() const { return View; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::IBeam; }

    private:
        using Line = std::vector<TermCell>;
        int W = 80, H = 24;
        std::vector<Line> Screen, AltBuf;
        std::deque<Line> Scrollback;
        int Cx = 0, Cy = 0, SavedX = 0, SavedY = 0;
        TermCell Pen, SavedPen;
        bool WrapPending = false, Alt = false, AppCursor = false, CursorVisible = true, AutoWrap = true, Bracketed = false, Origin = false;
        int Top = 0, Bottom = 23; // scroll region
        bool G0Lines = false, G1Lines = false, UseG1 = false, SavedG0 = false;
        std::vector<bool> Tabs;
        int View = 0; // lines scrolled back

        // parser
        enum class St : uint8_t { Ground, Esc, Csi, Osc, OscEsc, Charset, Ignore } State = St::Ground;
        std::string Seq;      // CSI params / OSC text
        char CharsetSlot = 0;
        uint32_t Utf8Cp = 0;
        int Utf8Need = 0;

        // selection, in absolute line numbers (0 = oldest scrollback line)
        bool Selecting = false, HasSel = false;
        long SelA_Line = 0, SelB_Line = 0;
        int SelA_Col = 0, SelB_Col = 0;

        std::unique_ptr<Pty> Proc;
        std::shared_ptr<int> Alive = std::make_shared<int>( 0 ); // guards posted callbacks after we're gone
        float CellW = 8, CellH = 16;

        void Put( uint32_t Cp );
        void Control( uint8_t C );
        void EscDispatch( char C );
        void CsiDispatch( char Final );
        void OscDispatch();
        void Sgr( const std::vector<int>& P );
        void LineFeed();
        void ScrollUp( int Top, int Bottom, int N );
        void ScrollDown( int Top, int Bottom, int N );
        void Resize( int Cols, int Rows );
        void EraseCells( Line& L, int From, int To );
        TermCell Blank() const;
        void Reply( std::string_view S );
        Color Resolve( uint32_t C, bool Fg, uint8_t Attr ) const;
        const Line& AbsLine( long N ) const; // scrollback + screen
        long AbsCount() const { return long( Scrollback.size() ) + H; }
        void CellAt( Vec2 P, long& Line, int& Col ) const;
        bool InSel( long Line, int Col ) const;
        bool DrawSpecial( DrawList& D, uint32_t Cp, Rect R, Color C ) const; // box / blocks / braille
        void UpdateMetrics();
        void Send( std::string_view Bytes );
        Rect Inner() const { return Bounds.Shrink( Style.Padding ); }
        long FirstVisible() const { return AbsCount() - H - View; }
    };
}
