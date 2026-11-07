// ListBox — a bordered, scrollable list. SelectMode::None makes it a plain display list,
// Single / Multi make rows selectable. Switch at runtime with SetMode().
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class ScrollView;
    class ListItem;

    enum class SelectMode : uint8_t { None, Single, Multi };

    struct ListBoxStyle
    {
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        float Radius = 8;
        float Height = 180; // default box height; set Layout.Height to override
    };

    class ListBox : public Widget
    {
    public:
        explicit ListBox( std::vector<std::string> Items = {}, SelectMode Mode = SelectMode::Single, int* Bind = nullptr );

        int* Bind;                      // Single mode: selected index (-1 = none)
        std::function<void()> OnChange; // selection changed
        std::function<void( int )> OnActivate; // row clicked (any mode except None)
        bool Reorderable = false;              // drag rows to reorder them
        std::function<void( int From, int To )> OnReorder;
        ListBoxStyle Style;

        // items
        void SetItems( std::vector<std::string> Items );
        void AddItem( std::string Text );
        void RemoveItem( size_t i );
        size_t Count() const { return Items.size(); }
        const std::string& GetItem( size_t i ) const { return Items[ i ]; }

        // selection
        SelectMode GetMode() const { return Mode; }
        void SetMode( SelectMode M );
        int GetSelected() const;                 // Single: index or -1. Multi: first selected or -1
        bool IsSelected( size_t i ) const;
        void SetSelected( size_t i, bool On );
        void ClearSelection();
        std::vector<int> SelectedIndices() const;

        void MoveItem( size_t From, size_t To ); // To = final index of the moved item

        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;

    private:
        std::vector<std::string> Items;
        std::vector<bool> Sel;
        SelectMode Mode;
        ScrollView* List = nullptr;
        void Rebuild();
        int DragFrom = -1, DropAt = -1; // drag-to-reorder state
        float DragStartY = 0, DragY = 0;
        bool DragMoved = false;
        int RowAtY( float Y, bool Insertion ) const;
        void Click( size_t i );
    };
}
