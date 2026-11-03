#include "tg/widgets/table.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <numeric>

namespace tg
{
    TableStyle& Table::Defaults()
    {
        static TableStyle S;
        return S;
    }

    Table::Table( std::vector<TableColumn> C ) : Style( Defaults() ), Cols( std::move( C ) )
    {
        Interactive = true;
        Layout.Height = Grow();
    }

    // ------------------------------------------------------------ data

    TableColumn& Table::AddColumn( std::string Title, Size Width, Align A )
    {
        TableColumn C;
        C.Title = std::move( Title );
        C.Width = Width;
        C.TextAlign = A;
        Cols.push_back( std::move( C ) );
        for ( auto& R : Rows ) R.resize( Cols.size() );
        Invalidate();
        return Cols.back();
    }

    void Table::SetColumns( std::vector<TableColumn> C )
    {
        Cols = std::move( C );
        for ( auto& R : Rows ) R.resize( Cols.size() );
        if ( SortCol >= int( Cols.size() ) ) SortCol = -1;
        Invalidate();
    }

    size_t Table::AddRow( std::vector<std::string> Cells )
    {
        Cells.resize( Cols.size() );
        Rows.push_back( std::move( Cells ) );
        Sel.push_back( false );
        Order.push_back( Rows.size() - 1 );
        if ( SortCol >= 0 ) Resort();
        Redraw();
        return Rows.size() - 1;
    }

    void Table::SetRows( std::vector<std::vector<std::string>> R )
    {
        Rows = std::move( R );
        for ( auto& Row : Rows ) Row.resize( Cols.size() );
        Sel.assign( Rows.size(), false );
        if ( Bind ) *Bind = -1;
        Resort();
        Redraw();
    }

    void Table::SetCell( size_t R, size_t C, std::string Text )
    {
        if ( R >= Rows.size() || C >= Cols.size() || Rows[ R ][ C ] == Text ) return;
        Rows[ R ][ C ] = std::move( Text );
        if ( SortCol == int( C ) ) Resort();
        Redraw();
    }

    const std::string& Table::GetCell( size_t R, size_t C ) const
    {
        static const std::string Empty;
        return R < Rows.size() && C < Cols.size() ? Rows[ R ][ C ] : Empty;
    }

    void Table::RemoveRow( size_t R )
    {
        if ( R >= Rows.size() ) return;
        Rows.erase( Rows.begin() + R );
        Sel.erase( Sel.begin() + R );
        if ( Bind && *Bind == int( R ) ) *Bind = -1;
        else if ( Bind && *Bind > int( R ) ) ( *Bind )--;
        Resort();
        Redraw();
    }

    void Table::ClearRows()
    {
        Rows.clear();
        Sel.clear();
        Order.clear();
        OrderDirty = true;
        if ( Bind ) *Bind = -1;
        Scroll = Target = 0;
        Redraw();
    }

    // ------------------------------------------------------------ sorting

    // "37%", "1.2 GB", "-4" -> number; otherwise not numeric.
    static bool LeadingNumber( const std::string& S, double& Out )
    {
        const char* P = S.c_str();
        while ( *P == ' ' ) P++;
        char* End = nullptr;
        Out = std::strtod( P, &End );
        return End != P;
    }

    static bool TextLessCI( const std::string& A, const std::string& B )
    {
        return std::lexicographical_compare( A.begin(), A.end(), B.begin(), B.end(),
            []( char a, char b ) { return std::tolower( (unsigned char)a ) < std::tolower( (unsigned char)b ); } );
    }

