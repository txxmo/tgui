// TreeView — expandable hierarchy (files, server groups, channels). Rows are drawn, not widgets.
//
//   auto& T = W.Tree();
//   auto& Eu = T.Add( "Europe" );
//   Eu.Add( "vps-fra-01" ); Eu.Add( "vps-ams-02" );
//   Eu.Expanded = true;
//   T.OnSelect = []( tg::TreeNode& N ) { ... };
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tg
{
    struct TreeNode
    {
        std::string Text;
        bool Expanded = false;
        int64_t Id = 0;          // your data (database id, index...)
        Paint Color = Tok::Text;
        std::vector<std::unique_ptr<TreeNode>> Children;
        TreeNode* Parent = nullptr;

        TreeNode& Add( std::string Text, int64_t Id = 0 );
        void Remove( TreeNode* Child );
        bool HasChildren() const { return !Children.empty(); }
        int Depth() const;
    };

    struct TreeStyle
    {
        float RowHeight = 28, Indent = 18, Radius = 8;
        Paint Fill = Tok::Surface;
        Paint Stroke = Tok::Border;
        Paint HoverFill = Color( 255, 255, 255, 12 );
        Paint SelectedFill = Color( 110, 183, 212, 46 );
        Paint Chevron = Tok::TextDim;
        Paint Guide = Tok::Border; // indentation guide lines
        Paint Thumb = Tok::BorderStrong;
    };

    class TreeView : public Widget
    {
    public:
        TreeView();

        TreeStyle Style;
        TreeNode Root; // invisible; its children are the top level
        std::function<void( TreeNode& )> OnSelect;
        std::function<void( TreeNode& )> OnActivate; // clicked (also when already selected)
        std::function<void( TreeNode&, bool )> OnToggle;

        TreeNode& Add( std::string Text, int64_t Id = 0 ) { Redraw(); return Root.Add( std::move( Text ), Id ); }
        void Clear();
        void ExpandAll( bool Expand = true );
        TreeNode* GetSelected() const { return Selected; }
        void Select( TreeNode* N );
        void Toggle( TreeNode& N );
        size_t VisibleRows();

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        TreeNode* Selected = nullptr;
        std::vector<TreeNode*> Rows; // flattened visible nodes (rebuilt each draw)
        float Scroll = 0, Target = 0;
        int HoverRow = -1, PressRow = -1;
        bool PressOnChevron = false;
        void Flatten();
        float MaxScroll() const;
        int RowAt( Vec2 P ) const;
        Rect ChevronRect( int Row ) const;
    };
}
