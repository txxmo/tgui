// Dialog — a modal window: dims everything behind it and blocks input until answered.
//
//   tg::Confirm( Ctx, "Delete server?", "vps-fra-01 will be destroyed.", { "Cancel", "Delete" },
//                []( int Choice ) { if ( Choice == 1 ) Delete(); } );       // -1 = dismissed (Escape)
//
//   auto& D = tg::OpenDialog( Ctx, "Rename" );                          // custom content
//   auto& Name = D.Body().Input( "New name" );
//   D.AddButton( "Cancel" );
//   D.AddButton( "Rename", [&] { Rename( Name.GetText() ); } ).Style = tg::Button::Primary();
#pragma once
#include "container.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Context;

    struct DialogStyle
    {
        float Rise = 6; // the card slides up this far while it fades in
        float Width = 420, Radius = 12;
        Paint Backdrop = Color( 0, 0, 0, 140 );
        Paint Fill = Tok::Surface;
        Paint Stroke = Tok::BorderStrong;
    };

    class Dialog : public Widget
    {
    public:
        explicit Dialog( std::string Title );

        DialogStyle Style;
        std::function<void( int )> OnResult; // index of the button pressed, -1 if dismissed

        Container& Body() { return *BodyC; }
        tg::Button& AddButton( std::string Text, std::function<void()> Fn = {} ); // every button closes the dialog
        void Close( int Result = -1 );
        bool IsClosed() const { return Closed; }

        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        Container* Card = nullptr;
        Container* BodyC = nullptr;
        Container* Buttons = nullptr;
        int ButtonCount = 0;
        bool Closed = false;
        float FadeT = 0;
    };

    Dialog& OpenDialog( Context& Ctx, std::string Title );

    // Message + buttons in one call. The last button is the primary one (Enter).
    Dialog& Confirm( Context& Ctx, std::string Title, std::string Message, std::vector<std::string> Buttons = { "Cancel", "OK" },
                     std::function<void( int )> OnResult = {} );
}
