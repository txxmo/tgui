#include "tg/widgets/window.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <vector>
#include <cmath>

namespace tg
{
    WindowStyle& Window::Defaults()
    {
        static WindowStyle S;
        return S;
    }

    WindowStyle Window::Classic()
    {
        WindowStyle S = Defaults();
        S.CenterTitle = true;
        S.AccentLine = true;
        S.TitleHeight = 30;
        S.Radius = 6;
        return S;
    }

    Window::Window( std::string T, Vec2 Pos, Vec2 Size ) : Container( Dir::Column, 10, Insets( 14 ) ), Title( std::move( T ) ), Chrome( Defaults() )
    {
        Interactive = true;
        RaiseOnClick = true;
        Layout.Offset = Pos;
        Layout.Width = Px( Size.x );
        Layout.Height = Size.y > 0 ? Px( Size.y ) : Fit();
        LabelWidth = 80;
    }

    void Window::Show()
    {
        Closing = false; // reopening mid-close just reverses
        Visible = true;
        BringToFront();
        Invalidate();
    }

    void Window::Close()
    {
        if ( !Visible || Closing ) return;
        Closing = true; // no input from now on; Draw() plays the animation and then hides it
        if ( OnClose ) OnClose();
        if ( Chrome.OpenAnim == WindowStyle::Anim::None || !GetContext() ) { Visible = false; Closing = false; OpenT = 0; }
        Invalidate();
    }

    void Window::SetPosition( Vec2 P )
    {
        Layout.Offset = P;
        Invalidate();
    }

    Vec2 Window::Measure( float Avail )
    {
        Vec2 C = MeasureChildren( std::max( 0.f, Avail - Layout.Padding.Horizontal() ) );
        float Foot = Footer.empty() ? 0 : GetTheme().Small->LineHeight;
        return { C.x + Layout.Padding.Horizontal(), C.y + Layout.Padding.Vertical() + Chrome.TitleHeight + Foot };
    }

    void Window::Arrange( Rect R )
    {
        Bounds = R;
        const float Th = Chrome.TitleHeight;
        TitleR = { R.x, R.y, R.w, Th };
        CloseR = { R.Right() - Th, R.y, Th, Th };
        GripR = { R.Right() - 14, R.Bottom() - 14, 14, 14 };
        BodyR = { R.x, R.y + Th, R.w, R.h - Th };
        Rect In = BodyR.Shrink( Layout.Padding );
        if ( !Footer.empty() ) In.h -= GetTheme().Small->LineHeight;
        ArrangeChildren( In );
    }

