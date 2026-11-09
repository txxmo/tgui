// Checkbox — a box + label. Bind it to a bool you own (like your old tgui) or read IsChecked().
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    struct CheckboxStyle
    {
        float BoxSize = 18;
        float Radius = 5;
        float LabelGap = 9;
        float CheckThickness = 2;
        Paint Box = Tok::SurfaceAlt;
        Paint BoxStroke = Tok::BorderStrong;
        Paint BoxHoverStroke = Tok::Accent;
        Paint Fill = Tok::Accent;         // box color when checked
        Paint Check = Tok::TextOnAccent;  // the tick
        Paint TextColor = Tok::Text;
        Paint TextColorOff = Tok::TextDim;
    };

    class Checkbox : public Widget
    {
    public:
        explicit Checkbox( std::string Text, bool* Bind = nullptr );

        std::string Text;
        bool* Bind;          // optional external storage
        std::function<void( bool )> OnChange;
        CheckboxStyle Style;

        static CheckboxStyle& Defaults();

        bool IsChecked() const { return Bind ? *Bind : Own; }
        void SetChecked( bool V );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
        // key binds (right-click -> "Add key bind")
        bool CanBind() const override { return true; }
        BindValue BindGet() const override;
        void BindSet( const BindValue& V ) override;
        void BindEditor( Container& Into, BindValue& V ) override;
        std::string BindText( const BindValue& V ) const override;
        BindValue BindDefault() const override;
        std::string BindName() const override { return Text; }

    private:
        bool Own = false;
        float CheckT = 0, HoverT = 0;
    };
}
