// VirtualList — a list that only lays out and draws the rows on screen: 100k rows scroll like 10.
// Rows are drawn by a callback (fast path), so there's no widget per row.
//
//   auto& L = W.Add<tg::VirtualList>( Lines.size() );
//   L.TextOf = [&]( size_t i ) { return Lines[ i ]; };                // simplest: one line of text
//   L.DrawRow = [&]( tg::DrawList& D, size_t i, tg::Rect R, bool Hover, bool Sel ) { ... }; // or draw it yourself
//   L.HeightOf = [&]( size_t i, float Width ) { return Measure( Msgs[ i ], Width ); };     // rows of different heights
//   L.StickToBottom = true;                                           // chat / logs: follow new rows
//   L.SetCount( Lines.size() );                                       // after adding rows
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class DrawList;

    struct VirtualListStyle
    {
        Paint Fill = Color( 0, 0, 0, 0 );
        Paint HoverFill = Color( 255, 255, 255, 10 );
        Paint SelectedFill = Color( 110, 183, 212, 46 );
        Paint TextColor = Tok::Text;
        Paint Scrollbar = Tok::BorderStrong;
        float Radius = 6;
        Insets RowPadding = { 10, 0 };
    };

    class VirtualList : public Widget
    {
    public:
        explicit VirtualList( size_t Count = 0, float RowHeight = 28 );
        VirtualListStyle Style;

        float RowHeight;                                             // when HeightOf isn't set
        std::function<float( size_t, float Width )> HeightOf;        // variable heights (cached; InvalidateHeights())
        std::function<void( DrawList&, size_t, Rect, bool Hovered, bool Selected )> DrawRow;
        std::function<std::string( size_t )> TextOf;                 // default row drawing
        std::function<void( size_t )> OnClick, OnActivate;          // click; double-click / Enter
        bool Selectable = true;
        bool StickToBottom = false; // stay at the end when rows are added (if the user was already at the end)

        size_t Count() const { return N; }
        void SetCount( size_t Count );
        void InvalidateHeights();
        int Selected = -1;
        void ScrollTo( size_t Row );   // bring a row into view
        void ScrollToBottom();
        float GetScroll() const { return Scroll; }
        int RowAt( Vec2 P ) const;
        Rect RowRect( size_t Row ) const;

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;

    private:
        size_t N;
        float Scroll = 0, Target = 0;
        std::vector<float> Offsets; // prefix sums for variable heights; size N+1
        float CachedWidth = -1;
        int Hover = -1;
        bool DragBar = false, AtBottom = true;
        float DragGrab = 0;
        float Total() const;
        float Top( size_t Row ) const;
        float Height( size_t Row ) const;
        void Rebuild();
        float MaxScroll() const;
        Rect BarRect() const;
    };
}
