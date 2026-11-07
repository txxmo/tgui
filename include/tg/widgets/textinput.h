// TextInput — single line text field. Enter fires OnSubmit (chat box), every edit fires OnChange.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    struct TextInputStyle
    {
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeFocused = Tok::Accent;
        Paint TextColor = Tok::Text;
        Paint Placeholder = Tok::TextDim;
        Paint Selection = Color( 110, 183, 212, 90 );
        float Radius = 6;
        Insets Padding = { 10, 8 };
    };

    class TextInput : public Widget
    {
    public:
        explicit TextInput( std::string Placeholder = {} );

        std::string Placeholder;
        std::function<void( const std::string& )> OnChange;
        std::function<void( const std::string& )> OnSubmit;
        std::function<void()> OnBlur; // lost keyboard focus
        bool ClearOnSubmit = false;
        bool Password = false;
        TextInputStyle Style;

        const std::string& GetText() const { return Text; }
        void SetText( std::string T );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::IBeam; }

    private:
        std::string Text;
        size_t Caret = 0, Anchor = 0; // byte offsets; selection is [min, max)
        float ScrollX = 0;
        double BlinkStart = 0;
        bool Dragging = false;

        std::string Shown() const; // Text, or bullets for Password
        size_t ByteAt( float LocalX ) const;
        void Insert( const std::string& S );
        void EraseSelection();
        void Changed();
    };
}
