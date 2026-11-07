// Button — click it, OnClick runs.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    struct ButtonStyle
    {
        Paint Fill = Tok::Raised;
        Paint Stroke = Tok::BorderStrong;
        Color HoverOverlay = { 255, 255, 255, 22 }; // blended over Fill while hovered
        Color PressOverlay = { 0, 0, 0, 45 };       // ...and while held down
        Paint TextColor = Tok::Text;
        float Radius = 6;
        Insets Padding = { 14, 8 };
    };

    class Button : public Widget
    {
    public:
        explicit Button( std::string Text = {}, std::function<void()> OnClick = {} );

        std::string Text;
        std::function<void()> OnClick;
        ButtonStyle Style;

        static ButtonStyle& Defaults(); // every new Button copies this
        static ButtonStyle Primary();   // accent-filled
        static ButtonStyle Danger();
        static ButtonStyle Ghost();     // no fill until hovered

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        float HoverT = 0, PressT = 0; // animation state, 0..1
    };
}