    void Table::EnsureOrder() const
    {
        if ( !OrderDirty && Order.size() == Rows.size() ) return;
        OrderDirty = false;
        Order.resize( Rows.size() );
        std::iota( Order.begin(), Order.end(), size_t( 0 ) );
        if ( SortCol < 0 || SortCol >= int( Cols.size() ) ) return;
        const size_t C = size_t( SortCol );
        const bool Asc = SortAsc;

        if ( const auto& Less = Cols[ C ].Less )
        {
            std::stable_sort( Order.begin(), Order.end(), [ & ]( size_t a, size_t b ) {
                return Asc ? Less( Rows[ a ][ C ], Rows[ b ][ C ] ) : Less( Rows[ b ][ C ], Rows[ a ][ C ] );
            } );
            return;
        }

        // Default order: parse every cell once, then sort on the parsed keys.
        struct Key { bool Num; double V; };
        std::vector<Key> Keys( Rows.size() );
        for ( size_t i = 0; i < Rows.size(); i++ ) Keys[ i ].Num = LeadingNumber( Rows[ i ][ C ], Keys[ i ].V );
        auto TextLess = [ & ]( size_t a, size_t b ) { return TextLessCI( Rows[ a ][ C ], Rows[ b ][ C ] ); };
        std::stable_sort( Order.begin(), Order.end(), [ & ]( size_t a, size_t b ) {
            if ( !Asc ) std::swap( a, b );
            const Key &A = Keys[ a ], &B = Keys[ b ];
            if ( A.Num != B.Num ) return A.Num;          // numbers before text: keeps the order consistent
            if ( A.Num && A.V != B.V ) return A.V < B.V;
            return TextLess( a, b );
        } );
    }

    void Table::SortBy( int Col, bool Asc )
    {
        SortCol = Col;
        SortAsc = Asc;
        Resort();
        Redraw();
    }

    // ------------------------------------------------------------ selection

    bool Table::IsSelected( size_t R ) const
    {
        if ( Mode == SelectMode::Single && Bind ) return int( R ) == *Bind;
        return R < Sel.size() && Sel[ R ];
    }

    int Table::GetSelected() const
    {
        if ( Mode == SelectMode::Single && Bind ) return *Bind;
        for ( size_t i = 0; i < Sel.size(); i++ )
            if ( Sel[ i ] ) return int( i );
        return -1;
    }

    std::vector<int> Table::SelectedRows() const
    {
        std::vector<int> Out;
        for ( size_t i = 0; i < Rows.size(); i++ )
            if ( IsSelected( i ) ) Out.push_back( int( i ) );
        return Out;
    }

    void Table::SetSelected( size_t R, bool On )
    {
        if ( R >= Rows.size() || Mode == SelectMode::None || IsSelected( R ) == On ) return;
        if ( Mode == SelectMode::Single )
        {
            Sel.assign( Rows.size(), false );
            Sel[ R ] = On;
            if ( Bind ) *Bind = On ? int( R ) : -1;
        }
        else Sel[ R ] = On;
        if ( OnSelect ) OnSelect();
        Redraw();
    }

    void Table::ClearSelection()
    {
        Sel.assign( Rows.size(), false );
        if ( Bind ) *Bind = -1;
        if ( OnSelect ) OnSelect();
        Redraw();
    }

    // ------------------------------------------------------------ geometry

    Rect Table::HeaderRect() const { return { Bounds.x, Bounds.y, Bounds.w, Style.HeaderHeight }; }
    Rect Table::BodyRect() const { return { Bounds.x, Bounds.y + Style.HeaderHeight, Bounds.w, Bounds.h - Style.HeaderHeight }; }
    float Table::MaxScroll() const { return std::max( 0.f, ContentHeight() - BodyRect().h ); }

    Rect Table::ThumbRect() const
    {
        Rect B = BodyRect();
        float Track = B.h - 6;
        float H = std::max( 24.f, Track * B.h / std::max( ContentHeight(), 1.f ) );
        float T = MaxScroll() > 0 ? Scroll / MaxScroll() : 0;
        return { B.Right() - 9, B.y + 3 + ( Track - H ) * T, 6, H };
    }

    float Table::ColX( size_t c ) const
    {
        float X = Bounds.x;
        for ( size_t i = 0; i < c && i < ColW.size(); i++ ) X += ColW[ i ];
        return X;
    }

