// Slider — drag to pick a value in [Min, Max]. Step > 0 snaps (Step = 1 gives an int slider).
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    struct SliderStyle
    {
        float TrackHeight = 4, KnobRadius = 7, Height = 22;
        Paint Track = Tok::Raised;
        Paint TrackFill = Tok::Accent;
        Paint Knob = Color( 255, 255, 255 );
        Paint KnobStroke = Tok::Accent;
    };

    class Slider : public Widget
    {
    public:
        Slider( float* Bind, float Min, float Max, float Step = 0 );

        float* Bind;
        float Min, Max, Step;
        std::function<void( float )> OnChange;
        SliderStyle Style;

        float GetValue() const { return Bind ? *Bind : Own; }
        void SetValue( float V );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        float Own = 0, HoverT = 0;
        void SetFromMouse( float X );
    };
}
