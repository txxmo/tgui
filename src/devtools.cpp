// F12 inspector and Ctrl+F12 performance stats, drawn on top of everything.
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/theme.h"
#include <cstdio>
#include <string>
#include <typeinfo>
#include <vector>
#if defined( __GNUC__ ) || defined( __clang__ )
#include <cxxabi.h>
#include <cstdlib>
#endif

namespace tg
{
    static std::string TypeName( const Widget& W )
    {
        const char* N = typeid( W ).name();
#if defined( __GNUC__ ) || defined( __clang__ )
        int St = 0;
        char* D = abi::__cxa_demangle( N, nullptr, nullptr, &St );
        std::string S = St == 0 && D ? D : N;
        std::free( D );
#else
        std::string S = N; // MSVC: "class tg::Button"
        for ( const char* P : { "class ", "struct " } )
            if ( S.rfind( P, 0 ) == 0 ) S = S.substr( std::string( P ).size() );
#endif
        return S;
    }

    static std::string SizeText( const Size& S )
    {
        char B[ 32 ];
        switch ( S.K )
        {
        case Size::Kind::Fit: return "Fit";
        case Size::Kind::Fixed: std::snprintf( B, sizeof( B ), "Px(%g)", S.V ); return B;
        case Size::Kind::Grow: std::snprintf( B, sizeof( B ), "Grow(%g)", S.V ); return B;
        }
        return "?";
    }

    static void Panel( DrawList& D, Vec2 At, const std::vector<std::pair<std::string, Color>>& Lines, Rect View )
    {
        const Font& F = *GetTheme().Small;
        float W = 0, H = 0;
        for ( auto& [ T, C ] : Lines ) W = std::max( W, F.Width( T ) );
        H = Lines.size() * F.LineHeight;
        Rect R = { At.x, At.y, W + 20, H + 14 };
        if ( R.Right() > View.Right() - 4 ) R.x = View.Right() - 4 - R.w;
        if ( R.Bottom() > View.Bottom() - 4 ) R.y = View.Bottom() - 4 - R.h;
        R.x = std::max( R.x, 4.f );
        R.y = std::max( R.y, 4.f );
        D.Shadow( R, 8, Color( 0, 0, 0, 140 ), 10, { 0, 3 } );
        D.RectFilled( R, Color( 14, 16, 21, 240 ), 8 );
        D.RectStroke( R, Color( 255, 255, 255, 30 ), 8 );
        float Y = R.y + 7;
        for ( auto& [ T, C ] : Lines ) { D.Text( F, { R.x + 10, Y }, T, C ); Y += F.LineHeight; }
    }

