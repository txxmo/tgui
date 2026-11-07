// Toggle — an iOS-style on/off switch. Same API as Checkbox.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    struct ToggleStyle
    {
        float TrackWidth = 36, TrackHeight = 20, KnobInset = 3, LabelGap = 10;
        Paint TrackOff = Tok::Raised;
        Paint TrackOn = Tok::Accent;
        Paint Knob = Color( 255, 255, 255 );
        Paint TextColor = Tok::Text;
    };

    class Toggle : public Widget
    {
    public:
        explicit Toggle( std::string Text, bool* Bind = nullptr );

        std::string Text;
        bool* Bind;
        std::function<void( bool )> OnChange;
        ToggleStyle Style;

        bool IsOn() const { return Bind ? *Bind : Own; }
        void SetOn( bool V );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        bool Own = false;
        float OnT = 0;
    };
}
