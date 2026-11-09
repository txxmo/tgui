// TextArea — multi-line text: word wrap, scrolling, selection, clipboard.
//
//   W.TextArea( "Notes", &Notes );                                  // a notes box
//   auto& Box = W.TextArea( "Message" );                            // chat composer:
//   Box.SubmitOnEnter = true;  Box.MinLines = 1;  Box.MaxLines = 6;  //   Enter sends, Shift+Enter = new line, grows to 6 lines
//   Box.OnSubmit = []( const std::string& T ) { Send( T ); };
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    struct TextAreaStyle
    {
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeFocused = Tok::Accent;
        Paint TextColor = Tok::Text;
        Paint Placeholder = Tok::TextDim;
        Paint Selection = Color( 110, 183, 212, 90 );
        Paint Thumb = Tok::BorderStrong;
        float Radius = 6;
        Insets Padding = { 10, 8 };
    };

    class TextArea : public Widget
    {
    public:
        explicit TextArea( std::string Placeholder = {} );

        std::string Placeholder;
        std::string* Bind = nullptr; // optional: kept in sync with the text
        std::function<void( const std::string& )> OnChange;
        std::function<void( const std::string& )> OnSubmit; // Ctrl+Enter, or Enter with SubmitOnEnter
        bool SubmitOnEnter = false;  // chat style: Enter submits, Shift+Enter inserts a newline
        bool ClearOnSubmit = false;
        bool ReadOnly = false;
        int MinLines = 4;            // natural height (when sized by content)
        int MaxLines = 0;            // grow with the text up to this many lines, then scroll (0 = don't grow past MinLines)
        TextAreaStyle Style;

        const std::string& GetText() const { return Text; }
        void SetText( std::string T );
        size_t LineCount(); // visual (wrapped) lines

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        bool WantsKeyboard() const override { return true; } // letters are text, not hotkeys / binds
        Rect CaretRect() const override { return CaretAt; }
        Cursor GetCursor() const override { return Cursor::IBeam; }

    private:
        struct Line { size_t Start, End; }; // byte range of a visual line (End excludes '\n' / the wrap space)
        std::string Text;
        std::vector<Line> Lines;
        float WrapW = 0;
        size_t Caret = 0, Anchor = 0;
        float WantX = -1;     // remembered column for Up/Down
        float ScrollY = 0;
        double BlinkStart = 0;
        bool Dragging = false;

        void Relayout();      // rebuild Lines for WrapW
        size_t LineOf( size_t Byte ) const;
        Vec2 PosOf( size_t Byte ) const; // caret position relative to the text origin
        size_t ByteAt( Vec2 Local ) const;
        Rect TextRect() const;
        void Insert( const std::string& S );
        void EraseSelection();
        void Changed();
        void KeepCaretVisible();
        Rect CaretAt;
    };
}
