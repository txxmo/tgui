// Label — static or dynamic text. Wrap = true for chat messages/paragraphs.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <string>

namespace tg
{
    class Font;

    struct LabelStyle
    {
        Paint TextColor = Tok::Text;
        Font* TextFont = nullptr; // nullptr = Theme().Body
        Align HAlign = Align::Start;
        Align VAlign = Align::Center;
        Paint SelectionFill = Color( 110, 183, 212, 90 );
    };

    class Label : public Widget
    {
    public:
        explicit Label( std::string Text = {}, bool Wrap = false );

        LabelStyle Style;

        void SetText( std::string Text ); // relayouts only if needed
        const std::string& GetText() const { return Text; }
        bool Wrap = false;

        // Selectable: drag to select, double-click a word, triple-click all, Ctrl+C / right-click Copy.
        // For chat messages, logs, IDs and IPs people want to copy.
        void SetSelectable( bool On );
        bool IsSelectable() const { return Selectable; }
        std::string SelectedText() const;
        void SelectAll() { SelA = 0; SelB = Text.size(); Redraw(); }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Selectable ? Cursor::IBeam : Cursor::Arrow; }

    protected:
        std::string Text;
        const Font& GetFont() const;

    private:
        bool Selectable = false, Dragging = false;
        size_t SelA = 0, SelB = 0;          // byte range [min, max)
        Vec2 Origin() const;                 // top-left of the text block (alignment applied)
        size_t ByteAt( Vec2 P ) const;
    };
}