    void Context::DrawDevTools()
    {
        DrawList& D = List;
        D.PushClip( { 0, 0, Viewport.x, Viewport.y }, false );
        const Color Hi( 110, 183, 212 ), Dim( 160, 168, 180 ), Txt( 230, 233, 238 ), Warn( 240, 180, 80 );
        if ( ShowInspector )
        {
            Widget* W = DeepestAt( Mouse );
            if ( W )
            {
                // the widget, its padding, its parent
                if ( Widget* P = W->GetParent() ) D.RectStroke( P->Bounds, Color( 240, 180, 80, 120 ), 0, 1 );
                D.RectFilled( W->Bounds, Color( 110, 183, 212, 40 ) );
                Rect In = W->Bounds.Shrink( W->Layout.Padding );
                if ( In.w != W->Bounds.w || In.h != W->Bounds.h ) D.RectStroke( In, Color( 120, 220, 140, 160 ), 0, 1 );
                D.RectStroke( W->Bounds, Hi, 0, 1.5f );
                std::vector<std::pair<std::string, Color>> L;
                L.push_back( { TypeName( *W ), Hi } );
                char B[ 160 ];
                Rect Bo = W->Bounds;
                std::snprintf( B, sizeof( B ), "x %.1f  y %.1f   %.1f x %.1f", Bo.x, Bo.y, Bo.w, Bo.h );
                L.push_back( { B, Txt } );
                static const char* Dirs[] = { "Column", "Row", "Stack", "Free" };
                const LayoutSpec& S = W->Layout;
                std::snprintf( B, sizeof( B ), "%s  gap %g   W %s  H %s", Dirs[ int( S.Direction ) ], S.Gap, SizeText( S.Width ).c_str(), SizeText( S.Height ).c_str() );
                L.push_back( { B, Txt } );
                std::snprintf( B, sizeof( B ), "padding %g %g %g %g", S.Padding.Left, S.Padding.Top, S.Padding.Right, S.Padding.Bottom );
                L.push_back( { B, Dim } );
                std::string Flags;
                auto Flag = [ & ]( bool On, const char* N ) { if ( On ) Flags += std::string( Flags.empty() ? "" : "  " ) + N; };
                Flag( W->Interactive, "interactive" );
                Flag( W->Focusable, "focusable" );
                Flag( !W->Enabled, "disabled" );
                Flag( W == HoverW, "hovered" );
                Flag( W == FocusW, "focused" );
                Flag( W->CanBind(), "bindable" );
                Flag( bool( W->ContextMenu ), "menu" );
                if ( !Flags.empty() ) L.push_back( { Flags, Warn } );
                std::snprintf( B, sizeof( B ), "%zu children", W->Children().size() );
                L.push_back( { B, Dim } );
                if ( W->CanBind() && !W->BindName().empty() ) L.push_back( { "label: " + W->BindName(), Dim } );
                // path from the root
                std::vector<std::string> Path;
                for ( Widget* P = W; P; P = P->GetParent() )
                {
                    std::string N = TypeName( *P );
                    if ( N.rfind( "tg::", 0 ) == 0 ) N = N.substr( 4 );
                    Path.insert( Path.begin(), N );
                }
                std::string Ps;
                for ( size_t i = Path.size() > 6 ? Path.size() - 6 : 0; i < Path.size(); i++ ) Ps += ( Ps.empty() ? "" : " > " ) + Path[ i ];
                L.push_back( { Ps, Dim } );
                Panel( D, { Mouse.x + 16, Mouse.y + 18 }, L, { 0, 0, Viewport.x, Viewport.y } );
            }
        }
        if ( ShowStats )
        {
            double Now = Clock();
            FpsCount++;
            if ( Now - FpsWindowStart >= 1 ) { Fps = float( FpsCount / ( Now - FpsWindowStart ) ); FpsCount = 0; FpsWindowStart = Now; }
            size_t Widgets = 0;
            std::vector<Widget*> St = { RootWidget.get(), OverlayWidget.get() };
            while ( !St.empty() ) { Widget* W = St.back(); St.pop_back(); Widgets++; for ( auto& K : W->Children() ) St.push_back( K.get() ); }
            char B[ 128 ];
            std::vector<std::pair<std::string, Color>> L;
            L.push_back( { "tg stats  (Ctrl+F12)", Hi } );
            std::snprintf( B, sizeof( B ), "frame %.2f ms   layout %.2f ms", FrameCpu * 1000, LayoutCpu * 1000 );
            L.push_back( { B, Txt } );
            std::snprintf( B, sizeof( B ), "%.0f frames/s (only redraws on change)", Fps );
            L.push_back( { B, Dim } );
            std::snprintf( B, sizeof( B ), "%zu draw calls   %zu vertices   %zu indices", LastCmds, LastVerts, LastIdx );
            L.push_back( { B, Txt } );
            std::snprintf( B, sizeof( B ), "%zu widgets   atlas %dx%d, %zu glyphs", Widgets, Fonts.Width, Fonts.Height, Fonts.GlyphCount() );
            L.push_back( { B, Dim } );
            Panel( D, { Viewport.x - 4, 4 }, L, { 0, 0, Viewport.x, Viewport.y } );
            RequestFrameIn( 0.5 ); // keep the numbers fresh
        }
        D.PopClip();
        LastCmds = D.Cmds.size();
        LastVerts = D.Vtx.size();
        LastIdx = D.Idx.size();
    }
}
