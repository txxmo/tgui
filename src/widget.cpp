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
        for ( auto& K : Kids )
        {
            if ( !K->Visible ) continue;
            const Rect& B = K->Bounds;
            if ( B.y > Clip.Bottom() || B.Bottom() < Clip.y || B.x > Clip.Right() || B.Right() < Clip.x ) continue;
            K->Draw( D );
        }
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
        S.x = std::max( S.x, Layout.MinWidth );
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

        if ( Layout.Direction == Dir::Row )
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
        if ( Layout.Direction == Dir::Column ) Out.y += Gaps;
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
        std::vector<Widget*> Vis;
        for ( auto& K : Kids )
            if ( K->Visible ) Vis.push_back( K.get() );
        if ( Vis.empty() ) return;

        const LayoutSpec& L = Layout;

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

        const bool Row = L.Direction == Dir::Row;
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
                I.Main = Fixed( MainS ) ? MainS.V : I.Weight > 0 ? 0 : K.MeasureCached( I.Cross ).y;
            }
            I.Main = std::max( I.Main, Row ? K.Layout.MinWidth : K.Layout.MinHeight );
            Used += I.Main;
            Weights += I.Weight;
        }

        // Pass 2: hand leftover space to Grow children.
        float Free = MainAvail - Used;
        if ( Weights > 0 && Free > 0 )
        {
            for ( auto& I : It )
                if ( I.Weight > 0 ) I.Main += std::floor( Free * I.Weight / Weights );
            Free = 0;
        }

        // Pass 3: cross size for Rows (height may depend on the width we just decided).
        if ( Row )
        {
            for ( size_t i = 0; i < Vis.size(); i++ )
            {
                Widget& K = *Vis[ i ];
                const Size& H = K.Layout.Height;
                It[ i ].Cross = Fixed( H ) ? H.V
                              : ( Grows( H ) || L.CrossAlign == Align::Stretch ) ? CrossAvail
                              : std::min( K.MeasureCached( It[ i ].Main ).y, CrossAvail );
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
