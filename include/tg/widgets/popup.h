// Popup — a floating panel on the overlay layer (dropdowns, color pickers, menus).
// Closes on click outside, Escape, or Close(). The opener ("Owner") gets OnClose.
//
//   auto& P = tg::OpenPopup( *this, Bounds );   // anchored under this widget
//   P.Add<tg::Label>( "hello" );
#pragma once
#include "box.h"
#include <functional>

namespace tg
{
    class Popup : public Box
    {
    public:
        Popup();
        ~Popup() override;

        Widget* Owner = nullptr;   // clicking the owner doesn't auto-close (it toggles instead)
        Rect Anchor;               // placed below this rect, or above if there's no room
        float MinWidth = 0;        // usually the anchor's width
        std::function<void()> OnClose;

        void Close();
        bool IsClosed() const { return Closed; }

        void Draw( DrawList& D ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        bool Closed = false;
    };

    // Create a popup on Owner's context overlay, anchored to Anchor.
    Popup& OpenPopup( Widget& Owner, Rect Anchor );
}
