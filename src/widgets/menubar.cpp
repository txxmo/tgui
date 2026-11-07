#include "tg/widgets/menubar.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/menu.h"
#include <cmath>

namespace tg
{
    MenuBar::MenuBar()
    {
        Interactive = true;
        Layout.Height = Px( Style.Height );
    }

    MenuBar::~MenuBar()
    {
        if ( OpenMenuPtr ) { OpenMenuPtr->OnClose = nullptr; OpenMenuPtr->Close(); }
    }

    void MenuBar::Add( std::string Title, std::function<void( Menu& )> Build )
    {
        Entries.push_back( { std::move( Title ), std::move( Build ) } );
        Invalidate();
    }

    Rect MenuBar::ItemRect( size_t i ) const
    {
        const Font& F = *GetTheme().Body;
        float X = Bounds.x + 4;
        for ( size_t k = 0; k < i; k++ ) X += F.Width( Entries[ k ].Title ) + Style.ItemPadding * 2;
        return { X, Bounds.y + 3, F.Width( Entries[ i ].Title ) + Style.ItemPadding * 2, Bounds.h - 6 };
    }

    int MenuBar::At( Vec2 P ) const
    {
        for ( size_t i = 0; i < Entries.size(); i++ )
            if ( ItemRect( i ).Contains( P ) ) return int( i );
        return -1;
    }

    void MenuBar::CloseMenu()
    {
        if ( OpenMenuPtr ) OpenMenuPtr->Close();
    }

    void MenuBar::OpenIndex( int i )
    {
        if ( i == Open ) return;
        if ( OpenMenuPtr ) { OpenMenuPtr->OnClose = nullptr; OpenMenuPtr->Close(); OpenMenuPtr = nullptr; Open = -1; }
        if ( i < 0 || i >= int( Entries.size() ) ) { Redraw(); return; }
        Menu& M = OpenMenu( *this, ItemRect( size_t( i ) ) );
        M.MinWidth = 200;
        OpenMenuPtr = &M;
        Open = i;
        M.OnClose = [ this ] { OpenMenuPtr = nullptr; Open = -1; Redraw(); };
        if ( Entries[ size_t( i ) ].Build ) Entries[ size_t( i ) ].Build( M );
        Redraw();
    }

    Vec2 MenuBar::Measure( float Avail )
    {
        float W = 8;
        for ( auto& E : Entries ) W += GetTheme().Body->Width( E.Title ) + Style.ItemPadding * 2;
        return { std::min( W, Avail ), Style.Height };
    }

    void MenuBar::Draw( DrawList& D )
    {
        const MenuBarStyle& S = Style;
        const Font& F = *GetTheme().Body;
        D.RectFilled( Bounds, S.Fill.Resolve() );
        D.RectFilled( { Bounds.x, Bounds.Bottom() - 1, Bounds.w, 1 }, S.Stroke.Resolve() );
        for ( size_t i = 0; i < Entries.size(); i++ )
        {
            Rect R = ItemRect( i );
            if ( int( i ) == Open ) D.RectFilled( R, S.Open.Resolve(), S.Radius );
            else if ( int( i ) == Hover && IsHovered() ) D.RectFilled( R, S.Hover.Resolve(), S.Radius );
            D.Text( F, { R.x + S.ItemPadding, std::floor( R.y + ( R.h - F.LineHeight ) * 0.5f ) }, Entries[ i ].Title, S.TextColor.Resolve() );
        }
    }

    bool MenuBar::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            int H = At( E.Pos );
            if ( H != Hover ) { Hover = H; Redraw(); }
            if ( Open >= 0 && H >= 0 && H != Open ) OpenIndex( H ); // slide between open menus
            return false;
        }
        case EventType::MouseLeave: Hover = -1; Redraw(); return false;
        case EventType::MouseDown:
        {
            if ( E.Button != MouseButton::Left ) return false;
            int H = At( E.Pos );
            if ( H < 0 ) return true;
            if ( H == Open ) CloseMenu();
            else OpenIndex( H );
            return true;
        }
        case EventType::MouseUp: return E.Button == MouseButton::Left;
        default: return false;
        }
    }
}
