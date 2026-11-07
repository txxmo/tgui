// SearchBox — a text field with a magnifier and a clear (x) button; Escape clears.
//
//   W.Search( "Filter servers", &Query ).OnChange = []( const std::string& Q ) { Refilter( Q ); };
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class TextInput;

    class SearchBox : public Widget
    {
    public:
        explicit SearchBox( std::string Placeholder = "Search", std::string* Bind = nullptr );

        std::string* Bind;
        std::function<void( const std::string& )> OnChange;
        std::function<void( const std::string& )> OnSubmit;
        Paint Fill = Tok::SurfaceAlt, Stroke = Tok::Border, StrokeFocused = Tok::Accent, Icon = Tok::TextDim;
        float Radius = 15; // pill by default

        const std::string& GetText() const;
        void SetText( std::string T );
        TextInput& Field() { return *Text; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        TextInput* Text = nullptr;
        Rect ClearR;
        bool HoverClear = false;
    };
}
