#include "tg/widgets/window.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
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
        Visible = true;
        BringToFront();
        Invalidate();
    }

    void Window::Close()
    {
        if ( !Visible ) return;
        Visible = false;
        if ( OnClose ) OnClose();
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

        if ( S.Shadow ) D.Shadow( Bounds, S.Radius, Color( 0, 0, 0, 120 ), 14, { 0, 6 } );
        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );

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
    }

    Widget* Window::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
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
