// CodeEditor — a monospace text editor: syntax highlighting, line numbers, multiple carets, undo / redo.
//
//   auto& Ed = W.Add<tg::CodeEditor>( Source, tg::CodeLang::Cpp );
//   Ed.OnChange = [&]( const std::string& T ) { Dirty = true; };
//
// Keys: arrows (+Ctrl by word, +Shift select), Home (smart) / End, PgUp / PgDn, Ctrl+Home / End,
// Backspace / Delete (+Ctrl by word), Enter keeps the indent, Tab / Shift+Tab indent the selected lines,
// Ctrl+A / C / X / V, Ctrl+Z / Y, Ctrl+D adds a caret at the next occurrence, Ctrl+/ toggles comments,
// Alt+click adds a caret, Escape goes back to one caret. Double-click: word, triple-click: line.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tg
{
    class Font;

    enum class CodeLang : uint8_t { Plain, Cpp, Json, Python, Lua, Shell };

    enum class CodeToken : uint8_t { Text, Keyword, Type, String, Number, Comment, Function, Preproc, Punct };

    struct CodeSpan { size_t Begin, End; CodeToken Kind; };

    struct CodeEditorStyle
    {
        Font* TextFont = nullptr; // nullptr = Theme().Mono
        Color Background = Color::Hex( 0x0E1116 );
        Color Gutter = Color::Hex( 0x0E1116 );
        Color GutterText = Color::Hex( 0x4B5363 );
        Color GutterCurrent = Color::Hex( 0xA9B1BE );
        Color CurrentLine = Color( 255, 255, 255, 8 );
        Color Selection = Color( 110, 183, 212, 70 );
        Color Caret = Color::Hex( 0x6EB7D4 );
        Color Colors[ 9 ] = { Color::Hex( 0xD7DAE0 ), Color::Hex( 0xC678DD ), Color::Hex( 0xE5C07B ), Color::Hex( 0x98C379 ), Color::Hex( 0xD19A66 ),
                              Color::Hex( 0x5C6370 ), Color::Hex( 0x61AFEF ), Color::Hex( 0xE06C75 ), Color::Hex( 0xABB2BF ) }; // by CodeToken
        float Radius = 8;
    };

    class CodeEditor : public Widget
    {
    public:
        explicit CodeEditor( std::string Text = {}, CodeLang Lang = CodeLang::Cpp );
        CodeEditorStyle Style;
        CodeLang Lang;
        bool ReadOnly = false, LineNumbers = true, InsertSpaces = true;
        int TabSize = 4;
        std::function<void( const std::string& )> OnChange;
        // your own highlighter: fill Out for one line; State carries across lines (block comments...)
        std::function<void( std::string_view Line, int& State, std::vector<CodeSpan>& Out )> Highlighter;

        void SetText( std::string Text );        // also resets undo
        const std::string& GetText() const { return Text; }
        size_t LineCount() const { return Starts.size(); }
        std::string Line( size_t I ) const;
        std::string SelectedText() const;
        size_t CaretCount() const { return Sels.size(); }
        void SetCaret( size_t Line, size_t Column ); // single caret (column in bytes)
        void Insert( const std::string& S );      // at every caret, replacing selections
        bool Undo();
        bool Redo();
        void ScrollToCaret();

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::IBeam; }
        bool WantsKeyboard() const override { return true; }
        Rect CaretRect() const override { return CaretAt; }

    private:
        struct Sel { size_t Caret = 0, Anchor = 0; float WantCol = -1; };
        std::string Text;
        std::vector<size_t> Starts;      // byte offset of each line
        std::vector<int> LineState;      // highlighter state at each line start (-1 = unknown)
        std::vector<Sel> Sels;
        float ScrollX = 0, ScrollY = 0;
        float CW = 8, LH = 16, GutterW = 40;
        bool Dragging = false;
        double BlinkStart = 0;
        struct Snapshot { std::string Text; std::vector<Sel> Sels; };
        std::vector<Snapshot> UndoStack, RedoStack;
        double LastEditTime = -10;
        int LastEditKind = 0;

        const Font& Mono() const;
        void Reindex( size_t FromByte = 0 );
        size_t LineOf( size_t Pos ) const;
        size_t LineEnd( size_t L ) const;
        int ColumnOf( size_t Pos ) const;               // visual column (tabs expanded)
        size_t PosAt( size_t L, float Col ) const;       // byte offset in line L nearest to a visual column
        size_t PosAtPoint( Vec2 P ) const;
        Rect TextRect() const;
        void Normalize();                                // sort + merge overlapping selections
        void Record( int Kind );                         // push an undo snapshot (merges quick runs of the same kind)
        void Edit( const std::function<void( Sel&, std::string& Insert, size_t& EraseFrom, size_t& EraseTo )>& Fn, int Kind );
        void Changed();
        void Move( int Dx, int Dy, bool Word, bool Extend );
        void HighlightLine( size_t L, std::vector<CodeSpan>& Out );
        void ToggleComment();
        void Indent( bool Out );
        void AddNextOccurrence();
        Rect CaretAt;
    };
}
