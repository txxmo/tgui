// RadioGroup and Segmented — pick exactly one of a few options.
//
//   W.Radio( { "Low", "Medium", "High" }, &Quality );          // classic radio buttons (column or row)
//   W.Segmented( { "1h", "24h", "7d", "30d" }, &Range );       // pill selector with a sliding highlight
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    struct RadioStyle
    {
        float Size = 18, LabelGap = 9, ItemGap = 10;
        Paint Ring = Tok::BorderStrong;
        Paint RingHover = Tok::Accent;
        Paint Fill = Tok::SurfaceAlt;
        Paint Dot = Tok::Accent;
        Paint TextColor = Tok::Text;
        Paint TextColorOff = Tok::TextDim;
    };

    class RadioGroup : public Widget
    {
    public:
        explicit RadioGroup( std::vector<std::string> Options = {}, int* Bind = nullptr, Dir Direction = Dir::Column );

        std::vector<std::string> Options;
        int* Bind;
        Dir Direction;
        std::function<void( int )> OnChange;
        RadioStyle Style;

        int GetIndex() const { return Bind ? *Bind : Own; }
        void SetIndex( int i );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        int Own = 0, HoverIdx = -1, PressIdx = -1;
        std::vector<float> DotT; // per-option fill animation
        Rect ItemRect( size_t i ) const;
        int ItemAt( Vec2 P ) const;
    };

    struct SegmentedStyle
    {
        float Height = 30, Radius = 7, Inset = 3, MinSegment = 48;
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint Active = Tok::Raised;
        Paint Text = Tok::TextDim;
        Paint ActiveText = Tok::Text;
    };

    class Segmented : public Widget
    {
    public:
        explicit Segmented( std::vector<std::string> Options = {}, int* Bind = nullptr );

        std::vector<std::string> Options;
        int* Bind;
        std::function<void( int )> OnChange;
        SegmentedStyle Style;

        int GetIndex() const { return Bind ? *Bind : Own; }
        void SetIndex( int i );
        Rect SegmentRect( size_t i ) const;

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        int Own = 0, PressIdx = -1;
        float SlideX = -1;
    };
}
