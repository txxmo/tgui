// Menu — a popup list of commands: right-click menus, "..." buttons, dropdown menus.
//
//   W.ContextMenu = []( tg::Menu& M ) {              // right-click anywhere on W (or its children)
//       M.Item( "Rename", [] { ... }, "F2" );
//       M.Check( "Pinned", &Pinned );
//       M.Separator();
//       M.Item( "Delete", [] { ... } ).Danger = true;
//   };
//
//   tg::OpenMenu( Button, Button.Bounds ).Item( ... );  // open one yourself, anchored under a rect
#pragma once
#include "popup.h"
#include <functional>
#include <string>

namespace tg
{
    class MenuItem : public Widget
    {
    public:
        MenuItem( std::string Text, std::function<void()> Fn, std::string Shortcut = {} );

        std::string Text, Shortcut;
        std::string Icon; // optional leading glyph, e.g. tg::Icon::Copy
        std::function<void()> OnClick;
        bool* Checked = nullptr; // shows a tick; clicking flips it
        bool Danger = false;     // red text (delete, kick, ...)

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        struct Leading { float IconX, TextX; };
        Leading LeadingLayout() const;
        float HoverT = 0;
    };

    class Menu : public Popup
    {
    public:
        Menu();

        MenuItem& Item( std::string Text, std::function<void()> Fn = {}, std::string Shortcut = {} );
        MenuItem& Check( std::string Text, bool* Value, std::string Shortcut = {} );
        void Separator();
        void Header( std::string Text ); // small dim section title
    };

    // Open a menu anchored under Anchor (Pos = a point, e.g. the mouse). Owner gets it back on close.
    Menu& OpenMenu( Widget& Owner, Rect Anchor );
    Menu& OpenMenu( Widget& Owner, Vec2 Pos );
}
