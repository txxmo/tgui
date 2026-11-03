// Box — a container with an optional background, border and drop shadow. Your panels/cards/rows.
#pragma once
#include "../theme.h"
#include "../widget.h"

namespace tg
{
    struct BoxStyle
    {
        Paint Fill = Color( 0, 0, 0, 0 );   // transparent = no background
        Paint Stroke = Color( 0, 0, 0, 0 ); // transparent = no border
        float StrokeWidth = 1;
        float Radius = 0;
        bool Shadow = false;
    };

    class Box : public Widget
    {
    public:
        Box() = default;
        explicit Box( Dir D, float Gap = 0, Insets Padding = {} )
        {
            Layout.Direction = D;
            Layout.Gap = Gap;
            Layout.Padding = Padding;
        }

        BoxStyle Style;

        // Ready-made looks. Card() = surface + border + radius, the usual dashboard tile.
        static BoxStyle Card();
        static BoxStyle Panel();

        void Draw( DrawList& D ) override;
    };

    // Convenience builders that read nicely when nesting:
    //   auto& Row = tg::AddRow( Parent, 8 );
    Box& AddRow( Widget& Parent, float Gap = 0, Insets Padding = {} );
    Box& AddColumn( Widget& Parent, float Gap = 0, Insets Padding = {} );
    Box& AddSpacer( Widget& Parent, float Weight = 1 ); // eats leftover space
    Box& AddDivider( Widget& Parent, bool Vertical = false );
}
