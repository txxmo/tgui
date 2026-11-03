#include "tg/widgets/groupbox.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    GroupboxStyle& Groupbox::Defaults()
    {
        static GroupboxStyle S;
        return S;
    }

    Groupbox::Groupbox( std::string T ) : Container( Dir::Column, 8, Insets( 12, 10 ) ), Title( std::move( T ) ), Frame( Defaults() )
    {
        Interactive = true;
        LabelWidth = 0;
    }

    Vec2 Groupbox::Measure( float Avail )
    {
        const Insets& P = Layout.Padding;
        Vec2 C = MeasureChildren( std::max( 0.f, Avail - P.Horizontal() ) );
        float Hw = Title.empty() ? 0 : GetTheme().Bold->Width( Title ) + 24;
        return { std::max( C.x + P.Horizontal(), Hw ), C.y + P.Vertical() + ( Title.empty() ? 0 : Frame.HeaderHeight ) };
    }

    void Groupbox::Arrange( Rect R )
    {
        Bounds = R;
        float Hh = Title.empty() ? 0 : Frame.HeaderHeight;
        HeaderR = { R.x, R.y, R.w, Hh };
        BodyR = { R.x, R.y + Hh, R.w, R.h - Hh };

        // Measure content; if it overflows, leave room for the scrollbar.
        const Insets& P = Layout.Padding;
        float InnerW = BodyR.w - P.Horizontal();
        ContentH = MeasureChildren( InnerW ).y + P.Vertical();
        if ( ContentH > BodyR.h + 0.5f ) ContentH = MeasureChildren( InnerW - 8 ).y + P.Vertical();
        Target = Clamp( Target, 0, MaxScroll() );
        Scroll = Clamp( Scroll, 0, MaxScroll() );
        Place();
    }

    void Groupbox::Place()
    {
        const Insets& P = Layout.Padding;
        float W = BodyR.w - P.Horizontal() - ( MaxScroll() > 0 ? 8 : 0 );
        float H = std::max( ContentH, BodyR.h ) - P.Vertical();
        ArrangeChildren( { BodyR.x + P.Left, BodyR.y + P.Top - std::round( Scroll ), W, H } );
    }

    void Groupbox::ScrollTo( float Y )
    {
        Target = Clamp( Y, 0, MaxScroll() );
        Redraw();
    }

    Rect Groupbox::ThumbRect() const
    {
        float Track = BodyR.h - 8;
        float H = std::max( 20.f, Track * BodyR.h / std::max( ContentH, 1.f ) );
        float T = MaxScroll() > 0 ? Scroll / MaxScroll() : 0;
        return { BodyR.Right() - 7, BodyR.y + 4 + ( Track - H ) * T, 4, H };
    }

    void Groupbox::Draw( DrawList& D )
    {
        const GroupboxStyle& S = Frame;
        if ( Scroll != Target ) { Animate( Scroll, Target, 22.f ); Place(); }

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        if ( !Title.empty() )
        {
            const Font& F = *GetTheme().Bold;
            D.RectFilled( HeaderR, S.HeaderFill.Resolve(), S.Radius, CornersTop );
            if ( S.HeaderSeparator ) D.RectFilled( { HeaderR.x, HeaderR.Bottom() - 1, HeaderR.w, 1 }, S.Stroke.Resolve() );
            float W = F.Width( Title );
            float X = S.TitleAlign == Align::Center ? std::floor( HeaderR.x + ( HeaderR.w - W ) * 0.5f )
                    : S.TitleAlign == Align::End ? HeaderR.Right() - 12 - W : HeaderR.x + 12;
            D.PushClip( HeaderR );
            D.Text( F, { X, std::floor( HeaderR.y + ( HeaderR.h - F.LineHeight ) * 0.5f ) }, Title, S.TitleColor.Resolve() );
            D.PopClip();
        }

        D.PushClip( BodyR );
        DrawChildren( D );
        D.PopClip();

        if ( MaxScroll() > 0 ) D.RectFilled( ThumbRect(), ( DraggingThumb ? GetTheme()[ Tok::TextDim ] : S.Thumb.Resolve() ), 2 );
        D.RectStroke( Bounds, S.Stroke.Resolve(), S.Radius );
    }

    Widget* Groupbox::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( MaxScroll() > 0 && ThumbRect().Shrink( -4 ).Contains( P ) ) return this;
        if ( BodyR.Contains( P ) )
            for ( auto It = Children().rbegin(); It != Children().rend(); ++It )
                if ( Widget* H = ( *It )->HitTest( P ) ) return H;
        return this;
    }

    bool Groupbox::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseWheel:
            if ( MaxScroll() <= 0 ) return false;
            ScrollTo( Target - E.Wheel.y * 40 );
            return true;
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left || MaxScroll() <= 0 || !ThumbRect().Shrink( -4 ).Contains( E.Pos ) ) return false;
            DraggingThumb = true;
            DragOffset = E.Pos.y - ThumbRect().y;
            return true;
        case EventType::MouseMove:
        {
            if ( !DraggingThumb ) return false;
            float Track = BodyR.h - 8 - ThumbRect().h;
            float T = Track > 0 ? Clamp( ( E.Pos.y - DragOffset - BodyR.y - 4 ) / Track, 0, 1 ) : 0;
            Scroll = Target = T * MaxScroll();
            Place();
            Redraw();
            return true;
        }
        case EventType::MouseUp:
            if ( !DraggingThumb ) return false;
            DraggingThumb = false;
            Redraw();
            return true;
        default:
            return false;
        }
    }
}
