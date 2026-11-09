#include "tg/widgets/swatches.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/icons.h"
#include "tg/renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    static std::vector<Color> DefaultPalette()
    {
        std::vector<Color> P;
        for ( uint32_t H : { 0xE5534Bu, 0xF97316u, 0xE3A33Bu, 0xEAB308u, 0x3FB97Au, 0x14B8A6u, 0x4DB6C8u, 0x6EB7D4u, 0x4F8FE0u, 0x6366F1u,
                             0xB277D6u, 0xEC4899u, 0xF5F5F5u, 0xA3A3A3u, 0x525252u, 0x171717u } )
            P.push_back( Color::Hex( H ) );
        return P;
    }

    Swatches::Swatches( Color* B, std::vector<Color> P ) : Bind( B ), Palette( P.empty() ? DefaultPalette() : std::move( P ) )
    {
        Interactive = true;
        Focusable = true;
    }

    void Swatches::SetColor( Color C )
    {
        if ( C == GetColor() ) return;
        ( Bind ? *Bind : Own ) = C;
        if ( OnChange ) OnChange( C );
        Redraw();
    }

    BindValue Swatches::BindGet() const { Color C = GetColor(); return { { double( C.r ), double( C.g ), double( C.b ), double( C.a ) } }; }
    void Swatches::BindSet( const BindValue& V )
    {
        if ( V.V.size() >= 4 ) SetColor( Color( uint8_t( V.V[ 0 ] ), uint8_t( V.V[ 1 ] ), uint8_t( V.V[ 2 ] ), uint8_t( V.V[ 3 ] ) ) );
    }

    int Swatches::Cols() const
    {
        if ( Columns > 0 ) return Columns;
        return std::max( 1, int( ( Bounds.w + Gap ) / ( Size + Gap ) ) );
    }

    Rect Swatches::Cell( size_t i ) const
    {
        int C = Cols();
        return { Bounds.x + ( i % C ) * ( Size + Gap ), Bounds.y + ( i / C ) * ( Size + Gap ), Size, Size };
    }

    Vec2 Swatches::Measure( float Avail )
    {
        int C = Columns > 0 ? Columns : std::max( 1, int( ( Avail + Gap ) / ( Size + Gap ) ) );
        int Rows = int( ( Palette.size() + C - 1 ) / C );
        return { std::min( Avail, C * ( Size + Gap ) - Gap ), Rows * ( Size + Gap ) - Gap };
    }

    void Swatches::Draw( DrawList& D )
    {
        Color Cur = GetColor();
        for ( size_t i = 0; i < Palette.size(); i++ )
        {
            Rect R = Cell( i );
            bool Sel = Palette[ i ] == Cur, Hv = int( i ) == Hover && IsHovered();
            if ( Sel || Hv ) D.RectStroke( { R.x - 3, R.y - 3, R.w + 6, R.h + 6 }, Sel ? GetTheme()[ Tok::Accent ] : GetTheme()[ Tok::BorderStrong ], Radius + 3, Sel ? 2 : 1 );
            D.RectFilled( R, Palette[ i ], Radius );
            D.RectStroke( R, Color( 255, 255, 255, 24 ), Radius );
        }
    }

    bool Swatches::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            int H = -1;
            for ( size_t i = 0; i < Palette.size(); i++ )
                if ( Cell( i ).Contains( E.Pos ) ) H = int( i );
            if ( H != Hover ) { Hover = H; Redraw(); }
            return false;
        }
        case EventType::MouseDown: return E.Button == MouseButton::Left;
        case EventType::MouseUp:
            for ( size_t i = 0; i < Palette.size(); i++ )
                if ( Cell( i ).Contains( E.Pos ) ) SetColor( Palette[ i ] );
            return true;
        case EventType::KeyDown:
        {
            auto It = std::find( Palette.begin(), Palette.end(), GetColor() );
            int I = It == Palette.end() ? -1 : int( It - Palette.begin() ), N = int( Palette.size() );
            if ( E.KeyCode == Key::Right ) { SetColor( Palette[ size_t( std::min( N - 1, I + 1 ) ) ] ); return true; }
            if ( E.KeyCode == Key::Left ) { SetColor( Palette[ size_t( std::max( 0, I - 1 ) ) ] ); return true; }
            return false;
        }
        default: return false;
        }
    }

    // ------------------------------------------------------------ Eyedropper

    Eyedropper::Eyedropper( Color* B ) : Bind( B ) { Interactive = true; Focusable = true; KeyActivates = true; Tooltip = "Pick a color from the window"; }

    Vec2 Eyedropper::Measure( float ) { return { 32, 30 }; }

    void Eyedropper::Start()
    {
        Context* C = GetContext();
        if ( !C || !C->GetRenderer().CanCapture() ) return;
        Picking = true;
        C->Capture( this ); // every click / key comes to us until we're done
        C->GetRenderer().CaptureNext = true;
        C->RequestFrame();
    }

    void Eyedropper::Cancel()
    {
        Picking = false;
        if ( Context* C = GetContext() ) C->ReleaseCapture( this );
        Redraw();
    }

    bool Eyedropper::Sample( Vec2 P, Color& Out ) const
    {
        Context* C = GetContext();
        if ( !C ) return false;
        const Renderer::Capture& Cap = C->GetRenderer().Captured;
        if ( !Cap.Valid() ) return false;
        float S = C->GetScale();
        Out = Color::Unpack( Cap.At( int( P.x * S ), int( P.y * S ) ) );
        Out.a = 255;
        return true;
    }

    void Eyedropper::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        D.RectFilled( Bounds, Picking ? T[ Tok::Accent ].Fade( 0.25f ) : IsHovered() ? T[ Tok::Raised ] : T[ Tok::SurfaceAlt ], 6 );
        D.RectStroke( Bounds, Picking ? T[ Tok::Accent ] : T[ Tok::Border ], 6 );
        const Font& F = *T.Body;
        std::string Ic = Icon::ByName( "pipette" ).empty() ? Icon::Crosshair : Icon::ByName( "pipette" );
        D.Text( F, { std::floor( Bounds.Center().x - F.Width( Ic ) * 0.5f ), std::floor( Bounds.Center().y - F.LineHeight * 0.5f ) }, Ic, T[ Tok::Text ] );
        if ( !Picking ) return;
        // a loupe next to the cursor: 9x9 magnified pixels + the color under the cursor
        Context* C = GetContext();
        if ( !C ) return;
        const Renderer::Capture& Cap = C->GetRenderer().Captured;
        Vec2 M = C->MousePos();
        C->RequestFrameIn( 1 / 60.0 ); // follow the mouse while picking
        if ( !Cap.Valid() ) { C->GetRenderer().CaptureNext = true; C->RequestFrame(); return; }
        float S = C->GetScale();
        const int N = 9;
        const float Px = 9;
        Rect L = { M.x + 18, M.y + 18, N * Px + 8, N * Px + 30 };
        Vec2 V = C->GetViewport();
        if ( L.Right() > V.x ) L.x = M.x - 18 - L.w;
        if ( L.Bottom() > V.y ) L.y = M.y - 18 - L.h;
        D.PushClip( { 0, 0, V.x, V.y }, false );
        D.Shadow( L, 8, Color( 0, 0, 0, 150 ), 10, { 0, 3 } );
        D.RectFilled( L, T[ Tok::Surface ], 8 );
        int Cx = int( M.x * S ), Cy = int( M.y * S );
        for ( int y = 0; y < N; y++ )
            for ( int x = 0; x < N; x++ )
            {
                Color Pc = Color::Unpack( Cap.At( Cx + x - N / 2, Cy + y - N / 2 ) );
                Pc.a = 255;
                D.RectFilled( { L.x + 4 + x * Px, L.y + 4 + y * Px, Px, Px }, Pc );
            }
        D.RectStroke( { L.x + 4 + ( N / 2 ) * Px, L.y + 4 + ( N / 2 ) * Px, Px, Px }, Color( 255, 255, 255 ), 0, 1.5f );
        Color U;
        Sample( M, U );
        char B[ 16 ];
        std::snprintf( B, sizeof( B ), "#%02X%02X%02X", U.r, U.g, U.b );
        const Font& Sm = *T.Small;
        D.RectFilled( { L.x + 6, L.Bottom() - 20, 12, 12 }, U, 3 );
        D.Text( Sm, { L.x + 24, L.Bottom() - 22 }, B, T[ Tok::Text ] );
        D.PopClip();
    }

    bool Eyedropper::OnEvent( const Event& E )
    {
        if ( !Picking )
        {
            if ( E.Button != MouseButton::Left ) return false;
            if ( E.Type == EventType::MouseDown ) return true;
            if ( E.Type == EventType::MouseUp && IsHovered() ) { Start(); return true; }
            return false;
        }
        Context* C = GetContext();
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button == MouseButton::Left && C )
            {
                Color P;
                if ( Sample( C->MousePos(), P ) )
                {
                    if ( Bind ) *Bind = P;
                    if ( OnPick ) OnPick( P );
                }
            }
            Cancel();
            return true;
        case EventType::KeyDown: if ( E.KeyCode == Key::Escape ) Cancel(); return true;
        default: return true;
        }
    }
}
