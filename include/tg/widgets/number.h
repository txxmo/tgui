// NumberInput — a number field: type a value, click -/+ , use Up/Down while focused (Shift = x10).
//
//   W.Number( "Port", &Port, 1, 65535, 1 );
//   W.Number( "Scale", &Scale, 0.1f, 4, 0.05f, "%.2f" );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class TextInput;

    struct NumberStyle
    {
        float ButtonWidth = 26, Radius = 6;
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeFocused = Tok::Accent;
        Paint Glyph = Tok::TextDim;
        Paint GlyphHover = Tok::Text;
    };

    class NumberInput : public Widget
    {
    public:
        NumberInput( float* Bind, float Min, float Max, float Step = 1, const char* Format = "%g" );

        float* Bind;
        float Min, Max, Step;
        std::string Format;
        std::function<void( float )> OnChange;
        NumberStyle Style;

        float GetValue() const { return Bind ? *Bind : Own; }
        void SetValue( float V );
        void StepBy( float Steps );
        TextInput& Field() { return *Text; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        float Own = 0, Shown = 0;
        TextInput* Text = nullptr;
        int HoverBtn = 0, PressBtn = 0; // -1 minus, +1 plus
        Rect MinusR, PlusR;
        std::string Formatted( float V ) const;
        void Commit();
    };
}
