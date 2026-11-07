// Headless run: software renderer, scripted input, PNG screenshots, and assertions.
// Doubles as the test suite (ctest) — no GPU or window needed.
#include "../common/demo_ui.h"
#include "soft_renderer.h"
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

    // ---------------- combo: open, pick the 3rd item, closes
    Click( Ctx, *App.RegionCombo );
    CHECK( App.RegionCombo->IsOpen() );
    tg::Popup* P = OpenPopup( Ctx );
    CHECK( P );
    Settle( Ctx );
    Shot( Ctx, "combo_open" );
    tg::ListItem* Third = Find<tg::ListItem>( *P, 2 );
    CHECK( Third && Third->Text == "London" );
    Click( Ctx, *Third );
    CHECK( App.Region == 2 );
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
    Click( Ctx, *App.AlertCombo );
    P = OpenPopup( Ctx );
    CHECK( P );
    Settle( Ctx );
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
    CHECK( !App.SettingsWin->Visible );
    Ctx.OnKey( tg::Key::Insert, true, 0 ); Ctx.OnKey( tg::Key::Insert, false, 0 );
    Settle( Ctx );
    CHECK( App.SettingsWin->Visible );
    Click( Ctx, Kb );
    Ctx.OnKey( tg::Key::Insert, true, 0 ); Ctx.OnKey( tg::Key::Insert, false, 0 );
    CHECK( App.SettingsWin->Visible && App.AimKey == tg::Key::Insert );
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
    CHECK( !App.SettingsWin->Visible );

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
        // checkable item flips its bool
        Ctx.OnMouseButton( tg::MouseButton::Right, true );
        Ctx.OnMouseButton( tg::MouseButton::Right, false );
        Ctx.Frame();
        M = dynamic_cast<tg::Menu*>( OpenPopup( Ctx ) );
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

    // ---------------- light theme
    tg::GetTheme().UseColors( tg::Theme::Light() );
    App.SettingsWin->Show();
    ClickTab( Ctx, App, 2 );
    Shot( Ctx, "controls_light" );

    std::printf( "headless: all checks passed\n" );
    return 0;
}
