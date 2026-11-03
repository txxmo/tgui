// Groupbox — a titled frame whose contents scroll when they don't fit (the old tgui Groupbox).
// It's a Container, so the helpers work on it directly:
//
//   auto& Cols = Page.Columns();                 // groupboxes side by side, full height
//   auto& G = Cols.Group( "Example Group" );
//   G.Checkbox( "Checkbox", &Value );
#pragma once
#include "container.h"
#include <string>

namespace tg
{
    struct GroupboxStyle
    {
        Paint Fill = Tok::Surface;
        Paint Stroke = Tok::Border;
        Paint HeaderFill = Tok::SurfaceAlt;
        Paint TitleColor = Tok::Text;
        Paint Thumb = Tok::BorderStrong;
        float Radius = 8, HeaderHeight = 30;
        Align TitleAlign = Align::Center; // tgui centered its group titles
        bool HeaderSeparator = true;
    };

    class Groupbox : public Container
    {
    public:
        explicit Groupbox( std::string Title = {} );

        std::string Title;
        GroupboxStyle Frame;
        static GroupboxStyle& Defaults();

        float GetScroll() const { return Scroll; }
        void ScrollTo( float Y );

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        float Scroll = 0, Target = 0, ContentH = 0;
        bool DraggingThumb = false;
        float DragOffset = 0;
        Rect HeaderR, BodyR;
        float MaxScroll() const { return std::max( 0.f, ContentH - BodyR.h ); }
        Rect ThumbRect() const;
        void Place(); // arrange children at the current scroll offset
    };
}
