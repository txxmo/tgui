// Toolbar and StatusBar — the strips at the top and bottom of an app window.
//
//   auto& Tb = W.Toolbar();
//   Tb.Tool( tg::Icon::Save, "Save (Ctrl+S)", [] { Save(); } );
//   Tb.ToolToggle( tg::Icon::Wifi, "Live updates", &Live );
//   Tb.Separator();
//   Tb.Spacer();
//   Tb.Tool( tg::Icon::Settings, "Settings", [] { ... } );
//
//   auto& Sb = W.StatusBar();
//   auto& Conn = Sb.Item( tg::Icon::Wifi + " connected" );
//   Sb.Spacer();
//   Sb.Item( "UTF-8" );
#pragma once
#include "button.h"
#include "container.h"

namespace tg
{
    class Label;

    // An icon button for toolbars; with Checked bound it's a toggle (accent while on).
    class ToolButton : public Button
    {
    public:
        ToolButton( std::string Icon, std::string Tip, std::function<void()> Fn = {}, bool* Checked = nullptr );
        bool* Checked;
        bool IsOn() const { return Checked && *Checked; }
        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool CanBind() const override { return Checked != nullptr || Button::CanBind(); }
        bool BindIsAction() const override { return Checked == nullptr; }
        BindValue BindGet() const override { return { { IsOn() ? 1.0 : 0.0 } }; }
        void BindSet( const BindValue& V ) override;
        std::string BindName() const override { return Tooltip; }
    };

    class Toolbar : public Container
    {
    public:
        Toolbar();
        ToolButton& Tool( std::string Icon, std::string Tip, std::function<void()> Fn );
        ToolButton& ToolToggle( std::string Icon, std::string Tip, bool* Value, std::function<void( bool )> OnChange = {} );
        void Separator();
        void Spacer();
        void Draw( DrawList& D ) override;
    };

    class StatusBar : public Container
    {
    public:
        StatusBar();
        Label& Item( std::string Text );
        void Separator();
        void Spacer();
        void Draw( DrawList& D ) override;
    };
}
