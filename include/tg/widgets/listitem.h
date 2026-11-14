// ListItem — one row in a ListBox / Combo / MultiCombo dropdown. Also usable on its own (menus, sidebars).
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    struct ListItemStyle
    {
        Insets Padding = { 10, 6 };
        float Radius = 5;
        float CheckSize = 14;
        Paint SelectedFill = Color( 110, 183, 212, 46 ); // soft accent
        Paint HoverFill = Color( 255, 255, 255, 14 );
        Paint TextColor = Tok::Text;
        Paint SelectedText = Tok::Text;
        Paint Check = Tok::Accent;
    };

    class ListItem : public Widget
    {
    public:
        explicit ListItem( std::string Text = {} );

        std::string Text;
        std::string Icon;   // optional leading glyph
        float IconWidth = 0; // minimum icon width for aligning mixed lists
        bool Selected = false;
        bool ShowCheck = false;   // checkbox-style tick on the left (multi-select)
        bool ShowMark = false;    // small tick on the right when selected (single-select dropdowns)
        std::function<void()> OnClick;
        bool BubbleRelease = false; // let MouseUp reach the parent too (ListBox drag-to-reorder)
        ListItemStyle Style;

        static ListItemStyle& Defaults();

        // false = plain, non-interactive row ("just a list")
        void SetSelectable( bool S ) { Interactive = S; Redraw(); }
        bool IsSelectable() const { return Interactive; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        float HoverT = 0, SelT = 0;
    };

    // Draws the little tick used by checkboxes / list items. T = 0..1 draw-in progress.
    void DrawCheckMark( DrawList& D, Rect Box, Color C, float Thickness, float T = 1 );
}
