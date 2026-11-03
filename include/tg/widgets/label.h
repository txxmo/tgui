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
    };

    class Label : public Widget
    {
    public:
        explicit Label( std::string Text = {}, bool Wrap = false );

        LabelStyle Style;

        void SetText( std::string Text ); // relayouts only if needed
        const std::string& GetText() const { return Text; }
        bool Wrap = false;

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    protected:
        std::string Text;
        const Font& GetFont() const;
    };
}
