// Headless run: software renderer, scripted input, PNG screenshots, and assertions.
// Doubles as the test suite (ctest) — no GPU or window needed.
#include "../common/demo_ui.h"
#include "soft_renderer.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK( x ) do { if ( !( x ) ) { std::fprintf( stderr, "CHECK failed: %s (line %d)\n", #x, __LINE__ ); return 1; } } while ( 0 )

static tg::SoftRenderer Soft;
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
    Click( Ctx, *App.NavButtons[ 1 ] );
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
    Click( Ctx, *App.NavButtons[ 2 ] );
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

    // ---------------- floating windows: drag by title, resize, close
    Click( Ctx, *App.NavButtons[ 3 ] );
    Settle( Ctx );
    tg::Vec2 Pos0 = App.SettingsWin->GetPosition();
    tg::Rect B = App.SettingsWin->Bounds;
    Drag( Ctx, { B.x + 100, B.y + 18 }, { B.x + 160, B.y + 58 } );
    tg::Vec2 Pos1 = App.SettingsWin->GetPosition();
    CHECK( Pos1.x == Pos0.x + 60 && Pos1.y == Pos0.y + 40 );
    // clicking the other window raises it above
    Click( Ctx, *Find<tg::TextInput>( *App.PlayersWin ) );
    CHECK( App.Pages[ 3 ]->Children().back().get() == App.PlayersWin );
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

    // ---------------- light theme
    tg::GetTheme().UseColors( tg::Theme::Light() );
    App.SettingsWin->Show();
    Click( Ctx, *App.NavButtons[ 2 ] );
    Shot( Ctx, "controls_light" );

    std::printf( "headless: all checks passed\n" );
    return 0;
}
