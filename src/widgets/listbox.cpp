#include "tg/widgets/listbox.h"
#include "tg/draw.h"
#include "tg/widgets/listitem.h"
#include "tg/widgets/scrollview.h"
#include "tg/context.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    ListBox::ListBox( std::vector<std::string> I, SelectMode M, int* B ) : Bind( B ), Items( std::move( I ) ), Mode( M )
    {
        Layout.Height = Px( Style.Height );
        Layout.Padding = Insets( 4 );
        List = &Add<ScrollView>();
        List->Content().Layout.Gap = 1;
        Rebuild();
    }

    void ListBox::Rebuild()
    {
        Sel.resize( Items.size(), false );
        List->Content().Clear();
        for ( size_t i = 0; i < Items.size(); i++ )
        {
            auto& Row = List->Content().Add<ListItem>( Items[ i ] );
            Row.ShowCheck = Mode == SelectMode::Multi;
            Row.SetSelectable( Mode != SelectMode::None );
            Row.OnClick = [ this, i ] { if ( !DragMoved ) Click( i ); };
            Row.BubbleRelease = true;
        }
    }

    void ListBox::SetItems( std::vector<std::string> I )
    {
        Items = std::move( I );
        Sel.assign( Items.size(), false );
        if ( Bind && *Bind >= int( Items.size() ) ) *Bind = -1;
        Rebuild();
    }

    void ListBox::AddItem( std::string Text )
    {
        Items.push_back( std::move( Text ) );
        Rebuild();
    }

    void ListBox::RemoveItem( size_t i )
    {
        if ( i >= Items.size() ) return;
        Items.erase( Items.begin() + i );
        Sel.erase( Sel.begin() + i );
        if ( Bind && *Bind == int( i ) ) *Bind = -1;
        else if ( Bind && *Bind > int( i ) ) ( *Bind )--;
        Rebuild();
    }

    void ListBox::SetMode( SelectMode M )
    {
        if ( M == Mode ) return;
        Mode = M;
        if ( M == SelectMode::None ) ClearSelection();
        if ( M == SelectMode::Single ) { int First = GetSelected(); ClearSelection(); if ( First >= 0 ) SetSelected( First, true ); }
        Rebuild();
    }

    bool ListBox::IsSelected( size_t i ) const
    {
        if ( Mode == SelectMode::Single && Bind ) return int( i ) == *Bind;
        return i < Sel.size() && Sel[ i ];
    }

    int ListBox::GetSelected() const
    {
        for ( size_t i = 0; i < Items.size(); i++ )
            if ( IsSelected( i ) ) return int( i );
        return -1;
    }

    std::vector<int> ListBox::SelectedIndices() const
    {
        std::vector<int> Out;
        for ( size_t i = 0; i < Items.size(); i++ )
            if ( IsSelected( i ) ) Out.push_back( int( i ) );
        return Out;
    }

    void ListBox::SetSelected( size_t i, bool On )
    {
        if ( i >= Items.size() || Mode == SelectMode::None || IsSelected( i ) == On ) return;
        if ( Mode == SelectMode::Single )
        {
            Sel.assign( Items.size(), false );
            if ( On ) Sel[ i ] = true;
            if ( Bind ) *Bind = On ? int( i ) : -1;
        }
        else Sel[ i ] = On;
        if ( OnChange ) OnChange();
        Redraw();
    }

    void ListBox::ClearSelection()
    {
        bool Any = GetSelected() >= 0;
        Sel.assign( Items.size(), false );
        if ( Bind ) *Bind = -1;
        if ( Any && OnChange ) OnChange();
        Redraw();
    }

    void ListBox::Click( size_t i )
    {
        if ( Mode == SelectMode::None ) return;
        if ( Mode == SelectMode::Single ) SetSelected( i, true );
        else SetSelected( i, !IsSelected( i ) );
        if ( OnActivate ) OnActivate( int( i ) );
    }

    void ListBox::MoveItem( size_t From, size_t To )
    {
        if ( From >= Items.size() || To >= Items.size() || From == To ) return;
        int BoundSel = Bind ? *Bind : -1;
        std::string It = std::move( Items[ From ] );
        bool S = Sel[ From ];
        Items.erase( Items.begin() + From );
        Sel.erase( Sel.begin() + From );
        Items.insert( Items.begin() + To, std::move( It ) );
        Sel.insert( Sel.begin() + To, S );
        if ( Bind && BoundSel >= 0 ) // keep the bound selection on the same item
        {
            int B = BoundSel;
            if ( B == int( From ) ) B = int( To );
            else if ( int( From ) < B && B <= int( To ) ) B--;
            else if ( int( To ) <= B && B < int( From ) ) B++;
            *Bind = B;
        }
        Rebuild();
        if ( OnReorder ) OnReorder( int( From ), int( To ) );
    }

    int ListBox::RowAtY( float Y, bool Insertion ) const
    {
        auto& Rows = List->Content().Children();
        if ( Rows.empty() ) return -1;
        for ( size_t i = 0; i < Rows.size(); i++ )
        {
            const Rect& B = Rows[ i ]->Bounds;
            float Edge = Insertion ? B.y + B.h * 0.5f : B.Bottom();
            if ( Y < Edge ) return int( i );
        }
        return Insertion ? int( Rows.size() ) : int( Rows.size() ) - 1;
    }

    bool ListBox::OnEvent( const Event& E )
    {
        if ( !Reorderable ) return false;
        Context* C = GetContext();
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            Widget* P = C ? C->Pressed() : nullptr;
            if ( !P || P->GetParent() != &List->Content() ) return false;
            if ( DragFrom < 0 ) { DragFrom = RowAtY( E.Pos.y, false ); DragStartY = E.Pos.y; DragMoved = false; }
            if ( !DragMoved && std::fabs( E.Pos.y - DragStartY ) > 4 ) DragMoved = true;
            if ( DragMoved ) { DragY = E.Pos.y; DropAt = RowAtY( E.Pos.y, true ); Redraw(); }
            return DragMoved;
        }
        case EventType::MouseUp:
        {
            bool Moved = DragMoved && DragFrom >= 0 && DropAt >= 0;
            int From = DragFrom, Drop = DropAt;
            DragFrom = DropAt = -1;
            DragMoved = false;
            if ( Moved )
            {
                int To = Drop > From ? Drop - 1 : Drop; // index after removal
                MoveItem( size_t( From ), size_t( std::clamp( To, 0, int( Items.size() ) - 1 ) ) );
            }
            Redraw();
            return Moved;
        }
        default:
            return false;
        }
    }

    void ListBox::Draw( DrawList& D )
    {
        // keep rows in sync with the selection (and with an external Bind changing under us)
        auto& Rows = List->Content().Children();
        for ( size_t i = 0; i < Rows.size(); i++ )
            static_cast<ListItem*>( Rows[ i ].get() )->Selected = IsSelected( i );

        D.RectFilled( Bounds, Style.Fill.Resolve(), Style.Radius );
        D.RectStroke( Bounds, Style.Stroke.Resolve(), Style.Radius );
        DrawChildren( D );

        // drag-to-reorder feedback: insertion line + a floating copy of the dragged row
        if ( DragMoved && DragFrom >= 0 && DragFrom < int( Rows.size() ) )
        {
            D.PushClip( Bounds );
            float Y = DropAt < int( Rows.size() ) ? Rows[ size_t( DropAt ) ]->Bounds.y : Rows.back()->Bounds.Bottom();
            D.RectFilled( { Bounds.x + 6, Y - 1, Bounds.w - 12, 2 }, GetTheme()[ Tok::Accent ], 1 );
            Rect Src = Rows[ size_t( DragFrom ) ]->Bounds;
            Rect Ghost = { Src.x, DragY - Src.h * 0.5f, Src.w, Src.h };
            D.RectFilled( Ghost, GetTheme()[ Tok::Raised ].WithAlpha( 220 ), 5 );
            const Font& F = *GetTheme().Body;
            D.Text( F, { Ghost.x + 10, std::floor( Ghost.y + ( Ghost.h - F.LineHeight ) * 0.5f ) }, Items[ size_t( DragFrom ) ], GetTheme()[ Tok::Text ] );
            D.PopClip();
        }
    }
}
