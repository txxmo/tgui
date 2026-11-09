// ColorPicker — a swatch field that opens a picker popup.
// ColorPanel — the picker itself (SV square + hue + alpha + hex), usable inline too.
//
//   Parent.Add<tg::ColorPicker>( &MyColor );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class Popup;
    class TextInput;

    // HSV helpers (H, S, V in 0..1).
    void ToHSV( Color C, float& H, float& S, float& V );
    Color FromHSV( float H, float S, float V, uint8_t A = 255 );
    std::string ToHex( Color C, bool Alpha = false ); // "#RRGGBB" / "#RRGGBBAA"
    bool ParseHex( const std::string& S, Color& Out ); // accepts RGB, RRGGBB, RRGGBBAA, with or without '#'

    void DrawChecker( DrawList& D, Rect R, float Cell = 5, float Radius = 0 ); // Radius: clipped to rounded corners

    struct ColorPanelStyle
    {
        float SquareSize = 180, BarWidth = 16, Gap = 8, Radius = 4;
        Paint Stroke = Tok::BorderStrong;
    };

    class ColorPanel : public Widget
    {
    public:
        explicit ColorPanel( Color* Bind = nullptr, bool ShowAlpha = true );

        Color* Bind;
        bool ShowAlpha;
        std::function<void( Color )> OnChange;
        ColorPanelStyle Style;

        Color GetColor() const { return Bind ? *Bind : Own; }
        void SetColor( Color C ); // from outside: updates HSV + hex field

        void Draw( DrawList& D ) override;

    private:
        friend class HsvArea;
        Color Own = { 255, 255, 255 };
        float H = 0, S = 0, V = 1;
        Color Last;              // detect external changes to *Bind
        TextInput* Hex = nullptr;
        void SetFromHSV( float H, float S, float V, uint8_t A );
        void Emit( Color C );
    };

    struct ColorPickerStyle
    {
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeOpen = Tok::Accent;
        Paint TextColor = Tok::Text;
        float Radius = 6, SwatchSize = 18;
        Insets Padding = { 6, 5 };
    };

    class ColorPicker : public Widget
    {
    public:
        explicit ColorPicker( Color* Bind = nullptr, bool ShowAlpha = true );
        ~ColorPicker() override;

        Color* Bind;
        bool ShowAlpha;
        bool ShowHex = true; // "#RRGGBB" text next to the swatch
        std::function<void( Color )> OnChange;
        ColorPickerStyle Style;

        Color GetColor() const { return Bind ? *Bind : Own; }
        void SetColor( Color C );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
        // key binds (right-click -> "Add key bind")
        bool CanBind() const override { return true; }
        BindValue BindGet() const override;
        void BindSet( const BindValue& V ) override;
        void BindEditor( Container& Into, BindValue& V ) override;
        std::string BindText( const BindValue& V ) const override;

    private:
        Color Own = { 255, 255, 255 };
        Popup* Open = nullptr;
        float HoverT = 0;
    };
}
