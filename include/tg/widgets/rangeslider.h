// RangeSlider — pick a [Low, High] range with two knobs.
//
//   W.Range( "Price", &Lo, &Hi, 0, 1000, 10 );
#pragma once
#include "slider.h"

namespace tg
{
    class RangeSlider : public Widget
    {
    public:
        RangeSlider( float* Low, float* High, float Min, float Max, float Step = 0 );

        float *Low, *High;
        float Min, Max, Step;
        std::function<void( float, float )> OnChange;
        SliderStyle Style; // same look as Slider

        float GetLow() const { return Low ? *Low : OwnLo; }
        float GetHigh() const { return High ? *High : OwnHi; }
        void SetRange( float Lo, float Hi );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        float OwnLo, OwnHi, HoverT = 0;
        int Dragging = 0; // 1 low, 2 high
        float ToValue( float X ) const;
        float ToX( float V ) const;
    };
}
