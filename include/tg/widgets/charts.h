// More charts for dashboards (LineChart lives in chart.h):
//
//   auto& B = W.BarChart();  B.SetBars( { { "Mon", 12 }, { "Tue", 30 }, ... } );
//   auto& P = W.Donut();     P.SetSlices( { { "nginx", 42, tg::Tok::Accent }, { "redis", 18, tg::Tok::Success } } );
//   W.Gauge( "CPU", 0.63f ).Unit = "%";
//   W.Sparkline().Push( 12.f );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <string>
#include <vector>

namespace tg
{
    // ------------------------------------------------------------ BarChart
    struct Bar { std::string Label; float Value = 0; Paint Color = Tok::Accent; };

    class BarChart : public Widget
    {
    public:
        BarChart();
        float Height = 160, Max = 0; // Max 0 = auto
        bool ShowValues = true;
        const char* Format = "%g";
        Paint Grid = Tok::Border, AxisText = Tok::TextDim;

        void SetBars( std::vector<Bar> B );
        void SetValue( size_t i, float V );
        const std::vector<Bar>& Bars() const { return Items; }
        int HoveredBar() const { return Hover; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;

    private:
        std::vector<Bar> Items;
        std::vector<float> Shown; // animated heights
        int Hover = -1;
        Rect BarRect( size_t i, float V ) const;
        Rect Plot() const;
        float Top() const;
    };

    // ------------------------------------------------------------ DonutChart
    struct Slice { std::string Label; float Value = 0; Paint Color = Tok::Accent; };

    class DonutChart : public Widget
    {
    public:
        DonutChart();
        float Size = 150, Thickness = 22;
        bool Legend = true;
        std::string Center;     // big text in the hole ("" = total)
        std::string CenterSub;  // small text under it
        const char* Format = "%g";

        void SetSlices( std::vector<Slice> S );
        const std::vector<Slice>& Slices() const { return Items; }
        int HoveredSlice() const { return Hover; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;

    private:
        std::vector<Slice> Items;
        std::vector<float> Pop; // hover "pop out" animation
        int Hover = -1;
        float Total() const;
        int SliceAt( Vec2 P ) const;
        Vec2 RingCenter() const { return { Bounds.x + Size * 0.5f, Bounds.y + Size * 0.5f }; }
    };

    // ------------------------------------------------------------ Gauge
    class Gauge : public Widget
    {
    public:
        explicit Gauge( std::string Label = {}, float Value = 0 );
        std::string Label;
        const char* Unit = "%";
        float Size = 130, Thickness = 12;
        float WarnAt = 0.7f, HotAt = 0.9f;
        bool Thresholds = true;
        Paint Track = Tok::Raised, Fill = Tok::Accent, WarnFill = Tok::Warning, HotFill = Tok::Danger;

        void SetValue( float V01 );
        float GetValue() const { return Value; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    private:
        float Value, Shown = -1;
    };

    // ------------------------------------------------------------ Sparkline
    class Sparkline : public Widget
    {
    public:
        explicit Sparkline( size_t Capacity = 40 );
        Paint LineColor = Tok::Accent;
        float Width = 100, Height = 28;
        bool Fill = true;
        void Push( float V );
        const std::vector<float>& Data() const { return Values; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    private:
        std::vector<float> Values;
        size_t Capacity;
    };
}
