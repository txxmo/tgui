// Headless run: software renderer, scripted input, PNG screenshots, and a few assertions.
// Doubles as a smoke test (ctest) — no GPU or window needed.
#include "../common/demo_ui.h"
#include "soft_renderer.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef TG_ASSET_DIR
#define TG_ASSET_DIR "assets"
#endif

#define CHECK( x ) do { if ( !( x ) ) { std::fprintf( stderr, "CHECK failed: %s (line %d)\n", #x, __LINE__ ); return 1; } } while ( 0 )

static tg::SoftRenderer Soft;

static void Shot( tg::Context& Ctx, const char* OutDir, const char* Name, float Scale )
{
    Ctx.Frame();
    tg::Vec2 V = Ctx.GetViewport();
    Ctx.Render( { V.x * Scale, V.y * Scale } );
    std::string Path = std::string( OutDir ) + "/" + Name + ".png";
    Soft.SavePNG( Path );
    std::printf( "wrote %s\n", Path.c_str() );
}

static void Click( tg::Context& Ctx, tg::Widget& W )
{
    tg::Vec2 P = W.Bounds.Center();
    Ctx.OnMouseMove( P );
    Ctx.OnMouseButton( tg::MouseButton::Left, true );
    Ctx.OnMouseButton( tg::MouseButton::Left, false );
}

// Let animations settle (the software renderer is slow, so we just run layout+draw).
static void Settle( tg::Context& Ctx )
{
    for ( int i = 0; i < 40 && Ctx.NeedsFrame(); i++ ) Ctx.Frame();
}

int main( int Argc, char** Argv )
{
    const char* OutDir = Argc > 1 ? Argv[ 1 ] : ".";
    float Scale = Argc > 2 ? float( std::atof( Argv[ 2 ] ) ) : 1.f;

    tg::Context Ctx( Soft );
    CHECK( Ctx.LoadDefaultFonts( TG_ASSET_DIR "/fonts", Scale ) );
    Ctx.SetViewport( { 1280, 800 }, Scale );
    Ctx.SetFixedTimestep( 1.f / 60.f ); // deterministic animations

    DemoApp App;
    App.Build( Ctx );
    Ctx.Frame();

    // --- dashboard
    Settle( Ctx );
    Shot( Ctx, OutDir, "dashboard", Scale );

    // --- nav click switches page
    Click( Ctx, *App.NavButtons[ 1 ] );
    Ctx.Frame();
    CHECK( App.Page == 1 );

    // --- chat: type a message and press Enter
    Click( Ctx, *App.Composer );
    CHECK( Ctx.Focused() == App.Composer );
    for ( const char* p = "hello from the headless test"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
    CHECK( App.Composer->GetText() == "hello from the headless test" );
    size_t Before = App.Messages->Content().Children().size();
    Ctx.OnKey( tg::Key::Enter, true, 0 );
    CHECK( App.Messages->Content().Children().size() == Before + 1 );
    CHECK( App.Composer->GetText().empty() );
    for ( int i = 0; i < 12; i++ ) App.AddMessage( i % 2 ? "rika" : "mo", "filler message to overflow the chat log so it has to scroll", std::string( i < 10 ? "11:0" : "11:" ) + std::to_string( i ) );
    for ( const char* p = "half-typed reply"; *p; p++ ) Ctx.OnText( uint32_t( *p ) );
    App.TickScreenShare( 1.7 );
    Settle( Ctx );
    CHECK( App.Messages->GetScroll() > 0 ); // stuck to bottom after overflow
    Shot( Ctx, OutDir, "chat", Scale );

    // --- controls: toggle a bound checkbox
    Click( Ctx, *App.NavButtons[ 2 ] );
    Ctx.Frame();
    bool Was = App.Notifications;
    tg::Widget* NotifBox = nullptr;
    // find the checkbox bound to App.Notifications
    std::vector<tg::Widget*> Stack = { &Ctx.Root() };
    while ( !Stack.empty() )
    {
        tg::Widget* W = Stack.back(); Stack.pop_back();
        if ( auto* C = dynamic_cast<tg::Checkbox*>( W ); C && C->Bind == &App.Notifications ) NotifBox = C;
        for ( auto& K : W->Children() ) Stack.push_back( K.get() );
    }
    CHECK( NotifBox );
    Click( Ctx, *NotifBox );
    CHECK( App.Notifications != Was );
    Settle( Ctx );
    Shot( Ctx, OutDir, "controls", Scale );

    // --- light theme
    tg::GetTheme().UseColors( tg::Theme::Light() );
    App.ShowPage( 0 );
    Settle( Ctx );
    Shot( Ctx, OutDir, "dashboard_light", Scale );

    std::printf( "headless: all checks passed\n" );
    return 0;
}
