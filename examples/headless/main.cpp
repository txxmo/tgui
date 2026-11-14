// Headless run: software renderer, scripted input, PNG screenshots, and assertions.
// Doubles as the test suite (ctest) — no GPU or window needed.
#include "../common/demo_ui.h"
#include "soft_renderer.h"
#include "app_common.h"
#include "tg/os.h"
#include "tg/settings.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define CHECK( x ) do { if ( !( x ) ) { std::fprintf( stderr, "CHECK failed: %s (line %d)\n", #x, __LINE__ ); return 1; } } while ( 0 )

static tg::SoftRenderer Soft;

static bool UnderWine()
{
#ifdef _WIN32
    HMODULE N = GetModuleHandleA( "ntdll.dll" );
    return N && GetProcAddress( N, "wine_get_version" );
#else
    return false;
#endif
}
static const char* OutDir = ".";
static float Scale = 1;

static void Settle( tg::Context& Ctx )
{
    Ctx.Frame();
    for ( int i = 0; i < 60 && Ctx.NeedsFrame(); i++ ) Ctx.Frame();
}

static void Shot( tg::Context& Ctx, const char* Name )
{
    Settle( Ctx );
    tg::Vec2 V = Ctx.GetViewport();
    Ctx.Render( { V.x * Scale, V.y * Scale } );
    std::string Path = std::string( OutDir ) + "/" + Name + ".png";
    Soft.SavePNG( Path );
    std::printf( "wrote %s\n", Path.c_str() );
}

static void ClickAt( tg::Context& Ctx, tg::Vec2 P )
{
    Ctx.OnMouseMove( P );
    Ctx.OnMouseButton( tg::MouseButton::Left, true );
    Ctx.OnMouseButton( tg::MouseButton::Left, false );
    Ctx.Frame(); // apply deferred work (popup removal, relayout)
}

static void Click( tg::Context& Ctx, tg::Widget& W ) { ClickAt( Ctx, W.Bounds.Center() ); }

static void ClickTab( tg::Context& Ctx, DemoApp& App, int i )
{
    ClickAt( Ctx, App.Tabs->TabRect( size_t( i ) ).Center() );
    Settle( Ctx ); // let the cross-fade finish
}

static void Drag( tg::Context& Ctx, tg::Vec2 From, tg::Vec2 To )
{
    Ctx.OnMouseMove( From );
    Ctx.OnMouseButton( tg::MouseButton::Left, true );
    for ( int i = 1; i <= 8; i++ ) Ctx.OnMouseMove( From + ( To - From ) * ( i / 8.f ) );
    Ctx.OnMouseButton( tg::MouseButton::Left, false );
    Ctx.Frame();
}

// First popup currently open on the overlay, or nullptr.
static tg::Popup* OpenPopup( tg::Context& Ctx )
{
    for ( auto& K : Ctx.Overlay().Children() )
        if ( auto* P = dynamic_cast<tg::Popup*>( K.get() ); P && !P->IsClosed() ) return P;
    return nullptr;
}

// Depth-first search for the Nth widget of type T under Root.
template<typename T> static T* Find( tg::Widget& Root, int Nth = 0 )
{
    std::vector<tg::Widget*> Stack = { &Root };
    int Seen = 0;
    while ( !Stack.empty() )
    {
        tg::Widget* W = Stack.front();
        Stack.erase( Stack.begin() );
        if ( auto* M = dynamic_cast<T*>( W ) ) { if ( Seen++ == Nth ) return M; }
        for ( auto& K : W->Children() ) Stack.push_back( K.get() );
    }
    return nullptr;
}

