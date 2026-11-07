// Splitter — two panes with a draggable divider (Dir::Row = side by side, Dir::Column = stacked).
//
//   auto& S = W.Split( tg::Dir::Row, 0.3f );     // first pane gets 30%
//   S.First().Tree() ...;  S.Second().Table( ... );
#pragma once
#include "container.h"

namespace tg
{
    struct SplitterStyle
    {
        float Handle = 6, MinFirst = 80, MinSecond = 80;
        Paint Line = Tok::Border;
        Paint LineHot = Tok::Accent;
    };

    class Splitter : public Widget
    {
    public:
        explicit Splitter( Dir D = Dir::Row, float Ratio = 0.5f );

        Dir Direction;
        float Ratio; // first pane's share, 0..1
        SplitterStyle Style;

        Container& First() { return *A; }
        Container& Second() { return *B; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;
        Cursor GetCursor() const override { return Direction == Dir::Row ? Cursor::ResizeH : Cursor::ResizeV; }

    private:
        Container *A, *B;
        Rect HandleR;
        bool Dragging = false, Hot = false;
    };
}
