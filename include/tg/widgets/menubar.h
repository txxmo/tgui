// MenuBar — File / Edit / View... across the top of a window or the app.
//
//   auto& Bar = App.Main().MenuBar();
//   Bar.Add( "File", [&]( tg::Menu& M ) {
//       M.Item( "New", [] { ... }, "Ctrl+N" );
//       M.Separator();
//       M.Item( "Quit", [&] { App.Quit(); } );
//   } );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Menu;

    struct MenuBarStyle
    {
        float Height = 30, ItemPadding = 12, Radius = 5;
        Paint Fill = Tok::Surface;
        Paint Stroke = Tok::Border;
        Paint Hover = Color( 255, 255, 255, 14 );
        Paint Open = Tok::Raised;
        Paint TextColor = Tok::Text;
    };

    class MenuBar : public Widget
    {
    public:
        MenuBar();
        ~MenuBar() override;

        MenuBarStyle Style;

        // The builder runs every time the menu opens, so it can reflect current state.
        void Add( std::string Title, std::function<void( Menu& )> Build );
        void OpenIndex( int i );
        void CloseMenu();
        int OpenIndexNow() const { return Open; }
        Rect ItemRect( size_t i ) const;
        size_t Count() const { return Entries.size(); }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;

    private:
        struct Entry { std::string Title; std::function<void( Menu& )> Build; };
        std::vector<Entry> Entries;
        int Open = -1, Hover = -1;
        Menu* OpenMenuPtr = nullptr;
        int At( Vec2 P ) const;
    };
}
