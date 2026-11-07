// TabView — a strip of tabs over a stack of pages; switching cross-fades (like the old tgui Window tabs).
//
//   auto& Tabs = Parent.Add<tg::TabView>();
//   auto& Visuals = Tabs.AddTab( "Visuals" );   // a Container: all the helpers work
//   Visuals.Checkbox( "Boxes", &Boxes );
//
// Any Container can also do it in one line:  auto& Page = W.Tab( "Visuals" );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Container;

    struct TabStyle
    {
        float Height = 32, Radius = 7, Inset = 3; // Inset = gap between the strip edge and the tabs
        float Gap = 10;                           // space between the strip and the page
        bool EqualWidth = true;                   // tabs share the full width (tgui style); false = fit text
        float FadeSpeed = 14;                     // 0 = switch instantly
        Paint StripFill = Tok::SurfaceAlt;
        Paint StripStroke = Tok::Border;
        Paint ActiveFill = Tok::Raised;
        Paint HoverFill = Color( 255, 255, 255, 10 );
        Paint Text = Tok::TextDim;
        Paint ActiveText = Tok::Text;
        Paint Underline = Tok::Accent;            // accent bar under the active tab
    };

    class TabView : public Widget
    {
    public:
        TabView();

        TabStyle Style;
        static TabStyle& Defaults();
        int* Bind = nullptr; // optional: the active tab index lives in your int (like tgui's &SelectedTab)
        std::function<void( int )> OnChange;

        Container& AddTab( std::string Name );
        Container& Page( size_t i ) { return *Pages[ i ]; }
        size_t TabCount() const { return Names.size(); }
        const std::string& TabName( size_t i ) const { return Names[ i ]; }

        int GetIndex() const { return Active; }
        void Select( int i );
        Rect TabRect( size_t i ) const;

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;
        Cursor GetCursor() const override;

    private:
        std::vector<std::string> Names;
        std::vector<Container*> Pages;
        int Active = 0, Prev = -1, HoverTab = -1, PressTab = -1;
        float FadeT = 1, SlideX = -1; // SlideX: animated x of the active-tab highlight
        Rect Strip, PageR;
        int TabAt( Vec2 P ) const;
    };
}
