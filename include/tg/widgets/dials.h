// Knob — a rotary control (audio, sensitivity, FOV). Drag up/down, scroll, or arrow keys; double-click resets.
// XYPad — a 2D value in a square (crosshair offset, pan/tilt, a color's saturation/value).
//
//   W.Knob( "Gain", &Gain, -24, 24 );
//   W.XYPad( "Offset", &Off, { -1, -1 }, { 1, 1 } );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class Knob : public Widget
    {
    public:
        Knob( std::string Label, float* Bind, float Min, float Max, float Step = 0 );
        std::string Label;
        float* Bind;
        float Min, Max, Step, Default;
        std::string Format = "%.0f";
        float Size = 56;
        Paint Track = Tok::Raised, Fill = Tok::Accent, Text = Tok::Text;
        std::function<void( float )> OnChange;

        float GetValue() const { return Bind ? *Bind : Own; }
        void SetValue( float V );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::ResizeV; }
        bool CanBind() const override { return true; }
        BindValue BindGet() const override { return { { GetValue() } }; }
        void BindSet( const BindValue& V ) override;
        void BindEditor( Container& Into, BindValue& V ) override;
        std::string BindText( const BindValue& V ) const override;
        std::string BindName() const override { return Label; }

    private:
        float Own = 0, Shown = -1, DragY = 0, DragV = 0, HoverT = 0;
    };

    class XYPad : public Widget
    {
    public:
        XYPad( std::string Label, Vec2* Bind, Vec2 Min = { 0, 0 }, Vec2 Max = { 1, 1 } );
        std::string Label;
        Vec2* Bind;
        Vec2 Min, Max;
        float Size = 140;
        int GridLines = 4;
        Paint Fill = Tok::SurfaceAlt, Grid = Tok::Border, Dot = Tok::Accent;
        std::function<void( Vec2 )> OnChange;

        Vec2 GetValue() const { return Bind ? *Bind : Own; }
        void SetValue( Vec2 V );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
        bool CanBind() const override { return true; }
        BindValue BindGet() const override { Vec2 V = GetValue(); return { { V.x, V.y } }; }
        void BindSet( const BindValue& V ) override;
        void BindEditor( Container& Into, BindValue& V ) override;
        std::string BindText( const BindValue& V ) const override;
        std::string BindName() const override { return Label; }

    private:
        Vec2 Own;
        Rect Pad() const;
        void SetFromMouse( Vec2 P );
    };
}