    void Table::ResolveWidths()
    {
        ColW.assign( Cols.size(), 0 );
        float Fixed = 0, Weights = 0;
        for ( size_t i = 0; i < Cols.size(); i++ )
        {
            if ( Cols[ i ].Width.K == Size::Kind::Grow ) Weights += Cols[ i ].Width.V;
            else
            {
                float W = Cols[ i ].Width.K == Size::Kind::Fixed ? Cols[ i ].Width.V
                        : GetTheme().Bold->Width( Cols[ i ].Title ) + Style.CellPadding.Horizontal() + 16;
                ColW[ i ] = std::max( W, Cols[ i ].MinWidth );
                Fixed += ColW[ i ];
            }
        }
        float Free = std::max( 0.f, Bounds.w - Fixed );
        for ( size_t i = 0; i < Cols.size(); i++ )
            if ( Cols[ i ].Width.K == Size::Kind::Grow )
                ColW[ i ] = std::max( Cols[ i ].MinWidth, std::floor( Free * Cols[ i ].Width.V / std::max( Weights, 1e-6f ) ) );
    }

    int Table::EdgeAt( Vec2 P ) const
    {
        if ( !HeaderRect().Contains( P ) ) return -1;
        for ( size_t i = 0; i + 1 < Cols.size(); i++ ) // last column's edge is the table border
            if ( std::fabs( P.x - ( ColX( i ) + ColW[ i ] ) ) <= 4 ) return int( i );
        return -1;
    }

    int Table::HeaderAt( Vec2 P ) const
    {
        if ( !HeaderRect().Contains( P ) ) return -1;
        for ( size_t i = 0; i < Cols.size(); i++ )
            if ( P.x >= ColX( i ) && P.x < ColX( i ) + ColW[ i ] ) return int( i );
        return -1;
    }

    int Table::RowAt( Vec2 P ) const
    {
        Rect B = BodyRect();
        if ( !B.Contains( P ) ) return -1;
        int Pos = int( std::floor( ( P.y - B.y + Scroll ) / Style.RowHeight ) );
        return Pos >= 0 && Pos < int( Rows.size() ) ? Pos : -1;
    }

    void Table::ScrollToRow( size_t R )
    {
        EnsureOrder();
        auto It = std::find( Order.begin(), Order.end(), R );
        if ( It == Order.end() ) return;
        float Y = float( It - Order.begin() ) * Style.RowHeight, H = BodyRect().h;
        if ( Y < Target ) Target = Y;
        else if ( Y + Style.RowHeight > Target + H ) Target = Y + Style.RowHeight - H;
        Target = Clamp( Target, 0, MaxScroll() );
        Redraw();
    }

    Vec2 Table::Measure( float Avail )
    {
        float W = 0;
        for ( auto& C : Cols ) W += C.Width.K == Size::Kind::Fixed ? C.Width.V : C.MinWidth;
        return { std::min( std::max( W, 200.f ), Avail ), Style.HeaderHeight + Style.RowHeight * float( std::min<size_t>( Rows.size(), 8 ) ) };
    }

    void Table::Arrange( Rect R )
    {
        Bounds = R;
        ResolveWidths();
        Target = Clamp( Target, 0, MaxScroll() );
        Scroll = Clamp( Scroll, 0, MaxScroll() );
    }

    // ------------------------------------------------------------ drawing

    void Table::Draw( DrawList& D )
    {
        const TableStyle& S = Style;
        const Font& F = *GetTheme().Body;
        const Font& Hf = *GetTheme().Bold;
        EnsureOrder();
        if ( ColW.size() != Cols.size() ) ResolveWidths();
        if ( Scroll != Target ) Animate( Scroll, Target, 22.f );

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );

