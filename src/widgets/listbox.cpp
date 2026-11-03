#include "tg/widgets/listbox.h"
#include "tg/draw.h"
#include "tg/widgets/listitem.h"
#include "tg/widgets/scrollview.h"

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
            Row.OnClick = [ this, i ] { Click( i ); };
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

    void ListBox::Draw( DrawList& D )
    {
        // keep rows in sync with the selection (and with an external Bind changing under us)
        auto& Rows = List->Content().Children();
        for ( size_t i = 0; i < Rows.size(); i++ )
            static_cast<ListItem*>( Rows[ i ].get() )->Selected = IsSelected( i );

        D.RectFilled( Bounds, Style.Fill.Resolve(), Style.Radius );
        D.RectStroke( Bounds, Style.Stroke.Resolve(), Style.Radius );
        DrawChildren( D );
    }
}