    void Window::Draw( DrawList& D )
    {
        const WindowStyle& S = Chrome;
        const Font& F = *GetTheme().Bold;
        Animate( CloseHoverT, OverClose && IsHovered() ? 1.f : 0.f, 20.f );

        // ---- open / close animation
        Context* Cx = GetContext();
        float Dt = Cx ? Cx->DeltaTime() : 1.f;
        if ( S.OpenAnim == WindowStyle::Anim::None ) OpenT = Closing ? 0.f : 1.f;
        else if ( !Closing ) OpenT = std::min( 1.f, OpenT + Dt / std::max( 0.01f, S.OpenTime ) );
        else OpenT = std::max( 0.f, OpenT - Dt / std::max( 0.01f, S.CloseTime ) );
        if ( Closing && OpenT <= 0 ) { Visible = false; Closing = false; Invalidate(); return; }
        if ( Cx && OpenT < 1 ) Cx->RequestFrame();
        const float T = OpenT;
        const bool Animating = T < 1;
        auto Sat = []( float X ) { return std::clamp( X, 0.f, 1.f ); };
        // shape: Spin starts as a circle in the middle and unfolds into the window
        float Morph = S.OpenAnim == WindowStyle::Anim::Spin ? EaseOutCubic( Sat( ( T - 0.12f ) / 0.7f ) ) : 1.f;
        float Dm = std::min( Bounds.w, Bounds.h ) * 0.42f;
        Rect Circle = { Bounds.Center().x - Dm * 0.5f, Bounds.Center().y - Dm * 0.5f, Dm, Dm };
        auto Lerp = []( float A, float B, float K ) { return A + ( B - A ) * K; };
        Rect Shape = { Lerp( Circle.x, Bounds.x, Morph ), Lerp( Circle.y, Bounds.y, Morph ), Lerp( Circle.w, Bounds.w, Morph ), Lerp( Circle.h, Bounds.h, Morph ) };
        float Rr = Lerp( Dm * 0.5f, S.Radius, Morph );
        size_t V0 = D.Vtx.size(), C0 = D.Cmds.size();

        if ( S.Shadow ) D.Shadow( Shape, Rr, Color( 0, 0, 0, 120 ), 14, { 0, 6 } );
        D.RectFilled( Shape, S.Fill.Resolve(), Rr );
        if ( Animating && S.OpenAnim == WindowStyle::Anim::Spin )
        {
            D.RectStroke( Shape, S.Stroke.Resolve(), Rr );
            // a spinning accent arc on the circle while it's still round
            float Ring = 1 - Sat( T / 0.55f );
            if ( Ring > 0 )
            {
                D.Path.clear();
                float A0 = T * 9.f;
                D.PathArc( Shape.Center(), Rr - 1, A0, A0 + 4.2f );
                std::vector<Vec2> Arc = D.Path;
                D.Polyline( Arc.data(), int( Arc.size() ), S.Accent.Resolve().Fade( Ring ), 2.5f );
            }
        }
        const size_t VContent = D.Vtx.size();

        // title bar
        D.RectFilled( TitleR, S.TitleFill.Resolve(), S.Radius, CornersTop );
        if ( S.AccentLine ) D.RectFilled( { TitleR.x, TitleR.Bottom() - 2, TitleR.w, 2 }, S.Accent.Resolve() );
        else D.RectFilled( { TitleR.x, TitleR.Bottom() - 1, TitleR.w, 1 }, GetTheme()[ Tok::Border ] );
        float Reserve = Closable ? S.TitleHeight : 0;
        float Tw = F.Width( Title );
        float Tx = S.CenterTitle ? std::floor( TitleR.x + ( TitleR.w - Tw ) * 0.5f ) : TitleR.x + 14;
        D.PushClip( { TitleR.x, TitleR.y, TitleR.w - Reserve, TitleR.h } );
        D.Text( F, { Tx, std::floor( TitleR.y + ( TitleR.h - 2 - F.LineHeight ) * 0.5f ) }, Title, S.TitleText.Resolve() );
        D.PopClip();

        // close button: an X that turns red on hover
        if ( Closable )
        {
            Vec2 C = CloseR.Center();
            D.CircleFilled( C, 11, S.CloseHover.Resolve().Fade( CloseHoverT * 0.9f ) );
            Color Xc = Color::Lerp( GetTheme()[ Tok::TextDim ], Color( 255, 255, 255 ), CloseHoverT );
            D.Line( C + Vec2( -4, -4 ), C + Vec2( 4, 4 ), Xc, 1.6f );
            D.Line( C + Vec2( 4, -4 ), C + Vec2( -4, 4 ), Xc, 1.6f );
        }

        // body
        D.PushClip( BodyR );
        DrawChildren( D );
        D.PopClip();

        if ( !Footer.empty() )
        {
            const Font& Sf = *GetTheme().Small;
            D.Text( Sf, { Bounds.Right() - Layout.Padding.Right - Sf.Width( Footer ) - ( Resizable ? 10 : 0 ), Bounds.Bottom() - Layout.Padding.Bottom - Sf.LineHeight + 4 },
                    Footer, GetTheme()[ Tok::TextDim ] );
        }

        // resize grip: three diagonal ticks
        if ( Resizable )
        {
            Color Gc = GetTheme()[ OverGrip || Drag == DragMode::Resize ? Tok::Text : Tok::TextDim ].Fade( 0.7f );
            for ( int i = 1; i <= 3; i++ )
            {
                float K = 3.5f * i;
                D.Line( { Bounds.Right() - 3 - K, Bounds.Bottom() - 3 }, { Bounds.Right() - 3, Bounds.Bottom() - 3 - K }, Gc, 1.2f );
            }
        }
        D.RectStroke( Bounds, S.Stroke.Resolve(), S.Radius );

        if ( Animating )
        {
            Vec2 Ctr = Bounds.Center();
            switch ( S.OpenAnim )
            {
            case WindowStyle::Anim::Spin:
            {
                // contents fade in once the shape is mostly a window; the whole thing spins and pops in
                D.FadeFrom( VContent, Sat( ( T - 0.5f ) / 0.45f ) );
                float Sc = Lerp( 0.55f, 1.f, EaseOutBack( T, 1.1f ) ), Ang = ( 1 - EaseOutCubic( T ) ) * -1.1f;
                D.TransformFrom( V0, C0, Ctr, Sc, Ang );
                D.FadeFrom( V0, Sat( T * 3.f ) );
                break;
            }
            case WindowStyle::Anim::Zoom:
                D.TransformFrom( V0, C0, Ctr, Lerp( 0.9f, 1.f, EaseOutBack( T ) ) );
                D.FadeFrom( V0, EaseOutCubic( T ) );
                break;
            case WindowStyle::Anim::Fade: D.FadeFrom( V0, EaseOutCubic( T ) ); break;
            default: break;
            }
        }
    }

