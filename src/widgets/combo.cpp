#include "tg/widgets/combo.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/listitem.h"
#include "tg/widgets/popup.h"
#include "tg/widgets/scrollview.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    // ------------------------------------------------------------ DropdownBase

    ComboStyle& DropdownBase::Defaults()
    {
        static ComboStyle S;
        return S;
    }

    DropdownBase::DropdownBase( std::vector<std::string> I ) : Style( Defaults() ), Items( std::move( I ) )
    {
        Interactive = true;
    }

    DropdownBase::~DropdownBase()
    {
        if ( Open ) { Open->OnClose = nullptr; Open->Close(); }
    }

    void DropdownBase::SetItems( std::vector<std::string> NewItems )
    {
        Items = std::move( NewItems );
        CloseList();
        Invalidate();
    }

    void DropdownBase::OpenList()
    {
        if ( Open || Items.empty() ) return;
        Popup& P = OpenPopup( *this, Bounds );
        Open = &P;
        P.OnClose = [ this ] { Open = nullptr; Rows.clear(); Redraw(); };

        auto& List = P.Add<ScrollView>();
        List.Layout.Height = Fit();
        List.MaxHeight = Style.MaxPopupHeight;
        List.Content().Layout.Gap = 1;
        Rows.clear();
        for ( size_t i = 0; i < Items.size(); i++ )
        {
            auto& Row = List.Content().Add<ListItem>( Items[ i ] );
            Row.ShowCheck = MultiSelect();
            Row.ShowMark = !MultiSelect();
            Row.OnClick = [ this, i ] { OnItemClicked( i ); };
            Rows.push_back( &Row );
        }
        SyncRows();
    }

    void DropdownBase::CloseList()
    {
        if ( Open ) Open->Close();
    }

    void DropdownBase::SyncRows()
    {
        for ( size_t i = 0; i < Rows.size(); i++ ) Rows[ i ]->Selected = IsItemSelected( i );
        Redraw();
    }

    Vec2 DropdownBase::Measure( float Avail )
    {
        const Font& F = *GetTheme().Body;
        float W = F.Width( Placeholder );
        for ( auto& I : Items ) W = std::max( W, F.Width( I ) );
        W = std::min( std::max( 120.f, std::ceil( W ) + 24 + Style.Padding.Horizontal() ), std::max( Avail, 120.f ) );
        return { W, F.LineHeight + Style.Padding.Vertical() };
    }

    void DropdownBase::Draw( DrawList& D )
    {
        const ComboStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Animate( OpenT, Open ? 1.f : 0.f, 20.f );
        Animate( HoverT, IsHovered() ? 1.f : 0.f );

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.RectFilled( Bounds, Color( 255, 255, 255, 10 ).Fade( HoverT ), S.Radius );
        D.RectStroke( Bounds, Color::Lerp( S.Stroke.Resolve(), S.StrokeOpen.Resolve(), OpenT ), S.Radius );

        Rect In = Bounds.Shrink( S.Padding );
        const float ChevronW = 16;
        std::string Txt = DisplayText( In.w - ChevronW - 6 );
        bool Empty = Txt.empty();
        D.PushClip( { In.x, Bounds.y, In.w - ChevronW, Bounds.h } );
        D.Text( F, { In.x, std::floor( In.y + ( In.h - F.LineHeight ) * 0.5f ) }, Empty ? Placeholder : Txt,
                ( Empty ? S.Placeholder : S.TextColor ).Resolve().Fade( Enabled ? 1 : 0.5f ) );
        D.PopClip();

        // chevron flips while open
        float Cx = In.Right() - ChevronW * 0.5f, Cy = In.y + In.h * 0.5f, Dy = 2.5f * ( 1 - 2 * OpenT );
        Vec2 Pts[ 3 ] = { { Cx - 4.5f, Cy - Dy }, { Cx, Cy + Dy }, { Cx + 4.5f, Cy - Dy } };
        D.Polyline( Pts, 3, S.Chevron.Resolve(), 1.6f );
    }

    bool DropdownBase::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() ) { if ( Open ) CloseList(); else OpenList(); }
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------ Combo

    Combo::Combo( std::vector<std::string> I, int* B ) : DropdownBase( std::move( I ) ), Bind( B ) {}

    void Combo::SetIndex( int i )
    {
        if ( i < -1 || i >= int( Items.size() ) ) i = -1;
        if ( i == GetIndex() ) return;
        ( Bind ? *Bind : Own ) = i;
        if ( OnChange ) OnChange( i );
        SyncRows();
    }

    const std::string* Combo::GetSelectedText() const
    {
        int i = GetIndex();
        return i >= 0 && i < int( Items.size() ) ? &Items[ i ] : nullptr;
    }

    std::string Combo::DisplayText( float ) const
    {
        const std::string* S = GetSelectedText();
        return S ? *S : std::string();
    }

    void Combo::OnItemClicked( size_t i )
    {
        SetIndex( int( i ) );
        CloseList();
    }

    // ------------------------------------------------------------ MultiCombo

    MultiCombo::MultiCombo( std::vector<std::string> I, std::vector<bool>* B ) : DropdownBase( std::move( I ) ), Bind( B )
    {
        Placeholder = "None";
    }

    std::vector<bool>& MultiCombo::Sel()
    {
        auto& V = Bind ? *Bind : Own;
        if ( V.size() < Items.size() ) V.resize( Items.size(), false );
        return V;
    }

    const std::vector<bool>& MultiCombo::Sel() const { return Bind ? *Bind : Own; }

    bool MultiCombo::IsSelected( size_t i ) const
    {
        const auto& V = Sel();
        return i < V.size() && V[ i ];
    }

    void MultiCombo::SetSelected( size_t i, bool On )
    {
        if ( i >= Items.size() || IsSelected( i ) == On ) return;
        Sel()[ i ] = On;
        if ( OnChange ) OnChange( Sel() );
        SyncRows();
    }

    std::vector<int> MultiCombo::SelectedIndices() const
    {
        std::vector<int> Out;
        for ( size_t i = 0; i < Items.size(); i++ )
            if ( IsSelected( i ) ) Out.push_back( int( i ) );
        return Out;
    }

    std::string MultiCombo::DisplayText( float MaxWidth ) const
    {
        std::vector<int> Idx = SelectedIndices();
        if ( Idx.empty() ) return {};
        std::string Joined;
        for ( int i : Idx ) Joined += ( Joined.empty() ? "" : ", " ) + Items[ i ];
        if ( GetTheme().Body->Width( Joined ) <= MaxWidth ) return Joined;
        return std::to_string( Idx.size() ) + " selected";
    }
}
