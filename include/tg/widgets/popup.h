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

        Widget* Owner = nullptr;   // gets the focus back / OnClose
        bool OwnerToggles = true;  // dropdowns: clicking the owner doesn't auto-close (it toggles instead).
                                   // false for right-click menus: any click outside the popup closes it
        Rect Anchor;               // placed below this rect, or above if there's no room
        float MinWidth = 0;        // usually the anchor's width
        std::function<void()> OnClose;
        bool Animated = true;      // slide + fade in from the anchor side, fade out when closed
        float OpenTime = 0.18f, CloseTime = 0.12f;

        void Close(); // stops input now; animates out, then removes itself
        bool IsClosed() const { return Closed; }

        void Draw( DrawList& D ) override;
        Widget* HitTest( Vec2 P ) override;
        void Finish(); // hide + remove now (no closing animation)

    private:
        bool Closed = false, Removing = false;
        float OpenT = 0;
    };

    // Create a popup on Owner's context overlay, anchored to Anchor.
    Popup& OpenPopup( Widget& Owner, Rect Anchor );
}
