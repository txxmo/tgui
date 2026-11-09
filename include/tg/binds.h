// tg/binds.h — key binds on any control. Right-click a checkbox, slider, combo... -> "Add key bind".
//
// Each bind is: a key (keyboard or mouse button), a trigger, and the value the control takes:
//
//   Hold      while the key is held the control has the value; letting go puts the old one back
//   Toggle    press: the value; press again: back to what it was
//   Off hold  the value is on all the time EXCEPT while the key is held
//   Press     each press sets the value (nothing is put back)
//
//   checkbox / toggle   on or off             slider / number   a number
//   combo / radio       an option             multicombo        each item on, off or left alone
//   color picker        a color               button            the key clicks it
//
// Users make these themselves from the UI. Your code can too, and save/restore them:
//
//   Ctx.Binds.Add( AimCheckbox, tg::Key::Mouse5, tg::BindTrigger::Hold, { { 1 } } );
//   SaveFile( "binds.txt", Ctx.Binds.Save() );
//   Ctx.Binds.Load( ReadFile( "binds.txt" ), Ctx.Root() );   // matched by the controls' labels
//   Ctx.Binds.OnEdit = [&] { SaveFile( "binds.txt", Ctx.Binds.Save() ); }; // after any UI change
#pragma once
#include "input.h"
#include "widget.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tg
{
    class Context;
    class Menu;

    enum class BindTrigger : uint8_t { Hold, Toggle, OffHold, Press };
    const char* BindTriggerName( BindTrigger T ); // "hold", "toggle", "off hold", "press"
    std::string BindNumber( double V, float Step ); // "75", "0.35" — for BindText
    inline bool BindHas( const BindValue& V, size_t i = 0 ) { return i < V.V.size() && V.V[ i ] == V.V[ i ]; } // set and not NaN
    void OnOffEditor( Container& Into, BindValue& V ); // [ On | Off ] — the bind editor of checkboxes / toggles

    struct Binding
    {
        uint32_t Id = 0;
        Widget* Target = nullptr;
        Key K = Key::None;
        BindTrigger Trigger = BindTrigger::Hold;
        BindValue Value;
        bool Active = false; // Hold / Toggle / OffHold: the value is applied right now
        BindValue Saved;     // what to put back when it stops being active
    };

    class KeyBinds
    {
    public:
        explicit KeyBinds( Context& C ) : Ctx( C ) {}
        ~KeyBinds();

        bool Enabled = true;          // right-click "Add key bind" on controls; binds react to keys
        std::function<void()> OnEdit; // the user added / changed / removed a bind in the UI (save them here)

        Binding& Add( Widget& Target, Key K, BindTrigger T, BindValue V );
        void Update( Binding& B, Key K, BindTrigger T, BindValue V );
        void Remove( uint32_t Id );   // puts the control's value back first if the bind is active
        void RemoveAll( Widget& Target );
        void Clear();
        std::vector<Binding*> For( const Widget& Target ) const;
        const std::vector<std::unique_ptr<Binding>>& All() const { return List; }
        std::string Describe( const Binding& B ) const; // "F5 · hold → 75"

        // The editor dialog (what the right-click menu opens). Existing = nullptr: a new bind.
        void OpenEditor( Widget& Target, Binding* Existing = nullptr );
        // Adds this control's bind items to a menu (Context does it for right-clicks).
        void AddMenuItems( Widget& Target, Menu& M );

        // One bind per line: label <tab> key <tab> trigger <tab> values. Load matches labels under Root.
        std::string Save() const;
        int Load( const std::string& Text, Widget& Root ); // returns how many binds were restored

        // ---- Context internals
        void OnKey( Key K, bool Down );
        void Forget( Widget* W );

    private:
        Context& Ctx;
        std::vector<std::unique_ptr<Binding>> List;
        uint32_t NextId = 1;
        void Activate( Binding& B );
        void Deactivate( Binding& B );
        void Start( Binding& B ); // initial state (Off hold starts active)
    };
}
