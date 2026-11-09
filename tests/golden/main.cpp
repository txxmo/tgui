// Golden-image tests: small scenes rendered with the software renderer and compared with
// reference PNGs in tests/golden/ref. Catches visual regressions that the behaviour tests can't.
//
//   tg_golden <ref-dir> <out-dir>            compare (CTest runs this)
//   tg_golden <ref-dir> <out-dir> --update   (re)write the references after an intended change
//
// On a mismatch it writes <out-dir>/<name>.actual.png and <name>.diff.png (differing pixels in red).
// The references are made with the built-in stb_truetype rasterizer: FreeType builds skip this test.
#if defined( TG_GOLDEN_SINGLE_HEADER ) // the same scenes, built from the amalgamated tg_single.h
#define TG_IMPLEMENTATION
#define TG_BACKEND_SOFT
#include "tg_single.h"
#else
#include "soft_renderer.h"
#include "tg/tg.h"
#endif
#if defined( TG_GOLDEN_VULKAN ) // the same scenes through the Vulkan renderer (headless, read back)
#include "vk_renderer.h"
#endif
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#ifndef STBI_INCLUDE_STB_IMAGE_H // (the single header may already carry it)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image.h"
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif
#endif

using namespace tg;

struct Scene
{
    const char* Name;
    Vec2 Size;
    std::function<void( Context&, Container& )> Build;
};

static void Settle( Context& C )
{
    C.Frame();
    for ( int i = 0; i < 240 && C.NeedsFrame(); i++ ) C.Frame();
}

// ---- the scenes: fixed data, no clocks, no hover
static std::vector<Scene> Scenes()
{
    std::vector<Scene> S;
    S.push_back( { "controls", { 380, 300 }, []( Context&, Container& Root ) {
        static bool A = true, B = false, T = true;
        static float V = 0.35f;
        static int Q = 2;
        auto& G = Root.Card( "Controls" );
        G.Checkbox( "Enabled", &A );
        G.Checkbox( "Disabled look", &B ).Enabled = false;
        G.Toggle( "Notifications", &T );
        G.Slider( "Volume", &V, 0, 1 );
        G.Combo( "Quality", { "Low", "Medium", "High" }, &Q );
        auto& R = G.Row( 8 );
        R.Button( "Cancel" );
        R.Button( "Save" ).Style = Button::Primary();
        G.Progress( 0.62f );
    } } );
    S.push_back( { "text", { 380, 200 }, []( Context&, Container& Root ) {
        Root.Heading( "Heading" );
        Root.Text( "Body text that wraps onto a second line when it runs out of room in the box.", true );
        Root.Caption( "a dim caption" );
        Root.Rich( "**bold**, *italic*, `code`, <accent>accent</> and an icon " + Icon::Rocket );
    } } );
    S.push_back( { "charts", { 420, 260 }, []( Context&, Container& Root ) {
        auto& R = Root.Row( 12 );
        auto& B = R.BarChart();
        B.Height = 140;
        B.SetBars( { { "Mon", 4 }, { "Tue", 7 }, { "Wed", 3 }, { "Thu", 9, Tok::Success }, { "Fri", 6 } } );
        B.Layout.Width = Grow();
        auto& D = R.Donut();
        D.Size = 120;
        D.Legend = false;
        D.SetSlices( { { "a", 3, Tok::Accent }, { "b", 2, Tok::Success }, { "c", 1, Tok::Warning } } );
        auto& Sp = Root.Sparkline( 12 );
        for ( float V : { 3.f, 5.f, 4.f, 8.f, 6.f, 9.f, 7.f, 10.f, 6.f, 4.f, 7.f, 8.f } ) Sp.Push( V );
    } } );
    S.push_back( { "table", { 420, 220 }, []( Context&, Container& Root ) {
        auto& T = Root.Table( { "Name", "CPU", "Memory" } );
        T.AddRow( { "nginx", "3.2%", "120 MB" } );
        T.AddRow( { "postgres", "11.0%", "1.4 GB" } );
        T.AddRow( { "redis", "0.4%", "64 MB" } );
        T.AddRow( { "worker", "57.9%", "800 MB" } );
        T.SetSelected( 1, true );
    } } );
    S.push_back( { "dock", { 520, 300 }, []( Context&, Container& Root ) {
        auto& Dk = Root.Dock();
        auto& Files = Dk.AddPanel( "Files", "files" );
        auto& Code = Dk.AddPanel( "Editor", "editor" );
        auto& Log = Dk.AddPanel( "Log", "log" );
        Dk.AddPanel( "Notes", "notes" );
        Dk.Dock( Files, &Code, DockSide::Left, 0.3f );
        Dk.Dock( Log, &Code, DockSide::Bottom, 0.35f );
        Dk.Activate( Code );
        Files.Text( "src/" );
        Files.Text( "include/" );
        Code.Text( "int main() { return 0; }" );
        Log.Text( "build ok" );
        Dk.Float( *Dk.Find( "notes" ), { 300, 30, 190, 120 } );
        Dk.Find( "notes" )->Text( "floating" );
    } } );
    S.push_back( { "effects", { 360, 140 }, []( Context&, Container& Root ) {
        struct Tile : Widget
        {
            Gradient G;
            explicit Tile( Gradient X ) : G( X ) { Layout.Width = Px( 100 ); Layout.Height = Px( 100 ); }
            void Draw( DrawList& D ) override
            {
                if ( Context* C = GetContext() ) FillGradient( D, *C, Bounds, G, 10 );
            }
        };
        auto& R = Root.Row( 12 );
        R.Add<Tile>( Gradient::Linear( 0, Color::Hex( 0x6EB7D4 ), Color::Hex( 0xB277D6 ) ) );
        R.Add<Tile>( Gradient::Radial( Color::Hex( 0xF5C25B ), Color::Hex( 0xE5534B ) ) );
        R.Add<Tile>( Gradient::Conic( Color::Hex( 0xE5534B ), Color::Hex( 0x3FB97A ), Color::Hex( 0x4F8FE0 ) ) );
    } } );
    S.push_back( { "light", { 380, 180 }, []( Context&, Container& Root ) {
        GetTheme().UseColors( Theme::Light() );
        static bool A = true;
        static float V = 0.7f;
        auto& G = Root.Card( "Light theme" );
        G.Checkbox( "Checked", &A );
        G.Slider( "Value", &V, 0, 1 );
        G.Button( "Primary" ).Style = Button::Primary();
    } } );
    return S;
}

