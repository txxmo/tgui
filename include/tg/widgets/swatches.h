// Swatches — a palette of colors to click; Eyedropper — pick any color shown in the window.
//
//   W.Swatches( &Accent );                              // tg's default palette
//   W.Swatches( &Accent, { 0xE5534B, 0x3FB97A, ... } );
//   W.Eyedropper( &Accent );                            // click, then click anywhere in the app (Escape cancels)
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <vector>

namespace tg
{
    class Swatches : public Widget
    {
    public:
        explicit Swatches( Color* Bind = nullptr, std::vector<Color> Palette = {} );
        Color* Bind;
        std::vector<Color> Palette;
        float Size = 22, Gap = 6, Radius = 6;
        int Columns = 0; // 0 = as many as fit
        std::function<void( Color )> OnChange;
        Color GetColor() const { return Bind ? *Bind : Own; }
        void SetColor( Color C );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
        bool CanBind() const override { return true; }
        BindValue BindGet() const override;
        void BindSet( const BindValue& V ) override;

    private:
        Color Own;
        int Hover = -1;
        Rect Cell( size_t i ) const;
        int Cols() const;
    };

    class Eyedropper : public Widget
    {
    public:
        explicit Eyedropper( Color* Bind = nullptr );
        Color* Bind;
        std::function<void( Color )> OnPick;
        bool IsPicking() const { return Picking; }
        void Start();
        void Cancel();

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        bool Picking = false;
        Color Under;
        bool Sample( Vec2 P, Color& Out ) const;
    };
}
