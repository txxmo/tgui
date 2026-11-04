// Keybind — click it, press any key or mouse button (incl. side buttons), that's the bind.
// Escape unbinds (like the old tgui). Right-click to choose Hold / Toggle / Always.
//
//   static tg::Key AimKey = tg::Key::Mouse5;
//   W.Keybind( "Aim key", &AimKey );
//   ...
//   if ( AimBind.IsActive() ) { ... }   // honours the mode (hold / toggle / always)
#pragma once
#include "../input.h"
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class Popup;

    enum class BindMode : uint8_t { Hold, Toggle, Always };
    const char* BindModeName( BindMode M );

    struct KeybindStyle
    {
        float MinBoxWidth = 64, BoxHeight = 26, Radius = 6, LabelGap = 10;
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeListening = Tok::Accent;
        Paint KeyText = Tok::Text;
        Paint NoneText = Tok::TextDim;
        Paint Label = Tok::Text;
        Paint ModeText = Tok::TextDim;
    };

    class Keybind : public Widget
    {
    public:
        explicit Keybind( std::string Label = {}, Key* Bind = nullptr );
        ~Keybind() override;

        std::string Text;      // label on the left; empty = just the key box
        Key* Bind;             // optional external storage
        BindMode* ModeBind = nullptr; // optional external storage for the mode
        bool AllowModes = true;       // right-click menu
        bool ShowMode = false;        // show "hold"/"toggle" next to the key
        std::function<void( Key )> OnChange;
        KeybindStyle Style;
        static KeybindStyle& Defaults();

        Key GetKey() const { return Bind ? *Bind : Own; }
        void SetKey( Key K );
        BindMode GetMode() const { return ModeBind ? *ModeBind : OwnMode; }
        void SetMode( BindMode M );

        bool IsListening() const { return Listening; }
        void StartListening();
        void StopListening();

        // Is the bind "on" right now? Hold: key held. Toggle: flips on each press. Always: true.
        bool IsActive() const;

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        Key Own = Key::None;
        BindMode OwnMode = BindMode::Hold;
        bool Listening = false;
        float HoverT = 0, ListenT = 0;
        uint32_t ToggleBase = 0; // press count when the toggle was armed
        Popup* Menu = nullptr;
        Rect BoxRect() const;
        std::string BoxText() const;
        void OpenModeMenu();
        void Rearm();
    };
}
