// Collapsible — a section with a clickable header that folds its contents away (accordion).
// It's a Container, so the helpers work on it:
//
//   auto& Adv = W.Collapsible( "Advanced", &AdvancedOpen );
//   Adv.Checkbox( "Verbose logging", &Verbose );
#pragma once
#include "container.h"
#include <functional>
#include <string>

namespace tg
{
    struct CollapsibleStyle
    {
        float HeaderHeight = 32, Radius = 8;
        Paint HeaderFill = Tok::SurfaceAlt;
        Paint HeaderHover = Color( 255, 255, 255, 10 );
        Paint Stroke = Tok::Border;
        Paint Title = Tok::Text;
        Paint Chevron = Tok::TextDim;
    };

    class Collapsible : public Container
    {
    public:
        explicit Collapsible( std::string Title, bool* Open = nullptr );

        std::string Title;
        bool* Bind;
        std::function<void( bool )> OnToggle;
        CollapsibleStyle Frame;

        bool IsOpen() const { return Bind ? *Bind : Own; }
        void SetOpen( bool O );

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;
        Cursor GetCursor() const override { return Cursor::Arrow; }

    private:
        bool Own = false;
        float OpenT = 0, HoverT = 0;
        Rect HeaderR;
        bool OverHeader = false;
    };
}
