// ScrollView — vertical scrolling area. Put one child (usually a Column) inside via Content().
// StickToBottom keeps it pinned to the newest item while the user is at the bottom (chat logs, consoles).
#pragma once
#include "../theme.h"
#include "../widget.h"

namespace tg
{
    struct ScrollStyle
    {
        float BarWidth = 6, BarMargin = 3, MinThumb = 24, WheelStep = 48;
        Paint Thumb = Tok::BorderStrong;
        Paint ThumbHover = Tok::TextDim;
    };

    class ScrollView : public Widget
    {
    public:
        ScrollView();

        ScrollStyle Style;
        bool StickToBottom = false;
        bool SmoothStick = false; // with StickToBottom: glide down to new content instead of jumping (chat)
        float MaxHeight = 0; // when sized by content (dropdowns): cap the natural height and scroll beyond it

        Widget& Content() { return *Children()[ 0 ]; }
        void ScrollTo( float Y );
        void ScrollToBottom();
        float GetScroll() const { return Scroll; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        float Scroll = 0, Target = 0, ContentH = 0;
        bool AtBottom = true, DraggingThumb = false;
        float DragOffset = 0;
        Rect ThumbRect() const;
        float MaxScroll() const;
    };
}
