// ProgressBar — 0..1 fill. Doubles as a usage meter (CPU, RAM, disk) on dashboards.
#pragma once
#include "../theme.h"
#include "../widget.h"

namespace tg
{
    struct ProgressStyle
    {
        float Height = 8, Radius = 4;
        Paint Track = Tok::Raised;
        Paint Fill = Tok::Accent;
        // If set, fill color goes Fill -> WarnFill -> HotFill as value passes the thresholds.
        bool Thresholds = false;
        float WarnAt = 0.7f, HotAt = 0.9f;
        Paint WarnFill = Tok::Warning;
        Paint HotFill = Tok::Danger;
    };

    class ProgressBar : public Widget
    {
    public:
        explicit ProgressBar( float Value = 0 ) : Value( Value ) {}

        ProgressStyle Style;
        void SetValue( float V );
        float GetValue() const { return Value; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    private:
        float Value, Shown = -1; // Shown animates toward Value
    };
}
