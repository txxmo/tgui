// LineChart — rolling time series (CPU %, bandwidth, latency...). Push() samples, it scrolls.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <vector>

namespace tg
{
    struct ChartStyle
    {
        Paint Line = Tok::Accent;
        Paint Grid = Tok::Border;
        Paint AxisText = Tok::TextDim;
        float LineWidth = 2;
        uint8_t FillAlpha = 50; // gradient under the line; 0 = none
        int GridLines = 4;
        float Height = 120;
    };

    class LineChart : public Widget
    {
    public:
        explicit LineChart( size_t Capacity = 120 );

        ChartStyle Style;
        float Min = 0, Max = 100; // fixed range. Set AutoRange for dynamic.
        bool AutoRange = false;
        const char* Unit = "";    // appended to axis labels, e.g. "%"

        void Push( float V );
        void SetData( std::vector<float> V );
        const std::vector<float>& Data() const { return Values; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    private:
        std::vector<float> Values;
        size_t Capacity;
    };
}
