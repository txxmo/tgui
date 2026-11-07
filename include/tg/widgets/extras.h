// Small things: Spinner (loading), Badge (status pill), Link (clickable text).
//
//   W.Spinner();
//   W.Badge( "LIVE", tg::Tok::Danger );
//   W.Link( "Open docs", [] { OpenUrl( "..." ); } );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class Spinner : public Widget
    {
    public:
        explicit Spinner( float Size = 18, Paint C = Tok::Accent ) : Color( C ) { Layout.Width = Px( Size ); Layout.Height = Px( Size ); }
        Paint Color;
        float Thickness = 2.5f, Speed = 1.2f; // turns per second
        void Draw( DrawList& D ) override;
    };

    class Badge : public Widget
    {
    public:
        explicit Badge( std::string Text, Paint Fill = Tok::Accent ) : Text( std::move( Text ) ), Fill( Fill ) {}
        std::string Text;
        Paint Fill;
        Paint TextColor = Color( 255, 255, 255 );
        bool Soft = true; // tinted background + colored text, instead of solid
        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
    };

    class Link : public Widget
    {
    public:
        explicit Link( std::string Text, std::function<void()> OnClick = {} ) : Text( std::move( Text ) ), OnClick( std::move( OnClick ) ) { Interactive = true; }
        std::string Text;
        std::function<void()> OnClick;
        Paint TextColor = Tok::Accent;
        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
    };
}