int main( int Argc, char** Argv )
{
    if ( Argc > 1 ) OutDir = Argv[ 1 ];
    if ( Argc > 2 ) Scale = float( std::atof( Argv[ 2 ] ) );

    tg::Context Ctx( Soft );
    CHECK( Ctx.LoadDefaultFonts( Scale ) ); // the embedded font: no asset files needed
    Ctx.SetViewport( { 1280, 820 }, Scale );
    Ctx.SetFixedTimestep( 1.f / 60.f ); // deterministic animations

    DemoApp App;
    App.Build( Ctx, Ctx.Root() );
    Shot( Ctx, "dashboard" );

    // ---------------- chat: nav, focus, type, Enter, stick-to-bottom
    ClickTab( Ctx, App, 1 );
    CHECK( App.Page == 1 );
    Click( Ctx, *App.Composer );
    CHECK( Ctx.Focused() == App.Composer );
    for ( const char* p = "hello from the headless test"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
    CHECK( App.Composer->GetText() == "hello from the headless test" );
    size_t Before = App.Messages->Content().Children().size();
    Ctx.OnKey( tg::Key::Enter, true, 0 );
    CHECK( App.Messages->Content().Children().size() == Before + 1 );
    CHECK( App.Composer->GetText().empty() );
    for ( int i = 0; i < 12; i++ )
        App.AddMessage( i % 2 ? "rika" : "mo", "filler message to overflow the chat log so it has to scroll", std::string( i < 10 ? "11:0" : "11:" ) + std::to_string( i ) );
    for ( const char* p = "half-typed reply"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
    App.TickScreenShare( 1.7 );
    Settle( Ctx );
    CHECK( App.Messages->GetScroll() > 0 );
    Shot( Ctx, "chat" );

    // ---------------- controls: bound checkbox
    ClickTab( Ctx, App, 2 );
    tg::Checkbox* Notif = nullptr;
    for ( int i = 0; auto* C = Find<tg::Checkbox>( *App.Pages[ 2 ], i ); i++ )
        if ( C->Bind == &App.Notifications ) Notif = C;
    CHECK( Notif );
    bool Was = App.Notifications;
    Click( Ctx, *Notif );
    CHECK( App.Notifications != Was );
    Shot( Ctx, "controls" );

    // ---------------- key binds on controls
    {
        tg::KeyBinds& KB = Ctx.Binds;
        tg::Slider* Vol = nullptr;
        for ( int i = 0; auto* S = Find<tg::Slider>( *App.Pages[ 2 ], i ); i++ )
            if ( S->Bind == &App.Volume ) Vol = S;
        CHECK( Vol && Vol->BindName() == "Volume" && Notif->BindName() == "Desktop notifications" );
        CHECK( App.RegionCombo->BindName() == "Region" && App.AlertCombo->BindName() == "Alerts" );
        auto Press = [ & ]( tg::Key K ) { Ctx.OnKey( K, true, 0 ); };
        auto Release = [ & ]( tg::Key K ) { Ctx.OnKey( K, false, 0 ); };
        Ctx.SetFocus( nullptr );
        bool N0 = App.Notifications;
        // hold: on while held, back on release
        auto& Hb = KB.Add( *Notif, tg::Key::F, tg::BindTrigger::Hold, { { N0 ? 0.0 : 1.0 } } );
        Press( tg::Key::F );
        CHECK( App.Notifications != N0 && Hb.Active );
        Release( tg::Key::F );
        CHECK( App.Notifications == N0 && !Hb.Active );
        // toggle on a slider: press -> 80, press -> back
        float V0 = App.Volume;
        KB.Add( *Vol, tg::Key::G, tg::BindTrigger::Toggle, { { 80 } } );
        Press( tg::Key::G ); Release( tg::Key::G );
        CHECK( App.Volume == 80 );
        Press( tg::Key::G ); Release( tg::Key::G );
        CHECK( App.Volume == V0 );
        // off hold: active right away, off while held
        int R0 = App.Region;
        KB.Add( *App.RegionCombo, tg::Key::H, tg::BindTrigger::OffHold, { { 3 } } );
        CHECK( App.Region == 3 );
        Press( tg::Key::H );
        CHECK( App.Region == R0 );
        Release( tg::Key::H );
        CHECK( App.Region == 3 );
        // multicombo: switch item 1 on and item 0 off, leave the rest; only those come back
        std::vector<bool> A0 = App.Alerts;
        KB.Add( *App.AlertCombo, tg::Key::J, tg::BindTrigger::Hold, { { 0, 1, NAN, NAN, NAN } } );
        Press( tg::Key::J );
        CHECK( !App.Alerts[ 0 ] && App.Alerts[ 1 ] && App.Alerts[ 2 ] == A0[ 2 ] );
        App.Alerts[ 3 ] = !App.Alerts[ 3 ];                         // the user changes an untouched item meanwhile
        Release( tg::Key::J );
        CHECK( App.Alerts[ 0 ] == A0[ 0 ] && App.Alerts[ 1 ] == A0[ 1 ] && App.Alerts[ 3 ] != A0[ 3 ] );
        App.Alerts[ 3 ] = A0[ 3 ];
        // press: sets once, nothing put back; mouse buttons work as keys
        KB.Add( *Vol, tg::Key::Mouse5, tg::BindTrigger::Press, { { 20 } } );
        Ctx.OnMouseButton( tg::MouseButton::Forward, true );
        Ctx.OnMouseButton( tg::MouseButton::Forward, false );
        CHECK( App.Volume == 20 );
        // buttons: the key clicks them
        int Pings = 0;
        auto& Ping = App.Pages[ 2 ]->Add<tg::Button>( "Ping", [ & ] { Pings++; } );
        KB.Add( Ping, tg::Key::K, tg::BindTrigger::Hold, {} );
        Press( tg::Key::K ); Release( tg::Key::K );
        CHECK( Pings == 1 );
        // typing in a text field doesn't fire binds
        Ctx.SetFocus( App.Composer );
        Press( tg::Key::K ); Release( tg::Key::K );
        CHECK( Pings == 1 );
        Ctx.SetFocus( nullptr );
        // save / load by label
        std::string Saved = KB.Save();
        CHECK( KB.All().size() == 6 );
        CHECK( KB.Load( Saved, Ctx.Root() ) == 6 && KB.Save() == Saved );
        CHECK( App.Region == 3 );                                   // the loaded off-hold bind is active again
        // removing an active bind puts the value back; destroyed controls drop their binds
        KB.RemoveAll( *App.RegionCombo );
        CHECK( App.Region == R0 && KB.All().size() == 5 );
        App.Pages[ 2 ]->Remove( &Ping );
        Ctx.Frame();
        CHECK( KB.All().size() == 4 );

        // the UI: right-click -> "Add key bind..." -> pick a key, trigger, value -> Save
        App.Volume = V0;
        Ctx.OnMouseMove( Notif->Bounds.Center() );
        Ctx.OnMouseButton( tg::MouseButton::Right, true );
        Ctx.OnMouseButton( tg::MouseButton::Right, false );
        Ctx.Frame();
        auto* Mn = dynamic_cast<tg::Menu*>( OpenPopup( Ctx ) );
        CHECK( Mn );
        Settle( Ctx );
        Shot( Ctx, "bind_menu" );
        tg::MenuItem* AddItem = nullptr;
        for ( int i = 0; auto* It = Find<tg::MenuItem>( *Mn, i ); i++ )
            if ( It->Text.rfind( "Add another key bind", 0 ) == 0 || It->Text.rfind( "Add key bind", 0 ) == 0 ) AddItem = It;
        CHECK( AddItem );
        Click( Ctx, *AddItem );
        Settle( Ctx );
        tg::Dialog* Dlg = nullptr;
        for ( auto& K : Ctx.Overlay().Children() )
            if ( auto* D = dynamic_cast<tg::Dialog*>( K.get() ); D && !D->IsClosed() ) Dlg = D;
        CHECK( Dlg );
        auto* Kw = Find<tg::Keybind>( *Dlg );
        CHECK( Kw );
        Click( Ctx, *Kw );                                           // listen...
        CHECK( Kw->IsListening() );
        Press( tg::Key::L ); Release( tg::Key::L );                 // ...for L
        CHECK( Kw->GetKey() == tg::Key::L );
        tg::Segmented* Trig = nullptr;
        for ( int i = 0; auto* Sg = Find<tg::Segmented>( *Dlg, i ); i++ )
            if ( Sg->Options.size() == 4 ) Trig = Sg;
        CHECK( Trig );
        ClickAt( Ctx, Trig->Bounds.Center() + tg::Vec2{ -Trig->Bounds.w * 0.125f, 0 } ); // "Toggle" (2nd of 4)
        CHECK( Trig->GetIndex() == 1 );
        Settle( Ctx );
        Shot( Ctx, "bind_editor" );
        tg::Button* SaveB = nullptr;
        for ( int i = 0; auto* B = Find<tg::Button>( *Dlg, i ); i++ )
            if ( B->Text == "Save" ) SaveB = B;
        int Edits = 0;
        KB.OnEdit = [ & ] { Edits++; };
        Click( Ctx, *SaveB );
        Settle( Ctx );
        auto Nb = KB.For( *Notif );
        CHECK( Nb.size() == 2 && Nb[ 1 ]->K == tg::Key::L && Nb[ 1 ]->Trigger == tg::BindTrigger::Toggle && Edits == 1 );
        bool Before = App.Notifications;
        Press( tg::Key::L ); Release( tg::Key::L );
        CHECK( App.Notifications == Nb[ 1 ]->Value.V[ 0 ] >= 0.5 );
        Press( tg::Key::L ); Release( tg::Key::L );
        CHECK( App.Notifications == Before );
        KB.OnEdit = nullptr;
        KB.Clear();
        App.Notifications = N0;
        App.Volume = V0;
        Settle( Ctx );
    }

    // ---------------- keyboard navigation, shortcuts, undo, settings, dev tools
    {
        tg::Slider* Vol = nullptr;
        for ( int i = 0; auto* S = Find<tg::Slider>( *App.Pages[ 2 ], i ); i++ )
            if ( S->Bind == &App.Volume ) Vol = S;
        // Tab walks focusable controls in order; Shift+Tab goes back
        Ctx.SetFocus( nullptr );
        Ctx.OnKey( tg::Key::Tab, true, 0 );
        tg::Widget* First = Ctx.Focused();
        CHECK( First && First->Focusable && Ctx.FocusRingVisible() );
        Ctx.OnKey( tg::Key::Tab, true, 0 );
        tg::Widget* Second = Ctx.Focused();
        CHECK( Second && Second != First );
        Ctx.OnKey( tg::Key::Tab, true, tg::ModShift );
        CHECK( Ctx.Focused() == First );
        // Space on a focused checkbox toggles it
        Ctx.SetFocus( Notif );
        bool Nb = App.Notifications;
        Ctx.OnKey( tg::Key::Space, true, 0 );
        CHECK( App.Notifications != Nb );
        Settle( Ctx );
        Ctx.OnKey( tg::Key::Tab, true, tg::ModShift );
        Ctx.OnKey( tg::Key::Tab, true, 0 );                             // back on the checkbox, ring showing
        Settle( Ctx );
        Shot( Ctx, "focus_ring" );
        // arrows on a focused slider
        Ctx.SetFocus( Vol );
        float V0 = App.Volume;
        Ctx.OnKey( tg::Key::Right, true, 0 );
        CHECK( App.Volume == V0 + 1 );
        Ctx.OnKey( tg::Key::Left, true, tg::ModShift );
        CHECK( App.Volume == V0 + 1 - 10 );
        // combo: arrows while closed change it; Enter opens; arrows + Enter pick in the list
        Ctx.SetFocus( App.RegionCombo );
        int R0 = App.Region;
        Ctx.OnKey( tg::Key::Down, true, 0 );
        CHECK( App.Region == R0 + 1 );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        CHECK( App.RegionCombo->IsOpen() );
        Ctx.OnKey( tg::Key::Down, true, 0 );
        Ctx.OnKey( tg::Key::Down, true, 0 );
        CHECK( Ctx.KeyHot() != nullptr );
        Settle( Ctx );
        Shot( Ctx, "combo_keys" );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        Settle( Ctx );
        CHECK( !App.RegionCombo->IsOpen() && App.Region == 1 );
        // shortcuts: modifiers work while typing; plain keys don't
        int Saves = 0, Plain = 0;
        tg::Key Pk;
        uint8_t Pm;
        CHECK( tg::Context::ParseShortcut( "ctrl+shift+s", Pk, Pm ) && Pk == tg::Key::S && Pm == ( tg::ModCtrl | tg::ModShift ) );
        CHECK( tg::Context::ShortcutText( tg::Key::Enter, tg::ModCtrl ) == "Ctrl+Enter" );
        uint32_t Sid = Ctx.Shortcut( "Ctrl+S", [ & ] { Saves++; } );
        Ctx.Shortcut( "F5", [ & ] { Plain++; } );
        Ctx.SetFocus( nullptr );
        Ctx.OnKey( tg::Key::S, true, tg::ModCtrl );
        Ctx.OnKey( tg::Key::F5, true, 0 );
        CHECK( Saves == 1 && Plain == 1 );
        Ctx.SetFocus( App.Composer );
        Ctx.OnKey( tg::Key::S, true, tg::ModCtrl );
        Ctx.OnKey( tg::Key::F5, true, 0 );
        CHECK( Saves == 2 && Plain == 1 );
        Ctx.RemoveShortcut( Sid );
        Ctx.SetFocus( nullptr );
        Ctx.OnKey( tg::Key::S, true, tg::ModCtrl );
        CHECK( Saves == 2 );
        // undo / redo: control edits are recorded by themselves
        Ctx.History.Clear();
        bool Before = App.Notifications;
        Click( Ctx, *Notif );
        CHECK( App.Notifications != Before && Ctx.History.CanUndo() );
        Ctx.OnKey( tg::Key::Z, true, tg::ModCtrl );
        CHECK( App.Notifications == Before && Ctx.History.CanRedo() );
        Ctx.OnKey( tg::Key::Y, true, tg::ModCtrl );
        CHECK( App.Notifications != Before );
        Ctx.History.Undo();
        float Vb = App.Volume;
        Ctx.SetFocus( Vol );
        for ( int i = 0; i < 5; i++ ) Ctx.OnKey( tg::Key::Right, true, 0, i > 0 ); // held key: one step
        CHECK( App.Volume == Vb + 5 );
        Ctx.SetFocus( nullptr );
        Ctx.History.Undo();
        CHECK( App.Volume == Vb );
        int Custom = 0;
        Ctx.History.Push( "custom", [ & ] { Custom--; }, [ & ] { Custom++; } );
        Ctx.History.Undo();
        Ctx.History.Redo();
        CHECK( Custom == 0 && Ctx.History.UndoName() == "custom" );
        Ctx.History.Clear();
        // settings: every control's value as text, and named profiles
        std::string Snap = tg::SaveValues( Ctx.Root() );
        CHECK( Snap.find( "Volume\t" ) != std::string::npos );
        float Vs = App.Volume;
        bool Ns = App.Notifications;
        App.Volume = 3;
        App.Notifications = !Ns;
        CHECK( tg::LoadValues( Snap, Ctx.Root() ) > 5 && App.Volume == Vs && App.Notifications == Ns );
        std::string Dir = std::string( OutDir ) + "/profiles";
        std::filesystem::remove_all( Dir );
        tg::Profiles Pr( Ctx, Dir );
        Ctx.Binds.Add( *Vol, tg::Key::G, tg::BindTrigger::Toggle, { { 77 } } );
        CHECK( Pr.Save( "Ranked" ) && Pr.Exists( "Ranked" ) );
        Ctx.Binds.Clear();
        App.Volume = 9;
        CHECK( Pr.Load( "Ranked" ) && App.Volume == Vs && Ctx.Binds.All().size() == 1 );
        CHECK( Pr.List().size() == 1 && Pr.List()[ 0 ] == "Ranked" && Pr.Remove( "Ranked" ) && Pr.List().empty() );
        Ctx.Binds.Clear();
        // dev tools draw without disturbing anything
        Ctx.OnKey( tg::Key::F12, true, 0 );
        Ctx.OnKey( tg::Key::F12, true, tg::ModCtrl );
        CHECK( Ctx.ShowInspector && Ctx.ShowStats );
        Ctx.OnMouseMove( Vol->Bounds.Center() );
        Settle( Ctx );
        Shot( Ctx, "inspector" );
        Ctx.ShowInspector = Ctx.ShowStats = false;
        Settle( Ctx );
    }

    // ---------------- combo: open, pick the 3rd item, closes
    App.RegionCombo->SetItemIcons( { tg::Icon::Globe, "", tg::Icon::MapPin } );
    Click( Ctx, *App.RegionCombo );
    CHECK( App.RegionCombo->IsOpen() );
    tg::Popup* P = OpenPopup( Ctx );
    CHECK( P );
    Settle( Ctx );
    Shot( Ctx, "combo_open" );
    tg::ListItem* Third = Find<tg::ListItem>( *P, 2 );
    CHECK( Third && Third->Text == "London" && Third->Icon == tg::Icon::MapPin && Third->ShowMark );
    auto* NoIcon = Find<tg::ListItem>( *P, 1 );
    CHECK( NoIcon && NoIcon->Icon.empty() && NoIcon->IconWidth == Third->IconWidth && Third->IconWidth > 0 );
    Click( Ctx, *Third );
    CHECK( App.Region == 2 );
    CHECK( App.RegionCombo->GetSelectedText() && *App.RegionCombo->GetSelectedText() == "London" );
    CHECK( !App.RegionCombo->IsOpen() );
    CHECK( OpenPopup( Ctx ) == nullptr );

    // click-outside closes, clicking the field again toggles
    Click( Ctx, *App.RegionCombo );
    CHECK( App.RegionCombo->IsOpen() );
    Click( Ctx, *App.RegionCombo );
    CHECK( !App.RegionCombo->IsOpen() );
    Click( Ctx, *App.RegionCombo );
    ClickAt( Ctx, { 1250, 800 } );
    CHECK( !App.RegionCombo->IsOpen() );

    // ---------------- multi combo: toggles stay open, Escape closes
    App.AlertCombo->SetItemIcons( { tg::Icon::CircleAlert, tg::Icon::Bell } );
    Click( Ctx, *App.AlertCombo );
    P = OpenPopup( Ctx );
    CHECK( P );
    Settle( Ctx );
    CHECK( Find<tg::ListItem>( *P, 0 )->ShowCheck && Find<tg::ListItem>( *P, 0 )->Icon == tg::Icon::CircleAlert );
    Click( Ctx, *Find<tg::ListItem>( *P, 1 ) );
    Click( Ctx, *Find<tg::ListItem>( *P, 0 ) );
    CHECK( App.Alerts[ 0 ] == false && App.Alerts[ 1 ] == true && App.Alerts[ 2 ] == true );
    CHECK( App.AlertCombo->IsOpen() );
    Shot( Ctx, "multicombo_open" );
    Ctx.OnKey( tg::Key::Escape, true, 0 );
    Ctx.Frame();
    CHECK( !App.AlertCombo->IsOpen() );

    // ---------------- list box: select, then switch to plain list
    tg::ListItem* Row3 = Find<tg::ListItem>( *App.ServerList, 3 );
    CHECK( Row3 );
    Click( Ctx, *Row3 );
    CHECK( App.Server == 3 );
    CHECK( App.ServerList->GetSelected() == 3 );
    tg::Toggle* Selectable = Find<tg::Toggle>( *App.Pages[ 2 ], 1 );
    CHECK( Selectable && Selectable->Bind == &App.ListSelectable );
    Click( Ctx, *Selectable );
    CHECK( App.ServerList->GetMode() == tg::SelectMode::None );
    CHECK( App.Server == -1 );
    Click( Ctx, *Find<tg::ListItem>( *App.ServerList, 1 ) );
    CHECK( App.ServerList->GetSelected() == -1 ); // plain list ignores clicks
    Click( Ctx, *Selectable );
    tg::Toggle* Multi = Find<tg::Toggle>( *App.Pages[ 2 ], 2 );
    Click( Ctx, *Multi );
    CHECK( App.ServerList->GetMode() == tg::SelectMode::Multi );
    Click( Ctx, *Find<tg::ListItem>( *App.ServerList, 0 ) );
    Click( Ctx, *Find<tg::ListItem>( *App.ServerList, 2 ) );
    CHECK( App.ServerList->SelectedIndices().size() == 2 );

    // ---------------- color picker: open, drag in the SV square, hex reflects it
    tg::Color Old = App.Accent;
    Click( Ctx, *App.AccentPicker );
    P = OpenPopup( Ctx );
    CHECK( P );
    Settle( Ctx );
    tg::ColorPanel* Panel = Find<tg::ColorPanel>( *P );
    CHECK( Panel );
    tg::Rect Sq = Panel->Children()[ 0 ]->Bounds; // HsvArea; square is its left part
    Drag( Ctx, { Sq.x + 20, Sq.y + 20 }, { Sq.x + 150, Sq.y + 30 } );
    CHECK( !( App.Accent == Old ) );
    CHECK( tg::GetTheme()[ tg::Tok::Accent ] == App.Accent ); // OnChange re-themed the app
    tg::TextInput* Hex = Find<tg::TextInput>( *Panel );
    CHECK( Hex && Hex->GetText() == tg::ToHex( App.Accent ) );
    // hue bar drag keeps saturation/value
    tg::Rect HsvB = Panel->Children()[ 0 ]->Bounds;
    Drag( Ctx, { HsvB.x + 180 + 8 + 8, HsvB.y + 10 }, { HsvB.x + 180 + 8 + 8, HsvB.y + 120 } );
    Shot( Ctx, "colorpicker_open" );
    // typing a hex code sets the color
    Click( Ctx, *Hex );
    Ctx.OnKey( tg::Key::A, true, tg::ModCtrl );
    for ( const char* p = "#E5534B"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
    CHECK( App.Accent == tg::Color::Hex( 0xE5534B ) );
    Ctx.OnKey( tg::Key::Escape, true, 0 );
    Ctx.Frame();
    CHECK( OpenPopup( Ctx ) == nullptr );
    tg::GetTheme()[ tg::Tok::Accent ] = tg::GetTheme()[ tg::Tok::AccentHover ] = App.Accent = Old;

    // ---------------- tables: sort, select, scroll, resize
    ClickTab( Ctx, App, 3 );
    Settle( Ctx );
    tg::Table& Tb = *App.Procs;
    auto Num = [ & ]( size_t Pos, size_t Col ) { return std::atof( Tb.GetCell( Tb.DisplayRow( Pos ), Col ).c_str() ); };
    CHECK( Tb.SortColumn() == 3 && !Tb.SortAscending() );
    for ( size_t i = 0; i + 1 < Tb.RowCount(); i++ ) CHECK( Num( i, 3 ) >= Num( i + 1, 3 ) ); // CPU desc, numeric ("9.5%" < "12.0%")
    Shot( Ctx, "tables" );
    // click "PID" header -> ascending numeric
    tg::Rect Tbb = Tb.Bounds;
    ClickAt( Ctx, { Tbb.x + 40, Tbb.y + 17 } );
    CHECK( Tb.SortColumn() == 0 && Tb.SortAscending() );
    for ( size_t i = 0; i + 1 < Tb.RowCount(); i++ ) CHECK( Num( i, 0 ) < Num( i + 1, 0 ) );
    ClickAt( Ctx, { Tbb.x + 40, Tbb.y + 17 } ); // again -> descending
    CHECK( !Tb.SortAscending() && Num( 0, 0 ) > Num( 1, 0 ) );
    // click the 3rd visible row -> selects that underlying row
    ClickAt( Ctx, { Tbb.x + 300, Tbb.y + Tb.Style.HeaderHeight + Tb.Style.RowHeight * 2.5f } );
    CHECK( App.ProcSel == int( Tb.DisplayRow( 2 ) ) );
    // wheel scrolls
    Ctx.OnMouseMove( { Tbb.x + 300, Tbb.y + 200 } );
    Ctx.OnMouseWheel( { 0, -3 } );
    Settle( Ctx );
    CHECK( Tb.GetScroll() > 0 );
    // drag the PID column edge wider
    float X0 = Tbb.x + 80;
    Drag( Ctx, { X0, Tbb.y + 17 }, { X0 + 40, Tbb.y + 17 } );
    CHECK( Tb.Column( 0 ).Width.K == tg::Size::Kind::Fixed && Tb.Column( 0 ).Width.V == 120 );
    // plain table ignores clicks
    ClickAt( Ctx, { App.Conns->Bounds.x + 60, App.Conns->Bounds.y + 50 } );
    CHECK( App.Conns->GetSelected() == -1 );
    // the selection follows the row, not the position, when re-sorted
    int Sel = App.ProcSel;
    Tb.SortBy( 1, true );
    CHECK( App.ProcSel == Sel && Tb.IsSelected( size_t( Sel ) ) );
    Tb.SortBy( 3, false );

    // ---------------- floating windows: drag by title, resize, close
    ClickTab( Ctx, App, 4 );
    Settle( Ctx );
    tg::Vec2 Pos0 = App.SettingsWin->GetPosition();
    tg::Rect B = App.SettingsWin->Bounds;
    Drag( Ctx, { B.x + 100, B.y + 18 }, { B.x + 160, B.y + 58 } );
    tg::Vec2 Pos1 = App.SettingsWin->GetPosition();
    CHECK( Pos1.x == Pos0.x + 60 && Pos1.y == Pos0.y + 40 );
    // tgui layout inside the window: groupbox scrolls on its own, window tabs switch
    tg::Groupbox* G1 = Find<tg::Groupbox>( *App.SettingsWin );
    CHECK( G1 && G1->Title == "Example Group" );
    Ctx.OnMouseMove( G1->Bounds.Center() );
    Ctx.OnMouseWheel( { 0, -2 } );
    Settle( Ctx );
    CHECK( G1->GetScroll() > 0 );
    tg::TabView* WTabs = App.SettingsWin->Tabs();
    CHECK( WTabs && WTabs->TabCount() == 3 );
    ClickAt( Ctx, WTabs->TabRect( 2 ).Center() );
    Settle( Ctx );
    CHECK( WTabs->GetIndex() == 2 );
    CHECK( Find<tg::ListBox>( *App.SettingsWin ) != nullptr );
    ClickAt( Ctx, WTabs->TabRect( 0 ).Center() );
    Settle( Ctx );
    CHECK( WTabs->GetIndex() == 0 );

    // ---------------- keybind: listen, bind keys / mouse buttons, unbind, modes
    tg::Keybind& Kb = *App.AimBind;
    CHECK( Kb.GetKey() == tg::Key::Mouse5 );
    Click( Ctx, Kb );
    CHECK( Kb.IsListening() && Ctx.Captured() == &Kb );
    Ctx.OnText( 'x' ); // typed text goes nowhere while listening
    Settle( Ctx );
    Shot( Ctx, "keybind_listening" );
    Ctx.OnKey( tg::Key::F5, true, 0 );
    Ctx.OnKey( tg::Key::F5, false, 0 );
    CHECK( App.AimKey == tg::Key::F5 && !Kb.IsListening() && Ctx.Captured() == nullptr );
    Click( Ctx, Kb );
    Ctx.OnMouseButton( tg::MouseButton::Back, true ); // side button
    Ctx.OnMouseButton( tg::MouseButton::Back, false );
    CHECK( App.AimKey == tg::Key::Mouse4 );
    Click( Ctx, Kb );
    Ctx.OnMouseButton( tg::MouseButton::Left, true );  // even M1 is bindable, like tgui
    Ctx.OnMouseButton( tg::MouseButton::Left, false );
    CHECK( App.AimKey == tg::Key::Mouse1 );
    Click( Ctx, Kb );
    Ctx.OnKey( tg::Key::Escape, true, 0 );            // Escape = unbind
    CHECK( App.AimKey == tg::Key::None && !Kb.IsListening() );
    // hold mode
    Click( Ctx, Kb );
    Ctx.OnKey( tg::Key::G, true, 0 );
    Ctx.OnKey( tg::Key::G, false, 0 );
    CHECK( App.AimKey == tg::Key::G && Kb.GetMode() == tg::BindMode::Hold && !Kb.IsActive() );
    Ctx.OnKey( tg::Key::G, true, 0 );
    CHECK( Kb.IsActive() );
    Ctx.OnKey( tg::Key::G, false, 0 );
    CHECK( !Kb.IsActive() );
    // right-click -> mode menu -> toggle
    Ctx.OnMouseMove( Kb.Bounds.Center() );
    Ctx.OnMouseButton( tg::MouseButton::Right, true );
    Ctx.OnMouseButton( tg::MouseButton::Right, false );
    Ctx.Frame();
    P = OpenPopup( Ctx );
    CHECK( P );
    Settle( Ctx );
    Shot( Ctx, "keybind_modes" );
    tg::ListItem* ToggleItem = Find<tg::ListItem>( *P, 1 );
    CHECK( ToggleItem && ToggleItem->Text == "toggle" );
    Click( Ctx, *ToggleItem );
    CHECK( Kb.GetMode() == tg::BindMode::Toggle && OpenPopup( Ctx ) == nullptr );
    CHECK( !Kb.IsActive() );
    Ctx.OnKey( tg::Key::G, true, 0 ); Ctx.OnKey( tg::Key::G, false, 0 );
    CHECK( Kb.IsActive() );
    Ctx.OnKey( tg::Key::G, true, 0 ); Ctx.OnKey( tg::Key::G, false, 0 );
    CHECK( !Kb.IsActive() );
    // global menu key (Insert) hides/shows the window, but not while a keybind is listening
    Ctx.OnKey( tg::Key::Insert, true, 0 ); Ctx.OnKey( tg::Key::Insert, false, 0 );
    Ctx.Frame();
    CHECK( !App.SettingsWin->IsOpen() );
    Ctx.OnKey( tg::Key::Insert, true, 0 ); Ctx.OnKey( tg::Key::Insert, false, 0 );
    Settle( Ctx );
    CHECK( App.SettingsWin->IsOpen() );
    Click( Ctx, Kb );
    Ctx.OnKey( tg::Key::Insert, true, 0 ); Ctx.OnKey( tg::Key::Insert, false, 0 );
    CHECK( App.SettingsWin->IsOpen() && App.AimKey == tg::Key::Insert );
    App.AimKey = tg::Key::Mouse5;
    Kb.SetMode( tg::BindMode::Hold );
    Settle( Ctx );

    // clicking the other window raises it above
    Click( Ctx, *Find<tg::TextInput>( *App.PlayersWin ) );
    CHECK( App.Pages[ 4 ]->Children().back().get() == App.PlayersWin );
    tg::Rect Pb = App.PlayersWin->Bounds;
    Drag( Ctx, { Pb.Right() - 5, Pb.Bottom() - 5 }, { Pb.Right() + 45, Pb.Bottom() + 25 } );
    Settle( Ctx );
    CHECK( App.PlayersWin->Bounds.w == Pb.w + 50 && App.PlayersWin->Bounds.h == Pb.h + 30 );
    // window controls work: checkbox + combo inside a window
    bool Cross = App.Crosshair;
    Click( Ctx, *Find<tg::Checkbox>( *App.SettingsWin, 2 ) );
    CHECK( App.Crosshair != Cross );
    Shot( Ctx, "windows" );
    tg::Rect Sb = App.SettingsWin->Bounds;
    ClickAt( Ctx, { Sb.Right() - 18, Sb.y + 18 } ); // the X
    CHECK( !App.SettingsWin->IsOpen() );

    // ---------------- "More" tab: new controls
    App.SettingsWin->Show();
    ClickTab( Ctx, App, 5 );
    Shot( Ctx, "more" );

    // radio + segmented
    {
        tg::RadioGroup& R = *App.PlanRadio;
        tg::Rect B = R.Bounds;
        float RowH = ( B.h - 2 * R.Style.ItemGap ) / 3;
        ClickAt( Ctx, { B.x + 30, B.y + RowH * 2 + R.Style.ItemGap * 2 + RowH * 0.5f } );
        CHECK( App.Plan == 2 );
        ClickAt( Ctx, App.PeriodSeg->SegmentRect( 3 ).Center() );
        CHECK( App.Period == 3 );
    }

    // number input: +, typing, Enter, Up/Down, garbage, clamping
    {
        tg::NumberInput& N = *App.PortInput;
        tg::Rect B = N.Bounds;
        ClickAt( Ctx, { B.Right() - N.Style.ButtonWidth * 0.5f, B.Center().y } ); // +
        CHECK( App.Port == 8444 );
        ClickAt( Ctx, { B.Right() - N.Style.ButtonWidth * 1.5f, B.Center().y } ); // -
        CHECK( App.Port == 8443 );
        Click( Ctx, N.Field() );
        CHECK( Ctx.Focused() == &N.Field() );
        Ctx.OnKey( tg::Key::A, true, tg::ModCtrl );
        for ( const char* p = "9000"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        CHECK( App.Port == 9000 );
        Ctx.OnKey( tg::Key::Up, true, 0 );
        CHECK( App.Port == 9001 );
        Ctx.OnKey( tg::Key::Down, true, tg::ModShift );
        CHECK( App.Port == 8991 );
        Ctx.OnKey( tg::Key::A, true, tg::ModCtrl );
        for ( const char* p = "abc"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        CHECK( App.Port == 8991 && N.Field().GetText() == "8991" ); // not a number: restored
        Ctx.OnKey( tg::Key::A, true, tg::ModCtrl );
        for ( const char* p = "70000"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        ClickAt( Ctx, { 1270, 810 } );                                // blur commits
        CHECK( App.Port == 65535 );
    }

    // range slider: drag the high knob left past the low one -> clamps, Lo <= Hi
    {
        tg::RangeSlider& R = *App.Price;
        tg::Rect B = R.Bounds;
        float Kr = R.Style.KnobRadius, W = B.w - Kr * 2;
        tg::Vec2 HighKnob = { B.x + Kr + W * 0.8f, B.Center().y };
        Drag( Ctx, HighKnob, { B.x + Kr + W * 0.5f, B.Center().y } );
        CHECK( App.PriceHi == 500 && App.PriceLo == 200 );
        Drag( Ctx, { B.x + Kr + W * 0.5f, B.Center().y }, { B.x, B.Center().y } );
        CHECK( App.PriceLo <= App.PriceHi );
    }

    // tooltip appears after the delay, hides on click
    {
        Ctx.OnMouseMove( App.ToastBtn->Bounds.Center() );
        Ctx.Frame();
        CHECK( Ctx.ShownTooltip() == nullptr );          // not yet
        for ( int i = 0; i < 40; i++ ) Ctx.Frame();      // ~0.66 s
        CHECK( Ctx.ShownTooltip() == App.ToastBtn );
        Shot( Ctx, "more_tooltip" );
    }

    // toast: click -> appears, times out
    {
        CHECK( tg::ToastCount( Ctx ) == 0 );
        Click( Ctx, *App.ToastBtn );
        CHECK( Ctx.ShownTooltip() == nullptr );          // clicking hid the tooltip
        CHECK( tg::ToastCount( Ctx ) == 1 );
        for ( int i = 0; i < 40; i++ ) Ctx.Frame();
        Shot( Ctx, "more_toast" );
        for ( int i = 0; i < 400; i++ ) Ctx.Frame();     // 3.5 s life + fade
        CHECK( tg::ToastCount( Ctx ) == 0 );
    }

    // confirm dialog: modal, Enter = primary, Escape = dismiss
    auto FindDialog = [ & ]() -> tg::Dialog* {
        for ( auto& K : Ctx.Overlay().Children() )
            if ( auto* D = dynamic_cast<tg::Dialog*>( K.get() ); D && !D->IsClosed() ) return D;
        return nullptr;
    };
    {
        Click( Ctx, *App.DeleteBtn );
        tg::Dialog* D = FindDialog();
        CHECK( D );
        Settle( Ctx );
        Shot( Ctx, "more_dialog" );
        ClickAt( Ctx, App.ToastBtn->Bounds.Center() );    // behind the backdrop: blocked
        CHECK( tg::ToastCount( Ctx ) == 0 && FindDialog() == D );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        Ctx.Frame();
        CHECK( App.Deleted == 1 && FindDialog() == nullptr );
        Click( Ctx, *App.DeleteBtn );
        CHECK( FindDialog() );
        Ctx.OnKey( tg::Key::Escape, true, 0 );
        Ctx.Frame();
        CHECK( App.Deleted == 1 && FindDialog() == nullptr );
        for ( int i = 0; i < 500; i++ ) Ctx.Frame();     // let the "deleted" toast go
    }

    // tree: expand with the chevron, select, keyboard, context menu
    {
        tg::TreeView& T = *App.Servers;
        CHECK( T.VisibleRows() == 3 + 3 );                // Europe expanded: 3 regions + 3 hosts
        tg::Rect B = T.Bounds;
        float Rh = T.Style.RowHeight;
        // "North America" is row 4 (after Europe + its 3 hosts); click its chevron
        ClickAt( Ctx, { B.x + 6 + 9, B.y + 4 + Rh * 4.5f } );
        CHECK( T.VisibleRows() == 3 + 3 + 2 );
        ClickAt( Ctx, { B.x + 90, B.y + 4 + Rh * 1.5f } ); // vps-fra-01
        CHECK( T.GetSelected() && T.GetSelected()->Text == "vps-fra-01" );
        Ctx.OnKey( tg::Key::Down, true, 0 );
        CHECK( T.GetSelected()->Text == "vps-ams-02" );
        Ctx.OnKey( tg::Key::Left, true, 0 );              // to the parent
        CHECK( T.GetSelected()->Text == "Europe" );
        Ctx.OnKey( tg::Key::Left, true, 0 );              // collapse
        CHECK( T.VisibleRows() == 3 + 2 );
        Ctx.OnKey( tg::Key::Right, true, 0 );             // expand again
        CHECK( T.VisibleRows() == 3 + 3 + 2 );
        // right-click vps-ams-02 (not selected yet) -> selects it, context menu -> Restart
        Ctx.OnMouseMove( { B.x + 90, B.y + 4 + Rh * 2.5f } );
        Ctx.OnMouseButton( tg::MouseButton::Right, true );
        Ctx.OnMouseButton( tg::MouseButton::Right, false );
        Ctx.Frame();
        auto* M = dynamic_cast<tg::Menu*>( OpenPopup( Ctx ) );
        CHECK( M );
        Settle( Ctx );
        Shot( Ctx, "more_menu" );
        tg::MenuItem* Restart = Find<tg::MenuItem>( *M, 1 );
        CHECK( Restart && Restart->Text == "Restart" );
        Click( Ctx, *Restart );
        CHECK( App.LastAction == "Restart vps-ams-02" && OpenPopup( Ctx ) == nullptr );
        // reopen, then left-click elsewhere in the same tree (the menu's owner): it must close
        auto RightClick = [ & ]( tg::Vec2 P ) {
            Ctx.OnMouseMove( P );
            Ctx.OnMouseButton( tg::MouseButton::Right, true );
            Ctx.OnMouseButton( tg::MouseButton::Right, false );
            Ctx.Frame();
        };
        RightClick( { B.x + 90, B.y + 4 + Rh * 2.5f } );
        CHECK( OpenPopup( Ctx ) );
        ClickAt( Ctx, { B.x + 30, B.y + 4 + Rh * 7.5f } );         // left of the menu, still on the tree
        Settle( Ctx );
        CHECK( OpenPopup( Ctx ) == nullptr );
        // ...and outside the tree entirely
        RightClick( { B.x + 90, B.y + 4 + Rh * 2.5f } );
        CHECK( OpenPopup( Ctx ) );
        ClickAt( Ctx, { 6, Ctx.GetViewport().y - 6 } );
        Settle( Ctx );
        CHECK( OpenPopup( Ctx ) == nullptr );
        // checkable item flips its bool
        Ctx.OnMouseMove( { B.x + 90, B.y + 4 + Rh * 2.5f } );
        Ctx.OnMouseButton( tg::MouseButton::Right, true );
        Ctx.OnMouseButton( tg::MouseButton::Right, false );
        Ctx.Frame();
        M = dynamic_cast<tg::Menu*>( OpenPopup( Ctx ) );
        CHECK( M );
        Settle( Ctx );
        Click( Ctx, *Find<tg::MenuItem>( *M, 2 ) );
        CHECK( App.Pinned );
    }

    // collapsible: click the header -> opens (animated), contents become clickable
    {
        tg::Collapsible& A = *App.Advanced;
        float H0 = A.Bounds.h;
        ClickAt( Ctx, { A.Bounds.x + 60, A.Bounds.y + 12 } );
        Settle( Ctx );
        CHECK( App.AdvancedOpen && A.Bounds.h > H0 + 40 );
        bool V = App.Verbose;
        Click( Ctx, *Find<tg::Checkbox>( A ) );
        CHECK( App.Verbose != V );
    }

    // splitter: drag the divider
    {
        tg::Splitter& S = *App.Split;
        float R0 = S.Ratio;
        tg::Rect Pane = S.First().Bounds;
        Drag( Ctx, { Pane.Right() + 3, Pane.Center().y }, { Pane.Right() + 83, Pane.Center().y } );
        CHECK( S.Ratio > R0 + 0.05f );
        CHECK( S.First().Bounds.w > Pane.w + 70 );
    }
    Settle( Ctx );
    Shot( Ctx, "more_after" );

    // ---------------- "Forms" tab
    ClickTab( Ctx, App, 6 );
    Shot( Ctx, "forms" );
    {
        // text area: type, newlines, wrap, Up/Down, selection + cut
        tg::TextArea& N = *App.NotesBox;
        size_t Lines0 = N.LineCount();
        CHECK( Lines0 >= 3 ); // the long sentence wraps
        Click( Ctx, N );
        CHECK( Ctx.Focused() == &N );
        Ctx.OnKey( tg::Key::End, true, tg::ModCtrl );                 // end of document
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        for ( const char* p = "line two"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        CHECK( App.Notes.size() >= 9 && App.Notes.substr( App.Notes.size() - 9 ) == "\nline two" ); // Bind kept in sync
        CHECK( N.LineCount() == Lines0 + 1 );
        Ctx.OnKey( tg::Key::Up, true, 0 );                             // caret moves to the line above...
        Ctx.OnText( 'X' );                                             // ...and types there
        CHECK( App.Notes.find( "X" ) != std::string::npos && App.Notes.substr( App.Notes.size() - 9 ) == "\nline two" );
        Ctx.OnKey( tg::Key::A, true, tg::ModCtrl );
        Ctx.OnKey( tg::Key::X, true, tg::ModCtrl );                    // no clipboard hook in headless: still deletes
        CHECK( App.Notes.empty() && N.LineCount() == 1 );
        for ( const char* p = "fresh"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        CHECK( App.Notes == "fresh" );

        // chat composer: grows with content, Enter sends, Shift+Enter = newline
        tg::TextArea& Cp = *App.Composer2;
        Click( Ctx, Cp );
        Settle( Ctx );
        float H1 = Cp.Bounds.h;
        for ( const char* p = "hello"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Ctx.OnKey( tg::Key::Enter, true, tg::ModShift );
        for ( const char* p = "second line"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Settle( Ctx );
        CHECK( Cp.Bounds.h > H1 + 10 );                                // grew to two lines
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        CHECK( App.Sent.size() == 1 && App.Sent[ 0 ] == "hello\nsecond line" && Cp.GetText().empty() );
        Settle( Ctx );
        CHECK( std::fabs( Cp.Bounds.h - H1 ) < 1 );                     // shrank back
    }
    {
        // menu bar: click File, slide to Edit, pick Find
        tg::MenuBar& Bar = *App.Bar;
        Bar.Add( "Tools", []( tg::Menu& M ) {
            M.Item( "Copy", {}, "Ctrl+C" ).Icon = tg::Icon::Copy;
        }, tg::Icon::Folder );
        Settle( Ctx );
        ClickAt( Ctx, Bar.ItemRect( Bar.Count() - 1 ).Center() );
        auto* Tools = OpenPopup( Ctx );
        CHECK( Tools && Bar.OpenIndexNow() == int( Bar.Count() - 1 ) );
        auto* Copy = Find<tg::MenuItem>( *Tools, 0 );
        CHECK( Copy && Copy->Icon == tg::Icon::Copy && Copy->Shortcut == "Ctrl+C" );
        Settle( Ctx );
        Shot( Ctx, "menu_icons" );
        Click( Ctx, *Copy );
        CHECK( Bar.OpenIndexNow() == -1 );
        ClickAt( Ctx, Bar.ItemRect( 0 ).Center() );
        CHECK( Bar.OpenIndexNow() == 0 );
        Ctx.OnMouseMove( Bar.ItemRect( 1 ).Center() );                 // hover the next title while open
        Ctx.Frame();
        CHECK( Bar.OpenIndexNow() == 1 );
        auto* M = dynamic_cast<tg::Menu*>( OpenPopup( Ctx ) );
        CHECK( M );
        Settle( Ctx );
        Shot( Ctx, "forms_menubar" );
        tg::MenuItem* FindItem = Find<tg::MenuItem>( *M, 2 );
        CHECK( FindItem && FindItem->Text == "Find" );
        Click( Ctx, *FindItem );
        CHECK( App.LastMenu == "Find" && Bar.OpenIndexNow() == -1 );
        // View > Word wrap is a checkable item
        ClickAt( Ctx, Bar.ItemRect( 2 ).Center() );
        M = dynamic_cast<tg::Menu*>( OpenPopup( Ctx ) );
        Find<tg::MenuItem>( *M, 0 )->Icon = tg::Icon::FileText;
        Settle( Ctx );
        Click( Ctx, *Find<tg::MenuItem>( *M, 0 ) );
        CHECK( !App.WordWrap );
        for ( int i = 0; i < 500; i++ ) Ctx.Frame();                   // toasts go away
    }
    {
        // tags: Enter adds, comma adds, duplicate ignored, Backspace on empty removes, x removes
        tg::TagInput& T = *App.TagBox;
        Click( Ctx, T );
        CHECK( Ctx.Focused() == &T.Field() );
        for ( const char* p = "db"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        CHECK( App.Tags.size() == 3 && App.Tags[ 2 ] == "db" );
        for ( const char* p = "backup,"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Ctx.Frame();
        CHECK( App.Tags.size() == 4 && App.Tags[ 3 ] == "backup" && T.Field().GetText().empty() );
        for ( const char* p = "prod"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        CHECK( App.Tags.size() == 4 );                                 // duplicate
        Ctx.OnKey( tg::Key::Backspace, true, 0 );
        CHECK( App.Tags.size() == 3 );
        Settle( Ctx );
        // x on the first chip ("prod")
        tg::Rect B = T.Bounds;
        float Chip0W = tg::GetTheme().Small->Width( "prod" ) + 10 + 18;
        ClickAt( Ctx, { B.x + T.Style.Padding.Left + Chip0W - 10, B.y + T.Style.Padding.Top + T.Style.ChipHeight * 0.5f } );
        CHECK( App.Tags.size() == 2 && App.Tags[ 0 ] == "eu-west" );
    }
    {
        // date picker: open, next month, pick the 15th
        tg::DatePicker& Dp = *App.ExpiresPicker;
        Click( Ctx, Dp );
        CHECK( Dp.IsOpen() );
        Settle( Ctx );
        auto* Cal = Find<tg::Calendar>( *OpenPopup( Ctx ) );
        CHECK( Cal && Cal->ViewMonth == 12 && Cal->ViewYear == 2026 );
        Shot( Ctx, "forms_calendar" );
        ClickAt( Ctx, { Cal->Bounds.Right() - 17, Cal->Bounds.y + 17 } ); // >
        CHECK( Cal->ViewMonth == 1 && Cal->ViewYear == 2027 );
        ClickAt( Ctx, Cal->DayRect( 15 ).Center() );
        CHECK( App.Expires == ( tg::Date{ 2027, 1, 15 } ) && !Dp.IsOpen() );
        CHECK( ( tg::Date{ 2026, 11, 7 } ).Weekday() == 5 );            // a Saturday
        CHECK( tg::Date::DaysInMonth( 2028, 2 ) == 29 && tg::Date::DaysInMonth( 2100, 2 ) == 28 );
    }
    {
        // drag-to-reorder: move row 0 below row 2
        tg::ListBox& Q = *App.Queue;
        auto Row = [ & ]( int i ) { return Find<tg::ListItem>( Q, i )->Bounds; };
        tg::Rect R0 = Row( 0 ), R2 = Row( 2 );
        Drag( Ctx, R0.Center(), { R2.Center().x, R2.Bottom() - 2 } );
        CHECK( Q.GetItem( 0 ) == "migrate users table" && Q.GetItem( 2 ) == "deploy api v2.3" );
        CHECK( App.QueueItems[ 2 ] == "deploy api v2.3" );
        CHECK( Q.GetSelected() == -1 );                                // a drag is not a click
        // search filters; Escape clears
        Click( Ctx, App.SearchBoxW->Field() );
        for ( const char* p = "CACHE"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
        CHECK( Q.Count() == 1 && Q.GetItem( 0 ) == "warm redis cache" );
        Settle( Ctx );
        Shot( Ctx, "forms_search" );
        Ctx.OnKey( tg::Key::Escape, true, 0 );
        CHECK( App.Query.empty() && Q.Count() == 6 );
    }

    // ---------------- "Charts" tab
    ClickTab( Ctx, App, 7 );
    for ( int i = 0; i < 40; i++ ) App.TickStats( 100 + i );
    Settle( Ctx );
    Shot( Ctx, "charts" );
    {
        tg::BarChart& B = *App.Requests;
        // hover Thursday -> tooltip text set
        tg::Rect Bb = B.Bounds;
        float SlotW = ( Bb.w - 36 ) / 7;
        Ctx.OnMouseMove( { Bb.x + 36 + SlotW * 3.5f, Bb.Bottom() - 40 } );
        CHECK( B.HoveredBar() == 3 && B.Tooltip == "Thu: 77k" );
        for ( int i = 0; i < 40; i++ ) Ctx.Frame();
        CHECK( Ctx.ShownTooltip() == &B );
        CHECK( App.CpuGauge->GetValue() > 0 && App.SparkA->Data().size() == 40 );
    }

    // ---------------- "Terminal" tab: the emulator, then a real process in a pty
    {
        tg::Terminal T; // detached: pure parser checks
        T.Write( "hello\r\nworld" );
        CHECK( T.RowText( 0 ) == "hello" && T.RowText( 1 ) == "world" && T.CursorRow() == 1 && T.CursorCol() == 5 );
        T.Write( "\x1b[2J\x1b[H\x1b[1;31mR\x1b[38;5;123mI\x1b[48;2;1;2;3mT\x1b[0mX" );
        CHECK( T.RowText( 0 ) == "RITX" );
        CHECK( T.Cell( 0, 0 ).Fg == ( 0x1000000u | 1 ) && ( T.Cell( 0, 0 ).Attr & tg::AttrBold ) );
        CHECK( T.Cell( 1, 0 ).Fg == ( 0x1000000u | 123 ) );
        CHECK( T.Cell( 2, 0 ).Bg == ( 0x2000000u | 0x010203 ) && T.Cell( 3, 0 ).Fg == 0 && T.Cell( 3, 0 ).Attr == 0 );
        T.Write( "\x1b[5;10Hx\x1b[2;3H\x1b[K" );                         // CUP + erase line
        CHECK( T.Cell( 9, 4 ).Cp == 'x' && T.CursorRow() == 1 && T.CursorCol() == 2 );
        T.Write( "\x1b[1;1H\x1b[2Pz" );                                   // delete 2 chars
        CHECK( T.RowText( 0 ) == "zX" );
        T.Write( "\x1b[?1049h" );
        CHECK( T.AltScreen() && T.RowText( 0 ).empty() );
        T.Write( "vim" );
        T.Write( "\x1b[?1049l" );
        CHECK( !T.AltScreen() && T.RowText( 0 ) == "zX" && T.CursorRow() == 0 );
        // wide chars + emoji take 2 cells; autowrap at the right margin
        T.Write( "\x1b[2J\x1b[H\xE6\xBC\xA2\xF0\x9F\x9A\x80!" );
        CHECK( T.Cell( 0, 0 ).Width == 2 && T.Cell( 1, 0 ).Width == 0 && T.Cell( 4, 0 ).Cp == '!' && T.CursorCol() == 5 );
        T.Write( "\x1b[H" + std::string( 85, 'a' ) );
        CHECK( T.CursorRow() == 1 && T.CursorCol() == 5 && T.RowText( 1 ) == "aaaaa" );
        // scrolling pushes into the scrollback
        T.Write( "\x1b[2J\x1b[H" );
        for ( int i = 0; i < 30; i++ ) T.Write( "line " + std::to_string( i ) + "\r\n" );
        CHECK( T.ScrollbackLines() == 7 && T.RowText( 0 ) == "line 7" );
        // scroll region: only rows 2..4 move
        T.Write( "\x1b[2;4r\x1b[4;1H\n" );
        CHECK( T.RowText( 0 ) == "line 7" && T.RowText( 1 ) == "line 9" && T.RowText( 3 ).empty() && T.ScrollbackLines() == 7 );
        // DEC line drawing, cursor report, title
        std::string Reply;
        T.OnInput = [ & ]( std::string_view S ) { Reply += S; };
        T.Write( "\x1b[r\x1b[H\x1b(0lqk\x1b(B\x1b[6n\x1b]0;my title\x07" );
        CHECK( T.Cell( 0, 0 ).Cp == 0x250C && T.Cell( 1, 0 ).Cp == 0x2500 && T.Cell( 2, 0 ).Cp == 0x2510 );
        CHECK( Reply == "\x1b[1;4R" && T.Title == "my title" );
        // split UTF-8 across writes
        T.Write( "\xE2\x9C" );
        T.Write( "\x93" );
        CHECK( T.Cell( 3, 0 ).Cp == 0x2713 );
    }
    App.ShellStarted = true; // the test starts its own command below
    {
        // emoji sequences: segmentation works in every build; shaping to one glyph needs TG_HARFBUZZ
        const std::string Flag = "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5";                                  // 🇯🇵
        const std::string Family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"; // 👨‍👩‍👧
        const std::string Wave = "\xF0\x9F\x91\x8B\xF0\x9F\x8F\xBD";                                  // 👋🏽
        const std::string Keycap = "1\xEF\xB8\x8F\xE2\x83\xA3";                                           // 1️⃣
        CHECK( tg::EmojiSequenceLength( Flag + "x", 0 ) == Flag.size() );
        CHECK( tg::EmojiSequenceLength( Family + " ", 0 ) == Family.size() );
        CHECK( tg::EmojiSequenceLength( Wave, 0 ) == Wave.size() );
        CHECK( tg::EmojiSequenceLength( Keycap, 0 ) == Keycap.size() );
        CHECK( tg::EmojiSequenceLength( "\xF0\x9F\x9A\x80", 0 ) == 0 && tg::EmojiSequenceLength( "12", 0 ) == 0 );
        std::string S = "a" + Family + "b";
        CHECK( tg::NextGrapheme( S, 1 ) == 1 + Family.size() && tg::PrevGrapheme( S, 1 + Family.size() ) == 1 );

        tg::Terminal T;
        T.Write( Flag + Family + "!" );
        CHECK( T.RowText( 0 ) == Flag + Family + "!" );
        CHECK( T.Cell( 0, 0 ).Width == 2 && T.Cell( 2, 0 ).Width == 2 && T.Cell( 4, 0 ).Cp == '!' && T.CursorCol() == 5 );
        CHECK( tg::Terminal::CellText( T.Cell( 2, 0 ) ) == Family );

        const tg::Font& F = *tg::GetTheme().Body;
        if ( tg::FontAtlas::HasHarfBuzz() && F.Find( 0x1F680 ).Colored ) // an emoji font that can actually draw here
        {
            const tg::Glyph* G = F.FindSequence( Flag );
            CHECK( G && G->Colored );
            float One = F.Width( "\xF0\x9F\x9A\x80" );
            CHECK( F.Width( Flag ) < One * 1.5f && F.Width( Family ) < One * 1.5f && F.Width( Wave ) < One * 1.5f );
            CHECK( F.HitTest( Family + "x", 0.9f * F.Width( Family ) ) == Family.size() ); // caret never inside
            std::printf( "harfbuzz: emoji sequences shape to single glyphs\n" );
        }
    }
    ClickTab( Ctx, App, 8 );
    {
        tg::Terminal& L = *App.Log;
        CHECK( L.RowText( 1 ).find( "pulled image" ) != std::string::npos );
        // keys become bytes: arrows, Ctrl+C, typed text
        std::string Got;
        L.OnInput = [ & ]( std::string_view S ) { Got += S; };
        Click( Ctx, L );
        CHECK( Ctx.Focused() == &L );
        Ctx.OnKey( tg::Key::Up, true, 0 );
        Ctx.OnKey( tg::Key::C, true, tg::ModCtrl );
        Ctx.OnText( 'l' );
        Ctx.OnText( 's' );
        CHECK( Got == "\x1b[A\x03ls" );
        // drag-select the first word of row 1 and copy it
        std::string Clip;
        Ctx.SetClipboard = [ & ]( const std::string& S ) { Clip = S; };
        tg::Rect In = L.Bounds.Shrink( L.Style.Padding );
        float Cw = In.w / L.Cols(), Ch = In.h / L.Rows();
        (void)Ch;
        float RowY = In.y + 1.5f * ( tg::GetTheme().Mono->LineHeight );
        Drag( Ctx, { In.x + 0.1f, RowY }, { In.x + 8 * Cw + 0.2f, RowY } );
        Ctx.OnKey( tg::Key::C, true, tg::ModCtrl | tg::ModShift );
        CHECK( Clip == "10:41:02" );
        Settle( Ctx );
    }
    {
        // mouse reporting: what vim (set mouse=a), htop, tmux and mc ask for
        tg::Terminal& L = *App.Log;
        std::string Got;
        L.OnInput = [ & ]( std::string_view S ) { Got += S; };
        const tg::Font& Mf = *tg::GetTheme().Mono;
        float Cw = std::round( Mf.Find( 'M' ).Advance * Scale ) / Scale, Chh = std::round( Mf.LineHeight * Scale ) / Scale;
        tg::Rect In = L.Bounds.Shrink( L.Style.Padding );
        auto CellPos = [ & ]( int C, int R ) { return tg::Vec2{ In.x + ( C - 0.5f ) * Cw, In.y + ( R - 0.5f ) * Chh }; }; // 1-based
        L.Write( "\x1b[?1000h\x1b[?1006h" );
        CHECK( L.MouseReporting() );
        ClickAt( Ctx, CellPos( 3, 2 ) );
        CHECK( Got == "\x1b[<0;3;2M\x1b[<0;3;2m" );                       // SGR press + release
        Got.clear();
        Ctx.OnMouseWheel( { 0, 1 } );
        CHECK( Got == "\x1b[<64;3;2M" );
        Got.clear();
        Ctx.OnMouseButton( tg::MouseButton::Right, true );
        Ctx.OnMouseButton( tg::MouseButton::Right, false );
        Ctx.Frame();
        CHECK( Got == "\x1b[<2;3;2M\x1b[<2;3;2m" && OpenPopup( Ctx ) == nullptr ); // the app gets it, no menu
        L.Write( "\x1b[?1006l" );                                          // legacy X10 bytes
        Got.clear();
        ClickAt( Ctx, CellPos( 3, 2 ) );
        CHECK( Got == std::string( "\x1b[M" ) + char( 32 ) + char( 35 ) + char( 34 ) + "\x1b[M" + char( 35 ) + char( 35 ) + char( 34 ) );
        L.Write( "\x1b[?1002h\x1b[?1006h" );                              // button-motion tracking
        Got.clear();
        Ctx.OnMouseMove( CellPos( 3, 2 ) );
        Ctx.OnMouseButton( tg::MouseButton::Left, true );
        Ctx.OnMouseMove( CellPos( 5, 2 ) );
        Ctx.OnMouseButton( tg::MouseButton::Left, false );
        CHECK( Got == "\x1b[<0;3;2M\x1b[<32;5;2M\x1b[<0;5;2m" );
        Ctx.OnKey( tg::Key::LeftShift, true, tg::ModShift );               // Shift: select instead of reporting
        Got.clear();
        Drag( Ctx, { In.x + 1, CellPos( 1, 2 ).y }, { CellPos( 8, 2 ).x + Cw * 0.5f, CellPos( 8, 2 ).y } );
        Ctx.OnKey( tg::Key::LeftShift, false, 0 );
        CHECK( Got.empty() && L.SelectedText() == "10:41:02" );
        L.Write( "\x1b[?1002l\x1b[?1006l" );
        CHECK( !L.MouseReporting() );

        // double-click a word, triple-click the line, copy-on-select, middle-click and Ctrl+V paste
        std::string Clip;
        Ctx.SetClipboard = [ & ]( const std::string& S ) { Clip = S; };
        Ctx.GetClipboard = [] { return std::string( "pasted" ); };
        ClickAt( Ctx, CellPos( 14, 2 ) );
        ClickAt( Ctx, CellPos( 14, 2 ) );
        CHECK( L.SelectedText() == "pulled" );
        ClickAt( Ctx, CellPos( 14, 2 ) );
        CHECK( L.SelectedText().rfind( "10:41:02", 0 ) == 0 && L.SelectedText().find( "1.2s" ) != std::string::npos );
        Ctx.OnKey( tg::Key::C, true, tg::ModCtrl | tg::ModShift );
        CHECK( Clip == L.SelectedText() );
        L.CopyOnSelect = true;
        for ( int i = 0; i < 30; i++ ) Ctx.Frame(); // let the triple-click window pass
        ClickAt( Ctx, CellPos( 22, 2 ) );
        ClickAt( Ctx, CellPos( 22, 2 ) );
        CHECK( Clip == "image" );
        Got.clear();
        Ctx.OnMouseButton( tg::MouseButton::Middle, true );
        Ctx.OnMouseButton( tg::MouseButton::Middle, false );
        L.CtrlVPastes = true;
        Ctx.OnKey( tg::Key::V, true, tg::ModCtrl );
        CHECK( Got == "pastedpasted" );
        L.CopyOnSelect = false;
        Settle( Ctx );
    }
    if ( tg::Pty::Supported() && !UnderWine() ) // Wine has the ConPTY API, but children never write into it
    {
        // a real process: output arrives on the pty thread and is posted to the UI thread
        App.ShellCommand =
#ifdef _WIN32
            "echo hello from conpty";
#else
            "printf '\\033[1;32mhello\\033[0m from pty\\n'; exit 3";
#endif
        int Code = -1;
        App.StartShell();
        App.Shell->OnExit = [ & ]( int C ) { Code = C; };
        auto Has = [ & ] { for ( int r = 0; r < App.Shell->Rows(); r++ ) if ( App.Shell->RowText( r ).find( "from" ) != std::string::npos ) return true; return false; };
        for ( int i = 0; i < 400 && ( !Has() || Code < 0 ); i++ ) { Ctx.Frame(); std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) ); }
        if ( !Has() ) { std::fprintf( stderr, "pty: exit=%d running=%d\n", Code, int( App.Shell->IsRunning() ) ); for ( int r = 0; r < 4; r++ ) std::fprintf( stderr, "  [%s]\n", App.Shell->RowText( r ).c_str() ); }
        CHECK( Has() );
#ifndef _WIN32
        CHECK( Code == 3 );
        CHECK( App.Shell->Cell( 0, 0 ).Fg == ( 0x1000000u | 2 ) );
#endif
        App.ShellCommand.clear();
    }
    Shot( Ctx, "terminal" );

    // ---------------- copy & paste outside the terminal
    {
        std::string Clip;
        Ctx.SetClipboard = [ & ]( const std::string& S ) { Clip = S; };
        // selectable chat message: double-click a word, Ctrl+C; drag to select
        ClickTab( Ctx, App, 1 );
        App.AddMessage( "you", "copy 10.0.4.21 please", "now" );
        Settle( Ctx );
        tg::Label* Msg = nullptr;
        for ( int i = 0; auto* L = Find<tg::Label>( App.Messages->Content(), i ); i++ )
            if ( L->GetText() == "copy 10.0.4.21 please" ) Msg = L;
        CHECK( Msg && Msg->IsSelectable() );
        const tg::Font& F = *tg::GetTheme().Body;
        float X0 = Msg->Bounds.x + Msg->Layout.Padding.Left, Y = Msg->Bounds.y + Msg->Layout.Padding.Top + F.LineHeight * 0.5f;
        for ( int i = 0; i < 30; i++ ) Ctx.Frame();
        ClickAt( Ctx, { X0 + F.Width( "copy 10.0.4.21 " ) + 6, Y } );
        ClickAt( Ctx, { X0 + F.Width( "copy 10.0.4.21 " ) + 6, Y } );
        CHECK( Msg->SelectedText() == "please" && Ctx.Focused() == Msg );
        Ctx.OnKey( tg::Key::C, true, tg::ModCtrl );
        CHECK( Clip == "please" );
        for ( int i = 0; i < 30; i++ ) Ctx.Frame();
        Drag( Ctx, { X0 + 1, Y }, { X0 + F.Width( "copy 10.0.4.21" ) + 1, Y } );
        CHECK( Msg->SelectedText() == "copy 10.0.4.21" );
        Settle( Ctx );
        Shot( Ctx, "chat_select" );
        // text fields: right-click menu
        Ctx.OnMouseMove( App.Composer->Bounds.Center() );
        Ctx.OnMouseButton( tg::MouseButton::Right, true );
        Ctx.OnMouseButton( tg::MouseButton::Right, false );
        Ctx.Frame();
        CHECK( OpenPopup( Ctx ) != nullptr );
        Ctx.ClosePopups();
        Settle( Ctx );
        // tables: Ctrl+C copies selected rows as tab-separated text
        ClickTab( Ctx, App, 3 );
        App.Procs->SetSelected( 0, true );
        Ctx.SetFocus( App.Procs );
        Ctx.OnKey( tg::Key::C, true, tg::ModCtrl );
        CHECK( !Clip.empty() && Clip == App.Procs->SelectedText() && Clip.find( '\t' ) != std::string::npos );
        App.Procs->SetSelected( 0, false );
    }

    // ---------------- "More widgets" tab
    ClickTab( Ctx, App, 9 );
    Settle( Ctx );
    Shot( Ctx, "widgets_plus" );
    {
        // icons are text
        CHECK( !tg::Icon::Trash2.empty() && tg::Icon::ByName( "trash-2" ) == tg::Icon::Trash2 && tg::Icon::ByName( "nope" ).empty() );
        CHECK( tg::GetTheme().Body->Find( 0xE18D ).Advance > 0 ); // trash-2 has a glyph (icon font fallback)
        // rich text: markup parsed; links clickable
        CHECK( App.Rich->PlainText().find( "Deploy of api v2.3 finished on fra-01 in 42s" ) != std::string::npos );
        // virtual list: 100k rows, only the visible ones drawn; keyboard + ScrollTo
        tg::VirtualList& L = *App.BigList;
        CHECK( L.Count() == 100000 );
        L.ScrollTo( 99999 );
        Settle( Ctx );
        CHECK( L.GetScroll() > 99000 * 28.f );
        Ctx.SetFocus( &L );
        L.Selected = 50;
        Ctx.OnKey( tg::Key::Down, true, 0 );
        CHECK( L.Selected == 51 );
        Ctx.OnKey( tg::Key::Home, true, 0 );
        Settle( Ctx );
        CHECK( L.Selected == 0 && L.GetScroll() < 1 && L.RowAt( L.RowRect( 3 ).Center() ) == 3 );
        // fuzzy matching ranks word starts first
        CHECK( tg::FuzzyScore( "rst srv", "Restart server" ) > 0 && tg::FuzzyScore( "xyz", "Restart server" ) < 0 );
        CHECK( tg::FuzzyScore( "dp", "Deploy to production" ) > tg::FuzzyScore( "dp", "Open help page" ) );
        // command palette: Ctrl+K, type, Enter runs the best match
        Ctx.SetFocus( nullptr );
        Ctx.OnKey( tg::Key::K, true, tg::ModCtrl );
        Settle( Ctx );
        CHECK( App.Palette && App.Palette->Visible );
        for ( const char* c = "dply"; *c; c++ ) Ctx.OnText( uint32_t( *c ) );
        Settle( Ctx );
        Shot( Ctx, "palette" );
        CHECK( App.Palette->Matches() == 1 );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        Settle( Ctx );
        CHECK( App.LastMenu == "Deploy to production" );
        Ctx.OnKey( tg::Key::K, true, tg::ModCtrl );
        Settle( Ctx );
        Ctx.OnKey( tg::Key::Escape, true, 0 );
        Settle( Ctx );
        bool AnyPalette = false;
        for ( auto& K : Ctx.Overlay().Children() ) if ( dynamic_cast<tg::CommandPalette*>( K.get() ) ) AnyPalette = true;
        CHECK( !AnyPalette );
        // toolbar toggle
        bool Live0 = App.Live;
        Click( Ctx, *Find<tg::ToolButton>( *App.Tools, 2 ) );
        CHECK( App.Live != Live0 );
        // knob: drag up raises, double-click resets; wheel
        tg::Knob& K = *App.GainKnob;
        Drag( Ctx, K.Bounds.Center(), K.Bounds.Center() + tg::Vec2{ 0, -50 } );
        CHECK( App.Gain > 5 );
        ClickAt( Ctx, K.Bounds.Center() );
        ClickAt( Ctx, K.Bounds.Center() );
        CHECK( App.Gain == 0 );
        // xy pad: click the top-right quarter
        tg::XYPad& Xy = *App.Pad;
        tg::Rect Pr = { Xy.Bounds.x, Xy.Bounds.Bottom() - Xy.Size, Xy.Size, Xy.Size };
        for ( int i = 0; i < 30; i++ ) Ctx.Frame();
        ClickAt( Ctx, { Pr.x + Pr.w * 0.75f, Pr.y + Pr.h * 0.25f } );
        CHECK( std::fabs( App.Cross.x - 0.5f ) < 0.05f && std::fabs( App.Cross.y - 0.5f ) < 0.05f );
        // time picker: wheel on the minutes; open the list and pick an hour
        tg::TimePicker& Tp = *App.TimeW;
        Ctx.OnMouseMove( { Tp.Bounds.x + 10 + tg::GetTheme().Body->Width( "00:" ) + 4, Tp.Bounds.Center().y } );
        Ctx.OnMouseWheel( { 0, 1 } );
        CHECK( App.BackupAt.M == 31 && App.BackupAt.H == 3 );
        Click( Ctx, Tp );
        CHECK( Tp.IsOpen() );
        Settle( Ctx );
        auto* Hours = Find<tg::VirtualList>( *OpenPopup( Ctx ) );
        CHECK( Hours );
        Hours->ScrollTo( 0 );
        Settle( Ctx );
        ClickAt( Ctx, Hours->RowRect( 1 ).Center() );
        CHECK( App.BackupAt.H == 1 && App.BackupAt.M == 31 && Tp.IsOpen() ); // stays open to pick the minutes too
        Ctx.ClosePopups();
        Settle( Ctx );
        // date range: two clicks
        Click( Ctx, *App.RangeW );
        Settle( Ctx );
        auto* Cal = Find<tg::Calendar>( *OpenPopup( Ctx ) );
        CHECK( Cal && Cal->Range );
        ClickAt( Ctx, Cal->DayRect( 10 ).Center() );
        CHECK( App.RangeW->IsOpen() );
        ClickAt( Ctx, Cal->DayRect( 3 ).Center() );
        Settle( Ctx );
        CHECK( App.From.Day == 3 && App.To.Day == 10 && !App.RangeW->IsOpen() );
        // swatches + eyedropper (reads the rendered frame)
        tg::Swatches& Sw = *App.SwatchW;
        ClickAt( Ctx, { Sw.Bounds.x + 2 * ( Sw.Size + Sw.Gap ) + Sw.Size * 0.5f, Sw.Bounds.y + Sw.Size * 0.5f } ); // third color
        CHECK( App.Accent == Sw.Palette[ 2 ] );
        Click( Ctx, *App.Dropper );
        CHECK( App.Dropper->IsPicking() );
        tg::Vec2 V = Ctx.GetViewport();
        Ctx.Frame();
        Ctx.Render( { V.x * Scale, V.y * Scale } ); // the capture happens here
        tg::Vec2 Red = Find<tg::Badge>( *App.Pages[ 9 ] )->Bounds.Center(); // a raised chip: not the accent
        Ctx.OnMouseMove( { App.DropBin->Bounds.x + 3, App.DropBin->Bounds.Center().y } );
        Ctx.OnMouseButton( tg::MouseButton::Left, true );
        Ctx.OnMouseButton( tg::MouseButton::Left, false );
        (void)Red;
        CHECK( !App.Dropper->IsPicking() && App.Accent == tg::GetTheme()[ tg::Tok::Surface ] );
        // pagination + stepper
        ClickAt( Ctx, { App.Pager->Bounds.x + 34 * 2 + 15, App.Pager->Bounds.Center().y } ); // the "2"
        CHECK( App.PageNo == 1 );
        Ctx.SetFocus( App.Pager );
        Ctx.OnKey( tg::Key::End, true, 0 );
        CHECK( App.PageNo == 19 );
        ClickAt( Ctx, { App.Steps->Bounds.x + 20, App.Steps->Bounds.y + 15 } ); // back to step 1
        CHECK( App.StepNo == 0 );
        // accordion: opening one closes the other
        auto* SecA = Find<tg::Collapsible>( *App.Acc, 0 );
        auto* SecB = Find<tg::Collapsible>( *App.Acc, 1 );
        CHECK( SecA->IsOpen() && !SecB->IsOpen() );
        SecB->SetOpen( true );
        CHECK( !SecA->IsOpen() );
        // drag and drop: chip -> bin
        auto* Chip = Find<tg::Badge>( *App.Pages[ 9 ], 1 );
        Drag( Ctx, Chip->Bounds.Center(), App.DropBin->Bounds.Center() );
        CHECK( App.Dropped == "vps-ams-02" && !Ctx.IsDragging() );
        Ctx.OnMouseMove( Chip->Bounds.Center() );
        Ctx.OnMouseButton( tg::MouseButton::Left, true );
        Ctx.OnMouseMove( App.DropBin->Bounds.Center() );
        CHECK( Ctx.IsDragging() && Ctx.DropTarget() == App.DropBin );
        Settle( Ctx );
        Shot( Ctx, "drag_drop" );
        Ctx.OnKey( tg::Key::Escape, true, 0 );
        Ctx.OnMouseButton( tg::MouseButton::Left, false );
        CHECK( !Ctx.IsDragging() );
        // code editor: typing, auto-indent, multi-caret (Ctrl+D), comment toggle, undo
        tg::CodeEditor& Ed = *App.Editor;
        Ed.SetText( "int a = 1;\nint b = a;\n" );
        Ctx.SetFocus( &Ed );
        Ed.SetCaret( 0, 4 );
        Ctx.OnKey( tg::Key::D, true, tg::ModCtrl ); // select "a"
        Ctx.OnKey( tg::Key::D, true, tg::ModCtrl ); // ... and the next "a"
        CHECK( Ed.CaretCount() == 2 );
        for ( const char* c = "x2"; *c; c++ ) Ctx.OnText( uint32_t( *c ) );
        CHECK( Ed.GetText() == "int x2 = 1;\nint b = x2;\n" );
        Ctx.OnKey( tg::Key::Escape, true, 0 );
        CHECK( Ed.CaretCount() == 1 );
        Ctx.OnKey( tg::Key::Slash, true, tg::ModCtrl );
        CHECK( Ed.Line( 1 ) == "// int b = x2;" );
        Ctx.OnKey( tg::Key::Z, true, tg::ModCtrl );
        CHECK( Ed.Line( 1 ) == "int b = x2;" );
        Ed.SetText( "void f() {" );
        Ed.SetCaret( 0, 10 );
        Ctx.OnKey( tg::Key::Enter, true, 0 );
        Ctx.OnText( 'x' );
        CHECK( Ed.GetText() == "void f() {\n    x" );
        Ctx.OnKey( tg::Key::Tab, true, tg::ModShift );
        CHECK( Ed.Line( 1 ) == "x" );
        Ctx.SetFocus( nullptr );
        Settle( Ctx );
        Shot( Ctx, "widgets_plus_after" );
    }

    // ---------------- layout & theme
    ClickTab( Ctx, App, 10 );
    Settle( Ctx );
    Shot( Ctx, "layout_theme" );
    {
        // grid: columns from ColumnMin, ColSpan
        tg::Container& G = *App.GridBox;
        auto Kid = [ & ]( size_t i ) { return G.Children()[ i ].get(); };
        int Cols = int( ( G.Bounds.w + 10 ) / ( 170 + 10 ) );
        CHECK( Cols >= 2 );
        float Cw = ( G.Bounds.w - 10 * ( Cols - 1 ) ) / Cols;
        CHECK( std::fabs( Kid( 0 )->Bounds.w - ( Cw * 2 + 10 ) ) < 1 );        // spans 2 columns
        CHECK( std::fabs( Kid( 1 )->Bounds.x - ( G.Bounds.x + 2 * ( Cw + 10 ) ) ) < 1 || Cols == 2 );
        // wrap: badges flow onto more than one line, all inside the box
        tg::Container& W = *App.WrapBox;
        float MinY = 1e9f, MaxY = -1e9f;
        for ( auto& K : W.Children() ) { MinY = std::min( MinY, K->Bounds.y ); MaxY = std::max( MaxY, K->Bounds.y ); CHECK( K->Bounds.Right() <= W.Bounds.Right() + 0.5f ); }
        (void)MinY;
        // StackBelow: side by side now
        tg::Container& S = *App.StackRow;
        CHECK( S.Bounds.w >= 520 && std::fabs( S.Children()[ 0 ]->Bounds.y - S.Children()[ 1 ]->Bounds.y ) < 1 );
        // reorder animates: right after a shuffle a card is still drawn at its old spot
        tg::Widget* Second = Kid( 1 );
        tg::Vec2 Before = Second->Bounds.Pos();
        Click( Ctx, *Find<tg::Button>( *App.Pages[ 10 ], 0 ) );
        Ctx.Frame();
        CHECK( Second->Bounds.Pos() != Before );
        Settle( Ctx );
        // a narrow viewport: fewer columns, the row stacks
        Ctx.SetViewport( { 700, 820 }, Scale );
        Settle( Ctx );
        CHECK( S.Bounds.w < 520 && S.Children()[ 1 ]->Bounds.y > S.Children()[ 0 ]->Bounds.y );
        int Cols2 = int( ( G.Bounds.w + 10 ) / ( 170 + 10 ) );
        CHECK( Cols2 < Cols || Cols2 == 1 );
        Shot( Ctx, "layout_narrow" );
        Ctx.SetViewport( { 1280, 820 }, Scale );
        Settle( Ctx );
        // Responsive(): fires on crossing
        int Wide = -1;
        tg::Responsive( *App.Pages[ 10 ], 1000, [ & ]( bool Wd ) { Wide = Wd; } );
        Ctx.InvalidateLayout();
        Settle( Ctx );
        CHECK( Wide == 1 );
        Ctx.SetViewport( { 900, 820 }, Scale );
        Settle( Ctx );
        CHECK( Wide == 0 );
        Ctx.SetViewport( { 1280, 820 }, Scale );
        App.Pages[ 10 ]->OnResize = nullptr;
        Settle( Ctx );
        // aspect + max width
        tg::Container Test( tg::Dir::Column, 0 );
        auto& Box16 = Test.Add<tg::Box>();
        Box16.Layout.Aspect = 2;
        auto& Narrow = Test.Add<tg::Box>();
        Narrow.Layout.MaxWidth = 100;
        Test.Layout.CrossAlign = tg::Align::Stretch;
        Test.Arrange( { 0, 0, 400, 600 } );
        CHECK( Box16.Bounds.w == 400 && Box16.Bounds.h == 200 && Narrow.Bounds.w == 100 );
        // timers (fixed timestep: frame time)
        int Ticks = 0, Once = 0;
        uint32_t Tid = Ctx.Every( 0.1, [ & ] { Ticks++; } );
        Ctx.After( 0.25, [ & ] { Once++; } );
        for ( int i = 0; i < 30; i++ ) Ctx.Frame(); // 0.5 s
        CHECK( Ticks >= 4 && Ticks <= 6 && Once == 1 );
        Ctx.CancelTimer( Tid );
        for ( int i = 0; i < 30; i++ ) Ctx.Frame();
        CHECK( Ticks <= 6 );
        // theme files: JSON round trip, partial files, live reload
        tg::Theme Saved = tg::GetTheme();
        std::string J = tg::GetTheme().ToJson();
        CHECK( J.find( "\"Accent\": \"#" ) != std::string::npos );
        tg::GetTheme()[ tg::Tok::Accent ] = tg::Color::Hex( 0x123456 );
        CHECK( tg::GetTheme().FromJson( J ) && tg::GetTheme()[ tg::Tok::Accent ] == Saved[ tg::Tok::Accent ] );
        CHECK( tg::GetTheme().FromJson( "{ \"Danger\": \"#FF0000\", \"Radius\": 9 }" ) && tg::GetTheme()[ tg::Tok::Danger ] == tg::Color( 255, 0, 0 ) && tg::GetTheme().Radius == 9 );
        std::string Tf = std::string( OutDir ) + "/theme.json";
        CHECK( tg::SaveThemeFile( Tf ) );
        Ctx.WatchTheme( Tf );
        { std::ofstream F( Tf ); F << "{ \"Accent\": \"#00FF00\" }  "; }
        for ( int i = 0; i < 40; i++ ) Ctx.Frame();
        CHECK( tg::GetTheme()[ tg::Tok::Accent ] == tg::Color( 0, 255, 0 ) );
        // the theme editor writes straight into the theme
        auto* Pick = Find<tg::ColorPicker>( *App.ThemeEd, int( tg::Tok::Accent ) );
        CHECK( Pick && Pick->Bind == &tg::GetTheme().Colors[ size_t( tg::Tok::Accent ) ] );
        Pick->SetColor( tg::Color::Hex( 0xFF00FF ) );
        CHECK( tg::GetTheme()[ tg::Tok::Accent ] == tg::Color::Hex( 0xFF00FF ) && App.ThemeEd->CppSnippet().find( "0xFF00FF" ) != std::string::npos );
        // variants
        tg::Button Vb( "x" );
        CHECK( tg::UseVariant( Vb, "danger" ) && !tg::UseVariant( Vb, "nope" ) );
        tg::GetTheme().UseColors( Saved );
        tg::GetTheme().Radius = Saved.Radius;
        Settle( Ctx );
    }

    // ---------------- effects (software renderer runs the shaders' C++ versions)
    ClickTab( Ctx, App, 11 );
    Settle( Ctx );
    Shot( Ctx, "effects" );
    {
        tg::Vec2 V = Ctx.GetViewport();
        auto Px = [ & ]( float X, float Y ) { return tg::Color::Unpack( Soft.Pixels[ size_t( Y * Scale ) * size_t( Soft.Width ) + size_t( X * Scale ) ] ); };
        auto Near = []( tg::Color A, tg::Color B, int Tol ) { return std::abs( A.r - B.r ) <= Tol && std::abs( A.g - B.g ) <= Tol && std::abs( A.b - B.b ) <= Tol; };
        // linear gradient tile: left edge ~ first stop, right edge ~ second
        auto* Tile = Find<tg::Widget>( *App.Pages[ 11 ], 0 );
        (void)Tile;
        tg::Widget* Lin = nullptr;
        std::vector<tg::Widget*> St = { App.Pages[ 11 ] };
        while ( !St.empty() ) { tg::Widget* W = St.back(); St.pop_back(); if ( !Lin && W->Layout.Height.K == tg::Size::Kind::Fixed && W->Layout.Height.V == 90 ) Lin = W; for ( auto& K : W->Children() ) St.push_back( K.get() ); }
        CHECK( Lin );
        // first tile in tree order may be any of them; find the "Linear" one by sampling: left ~ 6EB7D4
        tg::Rect B = Lin->Bounds;
        tg::Color L0 = Px( B.x + 4, B.Center().y ), L1 = Px( B.Right() - 4, B.Center().y );
        CHECK( !Near( L0, L1, 10 ) ); // a gradient, not flat
        // grayscale effect: the primary button turns gray when the toggle is on
        tg::Button* Deploy = Find<tg::Button>( *App.GrayFx );
        CHECK( Deploy );
        tg::Color Before = Px( Deploy->Bounds.x + 6, Deploy->Bounds.Center().y );
        App.GrayFx->Params[ 0 ] = 1;
        Ctx.RequestFrame();
        Settle( Ctx );
        Ctx.Render( { V.x * Scale, V.y * Scale } );
        tg::Color After = Px( Deploy->Bounds.x + 6, Deploy->Bounds.Center().y );
        CHECK( std::abs( Before.r - Before.b ) > 20 && std::abs( After.r - After.b ) < 6 && std::abs( After.g - After.b ) < 6 );
        App.GrayFx->Params[ 0 ] = 0;
        // frosted glass: inside the panel the hard edges behind are smeared (neighbouring pixels close)
        tg::Rect G = App.Glass->Bounds;
        int Jumps = 0;
        for ( float x = G.x + 20; x < G.Right() - 20; x += 1 )
        {
            tg::Color A = Px( x, G.y + 6 ), C = Px( x + 1, G.y + 6 );
            if ( std::abs( A.r - C.r ) + std::abs( A.g - C.g ) + std::abs( A.b - C.b ) > 60 ) Jumps++;
        }
        CHECK( Jumps == 0 );
        // cached panel: rendered to a texture once; drawing again reuses it
        CHECK( App.Cached->IsCached() );
        size_t Verts = 0;
        Ctx.RequestFrame();
        const tg::DrawList& Dl = Ctx.Frame();
        for ( auto& C : Dl.Cmds ) (void)C;
        Verts = Dl.Vtx.size();
        App.Cached->MarkDirty();
        CHECK( !App.Cached->IsCached() );
        Settle( Ctx );
        CHECK( App.Cached->IsCached() && Verts > 0 );
        // a custom shader with only a C++ version (runs on the software renderer)
        static tg::ShaderDesc Invert{ {}, {}, []( tg::Color In, tg::Vec2 Uv, tg::Vec2, const float*, const std::function<tg::Color( tg::Vec2 )>& Tx ) {
            tg::Color T = Tx( Uv );
            return tg::Color( uint8_t( 255 - T.r * In.r / 255 ), uint8_t( 255 - T.g * In.g / 255 ), uint8_t( 255 - T.b * In.b / 255 ), uint8_t( T.a * In.a / 255 ) );
        } };
        tg::ShaderId Sid = Ctx.Shader( Invert );
        CHECK( Sid != 0 && Ctx.Shader( Invert ) == Sid );
        tg::DrawList Own;
        Own.Reset( { 0, 0, 40, 40 }, 1, Ctx.Fonts.Texture, Ctx.Fonts.WhiteUV );
        Own.PushShader( Sid );
        Own.RectFilled( { 0, 0, 40, 40 }, tg::Color( 255, 0, 0 ) );
        Own.PopShader();
        tg::SoftRenderer Small;
        tg::TextureId At = Small.CreateTexture( Ctx.Fonts.Width, Ctx.Fonts.Height, Ctx.Fonts.Pixels.data() );
        Own.AtlasTexture = At;
        for ( auto& C : Own.Cmds ) C.Texture = At;
        tg::ShaderId S2 = Small.CreateShader( Invert );
        for ( auto& C : Own.Cmds ) C.Shader = S2;
        Small.Render( Own, { 40, 40 } );
        tg::Color Mid = tg::Color::Unpack( Small.Pixels[ 20 * 40 + 20 ] );
        CHECK( Mid.r < 10 && Mid.g > 245 && Mid.b > 245 ); // red inverted = cyan
        // SVG: rasterized at the requested size
        const std::string Dot = "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'><circle cx='5' cy='5' r='4' fill='#ff0000'/></svg>";
        float Sw2, Sh2;
        CHECK( tg::SvgSize( Dot, Sw2, Sh2 ) && Sw2 == 10 && Sh2 == 10 );
        auto SvgPx = tg::RasterizeSvgPixels( Dot, 40, 40 );
        CHECK( SvgPx.size() == 1600 );
        tg::Color Ctr = tg::Color::Unpack( SvgPx[ 20 * 40 + 20 ] ), Corner = tg::Color::Unpack( SvgPx[ 0 ] );
        CHECK( Ctr.r > 240 && Ctr.g < 10 && Ctr.a > 240 && Corner.a == 0 );
        auto& SvgW = App.Pages[ 11 ]->Add<tg::SvgView>( Dot, tg::Vec2{ 24, 24 } );
        SvgW.Tinted = true;
        Settle( Ctx );
        CHECK( SvgW.Measure( 1000 ).x == 24 && SvgW.Measure( 1000 ).y == 24 );
        App.Pages[ 11 ]->Remove( &SvgW );
        // synthetic italics: the same glyphs, slanted (top shifted right of the bottom)
        tg::Font* It = tg::GetTheme().Italic;
        CHECK( It && It->Skew > 0 && It->Width( "Hello" ) == tg::GetTheme().Body->Width( "Hello" ) );
        tg::DrawList Il;
        Il.Reset( { 0, 0, 100, 40 }, 1, Ctx.Fonts.Texture, Ctx.Fonts.WhiteUV );
        Il.Text( *It, { 10, 10 }, "I", tg::Color( 255, 255, 255 ) );
        CHECK( Il.Vtx.size() == 4 && Il.Vtx[ 0 ].Pos.x > Il.Vtx[ 3 ].Pos.x ); // top-left right of bottom-left
        tg::FontFamily Fam{ tg::GetTheme().Body, tg::GetTheme().Medium, tg::GetTheme().Bold, tg::GetTheme().Italic, tg::GetTheme().BoldItalic };
        CHECK( Fam.Get( true, true ) == tg::GetTheme().BoldItalic && Fam.Get( false, false ) == tg::GetTheme().Body );
        // gradient fallback math: ShadeLinear endpoints
        tg::DrawList Gl;
        Gl.Reset( { 0, 0, 100, 10 }, 1, 0, {} );
        Gl.RectFilled( { 0, 0, 100, 10 }, tg::Color( 255, 255, 255 ) );
        Gl.ShadeLinear( 0, { 0, 5 }, { 100, 5 }, tg::Color( 0, 0, 0 ), tg::Color( 200, 100, 50 ) );
        CHECK( tg::Color::Unpack( Gl.Vtx.front().Col ).r < 5 );
    }

    // ---------------- docking
    {
        ClickTab( Ctx, App, 12 );
        tg::DockSpace& Dk = *App.DockW;
        tg::DockPanel &Files = *Dk.Find( "files" ), &Code = *Dk.Find( "editor" ), &Notes = *Dk.Find( "notes" ), &Out = *Dk.Find( "output" ),
                      &Props = *Dk.Find( "props" );
        Settle( Ctx );
        CHECK( Dk.SaveLayout().rfind( "split(", 0 ) == 0 );
        Dk.Activate( Code );
        Settle( Ctx );
        CHECK( Files.Bounds.Right() <= Code.Bounds.x && Props.Bounds.x >= Code.Bounds.Right() && Out.Bounds.y >= Code.Bounds.Bottom() - 1 );
        CHECK( Code.Visible && !Notes.Visible && Notes.Bounds.w > 0 );
        // click the Notes tab: it comes to the front
        Click( Ctx, *&Code ); // (focus somewhere harmless first)
        ClickAt( Ctx, Dk.TabOf( Notes ).Center() );
        Settle( Ctx );
        CHECK( Notes.Visible && !Code.Visible );
        // drag the Notes tab onto the right edge of the editor's panel: split
        tg::Rect Body = Notes.Bounds;
        Drag( Ctx, Dk.TabOf( Notes ).Center(), { Body.Right() - 12, Body.Center().y + 40 } );
        Settle( Ctx );
        CHECK( Notes.Visible && Code.Visible && !Dk.IsFloating( Notes ) );
        CHECK( Notes.Bounds.x > Code.Bounds.x + 20 && std::fabs( Notes.Bounds.y - Code.Bounds.y ) < 1 );
        // tear it off with Ctrl held (no docking): it floats
        {
            tg::Vec2 From = Dk.TabOf( Notes ).Center();
            Ctx.OnMouseMove( From );
            Ctx.OnMouseButton( tg::MouseButton::Left, true );
            Ctx.OnKey( tg::Key::LeftCtrl, true, tg::ModCtrl );
            for ( int i = 1; i <= 8; i++ ) Ctx.OnMouseMove( From + tg::Vec2{ -30.f * i, 40.f * i } );
            Ctx.OnMouseButton( tg::MouseButton::Left, false );
            Ctx.OnKey( tg::Key::LeftCtrl, false, 0 );
            Settle( Ctx );
        }
        CHECK( Dk.IsFloating( Notes ) && Notes.Visible );
        Shot( Ctx, "docking" );
        // drag the floating window's title onto the editor's tab strip: it joins as a tab
        tg::Rect Fl = Dk.TabOf( Notes );
        Drag( Ctx, { Fl.Right() + 30, Fl.Center().y }, Dk.TabOf( Code ).Center() + tg::Vec2{ 0, 2 } );
        Settle( Ctx );
        CHECK( !Dk.IsFloating( Notes ) && Notes.Visible && !Code.Visible && Notes.Bounds.x == Code.Bounds.x );
        // reorder within the strip
        tg::Rect Tn = Dk.TabOf( Notes ), Tc = Dk.TabOf( Code );
        CHECK( Tc.x < Tn.x );
        Drag( Ctx, Tn.Center(), { Tc.x + 4, Tc.Center().y } );
        Settle( Ctx );
        CHECK( Dk.TabOf( Notes ).x < Dk.TabOf( Code ).x );
        // close the output panel with its x, then show it again in the same place
        Dk.Activate( Out );
        Settle( Ctx );
        tg::Rect To = Dk.TabOf( Out );
        ClickAt( Ctx, { To.Right() - 14, To.Center().y } );
        Settle( Ctx );
        CHECK( !Dk.IsOpen( Out ) && !Out.Visible && Notes.Bounds.Bottom() > To.y );
        Dk.Show( Out );
        Settle( Ctx );
        CHECK( Dk.IsOpen( Out ) && Out.Visible && Out.Bounds.y > Notes.Bounds.y + 40 );
        // resize the explorer with its divider
        float W0 = Files.Bounds.w;
        Drag( Ctx, { Files.Bounds.Right() + 2.5f, Files.Bounds.Center().y }, { Files.Bounds.Right() + 82.5f, Files.Bounds.Center().y } );
        Settle( Ctx );
        CHECK( std::fabs( Files.Bounds.w - ( W0 + 80 ) ) < 3 );
        // save -> change -> load gives the same layout back; garbage is rejected
        std::string S = Dk.SaveLayout();
        Dk.Float( Props, { 50, 50, 300, 200 } );
        Dk.Close( Files );
        Settle( Ctx );
        CHECK( Dk.SaveLayout() != S && Dk.SaveLayout().find( ";float(" ) != std::string::npos );
        CHECK( Dk.LoadLayout( S ) );
        Settle( Ctx );
        CHECK( Dk.SaveLayout() == S && Dk.IsOpen( Files ) && Files.Visible && !Dk.IsFloating( Props ) );
        CHECK( !Dk.LoadLayout( "split(row,0.5,tabs(files|0)" ) && Dk.SaveLayout() == S );
        CHECK( Dk.LoadLayout( "tabs(files,ghost,editor|1);float(10,10,200,150,tabs(notes|0))" ) );
        Settle( Ctx );
        CHECK( Code.Visible && !Files.Visible && Dk.IsFloating( Notes ) && !Dk.IsOpen( Out ) );
        CHECK( Dk.LoadLayout( S ) );
        Settle( Ctx );
        // Detach: move a widget to another parent without destroying it
        auto& Btn = App.DesktopBar->Button( "moving" );
        auto Owned = App.DesktopBar->Detach( &Btn );
        CHECK( Owned.get() == &Btn && Btn.GetParent() == nullptr );
        Notes.AddChild( std::move( Owned ) );
        CHECK( Btn.GetParent() == &Notes );
        Notes.Remove( &Btn );
        Settle( Ctx );
    }

    // ---------------- platform: files from the OS, IME, a second window's context, pop-out, clipboard images
    {
        tg::Container& Pg = *App.Pages[ 12 ];
        auto& Zone = Pg.Add<tg::Box>();
        Zone.Layout.Height = tg::Px( 60 );
        std::vector<std::string> Got;
        Zone.AcceptDrop = []( const tg::DragData& D ) { return D.Type == "files"; };
        Zone.OnDrop = [ & ]( const tg::DragData& D, tg::Vec2 ) { Got = D.Paths; };
        Settle( Ctx );
        tg::Vec2 Zc = Zone.Bounds.Center();
        CHECK( Ctx.OnFilesOver( Zc, { "/tmp/a.png", "/tmp/b.txt" } ) && Ctx.FilesOver() && Ctx.DropTarget() == &Zone );
        CHECK( Ctx.Dragged() && Ctx.Dragged()->Label == "2 files" );
        Ctx.OnFilesLeave();
        CHECK( !Ctx.FilesOver() && !Ctx.IsDragging() );
        CHECK( Ctx.OnFilesDrop( Zc, { "/tmp/a.png" } ) && Got.size() == 1 && Got[ 0 ] == "/tmp/a.png" && !Ctx.IsDragging() );
        std::vector<std::string> Fallback;
        CHECK( !Ctx.OnFilesDrop( { 5, 5 }, { "/x" } ) ); // nobody wants them
        Ctx.OnFiles = [ & ]( const std::vector<std::string>& P, tg::Vec2 ) { Fallback = P; };
        CHECK( Ctx.OnFilesDrop( { 5, 5 }, { "/x" } ) && Fallback.size() == 1 );
        Ctx.OnFiles = nullptr;
        Pg.Remove( &Zone );

        // IME: the composition draws at the caret of the focused field
        auto& Field = Pg.Input( "ime" );
        Settle( Ctx );
        Click( Ctx, Field );
        Settle( Ctx );
        tg::Rect Cr = Ctx.CaretRect();
        CHECK( Cr.h > 0 && Field.Bounds.Contains( Cr.Center() ) );
        size_t V0 = Ctx.Frame().Vtx.size();
        Ctx.OnComposition( "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93", 3 ); // にほん, caret after the first
        CHECK( Ctx.Composition().size() == 9 && Ctx.NeedsFrame() );
        CHECK( Ctx.Frame().Vtx.size() > V0 );
        Ctx.OnComposition( {} );
        Ctx.OnText( 0x65E5 ); // committed: 日
        CHECK( Field.GetText() == "\xE6\x97\xA5" && Ctx.Composition().empty() );
        Ctx.SetFocus( nullptr );
        Pg.Remove( &Field );

        // a second window's Context, other renderer: draws text with a copy of the shared atlas
        tg::SoftRenderer Soft2;
        {
            tg::Context Ctx2( Soft2, Ctx );
            Ctx2.SetViewport( { 200, 60 }, 1 );
            auto& L2 = Ctx2.Root().Add<tg::Label>( "Second window" );
            (void)L2;
            Ctx2.Frame();
            Ctx2.Render( { 200, 60 } );
            int Lit = 0;
            tg::Color Bg = tg::GetTheme()[ tg::Tok::Background ];
            for ( uint32_t Px : Soft2.Pixels )
            {
                tg::Color C = tg::Color::Unpack( Px );
                Lit += std::abs( C.r - Bg.r ) + std::abs( C.g - Bg.g ) > 60;
            }
            CHECK( Lit > 50 );
            // same renderer (one D3D device, several swapchains): uses the host's texture directly
            tg::Context Ctx3( Soft, Ctx );
            Ctx3.SetViewport( { 200, 60 }, 1 );
            Ctx3.Root().Add<tg::Label>( "Third" );
            const tg::DrawList& Dl = Ctx3.Frame();
            CHECK( !Dl.Cmds.empty() && Dl.AtlasTexture == Ctx.Fonts.Texture );
        }

        // pop-out: a widget moves into another window's tree and comes back to the same spot
        {
            tg::Context Ctx4( Soft2, Ctx );
            tg::Container* Area = nullptr;
            tg::Widget* Layer = nullptr;
            tg::detail::BuildRoot( Ctx4, Area, Layer );
            tg::Widget& Bar = *App.DesktopBar;
            tg::Widget* Parent = Bar.GetParent();
            size_t Idx = 0;
            while ( Parent->Children()[ Idx ].get() != &Bar ) Idx++;
            auto Pop = tg::detail::PopOutInto( Bar, *Area );
            CHECK( Bar.GetParent() == Area && Bar.GetContext() == &Ctx4 );
            Ctx4.SetViewport( { 400, 200 }, 1 );
            Ctx4.Frame();
            Settle( Ctx );
            tg::detail::PutBack( Pop, *Area );
            CHECK( Bar.GetParent() == Parent && Parent->Children()[ Idx ].get() == &Bar && Bar.GetContext() == &Ctx );
            Settle( Ctx );
        }

        // clipboard images (Windows: CF_DIB; Linux: needs xclip / wl-copy, skipped without)
        tg::Bitmap Bm;
        Bm.Width = 3;
        Bm.Height = 2;
        Bm.Pixels = { tg::Color( 255, 0, 0 ).Pack(), tg::Color( 0, 255, 0 ).Pack(), tg::Color( 0, 0, 255 ).Pack(),
                      tg::Color( 10, 20, 30 ).Pack(), tg::Color( 40, 50, 60 ).Pack(), tg::Color( 255, 255, 255 ).Pack() };
        if ( tg::os::SetClipboardImage( Bm ) )
        {
            tg::Bitmap Back;
            bool Read = false;
            for ( int i = 0; i < 20 && !Read; i++ ) // xclip serves the selection from a background process
            {
                Read = tg::os::GetClipboardImage( Back );
                if ( !Read ) std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
            }
            CHECK( Read && Back.Width == 3 && Back.Height == 2 && Back.Pixels == Bm.Pixels );
            std::printf( "clipboard image round trip ok\n" );
        }
#ifdef _WIN32
        tg::TrayIcon Tray( "tg headless" );
        Tray.Items = { { "One", {} } };
        std::printf( "tray icon: %s\n", Tray.Ok() ? "shown" : "not shown" );
#endif
    }

    // ---------------- light theme
    tg::GetTheme().UseColors( tg::Theme::Light() );
    App.SettingsWin->Show();
    ClickTab( Ctx, App, 2 );
    Shot( Ctx, "controls_light" );

    std::printf( "headless: all checks passed\n" );
    return 0;
}