        // ---- body: only the rows that are on screen
        Rect Body = BodyRect();
        D.PushClip( Body );
        size_t First = size_t( std::max( 0.f, std::floor( Scroll / S.RowHeight ) ) );
        size_t Last = std::min( Rows.size(), size_t( std::ceil( ( Scroll + Body.h ) / S.RowHeight ) ) + 1 );
        Color Tc = S.Text.Resolve();
        for ( size_t Pos = First; Pos < Last; Pos++ )
        {
            size_t R = Order[ Pos ];
            Rect Row = { Body.x, Body.y + Pos * S.RowHeight - Scroll, Body.w, S.RowHeight };
            if ( S.Striped && Pos % 2 ) D.RectFilled( Row, S.Stripe.Resolve() );
            if ( IsSelected( R ) ) D.RectFilled( Row, S.SelectedFill.Resolve() );
            if ( int( Pos ) == HoverRow && Mode != SelectMode::None ) D.RectFilled( Row, S.HoverFill.Resolve() );

            for ( size_t C = 0; C < Cols.size(); C++ )
            {
                Rect Cell = { ColX( C ), Row.y, ColW[ C ], Row.h };
                if ( DrawCell && DrawCell( D, Cell, R, C ) ) continue;
                Rect In = Cell.Shrink( S.CellPadding );
                const std::string& Txt = Rows[ R ][ C ];
                Color Col = Tc;
                if ( CellColor ) CellColor( R, C, Col );
                float Tw = F.Width( Txt );
                float X = Cols[ C ].TextAlign == Align::End ? In.Right() - Tw : Cols[ C ].TextAlign == Align::Center ? In.x + std::floor( ( In.w - Tw ) * 0.5f ) : In.x;
                D.PushClip( In );
                D.Text( F, { X, std::floor( Row.y + ( Row.h - F.LineHeight ) * 0.5f ) }, Txt, Col );
                D.PopClip();
            }
        }
        if ( S.GridLines )
            for ( size_t C = 0; C + 1 < Cols.size(); C++ )
                D.RectFilled( { ColX( C ) + ColW[ C ], Body.y, 1, Body.h }, S.Stroke.Resolve() );
        D.PopClip();

        // ---- header (drawn after the body so it stays on top)
        Rect H = HeaderRect();
        D.RectFilled( H, S.HeaderFill.Resolve(), S.Radius, CornersTop );
        D.RectFilled( { H.x, H.Bottom() - 1, H.w, 1 }, S.Stroke.Resolve() );
        for ( size_t C = 0; C < Cols.size(); C++ )
        {
            Rect Cell = { ColX( C ), H.y, ColW[ C ], H.h };
            Rect In = Cell.Shrink( S.CellPadding );
            bool Sorted = SortCol == int( C );
            const std::string& T = Cols[ C ].Title;
            float Tw = Hf.Width( T ) + ( Sorted ? 14 : 0 );
            float X = Cols[ C ].TextAlign == Align::End ? In.Right() - Tw : Cols[ C ].TextAlign == Align::Center ? In.x + std::floor( ( In.w - Tw ) * 0.5f ) : In.x;
            Color Hc = Sorted || PressedHeader == int( C ) ? GetTheme()[ Tok::Text ] : S.HeaderText.Resolve();
            D.PushClip( In );
            float Ty = std::floor( H.y + ( H.h - Hf.LineHeight ) * 0.5f );
            D.Text( Hf, { X, Ty }, T, Hc );
            if ( Sorted ) // little up/down triangle
            {
                float Ax = X + Hf.Width( T ) + 8, Ay = H.y + H.h * 0.5f;
                Vec2 Up[ 3 ] = { { Ax - 4, Ay + 2 }, { Ax, Ay - 3 }, { Ax + 4, Ay + 2 } };
                Vec2 Dn[ 3 ] = { { Ax + 4, Ay - 2 }, { Ax, Ay + 3 }, { Ax - 4, Ay - 2 } };
                D.ConvexFill( SortAsc ? Up : Dn, 3, Hc );
            }
            D.PopClip();
            if ( C + 1 < Cols.size() )
            {
                bool Hot = HoverEdge == int( C ) || ResizeCol == int( C );
                D.RectFilled( { Cell.Right() - ( Hot ? 1 : 0.5f ), H.y + 8, Hot ? 2.f : 1.f, H.h - 16 }, Hot ? GetTheme()[ Tok::Accent ] : S.Stroke.Resolve() );
            }
        }

