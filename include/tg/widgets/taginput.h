// TagInput — a list of chips you add by typing (Enter or comma) and remove with x or Backspace.
//
//   static std::vector<std::string> Tags = { "prod", "eu" };
//   W.Tags( &Tags, "Add tag" );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class TextInput;

    struct TagStyle
    {
        float ChipHeight = 24, Gap = 6, Radius = 6;
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeFocused = Tok::Accent;
        Paint Chip = Tok::Raised;
        Paint ChipText = Tok::Text;
        Paint ChipX = Tok::TextDim;
        Insets Padding = { 6, 5 };
    };

    class TagInput : public Widget
    {
    public:
        explicit TagInput( std::vector<std::string>* Bind = nullptr, std::string Placeholder = "Add..." );

        std::vector<std::string>* Bind;
        bool AllowDuplicates = false;
        std::function<void( const std::vector<std::string>& )> OnChange;
        TagStyle Style;

        const std::vector<std::string>& GetTags() const { return Bind ? *Bind : Own; }
        void AddTag( std::string T );
        void RemoveTag( size_t i );
        TextInput& Field() { return *Text; }

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        std::vector<std::string> Own;
        TextInput* Text = nullptr;
        std::vector<Rect> Chips; // laid-out chip rects
        int HoverX = -1, PressX = -1;
        std::vector<std::string>& Tags() { return Bind ? *Bind : Own; }
        float ChipWidth( const std::string& T ) const;
        Vec2 Flow( float Width, bool Place ); // lay chips + field out in rows
        Rect XRect( size_t i ) const;
    };
}
