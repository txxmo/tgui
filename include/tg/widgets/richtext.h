// RichText — formatted, word-wrapped text from light markup. For chat messages, notes, help text.
//
//   W.Rich( "**Deploy** finished on `fra-01` in <success>42s</>. See [the log](https://ci/123) @mo" );
//
//   **bold**   *italic*   `code`   [label](url)   @mention   \* (escape)
//   <accent>..</>  <dim>..</>  <success>..</>  <warning>..</>  <danger>..</>  <#E5534B>..</>  colored
//   tg::Icon::... codepoints and emoji work like in any text.
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Font;

    struct RichTextStyle
    {
        Paint Text = Tok::Text;
        Paint Link = Tok::Accent;
        Paint Mention = Tok::Accent;
        Paint CodeText = Tok::Text;
        Paint CodeFill = Tok::SurfaceAlt;
        Font* TextFont = nullptr;   // nullptr = Theme().Body; bold = Theme().Bold, code = Theme().Mono
        bool Mentions = true;       // highlight @names
    };

    class RichText : public Widget
    {
    public:
        explicit RichText( std::string Markup = {} );
        RichTextStyle Style;
        std::function<void( const std::string& Url )> OnLink;    // [label](url) clicked
        std::function<void( const std::string& Name )> OnMention; // @name clicked

        void SetText( std::string Markup );
        const std::string& GetText() const { return Source; }
        std::string PlainText() const; // without markup (copying, search)

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override;

    private:
        enum Kind : uint8_t { Plain, Bold, Italic, Code, Link, Mention, Colored };
        struct Run
        {
            std::string Text;
            Kind K = Plain;
            bool Strong = false, Em = false;
            Paint Col = Tok::Text;
            std::string Target; // link url / mention name
        };
        struct Piece
        {
            size_t RunIdx;
            std::string Text;
            Rect R; // relative to the widget's top-left
        };
        std::string Source;
        std::vector<Run> Runs;
        std::vector<Piece> Pieces;
        float LaidWidth = -1;
        Vec2 LaidSize;
        int HoverRun = -1;
        void Parse();
        Vec2 Flow( float Width );
        const Font& FontOf( const Run& R ) const;
        int RunAt( Vec2 P ) const;
    };
}