        // ---- scrollbar
        if ( MaxScroll() > 0 )
            D.RectFilled( ThumbRect(), ( DraggingThumb ? GetTheme()[ Tok::TextDim ] : S.Thumb.Resolve() ), 3 );

        D.RectStroke( Bounds, S.Stroke.Resolve(), S.Radius );
    }

    // ------------------------------------------------------------ input

    Cursor Table::GetCursor() const
    {
        if ( ResizeCol >= 0 || HoverEdge >= 0 ) return Cursor::ResizeH;
        if ( HoverRow >= 0 && Mode != SelectMode::None ) return Cursor::Hand;
        return Cursor::Arrow;
    }

    bool Table::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseWheel:
            if ( MaxScroll() <= 0 ) return false;
            Target = Clamp( Target - E.Wheel.y * Style.RowHeight * 3, 0, MaxScroll() );
            HoverRow = -1;
            Redraw();
            return true;

        case EventType::MouseMove:
        {
            if ( ResizeCol >= 0 )
            {
                Cols[ ResizeCol ].Width = Px( std::max( Cols[ ResizeCol ].MinWidth, DragStartW + E.Pos.x - DragStartX ) );
                ResolveWidths();
                Redraw();
                return true;
            }
            if ( DraggingThumb )
            {
                Rect B = BodyRect();
                float Track = B.h - 6 - ThumbRect().h;
                float T = Track > 0 ? Clamp( ( E.Pos.y - DragOffset - B.y - 3 ) / Track, 0, 1 ) : 0;
                Scroll = Target = T * MaxScroll();
                Redraw();
                return true;
            }
            int Hr = RowAt( E.Pos ), He = EdgeAt( E.Pos );
            if ( Hr != HoverRow || He != HoverEdge ) { HoverRow = Hr; HoverEdge = He; Redraw(); }
            return false;
        }

        case EventType::MouseLeave:
            HoverRow = HoverEdge = -1;
            Redraw();
            return false;

        case EventType::MouseDown:
        {
            if ( E.Button != MouseButton::Left ) return false;
            if ( MaxScroll() > 0 && ThumbRect().Shrink( -3 ).Contains( E.Pos ) )
            {
                DraggingThumb = true;
                DragOffset = E.Pos.y - ThumbRect().y;
                return true;
            }
            int Edge = EdgeAt( E.Pos );
            if ( Edge >= 0 )
            {
                ResizeCol = Edge;
                DragStartX = E.Pos.x;
                DragStartW = ColW[ Edge ];
                return true;
            }
            PressedHeader = HeaderAt( E.Pos );
            return true;
        }

        case EventType::MouseUp:
        {
            if ( E.Button != MouseButton::Left ) return false;
            if ( ResizeCol >= 0 || DraggingThumb ) { ResizeCol = -1; DraggingThumb = false; Redraw(); return true; }
            int Hd = HeaderAt( E.Pos );
            if ( Hd >= 0 && Hd == PressedHeader && Cols[ Hd ].Sortable )
                SortBy( Hd, SortCol == Hd ? !SortAsc : true );
            PressedHeader = -1;
            int Pos = RowAt( E.Pos );
            if ( Pos >= 0 && IsHovered() && Mode != SelectMode::None )
            {
                EnsureOrder();
                size_t R = Order[ Pos ];
                if ( Mode == SelectMode::Single ) SetSelected( R, true );
                else SetSelected( R, !IsSelected( R ) );
                if ( OnActivate ) { auto Fn = OnActivate; Fn( R ); }
            }
            return true;
        }

        default:
            return false;
        }
    }
}
