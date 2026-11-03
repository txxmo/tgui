// Table — columns + rows of text, built for data: process lists, server lists, logs, leaderboards.
//
//   auto& T = Parent.Add<tg::Table>();
//   T.AddColumn( "Name", tg::Grow() );
//   T.AddColumn( "CPU", tg::Px( 80 ), tg::Align::End );
//   T.AddRow( { "nginx", "3.2%" } );
//
// - Click a header to sort (again to reverse). Numbers like "37%" or "1.2 GB" sort as numbers.
// - Drag a header's right edge to resize that column.
// - Rows are drawn, not widgets, and only the visible ones: 100k rows is fine.
// - Rows keep their index when sorted, so selection / Bind / SetCell use your original row numbers.
// - DrawCell lets you paint any cell yourself (bars, status dots, colored text).
#pragma once
#include "../theme.h"
#include "../widget.h"
#include "listbox.h" // SelectMode
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Font;

    struct TableColumn
    {
        std::string Title;
        Size Width = Grow();          // Px(n) or Grow(weight)
        Align TextAlign = Align::Start;
        bool Sortable = true;
        float MinWidth = 40;
        // Custom ordering. Default: numeric if both cells start with a number, else case-insensitive text.
        std::function<bool( const std::string& A, const std::string& B )> Less;
    };

    struct TableStyle
    {
        float RowHeight = 30, HeaderHeight = 34;
        Insets CellPadding = { 12, 0 };
        float Radius = 8;
        bool Striped = true;
        bool GridLines = false; // vertical lines between columns
        Paint Fill = Tok::Surface;
        Paint Stroke = Tok::Border;
        Paint HeaderFill = Tok::SurfaceAlt;
        Paint HeaderText = Tok::TextDim;
        Paint Text = Tok::Text;
        Paint Stripe = Color( 255, 255, 255, 6 );
        Paint HoverFill = Color( 255, 255, 255, 12 );
        Paint SelectedFill = Color( 110, 183, 212, 46 );
        Paint Thumb = Tok::BorderStrong;
    };

    class Table : public Widget
    {
    public:
        explicit Table( std::vector<TableColumn> Columns = {} );

        TableStyle Style;
        static TableStyle& Defaults();

        // ---- columns
        TableColumn& AddColumn( std::string Title, Size Width = Grow(), Align TextAlign = Align::Start );
        void SetColumns( std::vector<TableColumn> Columns );
        size_t ColumnCount() const { return Cols.size(); }
        TableColumn& Column( size_t i ) { return Cols[ i ]; }

        // ---- rows (indices are stable: sorting changes display order, not row numbers)
        size_t AddRow( std::vector<std::string> Cells );
        void SetRows( std::vector<std::vector<std::string>> Rows );
        void SetCell( size_t Row, size_t Col, std::string Text );
        const std::string& GetCell( size_t Row, size_t Col ) const;
        void RemoveRow( size_t Row );
        void ClearRows();
        size_t RowCount() const { return Rows.size(); }

        // ---- sorting
        void SortBy( int Col, bool Ascending = true ); // -1 = insertion order
        int SortColumn() const { return SortCol; }
        bool SortAscending() const { return SortAsc; }
        size_t DisplayRow( size_t Position ) const { EnsureOrder(); return Order[ Position ]; } // row shown at Position

        // ---- selection
        SelectMode Mode = SelectMode::Single;
        int* Bind = nullptr; // Single mode: selected row
        std::function<void()> OnSelect;
        std::function<void( size_t Row )> OnActivate; // a row was clicked
        int GetSelected() const;
        bool IsSelected( size_t Row ) const;
        void SetSelected( size_t Row, bool On );
        void ClearSelection();
        std::vector<int> SelectedRows() const;

        // ---- scrolling
        void ScrollToRow( size_t Row );
        float GetScroll() const { return Scroll; }

        // ---- custom cells: return true if you drew it, false to get the default text
        std::function<bool( DrawList& D, Rect Cell, size_t Row, size_t Col )> DrawCell;
        // per-cell text color override (e.g. red for "stopped"); return false to keep the default
        std::function<bool( size_t Row, size_t Col, Color& Out )> CellColor;

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override;

    private:
        std::vector<TableColumn> Cols;
        std::vector<std::vector<std::string>> Rows;
        mutable std::vector<size_t> Order; // display position -> row index
        mutable bool OrderDirty = true;    // re-sort lazily: bulk AddRow() stays O(n)
        std::vector<bool> Sel;
        std::vector<float> ColW;    // resolved widths
        int SortCol = -1;
        bool SortAsc = true;
        float Scroll = 0, Target = 0;
        int HoverRow = -1;          // display position under the mouse
        int ResizeCol = -1, HoverEdge = -1, PressedHeader = -1;
        bool DraggingThumb = false;
        float DragStartX = 0, DragStartW = 0, DragOffset = 0;

        Rect HeaderRect() const;
        Rect BodyRect() const;
        float ContentHeight() const { return float( Rows.size() ) * Style.RowHeight; }
        float MaxScroll() const;
        Rect ThumbRect() const;
        void ResolveWidths();
        void Resort() { OrderDirty = true; }
        void EnsureOrder() const;
        int EdgeAt( Vec2 P ) const;   // column whose right edge is under P
        int HeaderAt( Vec2 P ) const; // column header under P
        int RowAt( Vec2 P ) const;    // display position under P
        float ColX( size_t c ) const;
    };
}