static bool Load( const std::string& Path, int& W, int& H, std::vector<uint32_t>& Px )
{
    FILE* F = std::fopen( Path.c_str(), "rb" );
    if ( !F ) return false;
    std::vector<unsigned char> Bytes;
    unsigned char Buf[ 65536 ];
    for ( size_t N; ( N = std::fread( Buf, 1, sizeof Buf, F ) ) > 0; ) Bytes.insert( Bytes.end(), Buf, Buf + N );
    std::fclose( F );
    int N = 0;
    unsigned char* D = stbi_load_from_memory( Bytes.data(), int( Bytes.size() ), &W, &H, &N, 4 );
    if ( !D ) return false;
    Px.assign( (const uint32_t*)D, (const uint32_t*)D + size_t( W ) * H );
    stbi_image_free( D );
    return true;
}

static void Save( SoftRenderer& R, const std::string& Path ) { R.SavePNG( Path ); }

int main( int Argc, char** Argv )
{
    if ( Argc < 3 )
    {
        std::fprintf( stderr, "usage: tg_golden <ref-dir> <out-dir> [--update]\n" );
        return 2;
    }
    std::string Ref = Argv[ 1 ], Out = Argv[ 2 ];
    bool Update = Argc > 3 && !std::strcmp( Argv[ 3 ], "--update" );
    // How different a pixel may be (any channel), and how many pixels may differ: anti-aliasing and
    // float rounding differ a little between compilers / CPUs; real regressions change far more.
    int Tol = 40;
    double MaxFrac = 0.004;
#if defined( TG_GOLDEN_VULKAN )
    // GPUs interpolate and filter a little differently from the software rasterizer (and from each other)
    Tol = 48;
    MaxFrac = 0.02;
    VulkanRenderer Gpu;
    if ( !Gpu.InitHeadless() )
    {
        std::printf( "golden (vulkan): no Vulkan device, skipped\n" );
        return 0;
    }
    std::printf( "golden (vulkan): %s\n", Gpu.DeviceName() );
#endif

    int Failed = 0;
    for ( Scene& Sc : Scenes() )
    {
        GetTheme() = Theme::Dark();
        SoftRenderer R;
#if defined( TG_GOLDEN_VULKAN )
        Context C( Gpu );
#else
        Context C( R );
#endif
        C.SetFixedTimestep( 1.f / 60 );
        C.TooltipDelay = 1e9f;
        if ( !C.LoadDefaultFonts( 1.f ) ) { std::fprintf( stderr, "fonts failed\n" ); return 1; }
        C.SetViewport( Sc.Size, 1 );
        C.OnMouseMove( { -1000, -1000 } );
        auto& Root = C.Root().Add<Container>( Dir::Column, 10, Insets( 12 ) );
        Root.Layout.Width = Grow();
        Root.Layout.Height = Grow();
        Sc.Build( C, Root );
        Settle( C );
        C.Render( Sc.Size );
#if defined( TG_GOLDEN_VULKAN )
        R.Width = Gpu.Captured.W; // compare / save the GPU's frame through the same code
        R.Height = Gpu.Captured.H;
        R.Pixels = Gpu.Captured.Px;
        if ( Update ) { std::printf( "%-10s (vulkan never updates references)\n", Sc.Name ); continue; }
#endif
        std::string RefPath = Ref + "/" + Sc.Name + ".png";
        if ( Update )
        {
            Save( R, RefPath );
            std::printf( "updated %s\n", RefPath.c_str() );
            continue;
        }
        int W = 0, H = 0;
        std::vector<uint32_t> Want;
        if ( !Load( RefPath, W, H, Want ) )
        {
            std::printf( "%-10s MISSING reference %s (run with --update)\n", Sc.Name, RefPath.c_str() );
            Save( R, Out + "/" + Sc.Name + ".actual.png" );
            Failed++;
            continue;
        }
        if ( W != R.Width || H != R.Height )
        {
            std::printf( "%-10s FAIL size %dx%d, reference %dx%d\n", Sc.Name, R.Width, R.Height, W, H );
            Save( R, Out + "/" + Sc.Name + ".actual.png" );
            Failed++;
            continue;
        }
        size_t Bad = 0;
        int Worst = 0;
        SoftRenderer Diff;
        Diff.Width = W;
        Diff.Height = H;
        Diff.Pixels.resize( size_t( W ) * H );
        for ( size_t i = 0; i < Want.size(); i++ )
        {
            Color A = Color::Unpack( R.Pixels[ i ] ), B = Color::Unpack( Want[ i ] );
            int D = std::max( { std::abs( A.r - B.r ), std::abs( A.g - B.g ), std::abs( A.b - B.b ), std::abs( A.a - B.a ) } );
            Worst = std::max( Worst, D );
            bool Off = D > Tol;
            Bad += Off;
            uint8_t L = uint8_t( ( B.r + B.g + B.b ) / 12 ); // dim copy of the reference, differences in red
            Diff.Pixels[ i ] = Off ? Color( 255, 40, 40 ).Pack() : Color( L, L, L ).Pack();
        }
        double Frac = double( Bad ) / double( Want.size() );
        bool Ok = Frac <= MaxFrac;
        std::printf( "%-10s %s  %zu px differ (%.3f%%), max delta %d\n", Sc.Name, Ok ? "ok  " : "FAIL", Bad, Frac * 100, Worst );
        if ( !Ok )
        {
            Save( R, Out + "/" + Sc.Name + ".actual.png" );
            Save( Diff, Out + "/" + Sc.Name + ".diff.png" );
            Failed++;
        }
    }
    if ( Failed ) std::printf( "golden: %d scene(s) failed\n", Failed );
    else std::printf( "golden: all scenes match\n" );
    return Failed ? 1 : 0;
}
