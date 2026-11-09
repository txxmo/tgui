#include "tg/widgets/treeview.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    TreeNode& TreeNode::Add( std::string T, int64_t I )
    {
        auto N = std::make_unique<TreeNode>();
        N->Text = std::move( T );
        N->Id = I;
        N->Parent = this;
        Children.push_back( std::move( N ) );
        return *Children.back();
    }

    void TreeNode::Remove( TreeNode* C )
    {
        Children.erase( std::remove_if( Children.begin(), Children.end(), [ C ]( auto& P ) { return P.get() == C; } ), Children.end() );
    }

    int TreeNode::Depth() const
    {
        int D = -1;
        for ( const TreeNode* N = this; N->Parent; N = N->Parent ) D++;
        return D;
    }

    TreeView::TreeView()
    {
        Interactive = true;
        Focusable = true;
        Layout.Height = Grow();
        Root.Expanded = true;
    }

    void TreeView::Clear()
    {
        Root.Children.clear();
        Selected = nullptr;
        Scroll = Target = 0;
        Redraw();
    }

    void TreeView::ExpandAll( bool E )
    {
        std::function<void( TreeNode& )> Walk = [ & ]( TreeNode& N ) { if ( N.HasChildren() ) N.Expanded = E; for ( auto& C : N.Children ) Walk( *C ); };
        for ( auto& C : Root.Children ) Walk( *C );
        if ( !E && Selected && Selected->Parent != &Root ) Selected = nullptr;
        Redraw();
    }

    void TreeView::Select( TreeNode* N )
    {
        if ( N == Selected ) return;
        Selected = N;
        if ( N && OnSelect ) OnSelect( *N );
        Redraw();
    }

    void TreeView::Toggle( TreeNode& N )
    {
        if ( !N.HasChildren() ) return;
        N.Expanded = !N.Expanded;
        if ( OnToggle ) OnToggle( N, N.Expanded );
        Redraw();
    }

    void TreeView::Flatten()
    {
        Rows.clear();
        std::function<void( TreeNode& )> Walk = [ & ]( TreeNode& N ) {
            for ( auto& C : N.Children )
            {
                Rows.push_back( C.get() );
                if ( C->Expanded ) Walk( *C );
            }
        };
        Walk( Root );
    }

    size_t TreeView::VisibleRows() { Flatten(); return Rows.size(); }
    float TreeView::MaxScroll() const { return std::max( 0.f, Rows.size() * Style.RowHeight + 8 - Bounds.h ); }

    int TreeView::RowAt( Vec2 P ) const
    {
        if ( !Bounds.Contains( P ) ) return -1;
        int R = int( std::floor( ( P.y - Bounds.y - 4 + Scroll ) / Style.RowHeight ) );
        return R >= 0 && R < int( Rows.size() ) ? R : -1;
    }

    Rect TreeView::ChevronRect( int R ) const
    {
        float X = Bounds.x + 6 + Rows[ R ]->Depth() * Style.Indent;
        return { X, Bounds.y + 4 + R * Style.RowHeight - Scroll, 18, Style.RowHeight };
    }

    Vec2 TreeView::Measure( float Avail )
    {
        Flatten();
        return { std::min( Avail, 220.f ), std::min( 320.f, Rows.size() * Style.RowHeight + 8 ) };
    }

    void TreeView::Draw( DrawList& D )
    {
        const TreeStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Flatten();
        Target = Clamp( Target, 0, MaxScroll() );
        if ( Scroll != Target ) Animate( Scroll, Target, 22.f );
        Scroll = Clamp( Scroll, 0, MaxScroll() );

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.PushClip( Bounds.Shrink( 1 ) );
        int First = std::max( 0, int( ( Scroll - 4 ) / S.RowHeight ) );
        int Last = std::min( int( Rows.size() ), int( ( Scroll + Bounds.h ) / S.RowHeight ) + 2 );
        for ( int R = First; R < Last; R++ )
        {
            TreeNode& N = *Rows[ R ];
            Rect Row = { Bounds.x + 4, Bounds.y + 4 + R * S.RowHeight - Scroll, Bounds.w - 8, S.RowHeight };
            if ( &N == Selected ) D.RectFilled( Row, S.SelectedFill.Resolve(), 5 );
            else if ( R == HoverRow && IsHovered() ) D.RectFilled( Row, S.HoverFill.Resolve(), 5 );

            int Depth = N.Depth();
            for ( int G = 0; G < Depth; G++ ) // indentation guides
                D.RectFilled( { Bounds.x + 15 + G * S.Indent, Row.y, 1, Row.h }, S.Guide.Resolve() );

            Rect Ch = ChevronRect( R );
            if ( N.HasChildren() )
            {
                Vec2 C = Ch.Center();
                Vec2 P[ 3 ];
                if ( N.Expanded ) { P[ 0 ] = { C.x - 4, C.y - 2 }; P[ 1 ] = { C.x, C.y + 2 }; P[ 2 ] = { C.x + 4, C.y - 2 }; }
                else { P[ 0 ] = { C.x - 2, C.y - 4 }; P[ 1 ] = { C.x + 2, C.y }; P[ 2 ] = { C.x - 2, C.y + 4 }; }
                D.Polyline( P, 3, S.Chevron.Resolve(), 1.6f );
            }
            D.Text( F, { Ch.Right() + 2, std::floor( Row.y + ( Row.h - F.LineHeight ) * 0.5f ) }, N.Text, N.Color.Resolve() );
        }
        D.PopClip();
        if ( MaxScroll() > 0 )
        {
            float Track = Bounds.h - 8, H = std::max( 20.f, Track * Bounds.h / ( Rows.size() * S.RowHeight + 8 ) );
            D.RectFilled( { Bounds.Right() - 7, Bounds.y + 4 + ( Track - H ) * ( Scroll / MaxScroll() ), 4, H }, S.Thumb.Resolve(), 2 );
        }
        D.RectStroke( Bounds, S.Stroke.Resolve(), S.Radius );
    }

    bool TreeView::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove: { int H = RowAt( E.Pos ); if ( H != HoverRow ) { HoverRow = H; Redraw(); } return false; }
        case EventType::MouseLeave: HoverRow = -1; Redraw(); return false;
        case EventType::MouseWheel:
            if ( MaxScroll() <= 0 ) return false;
            Target = Clamp( Target - E.Wheel.y * Style.RowHeight * 3, 0, MaxScroll() );
            Redraw();
            return true;
        case EventType::MouseDown:
            if ( E.Button == MouseButton::Right ) // right-click selects the row first, so a context menu acts on it
            {
                int R = RowAt( E.Pos );
                if ( R >= 0 ) Select( Rows[ R ] );
                return false;
            }
            if ( E.Button != MouseButton::Left ) return false;
            PressRow = RowAt( E.Pos );
            PressOnChevron = PressRow >= 0 && ChevronRect( PressRow ).Shrink( -2 ).Contains( E.Pos );
            return true;
        case EventType::MouseUp:
        {
            if ( E.Button != MouseButton::Left ) return false;
            int R = RowAt( E.Pos );
            if ( R >= 0 && R == PressRow )
            {
                TreeNode& N = *Rows[ R ];
                if ( PressOnChevron ) Toggle( N );
                else
                {
                    Select( &N );
                    if ( OnActivate ) OnActivate( N );
                }
            }
            PressRow = -1;
            return true;
        }
        case EventType::KeyDown:
        {
            Flatten();
            auto It = std::find( Rows.begin(), Rows.end(), Selected );
            int I = It == Rows.end() ? -1 : int( It - Rows.begin() );
            if ( E.KeyCode == Key::Down && I + 1 < int( Rows.size() ) ) { Select( Rows[ I + 1 ] ); return true; }
            if ( E.KeyCode == Key::Up && I > 0 ) { Select( Rows[ I - 1 ] ); return true; }
            if ( E.KeyCode == Key::Right && Selected && Selected->HasChildren() && !Selected->Expanded ) { Toggle( *Selected ); return true; }
            if ( E.KeyCode == Key::Left && Selected )
            {
                if ( Selected->Expanded ) Toggle( *Selected );
                else if ( Selected->Parent != &Root ) Select( Selected->Parent );
                return true;
            }
            return false;
        }
        default:
            return false;
        }
    }
}