    Widget* Window::HitTest( Vec2 P )
    {
        if ( !Visible || Closing || !Bounds.Contains( P ) ) return nullptr;
        if ( TitleR.Contains( P ) || ( Resizable && GripR.Contains( P ) ) ) return this;
        if ( Widget* H = Widget::HitTest( P ) ) return H;
        return this; // the window body eats clicks so they don't hit what's behind
    }

    Cursor Window::GetCursor() const
    {
        if ( Drag == DragMode::Resize || OverGrip ) return Cursor::ResizeH;
        return Cursor::Arrow;
    }

    bool Window::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return true;
            if ( Closable && CloseR.Contains( E.Pos ) ) return true; // act on release
            if ( Resizable && GripR.Contains( E.Pos ) ) Drag = DragMode::Resize;
            else if ( Movable && TitleR.Contains( E.Pos ) ) Drag = DragMode::Move;
            DragStart = E.Pos;
            StartOffset = Layout.Offset;
            StartSize = Bounds.Size();
            return true;

        case EventType::MouseMove:
        {
            bool OC = Closable && CloseR.Contains( E.Pos ), OG = Resizable && GripR.Contains( E.Pos );
            if ( OC != OverClose || OG != OverGrip ) { OverClose = OC; OverGrip = OG; Redraw(); }
            if ( Drag == DragMode::None ) return false;
            Vec2 Dl = E.Pos - DragStart;
            if ( Drag == DragMode::Move )
            {
                // keep the title bar reachable
                Rect P = GetParent() ? GetParent()->Bounds : Bounds;
                Vec2 O = StartOffset + Dl;
                O.x = Clamp( O.x, 60 - Bounds.w, P.w - 60 );
                O.y = Clamp( O.y, 0, std::max( 0.f, P.h - Chrome.TitleHeight ) );
                Layout.Offset = O;
                // Only this window moved: re-arrange just it instead of the whole tree.
                Arrange( { P.x + O.x, P.y + O.y, Bounds.w, Bounds.h } );
                Redraw();
            }
            else
            {
                Layout.Width = Px( std::max( Chrome.MinWidth, StartSize.x + Dl.x ) );
                Layout.Height = Px( std::max( Chrome.MinHeight, StartSize.y + Dl.y ) );
                Invalidate();
            }
            return true;
        }

        case EventType::MouseUp:
            if ( Drag == DragMode::None && Closable && CloseR.Contains( E.Pos ) && IsHovered() ) Close();
            Drag = DragMode::None;
            return true;

        case EventType::MouseLeave:
            OverClose = OverGrip = false;
            Redraw();
            return false;

        default:
            return false;
        }
    }
}
