// Window — a floating, draggable, resizable panel with a title bar (like the old tgui Window).
// Lives in a Dir::Free parent (App::Window() handles that). It's a Container, so all helpers work:
//
//   auto& W = App.Window( "Settings", { 40, 40 }, { 320, 0 } );   // height 0 = fit content
//   W.Checkbox( "Enabled", &Enabled );
#pragma once
#include "container.h"
#include <functional>
#include <string>

namespace tg
{
    struct WindowStyle
    {
        Paint Fill = Tok::Surface;
        Paint Stroke = Tok::BorderStrong;
        Paint TitleFill = Tok::SurfaceAlt;
        Paint TitleText = Tok::Text;
        Paint CloseHover = Tok::Danger;
        float Radius = 10, TitleHeight = 36;
        bool Shadow = true;
        float MinWidth = 180, MinHeight = 90;
    };

    class Window : public Container
    {
    public:
        explicit Window( std::string Title, Vec2 Pos = { 40, 40 }, Vec2 Size = { 340, 0 } );

        std::string Title;
        bool Movable = true, Resizable = true, Closable = true;
        std::function<void()> OnClose;
        WindowStyle Chrome; // window frame look (Style is the inherited Box style, unused here)

        static WindowStyle& Defaults();

        void Show();  // make visible + bring to front
        void Close(); // hide (keeps the widgets; Show() brings it back)
        void SetPosition( Vec2 P );
        Vec2 GetPosition() const { return Layout.Offset; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;
        Cursor GetCursor() const override;

    private:
        enum class DragMode : uint8_t { None, Move, Resize } Drag = DragMode::None;
        Vec2 DragStart, StartOffset, StartSize;
        float CloseHoverT = 0;
        Rect TitleR, CloseR, GripR, BodyR;
        bool OverClose = false, OverGrip = false;
    };
}
