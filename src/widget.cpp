#include "tg/widget.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/widgets/label.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Widget::~Widget()
    {
        Kids.clear(); // children first, while our Parent chain is still intact
        if ( Context* C = GetContext() ) C->Forget( this );
    }

    // ------------------------------------------------------------ tree

    Widget& Widget::AddChild( std::unique_ptr<Widget> W )
    {
        W->Parent = this;
        Kids.push_back( std::move( W ) );
        Invalidate();
        return *Kids.back();
    }

    void Widget::Remove( Widget* W )
    {
        auto It = std::find_if( Kids.begin(), Kids.end(), [ W ]( auto& K ) { return K.get() == W; } );
        if ( It == Kids.end() ) return;
        Kids.erase( It );
        Invalidate();
    }

    void Widget::Clear()
    {
        Kids.clear();
        Invalidate();
    }

    void Widget::MoveChild( size_t From, size_t To )
    {
        if ( From >= Kids.size() || To >= Kids.size() || From == To ) return;
        auto P = std::move( Kids[ From ] );
        Kids.erase( Kids.begin() + long( From ) );
        Kids.insert( Kids.begin() + long( To ), std::move( P ) );
        Invalidate();
    }

    void Widget::BringToFront()
    {
        if ( !Parent ) return;
        auto& K = Parent->Kids;
        auto It = std::find_if( K.begin(), K.end(), [ this ]( auto& P ) { return P.get() == this; } );
        if ( It == K.end() || It + 1 == K.end() ) return;
        std::rotate( It, It + 1, K.end() );
        Redraw();
    }

    Context* Widget::GetContext() const
    {
        const Widget* W = this;
        while ( W->Parent ) W = W->Parent;
        return W->Ctx;
    }

    bool Widget::IsHovered() const { auto* C = GetContext(); return C && ( C->Hovered() == this || C->KeyHot() == this ); }
    bool Widget::IsPressed() const { auto* C = GetContext(); return C && C->Pressed() == this; }
    bool Widget::IsFocused() const { auto* C = GetContext(); return C && C->Focused() == this; }
    void Widget::Invalidate() { if ( auto* C = GetContext() ) C->InvalidateLayout(); }
    void Widget::Redraw() { if ( auto* C = GetContext() ) C->RequestFrame(); }

    void Widget::BindEditor( Container&, BindValue& ) {}
    std::string Widget::BindText( const BindValue& ) const { return {}; }

    // Controls made by the helpers sit next to their label ("Volume" above a slider, "Region" left of a
    // combo) inside a small wrapper: use the first label in the parent's subtree.
    std::string Widget::BindName() const
    {
        const Widget* P = GetParent();
        if ( !P ) return {};
        std::vector<const Widget*> Queue;
        for ( auto& K : P->Children() )
            if ( K.get() != this ) Queue.push_back( K.get() );
        for ( size_t i = 0; i < Queue.size(); i++ )
        {
            if ( auto* L = dynamic_cast<const Label*>( Queue[ i ] ); L && !L->GetText().empty() ) return L->GetText();
            for ( auto& K : Queue[ i ]->Children() ) Queue.push_back( K.get() );
        }
        return {};
    }

    float Widget::Animate( float& V, float Target, float Speed )
    {
        Context* C = GetContext();
        if ( !C ) return V = Target;
        V += ( Target - V ) * ( 1.f - std::exp( -Speed * C->DeltaTime() ) );
        if ( std::fabs( Target - V ) < 0.002f ) V = Target;
        else C->RequestFrame();
        return V;
    }

    // ------------------------------------------------------------ events / drawing

    bool Widget::OnEvent( const Event& ) { return false; }

    Widget* Widget::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        for ( auto It = Kids.rbegin(); It != Kids.rend(); ++It )
            if ( Widget* H = ( *It )->HitTest( P ) ) return H;
        return Interactive ? this : nullptr;
    }

    void Widget::Draw( DrawList& D ) { DrawChildren( D ); }

    void Widget::DrawChildren( DrawList& D )
    {
        Rect Clip = D.CurrentClip();
        Context* C = nullptr;
        for ( auto& K : Kids )
        {
            if ( !K->Visible ) continue;
            Vec2 Slide = K->Slide;
            if ( Slide.x != 0 || Slide.y != 0 ) // layout animation: ease back towards the real place (even off screen)
            {
                if ( !C ) C = GetContext();
                float Keep = C ? std::exp( -14.f * C->DeltaTime() ) : 0.f;
                K->Slide = K->Slide * Keep;
                if ( std::fabs( K->Slide.x ) < 0.3f && std::fabs( K->Slide.y ) < 0.3f ) K->Slide = {};
                if ( C ) C->RequestFrame();
            }
            const Rect B = K->Bounds.Offset( Slide );
            if ( B.y > Clip.Bottom() || B.Bottom() < Clip.y || B.x > Clip.Right() || B.Right() < Clip.x ) continue;
            if ( Slide.x == 0 && Slide.y == 0 ) { K->Draw( D ); continue; }
            size_t V0 = D.Vtx.size(), C0 = D.Cmds.size();
            K->Draw( D );
            D.TransformFrom( V0, C0, {}, 1, 0, Slide );
        }
    }

    void Responsive( Widget& W, float Bp, std::function<void( bool )> Fn )
    {
        auto State = std::make_shared<int>( -1 );
        W.OnResize = [ Bp, Fn, State ]( Widget&, float Wd ) {
            int Wide = Wd >= Bp ? 1 : 0;
            if ( Wide == *State ) return;
            *State = Wide;
            if ( Fn ) Fn( Wide == 1 );
        };
    }

    // ------------------------------------------------------------ layout

    Vec2 Widget::MeasureCached( float Avail )
    {
        if ( CacheGen != LayoutGeneration )
        {
            CacheGen = LayoutGeneration;
            Cache[ 0 ].Avail = Cache[ 1 ].Avail = -1;
        }
        for ( auto& E : Cache )
            if ( E.Avail == Avail ) return E.Size;
        Vec2 S = Measure( Avail );
        if ( Layout.MaxWidth > 0 ) S.x = std::min( S.x, Layout.MaxWidth );
        S.x = std::max( S.x, Layout.MinWidth );
        if ( Layout.Aspect > 0 ) S.y = S.x / Layout.Aspect;
        if ( Layout.MaxHeight > 0 ) S.y = std::min( S.y, Layout.MaxHeight );
        S.y = std::max( S.y, Layout.MinHeight );
        Cache[ 1 ] = Cache[ 0 ];
        Cache[ 0 ] = { Avail, S };
        return S;
    }

    Vec2 Widget::Measure( float Avail )
    {
        const Insets& P = Layout.Padding;
        Vec2 C = MeasureChildren( std::max( 0.f, Avail - P.Horizontal() ) );
        return { C.x + P.Horizontal(), C.y + P.Vertical() };
    }

    void Widget::Arrange( Rect R )
    {
        Bounds = R;
        ArrangeChildren( R.Shrink( Layout.Padding ) );
    }

    static float AlignOffset( Align A, float Free );
    static bool Fixed( const Size& S ) { return S.K == Size::Kind::Fixed; }
    static bool Grows( const Size& S ) { return S.K == Size::Kind::Grow; }

    // Width a child gets on the cross axis of a Column (or for a Stack), given the inner width.
    static float CrossWidth( Widget& K, Align CrossAlign, float InnerW )
    {
        const Size& W = K.Layout.Width;
        if ( Fixed( W ) ) return W.V;
        if ( Grows( W ) || CrossAlign == Align::Stretch ) return InnerW;
        return std::min( K.MeasureCached( InnerW ).x, InnerW );
    }

    Vec2 Widget::MeasureChildren( float InnerW )
    {
        Vec2 Out;
        std::vector<Widget*> Vis;
        for ( auto& K : Kids )
            if ( K->Visible ) Vis.push_back( K.get() );
        if ( Vis.empty() ) return Out;
        const float Gaps = Layout.Gap * float( Vis.size() - 1 );

        if ( Layout.Direction == Dir::Free )
        {
            for ( Widget* K : Vis )
            {
                const LayoutSpec& L = K->Layout;
                float W = Fixed( L.Width ) ? L.Width.V : K->MeasureCached( InnerW ).x;
                float H = Fixed( L.Height ) ? L.Height.V : K->MeasureCached( W ).y;
                Out.x = std::max( Out.x, L.Offset.x + W );
                Out.y = std::max( Out.y, L.Offset.y + H );
            }
            return Out;
        }

        if ( Layout.Direction == Dir::Grid || Layout.Direction == Dir::Wrap )
            return MeasureFlow( Vis, InnerW, nullptr );

        if ( Layout.Direction == Dir::Row && !( Layout.StackBelow > 0 && InnerW < Layout.StackBelow ) )
        {
            // Same width split as ArrangeChildren, so wrapped text inside Grow children
            // is measured at the width it will actually get.
            std::vector<float> W( Vis.size() );
            float Used = Gaps, Weights = 0;
            for ( size_t i = 0; i < Vis.size(); i++ )
            {
                const Size& S = Vis[ i ]->Layout.Width;
                float Nat = Fixed( S ) ? S.V : Vis[ i ]->MeasureCached( InnerW ).x;
                Out.x += Nat;
                if ( Grows( S ) ) { Weights += S.V; W[ i ] = -1; continue; }
                W[ i ] = Nat;
                Used += Nat;
            }
            float Free = std::max( 0.f, InnerW - Used );
            for ( size_t i = 0; i < Vis.size(); i++ )
            {
                if ( W[ i ] < 0 ) W[ i ] = std::max( Vis[ i ]->Layout.MinWidth, std::floor( Free * Vis[ i ]->Layout.Width.V / Weights ) );
                const Size& H = Vis[ i ]->Layout.Height;
                Out.y = std::max( Out.y, Fixed( H ) ? H.V : Vis[ i ]->MeasureCached( W[ i ] ).y );
            }
            Out.x += Gaps;
            return Out;
        }

        for ( Widget* K : Vis )
        {
            const LayoutSpec& L = K->Layout;
            float NatW = Fixed( L.Width ) ? L.Width.V : K->MeasureCached( InnerW ).x;
            float W = CrossWidth( *K, Layout.CrossAlign, InnerW );
            float H = Fixed( L.Height ) ? L.Height.V : K->MeasureCached( W ).y;
            Out.x = std::max( Out.x, NatW );
            Out.y = Layout.Direction == Dir::Column ? Out.y + H : std::max( Out.y, H );
        }
        bool Col = Layout.Direction == Dir::Column || Layout.Direction == Dir::Row; // a Row stacked below its breakpoint
        if ( Layout.Direction == Dir::Row ) // re-measure as a column
        {
            Out = {};
            for ( Widget* K : Vis )
            {
                float W = Grows( K->Layout.Width ) || Layout.CrossAlign == Align::Stretch ? InnerW : std::min( InnerW, K->MeasureCached( InnerW ).x );
                Out.x = std::max( Out.x, W );
                Out.y += Fixed( K->Layout.Height ) ? K->Layout.Height.V : K->MeasureCached( W ).y;
            }
        }
        if ( Col ) Out.y += Gaps;
        return Out;
    }

    // Grid and Wrap share one placement pass; with Place == nullptr it only measures.
    Vec2 Widget::MeasureFlow( const std::vector<Widget*>& Vis, float InnerW, const Rect* Place )
    {
        const float Gap = Layout.Gap, RGap = Layout.RowGap >= 0 ? Layout.RowGap : Layout.Gap;
        Vec2 Out;
        if ( Layout.Direction == Dir::Grid )
        {
            int Cols = Layout.Columns;
            if ( Layout.ColumnMin > 0 ) Cols = int( ( InnerW + Gap ) / ( Layout.ColumnMin + Gap ) );
            Cols = std::max( 1, Cols );
            float CW = std::max( 0.f, ( InnerW - Gap * ( Cols - 1 ) ) / Cols );
            // place cells row-major, skipping cells taken by row spans
            std::vector<std::vector<bool>> Taken;
            struct Cell { Widget* W; int R, C, Cs, Rs; float H; };
            std::vector<Cell> Cells;
            int Row = 0, Col = 0;
            auto Take = [ & ]( int R, int C ) { while ( int( Taken.size() ) <= R ) Taken.push_back( std::vector<bool>( size_t( Cols ), false ) ); return Taken[ size_t( R ) ][ size_t( C ) ]; };
            for ( Widget* K : Vis )
            {
                int Cs = std::clamp( K->Layout.ColSpan, 1, Cols ), Rs = std::max( 1, K->Layout.RowSpan );
                for ( ;; )
                {
                    if ( Col + Cs > Cols ) { Row++; Col = 0; }
                    bool Free = true;
                    for ( int c = Col; c < Col + Cs && Free; c++ ) Free = !Take( Row, c );
                    if ( Free ) break;
                    Col++;
                }
                for ( int r = Row; r < Row + Rs; r++ )
                    for ( int c = Col; c < Col + Cs; c++ ) { Take( r, c ); Taken[ size_t( r ) ][ size_t( c ) ] = true; }
                float W = CW * Cs + Gap * ( Cs - 1 );
                float H = Fixed( K->Layout.Height ) ? K->Layout.Height.V : K->Layout.Aspect > 0 ? W / K->Layout.Aspect : K->MeasureCached( W ).y;
                Cells.push_back( { K, Row, Col, Cs, Rs, H } );
                Col += Cs;
            }
            int Rows = int( Taken.size() );
            std::vector<float> RowH( size_t( Rows ), 0 );
            for ( auto& C : Cells )
                if ( C.Rs == 1 ) RowH[ size_t( C.R ) ] = std::max( RowH[ size_t( C.R ) ], C.H );
            for ( auto& C : Cells ) // spanning cells that are taller than their rows grow the last row
                if ( C.Rs > 1 )
                {
                    float Sum = RGap * ( C.Rs - 1 );
                    for ( int r = C.R; r < C.R + C.Rs && r < Rows; r++ ) Sum += RowH[ size_t( r ) ];
                    if ( C.H > Sum ) RowH[ size_t( std::min( Rows - 1, C.R + C.Rs - 1 ) ) ] += C.H - Sum;
                }
            std::vector<float> RowY( size_t( Rows ), 0 );
            float Y = 0;
            for ( int r = 0; r < Rows; r++ ) { RowY[ size_t( r ) ] = Y; Y += RowH[ size_t( r ) ] + ( r + 1 < Rows ? RGap : 0 ); }
            Out = { InnerW, Y };
            if ( Place )
                for ( auto& C : Cells )
                {
                    float W = CW * C.Cs + Gap * ( C.Cs - 1 );
                    float H = RGap * ( C.Rs - 1 );
                    for ( int r = C.R; r < C.R + C.Rs && r < Rows; r++ ) H += RowH[ size_t( r ) ];
                    float Ch = Layout.CrossAlign == Align::Stretch || Grows( C.W->Layout.Height ) ? H : std::min( H, C.H );
                    C.W->Arrange( { Place->x + C.C * ( CW + Gap ), Place->y + RowY[ size_t( C.R ) ] + AlignOffset( Layout.CrossAlign, H - Ch ), W, Ch } );
                }
            return Out;
        }
        // Wrap: lines of natural-width children
        struct Item { Widget* W; float Wd, Ht; };
        std::vector<std::vector<Item>> Lines( 1 );
        float X = 0;
        for ( Widget* K : Vis )
        {
            float W = Fixed( K->Layout.Width ) ? K->Layout.Width.V : std::min( InnerW, K->MeasureCached( InnerW ).x );
            float H = Fixed( K->Layout.Height ) ? K->Layout.Height.V : K->MeasureCached( W ).y;
            if ( X > 0 && X + W > InnerW ) { Lines.emplace_back(); X = 0; }
            Lines.back().push_back( { K, W, H } );
            X += W + Gap;
        }
        float Y = 0;
        for ( size_t l = 0; l < Lines.size(); l++ )
        {
            float LineW = -Gap, LineH = 0;
            for ( auto& I : Lines[ l ] ) { LineW += I.Wd + Gap; LineH = std::max( LineH, I.Ht ); }
            Out.x = std::max( Out.x, LineW );
            if ( Place )
            {
                float Px = AlignOffset( Layout.MainAlign, std::max( 0.f, InnerW - LineW ) );
                for ( auto& I : Lines[ l ] )
                {
                    float H = Layout.CrossAlign == Align::Stretch ? LineH : I.Ht;
                    I.W->Arrange( { Place->x + Px, Place->y + Y + AlignOffset( Layout.CrossAlign, LineH - H ), I.Wd, H } );
                    Px += I.Wd + Gap;
                }
            }
            Y += LineH + ( l + 1 < Lines.size() ? RGap : 0 );
        }
        Out.y = Y;
        return Out;
    }

    static float AlignOffset( Align A, float Free )
    {
        switch ( A )
        {
        case Align::Center: return std::floor( Free * 0.5f );
        case Align::End: return Free;
        default: return 0;
        }
    }

    void Widget::ArrangeChildren( Rect In )
    {
        // remember where children were: AnimateChildren slides them; OnResize hears about width changes
        std::vector<std::pair<Widget*, Vec2>> Was;
        for ( auto& K : Kids )
            if ( K->Visible ) Was.push_back( { K.get(), K->Bounds.w > 0 ? K->Bounds.Pos() : Vec2{ -1e9f, -1e9f } } );
        ArrangeChildrenImpl( In );
        if ( Layout.AnimateChildren )
            for ( auto& [ K, P ] : Was )
                if ( P.x > -1e8f ) K->Slide = K->Slide + ( P - K->Bounds.Pos() );
    }

    void Widget::ArrangeChildrenImpl( Rect In )
    {
        std::vector<Widget*> Vis;
        for ( auto& K : Kids )
            if ( K->Visible ) Vis.push_back( K.get() );
        if ( Vis.empty() ) return;

        const LayoutSpec& L = Layout;
        if ( L.Direction == Dir::Grid || L.Direction == Dir::Wrap ) { MeasureFlow( Vis, In.w, &In ); return; }

        if ( L.Direction == Dir::Free )
        {
            for ( Widget* K : Vis )
            {
                const LayoutSpec& KL = K->Layout;
                float W = Fixed( KL.Width ) ? KL.Width.V : Grows( KL.Width ) ? In.w - KL.Offset.x : K->MeasureCached( In.w ).x;
                float H = Fixed( KL.Height ) ? KL.Height.V : Grows( KL.Height ) ? In.h - KL.Offset.y : K->MeasureCached( W ).y;
                K->Arrange( { In.x + KL.Offset.x, In.y + KL.Offset.y, W, H } );
            }
            return;
        }

        if ( L.Direction == Dir::Stack )
        {
            for ( Widget* K : Vis )
            {
                float W = CrossWidth( *K, L.CrossAlign, In.w );
                float H = Fixed( K->Layout.Height ) ? K->Layout.Height.V
                        : ( Grows( K->Layout.Height ) || L.CrossAlign == Align::Stretch ) ? In.h
                        : K->MeasureCached( W ).y;
                K->Arrange( { In.x + AlignOffset( L.CrossAlign, In.w - W ), In.y + AlignOffset( L.CrossAlign, In.h - H ), W, H } );
            }
            return;
        }

        const bool Row = L.Direction == Dir::Row && !( L.StackBelow > 0 && In.w < L.StackBelow );
        const float MainAvail = Row ? In.w : In.h;
        const float CrossAvail = Row ? In.h : In.w;

        // Pass 1: cross size (for Column) and base main size of each child.
        struct Item { float Main, Cross, Weight; };
        std::vector<Item> It( Vis.size() );
        float Used = L.Gap * float( Vis.size() - 1 ), Weights = 0;
        for ( size_t i = 0; i < Vis.size(); i++ )
        {
            Widget& K = *Vis[ i ];
            const Size& MainS = Row ? K.Layout.Width : K.Layout.Height;
            Item& I = It[ i ];
            I.Weight = Grows( MainS ) ? MainS.V : 0;
            if ( Row )
            {
                I.Main = Fixed( MainS ) ? MainS.V : I.Weight > 0 ? 0 : K.MeasureCached( In.w ).x;
            }
            else
            {
                I.Cross = CrossWidth( K, L.CrossAlign, In.w );
                if ( K.Layout.MaxWidth > 0 ) I.Cross = std::min( I.Cross, K.Layout.MaxWidth );
                I.Main = Fixed( MainS ) ? MainS.V : K.Layout.Aspect > 0 ? I.Cross / K.Layout.Aspect : I.Weight > 0 ? 0 : K.MeasureCached( I.Cross ).y;
                if ( K.Layout.Aspect > 0 ) I.Weight = 0;
            }
            I.Main = std::max( I.Main, Row ? K.Layout.MinWidth : K.Layout.MinHeight );
            Used += I.Main;
            Weights += I.Weight;
        }

        // Pass 2: hand leftover space to Grow children.
        float Free = MainAvail - Used;
        if ( Weights > 0 && Free > 0 )
        {
            float Given = 0;
            for ( size_t i = 0; i < It.size(); i++ )
            {
                Item& I = It[ i ];
                if ( I.Weight <= 0 ) continue;
                float Add = std::floor( Free * I.Weight / Weights );
                float Max = Row ? Vis[ i ]->Layout.MaxWidth : Vis[ i ]->Layout.MaxHeight;
                if ( Max > 0 ) Add = std::min( Add, std::max( 0.f, Max - I.Main ) );
                I.Main += Add;
                Given += Add;
            }
            Free -= Given; // what MaxWidth / MaxHeight held back stays free (MainAlign places it)
        }

        // Pass 3: cross size for Rows (height may depend on the width we just decided).
        if ( Row )
        {
            for ( size_t i = 0; i < Vis.size(); i++ )
            {
                Widget& K = *Vis[ i ];
                const Size& H = K.Layout.Height;
                It[ i ].Cross = Fixed( H ) ? H.V
                              : K.Layout.Aspect > 0 ? It[ i ].Main / K.Layout.Aspect
                              : ( Grows( H ) || L.CrossAlign == Align::Stretch ) ? CrossAvail
                              : std::min( K.MeasureCached( It[ i ].Main ).y, CrossAvail );
                if ( K.Layout.MaxHeight > 0 ) It[ i ].Cross = std::min( It[ i ].Cross, K.Layout.MaxHeight );
            }
        }

        // Pass 4: place.
        float Cursor = AlignOffset( L.MainAlign, std::max( 0.f, Free ) );
        for ( size_t i = 0; i < Vis.size(); i++ )
        {
            const Item& I = It[ i ];
            float CrossPos = AlignOffset( L.CrossAlign, CrossAvail - I.Cross );
            Rect R = Row ? Rect{ In.x + Cursor, In.y + CrossPos, I.Main, I.Cross }
                         : Rect{ In.x + CrossPos, In.y + Cursor, I.Cross, I.Main };
            Vis[ i ]->Arrange( R );
            Cursor += I.Main + L.Gap;
        }
    }
}
