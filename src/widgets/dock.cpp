#include "tg/widgets/dock.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tg
{
    struct DockSpace::Node
    {
        bool IsSplit = false;
        Dir D = Dir::Row;
        float Ratio = 0.5f;
        std::unique_ptr<Node> A, B;
        Node* Parent = nullptr;
        std::vector<DockPanel*> Tabs;
        int Active = 0;
        bool Floating = false;
        Rect FloatR;           // floating: relative to the DockSpace
        Rect R, Bar, Body, Handle;

        DockPanel* Current() const { return Tabs.empty() ? nullptr : Tabs[ size_t( std::clamp( Active, 0, int( Tabs.size() ) - 1 ) ) ]; }
        bool Empty() const { return !IsSplit && Tabs.empty(); }
    };

    DockPanel::DockPanel( std::string T, std::string I ) : Container( Dir::Column, 10, Insets( 12 ) ), Title( std::move( T ) ), Id( std::move( I ) )
    {
        Layout.Width = Grow();
        Layout.Height = Grow();
    }

    DockSpace::DockSpace() : Root( std::make_unique<Node>() )
    {
        Interactive = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
    }

    DockSpace::~DockSpace() = default;

    // ---- tree helpers

    template<typename F>
    static void ForLeaves( DockSpace::Node* N, F&& Fn )
    {
        if ( !N ) return;
        if ( N->IsSplit )
        {
            ForLeaves( N->A.get(), Fn );
            ForLeaves( N->B.get(), Fn );
        }
        else Fn( N );
    }

    DockSpace::Node* DockSpace::FirstLeaf( Node* N ) const
    {
        while ( N && N->IsSplit ) N = N->A.get();
        return N;
    }

    DockSpace::Node* DockSpace::LeafOf( const DockPanel* P, Node** FloatRoot ) const
    {
        Node* Found = nullptr;
        ForLeaves( Root.get(), [ & ]( Node* L ) { if ( std::find( L->Tabs.begin(), L->Tabs.end(), P ) != L->Tabs.end() ) Found = L; } );
        if ( Found ) return Found;
        for ( auto& F : Floats )
            if ( std::find( F->Tabs.begin(), F->Tabs.end(), P ) != F->Tabs.end() )
            {
                if ( FloatRoot ) *FloatRoot = F.get();
                return F.get();
            }
        return nullptr;
    }

    void DockSpace::Prune( Node* Leaf )
    {
        if ( !Leaf->Empty() || Leaf == Root.get() ) return;
        Node* S = Leaf->Parent;
        if ( !S ) return;
        std::unique_ptr<Node> Sibling = std::move( S->A.get() == Leaf ? S->B : S->A );
        Sibling->Parent = S->Parent;
        if ( !S->Parent ) Root = std::move( Sibling ); // destroys S (and Leaf)
        else if ( S->Parent->A.get() == S ) S->Parent->A = std::move( Sibling );
        else S->Parent->B = std::move( Sibling );
    }

    void DockSpace::Unlink( DockPanel* P )
    {
        Node* L = LeafOf( P );
        if ( !L ) return;
        auto It = std::find( L->Tabs.begin(), L->Tabs.end(), P );
        int Idx = int( It - L->Tabs.begin() );
        L->Tabs.erase( It );
        if ( L->Active > Idx || L->Active >= int( L->Tabs.size() ) ) L->Active = std::max( 0, L->Active - 1 );
        if ( Doing != Act::None && ( ActNode == L || ActPanel == P ) && L->Tabs.empty() ) { Doing = Act::None; ActNode = nullptr; }
        if ( L->Floating )
        {
            if ( L->Tabs.empty() )
            {
                if ( DropLeaf == L ) { DropLeaf = nullptr; HasDrop = false; }
                Floats.erase( std::find_if( Floats.begin(), Floats.end(), [ L ]( auto& F ) { return F.get() == L; } ) );
            }
        }
        else
        {
            if ( L->Tabs.empty() && DropLeaf == L ) { DropLeaf = nullptr; HasDrop = false; }
            Prune( L );
        }
        Invalidate();
    }

    // ---- public API

    DockPanel& DockSpace::AddPanel( std::string Title, std::string Id )
    {
        if ( Id.empty() ) Id = Title;
        DockPanel& P = Add<DockPanel>( std::move( Title ), std::move( Id ) );
        P.Visible = false;
        Dock( P, LastAdded && LastAdded->Open ? LastAdded : nullptr, DockSide::Center );
        LastAdded = &P;
        return P;
    }

    void DockSpace::Dock( DockPanel& P, DockPanel* Target, DockSide Side, float Ratio )
    {
        if ( Target == &P || P.GetParent() != this ) return; // (not ours, or popped out into another window)
        Unlink( &P );
        P.Open = true;
        Node* T = Target ? LeafOf( Target ) : nullptr;
        if ( T && T->Floating ) Side = DockSide::Center;
        if ( Side == DockSide::Center )
        {
            if ( !T ) T = FirstLeaf( Root.get() );
            T->Tabs.push_back( &P );
            T->Active = int( T->Tabs.size() ) - 1;
            Invalidate();
            return;
        }
        Ratio = std::clamp( Ratio, 0.05f, 0.95f );
        std::unique_ptr<Node>* Slot = &Root;
        if ( T )
        {
            if ( T->Parent ) Slot = T->Parent->A.get() == T ? &T->Parent->A : &T->Parent->B;
        }
        else if ( Root->Empty() ) // nothing docked: just fill it
        {
            Root->Tabs.push_back( &P );
            Root->Active = 0;
            Invalidate();
            return;
        }
        auto New = std::make_unique<Node>();
        New->Tabs.push_back( &P );
        auto S = std::make_unique<Node>();
        S->IsSplit = true;
        S->D = Side == DockSide::Left || Side == DockSide::Right ? Dir::Row : Dir::Column;
        std::unique_ptr<Node> Old = std::move( *Slot );
        S->Parent = Old->Parent;
        Old->Parent = S.get();
        New->Parent = S.get();
        if ( Side == DockSide::Left || Side == DockSide::Top )
        {
            S->A = std::move( New );
            S->B = std::move( Old );
            S->Ratio = Ratio;
        }
        else
        {
            S->A = std::move( Old );
            S->B = std::move( New );
            S->Ratio = 1 - Ratio;
        }
        *Slot = std::move( S );
        Invalidate();
    }

    void DockSpace::Float( DockPanel& P, Rect R )
    {
        if ( P.GetParent() != this ) return;
        Unlink( &P );
        P.Open = true;
        auto N = std::make_unique<Node>();
        N->Floating = true;
        N->Tabs.push_back( &P );
        N->FloatR = R;
        Floats.push_back( std::move( N ) );
        Invalidate();
    }

    void DockSpace::Close( DockPanel& P )
    {
        if ( !P.Open ) return;
        Node* L = LeafOf( &P );
        P.WasFloat = false;
        P.ReturnTo.clear();
        if ( L )
        {
            if ( L->Floating )
            {
                P.WasFloat = L->Tabs.size() == 1;
                P.LastFloat = L->FloatR;
            }
            for ( DockPanel* Q : L->Tabs ) // a tab mate: come back next to it
                if ( Q != &P )
                {
                    P.ReturnTo = Q->Id;
                    P.ReturnSide = DockSide::Center;
                    break;
                }
            if ( P.ReturnTo.empty() && !L->Floating && L->Parent ) // alone: come back on the same side of the neighbour
            {
                Node* S = L->Parent;
                bool First = S->A.get() == L;
                Node* Other = FirstLeaf( ( First ? S->B : S->A ).get() );
                if ( DockPanel* Q = Other->Current() )
                {
                    P.ReturnTo = Q->Id;
                    P.ReturnSide = S->D == Dir::Row ? ( First ? DockSide::Left : DockSide::Right ) : ( First ? DockSide::Top : DockSide::Bottom );
                    P.ReturnRatio = First ? S->Ratio : 1 - S->Ratio;
                }
            }
        }
        Unlink( &P );
        P.Open = false;
        P.Visible = false;
        if ( P.OnClose ) P.OnClose( P );
        Invalidate();
    }

    void DockSpace::Show( DockPanel& P )
    {
        if ( P.GetParent() != this ) return;
        if ( P.Open )
        {
            Activate( P );
            return;
        }
        DockPanel* Q = P.ReturnTo.empty() ? nullptr : Find( P.ReturnTo );
        if ( P.WasFloat ) Float( P, P.LastFloat );
        else if ( Q && Q->Open ) Dock( P, Q, P.ReturnSide, P.ReturnRatio );
        else Dock( P, nullptr, DockSide::Center );
    }

    void DockSpace::Activate( DockPanel& P )
    {
        Node* L = LeafOf( &P );
        if ( !L ) return;
        L->Active = int( std::find( L->Tabs.begin(), L->Tabs.end(), &P ) - L->Tabs.begin() );
        if ( L->Floating ) // to the top
        {
            auto It = std::find_if( Floats.begin(), Floats.end(), [ L ]( auto& F ) { return F.get() == L; } );
            std::rotate( It, It + 1, Floats.end() );
        }
        Invalidate();
    }

    bool DockSpace::IsOpen( const DockPanel& P ) const { return P.Open; }
    bool DockSpace::IsFloating( const DockPanel& P ) const
    {
        Node* L = LeafOf( &P );
        return L && L->Floating;
    }

    std::vector<DockPanel*> DockSpace::Panels() const
    {
        std::vector<DockPanel*> Out;
        for ( auto& K : Children() )
            if ( auto* P = dynamic_cast<DockPanel*>( K.get() ) ) Out.push_back( P );
        return Out;
    }

    DockPanel* DockSpace::Find( const std::string& Id ) const
    {
        for ( DockPanel* P : Panels() )
            if ( P->Id == Id ) return P;
        return nullptr;
    }

    Rect DockSpace::TabOf( const DockPanel& P ) const
    {
        Node* L = LeafOf( &P );
        if ( !L ) return {};
        return TabRect( L, size_t( std::find( L->Tabs.begin(), L->Tabs.end(), &P ) - L->Tabs.begin() ) );
    }

    void DockSpace::Changed()
    {
        if ( OnLayoutChange ) OnLayoutChange();
    }

    // ---- layout

    Vec2 DockSpace::Measure( float Avail ) { return { Avail, 240 }; }

    void DockSpace::Place( Node* N, Rect R )
    {
        N->R = R;
        if ( N->IsSplit )
        {
            const bool Row = N->D == Dir::Row;
            float Total = std::max( 0.f, ( Row ? R.w : R.h ) - Style.Handle );
            float Lo = std::min( Style.MinPane, Total * 0.5f );
            float First = std::clamp( std::round( Total * N->Ratio ), Lo, Total - Lo );
            if ( Row )
            {
                N->Handle = { R.x + First, R.y, Style.Handle, R.h };
                Place( N->A.get(), { R.x, R.y, First, R.h } );
                Place( N->B.get(), { N->Handle.Right(), R.y, R.Right() - N->Handle.Right(), R.h } );
            }
            else
            {
                N->Handle = { R.x, R.y + First, R.w, Style.Handle };
                Place( N->A.get(), { R.x, R.y, R.w, First } );
                Place( N->B.get(), { R.x, N->Handle.Bottom(), R.w, R.Bottom() - N->Handle.Bottom() } );
            }
            return;
        }
        N->Bar = { R.x, R.y, R.w, std::min( Style.TabHeight, R.h ) };
        N->Body = { R.x, N->Bar.Bottom(), R.w, std::max( 0.f, R.Bottom() - N->Bar.Bottom() ) };
        N->Active = std::clamp( N->Active, 0, std::max( 0, int( N->Tabs.size() ) - 1 ) );
        for ( size_t i = 0; i < N->Tabs.size(); i++ )
        {
            DockPanel* P = N->Tabs[ i ];
            P->Visible = int( i ) == N->Active;
            if ( P->Visible ) P->Arrange( N->Body );
        }
    }

    void DockSpace::Arrange( Rect R )
    {
        Bounds = R;
        for ( DockPanel* P : Panels() )
            if ( !LeafOf( P ) ) P->Visible = false;
        Place( Root.get(), R );
        for ( auto& F : Floats )
        {
            Rect& FR = F->FloatR;
            FR.w = std::clamp( FR.w, 120.f, std::max( 120.f, R.w ) );
            FR.h = std::clamp( FR.h, Style.TabHeight + 40, std::max( Style.TabHeight + 40, R.h ) );
            FR.x = std::clamp( FR.x, 40 - FR.w, std::max( 0.f, R.w - 40 ) ); // keep a grabbable piece on screen
            FR.y = std::clamp( FR.y, 0.f, std::max( 0.f, R.h - Style.TabHeight ) );
            Place( F.get(), FR.Offset( R.Pos() ) );
        }
    }

    Rect DockSpace::TabRect( Node* N, size_t i ) const
    {
        const Font& F = *GetTheme().Medium;
        float X = N->Bar.x + ( N->Floating ? 6 : 4 );
        float Avail = N->Bar.w - ( N->Floating ? 12 : 8 );
        std::vector<float> W( N->Tabs.size() );
        float Sum = 0;
        for ( size_t k = 0; k < N->Tabs.size(); k++ )
        {
            W[ k ] = std::ceil( F.Width( N->Tabs[ k ]->Title ) ) + Style.TabPad * 2 + ( N->Tabs[ k ]->Closable ? 16 : 0 ) + 4;
            Sum += W[ k ];
        }
        float Scale = Sum > Avail && Sum > 0 ? Avail / Sum : 1;
        for ( size_t k = 0; k < i; k++ ) X += std::floor( W[ k ] * Scale );
        return { X, N->Bar.y + 4, std::floor( W[ i ] * Scale ) - 2, N->Bar.h - 4 };
    }

    Rect DockSpace::CloseRect( Node* N, size_t i ) const
    {
        Rect T = TabRect( N, i );
        return { T.Right() - 22, T.y + ( T.h - 16 ) * 0.5f, 16, 16 };
    }

    // ---- drawing

    void DockSpace::DrawLeaf( DrawList& D, Node* N, bool Floating )
    {
        const Font& F = *GetTheme().Medium;
        Color Bar = Style.TabBar.Resolve();
        if ( Floating ) D.RectFilled( N->Bar, Bar, Style.FloatRadius, CornersTop );
        else D.RectFilled( N->Bar, Bar );
        D.RectFilled( N->Body, Style.Body.Resolve(), Floating ? Style.FloatRadius : 0, Floating ? CornersBottom : CornersAll );
        D.PushClip( N->Bar );
        for ( size_t i = 0; i < N->Tabs.size(); i++ )
        {
            DockPanel* P = N->Tabs[ i ];
            Rect T = TabRect( N, i );
            bool Act = int( i ) == N->Active, Hot = T.Contains( Mouse ) && Doing == Act::None;
            if ( Act ) D.RectFilled( T, Style.ActiveTab.Resolve(), 6, CornersTop );
            else if ( Hot ) D.RectFilled( T, Style.ActiveTab.Resolve().WithAlpha( 90 ), 6, CornersTop );
            if ( Act ) D.RectFilled( { T.x + 6, T.y, T.w - 12, 2 }, Style.Accent.Resolve(), 1 );
            float TextW = T.w - Style.TabPad * 2 - ( P->Closable ? 16 : 0 );
            std::string S = Ellipsize( F, P->Title, std::max( 0.f, TextW ) );
            D.Text( F, { std::floor( T.x + Style.TabPad ), std::floor( T.y + ( T.h - F.LineHeight ) * 0.5f ) }, S,
                    ( Act ? Style.ActiveText : Style.Text ).Resolve() );
            if ( P->Closable && ( Act || Hot ) && T.w > 40 )
            {
                Rect C = CloseRect( N, i );
                bool CH = C.Contains( Mouse ) && Doing == Act::None;
                if ( CH ) D.RectFilled( C, Style.Line.Resolve(), 4 );
                Color X = ( CH ? Style.ActiveText : Style.Text ).Resolve();
                Vec2 M = C.Center();
                D.Line( { M.x - 3.5f, M.y - 3.5f }, { M.x + 3.5f, M.y + 3.5f }, X, 1.4f );
                D.Line( { M.x + 3.5f, M.y - 3.5f }, { M.x - 3.5f, M.y + 3.5f }, X, 1.4f );
            }
        }
        D.PopClip();
        D.RectFilled( { N->Bar.x, N->Bar.Bottom() - 1, N->Bar.w, 1 }, Style.Line.Resolve() );
        if ( DockPanel* P = N->Current() )
        {
            D.PushClip( N->Body );
            P->Draw( D );
            D.PopClip();
        }
        else if ( !Floating )
        {
            const char* Msg = "Drop panels here";
            const Font& S = *GetTheme().Small;
            D.Text( S, { std::floor( N->Body.Center().x - S.Width( Msg ) * 0.5f ), std::floor( N->Body.Center().y ) }, Msg, Style.Text.Resolve() );
        }
    }

    static void DrawTree( DockSpace& Self, DrawList& D, DockSpace::Node* N, const std::function<void( DockSpace::Node* )>& Leaf,
                          const std::function<void( DockSpace::Node* )>& Split )
    {
        if ( N->IsSplit )
        {
            DrawTree( Self, D, N->A.get(), Leaf, Split );
            DrawTree( Self, D, N->B.get(), Leaf, Split );
            Split( N );
        }
        else Leaf( N );
    }

    void DockSpace::Draw( DrawList& D )
    {
        D.PushClip( Bounds );
        DrawTree( *this, D, Root.get(), [ & ]( Node* L ) { DrawLeaf( D, L, false ); },
                  [ & ]( Node* S ) {
                      bool Hot = ( Doing == Act::Split && ActNode == S ) || ( Doing == Act::None && S->Handle.Shrink( -2 ).Contains( Mouse ) );
                      Color C = ( Hot ? Style.Accent : Style.Line ).Resolve();
                      Vec2 M = S->Handle.Center();
                      if ( S->D == Dir::Row ) D.RectFilled( { std::floor( M.x - ( Hot ? 1 : 0.5f ) ), S->Handle.y, Hot ? 2.f : 1.f, S->Handle.h }, C );
                      else D.RectFilled( { S->Handle.x, std::floor( M.y - ( Hot ? 1 : 0.5f ) ), S->Handle.w, Hot ? 2.f : 1.f }, C );
                  } );
        for ( auto& F : Floats )
        {
            Node* N = F.get();
            D.Shadow( N->R, Style.FloatRadius, Color( 0, 0, 0, 110 ), 18, { 0, 6 } );
            DrawLeaf( D, N, true );
            D.RectStroke( N->R, ( Doing == Act::MoveFloat && ActNode == N ? Style.Accent : Style.Line ).Resolve(), Style.FloatRadius );
            Rect G = { N->R.Right() - Style.Grip, N->R.Bottom() - Style.Grip, Style.Grip, Style.Grip }; // resize grip
            Color GC = Style.Text.Resolve().WithAlpha( 120 );
            for ( int k = 1; k <= 2; k++ )
                D.Line( { G.Right() - 3 - k * 4.f, G.Bottom() - 3 }, { G.Right() - 3, G.Bottom() - 3 - k * 4.f }, GC, 1.2f );
        }
        // where a dragged window would land
        float Target = HasDrop ? 1.f : 0.f;
        if ( HasDrop )
        {
            if ( PreviewT < 0.01f ) PreviewAnim = DropPreview;
            float K = 1 - std::exp( -18 * 0.016f );
            PreviewAnim = { PreviewAnim.x + ( DropPreview.x - PreviewAnim.x ) * K, PreviewAnim.y + ( DropPreview.y - PreviewAnim.y ) * K,
                            PreviewAnim.w + ( DropPreview.w - PreviewAnim.w ) * K, PreviewAnim.h + ( DropPreview.h - PreviewAnim.h ) * K };
            if ( std::fabs( PreviewAnim.x - DropPreview.x ) + std::fabs( PreviewAnim.w - DropPreview.w ) + std::fabs( PreviewAnim.y - DropPreview.y ) +
                     std::fabs( PreviewAnim.h - DropPreview.h ) > 0.5f )
                Redraw();
        }
        Animate( PreviewT, Target, 20 );
        if ( PreviewT > 0.01f )
        {
            Color A = Style.Accent.Resolve();
            Rect P = PreviewAnim.Shrink( 3 );
            D.RectFilled( P, A.WithAlpha( uint8_t( 60 * PreviewT ) ), 6 );
            D.RectStroke( P, A.WithAlpha( uint8_t( 220 * PreviewT ) ), 6, 2 );
        }
        // the compass on the hovered panel while dragging
        if ( Doing == Act::MoveFloat && Moved && DropLeaf && !DropLeaf->Floating )
        {
            Vec2 C = DropLeaf->Body.Center();
            Color A = Style.Accent.Resolve(), Bg = Style.ActiveTab.Resolve();
            const Vec2 Off[ 5 ] = { { 0, 0 }, { -30, 0 }, { 30, 0 }, { 0, -30 }, { 0, 30 } };
            for ( int k = 0; k < 5; k++ )
            {
                Rect Sq = { C.x + Off[ k ].x - 12, C.y + Off[ k ].y - 12, 24, 24 };
                bool On = HasDrop && int( DropSide ) == k;
                D.RectFilled( Sq, On ? A : Bg, 5 );
                D.RectStroke( Sq, A, 5 );
                Rect In = Sq.Shrink( 6 );
                Color IC = On ? Bg : A;
                switch ( DockSide( k ) )
                {
                case DockSide::Center: D.RectStroke( In, IC, 2 ); break;
                case DockSide::Left: D.RectFilled( { In.x, In.y, In.w * 0.45f, In.h }, IC, 1 ); break;
                case DockSide::Right: D.RectFilled( { In.x + In.w * 0.55f, In.y, In.w * 0.45f, In.h }, IC, 1 ); break;
                case DockSide::Top: D.RectFilled( { In.x, In.y, In.w, In.h * 0.45f }, IC, 1 ); break;
                case DockSide::Bottom: D.RectFilled( { In.x, In.y + In.h * 0.55f, In.w, In.h * 0.45f }, IC, 1 ); break;
                }
            }
        }
        D.PopClip();
    }

    // ---- input

    Widget* DockSpace::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( Doing != Act::None ) return this;
        for ( size_t i = Floats.size(); i-- > 0; )
        {
            Node* N = Floats[ i ].get();
            if ( !N->R.Contains( P ) ) continue;
            Rect G = { N->R.Right() - Style.Grip, N->R.Bottom() - Style.Grip, Style.Grip, Style.Grip };
            if ( N->Bar.Contains( P ) || G.Contains( P ) ) return this;
            if ( DockPanel* C = N->Current() )
                if ( Widget* H = C->HitTest( P ) ) return H;
            return this; // the window's body eats the click (raises it)
        }
        Widget* Out = nullptr;
        bool Mine = false;
        std::function<void( Node* )> Walk = [ & ]( Node* N ) {
            if ( Out || Mine || ( !N->R.Contains( P ) && !( N->IsSplit && N->Handle.Shrink( -2 ).Contains( P ) ) ) ) return;
            if ( N->IsSplit )
            {
                if ( N->Handle.Shrink( -2 ).Contains( P ) ) { Mine = true; return; }
                Walk( N->A.get() );
                Walk( N->B.get() );
                return;
            }
            if ( N->Bar.Contains( P ) ) { Mine = true; return; }
            if ( DockPanel* C = N->Current() ) Out = C->HitTest( P );
        };
        Walk( Root.get() );
        return Mine ? this : Out;
    }

    Cursor DockSpace::GetCursor() const
    {
        Node* S = Doing == Act::Split ? ActNode : nullptr;
        if ( !S && Doing == Act::None )
        {
            std::function<Node*( Node* )> Find = [ & ]( Node* N ) -> Node* {
                if ( !N->IsSplit ) return nullptr;
                if ( N->Handle.Shrink( -2 ).Contains( Mouse ) ) return N;
                if ( Node* X = Find( N->A.get() ) ) return X;
                return Find( N->B.get() );
            };
            bool OverFloat = false;
            for ( auto& F : Floats ) OverFloat |= F->R.Contains( Mouse );
            if ( !OverFloat ) S = Find( Root.get() );
        }
        if ( S ) return S->D == Dir::Row ? Cursor::ResizeH : Cursor::ResizeV;
        return Cursor::Arrow;
    }

    bool DockSpace::PickDrop( Vec2 P )
    {
        HasDrop = false;
        DropLeaf = nullptr;
        if ( !Bounds.Contains( P ) ) return false;
        // other floating windows: drop in as a tab
        for ( size_t i = Floats.size(); i-- > 0; )
        {
            Node* N = Floats[ i ].get();
            if ( N == ActNode || !N->R.Contains( P ) ) continue;
            if ( !N->Bar.Contains( P ) ) return false; // over its body: nothing
            DropLeaf = N;
            DropSide = DockSide::Center;
            DropPreview = N->R;
            return HasDrop = true;
        }
        const Rect& B = Bounds;
        if ( Root->Empty() )
        {
            DropLeaf = Root.get();
            DropSide = DockSide::Center;
            DropPreview = B;
            return HasDrop = true;
        }
        Node* L = nullptr;
        ForLeaves( Root.get(), [ & ]( Node* N ) { if ( N->R.Contains( P ) ) L = N; } );
        if ( L && L->Bar.Contains( P ) ) // a tab strip: join as a tab (wins over the rim)
        {
            DropLeaf = L;
            DropSide = DockSide::Center;
            DropPreview = L->R;
            return HasDrop = true;
        }
        // the outer rim: the whole side of the space
        float Rim = Style.RimZone;
        float DL = P.x - B.x, DR = B.Right() - P.x, DT = P.y - B.y, DB = B.Bottom() - P.y;
        float M = std::min( { DL, DR, DT, DB } );
        if ( M < Rim )
        {
            DropSide = M == DL ? DockSide::Left : M == DR ? DockSide::Right : M == DT ? DockSide::Top : DockSide::Bottom;
            const float K = 0.3f;
            DropPreview = DropSide == DockSide::Left ? Rect{ B.x, B.y, B.w * K, B.h }
                        : DropSide == DockSide::Right ? Rect{ B.Right() - B.w * K, B.y, B.w * K, B.h }
                        : DropSide == DockSide::Top ? Rect{ B.x, B.y, B.w, B.h * K }
                                                    : Rect{ B.x, B.Bottom() - B.h * K, B.w, B.h * K };
            return HasDrop = true;
        }
        if ( !L ) return false;
        DropLeaf = L;
        // compass squares first, then regions
        Vec2 C = L->Body.Center();
        const Vec2 Off[ 5 ] = { { 0, 0 }, { -30, 0 }, { 30, 0 }, { 0, -30 }, { 0, 30 } };
        int Pick = -1;
        for ( int k = 0; k < 5; k++ )
            if ( Rect{ C.x + Off[ k ].x - 13, C.y + Off[ k ].y - 13, 26, 26 }.Contains( P ) ) Pick = k;
        if ( Pick < 0 )
        {
            if ( L->Bar.Contains( P ) ) Pick = 0;
            else
            {
                float NX = ( P.x - L->R.x ) / std::max( 1.f, L->R.w ), NY = ( P.y - L->R.y ) / std::max( 1.f, L->R.h );
                if ( NX > 0.3f && NX < 0.7f && NY > 0.3f && NY < 0.7f ) Pick = 0;
                else
                {
                    float E[ 4 ] = { NX, 1 - NX, NY, 1 - NY };
                    Pick = int( std::min_element( E, E + 4 ) - E ) + 1;
                }
            }
        }
        DropSide = DockSide( Pick );
        const Rect& R = L->R;
        DropPreview = DropSide == DockSide::Center ? R
                    : DropSide == DockSide::Left ? Rect{ R.x, R.y, R.w * 0.5f, R.h }
                    : DropSide == DockSide::Right ? Rect{ R.x + R.w * 0.5f, R.y, R.w * 0.5f, R.h }
                    : DropSide == DockSide::Top ? Rect{ R.x, R.y, R.w, R.h * 0.5f }
                                                : Rect{ R.x, R.y + R.h * 0.5f, R.w, R.h * 0.5f };
        return HasDrop = true;
    }

    bool DockSpace::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseLeave:
            if ( Doing == Act::None ) Mouse = { -1e9f, -1e9f };
            Redraw();
            return false;
        case EventType::MouseDown:
        {
            Mouse = E.Pos;
            PressAt = E.Pos;
            Moved = false;
            Doing = Act::None;
            ActNode = nullptr;
            ActPanel = nullptr;
            // which leaf's bar / which float / which handle
            Node* N = nullptr;
            for ( size_t i = Floats.size(); i-- > 0 && !N; )
                if ( Floats[ i ]->R.Contains( E.Pos ) ) N = Floats[ i ].get();
            if ( N && E.Button == MouseButton::Left ) // raise it
            {
                auto It = std::find_if( Floats.begin(), Floats.end(), [ N ]( auto& F ) { return F.get() == N; } );
                std::rotate( It, It + 1, Floats.end() );
                Redraw();
                Rect G = { N->R.Right() - Style.Grip, N->R.Bottom() - Style.Grip, Style.Grip, Style.Grip };
                if ( G.Contains( E.Pos ) )
                {
                    Doing = Act::ResizeFloat;
                    ActNode = N;
                    GrabOff = { N->R.Right() - E.Pos.x, N->R.Bottom() - E.Pos.y };
                    return true;
                }
            }
            if ( !N )
            {
                std::function<Node*( Node* )> Find = [ & ]( Node* X ) -> Node* {
                    if ( X->IsSplit )
                    {
                        if ( X->Handle.Shrink( -2 ).Contains( E.Pos ) ) return X;
                        if ( Node* Y = Find( X->A.get() ) ) return Y;
                        return Find( X->B.get() );
                    }
                    return X->Bar.Contains( E.Pos ) ? X : nullptr;
                };
                N = Find( Root.get() );
                if ( N && N->IsSplit )
                {
                    if ( E.Button != MouseButton::Left ) return false;
                    Doing = Act::Split;
                    ActNode = N;
                    return true;
                }
            }
            if ( !N || !N->Bar.Contains( E.Pos ) ) return N != nullptr; // floating body: just raised
            for ( size_t i = 0; i < N->Tabs.size(); i++ )
            {
                if ( !TabRect( N, i ).Contains( E.Pos ) ) continue;
                DockPanel* P = N->Tabs[ i ];
                bool OnX = P->Closable && CloseRect( N, i ).Contains( E.Pos ) && ( int( i ) == N->Active || true );
                if ( E.Button == MouseButton::Middle || ( E.Button == MouseButton::Left && OnX ) )
                {
                    if ( P->Closable )
                    {
                        Close( *P );
                        Changed();
                    }
                    return true;
                }
                if ( E.Button != MouseButton::Left ) return false;
                N->Active = int( i );
                Invalidate();
                Doing = Act::Tab;
                ActNode = N;
                ActPanel = P;
                GrabOff = { E.Pos.x - TabRect( N, i ).x, E.Pos.y - N->Bar.y };
                return true;
            }
            if ( E.Button != MouseButton::Left ) return false;
            if ( N->Floating )
            {
                if ( E.Clicks == 2 ) // dock it back
                {
                    std::vector<DockPanel*> Ts = N->Tabs;
                    for ( DockPanel* P : Ts ) Dock( *P, nullptr, DockSide::Center );
                    Changed();
                    return true;
                }
                Doing = Act::MoveFloat;
                ActNode = N;
                GrabOff = E.Pos - N->R.Pos();
            }
            return true;
        }
        case EventType::MouseMove:
        {
            Mouse = E.Pos;
            if ( Doing == Act::None )
            {
                Redraw(); // hover on tabs / close buttons / handles
                return false;
            }
            if ( !Moved && std::hypot( E.Pos.x - PressAt.x, E.Pos.y - PressAt.y ) < 5 ) return true;
            bool First = !Moved;
            Moved = true;
            switch ( Doing )
            {
            case Act::Split:
            {
                Node* S = ActNode;
                const bool Row = S->D == Dir::Row;
                float Total = ( Row ? S->R.w : S->R.h ) - Style.Handle;
                float Pos = ( Row ? E.Pos.x - S->R.x : E.Pos.y - S->R.y ) - Style.Handle * 0.5f;
                S->Ratio = Total > 0 ? std::clamp( Pos / Total, 0.f, 1.f ) : 0.5f;
                Arrange( Bounds );
                Redraw();
                return true;
            }
            case Act::Tab:
            {
                Node* N = ActNode;
                // still on the strip: reorder
                if ( N->Bar.Shrink( Insets( 0, -10 ) ).Contains( E.Pos ) && N->Tabs.size() > 1 )
                {
                    int From = int( std::find( N->Tabs.begin(), N->Tabs.end(), ActPanel ) - N->Tabs.begin() );
                    for ( size_t i = 0; i < N->Tabs.size(); i++ )
                    {
                        Rect T = TabRect( N, i );
                        if ( int( i ) != From && E.Pos.x >= T.x && E.Pos.x < T.Right() )
                        {
                            N->Tabs.erase( N->Tabs.begin() + From );
                            N->Tabs.insert( N->Tabs.begin() + int( i ), ActPanel );
                            N->Active = int( i );
                            Invalidate();
                            break;
                        }
                    }
                    return true;
                }
                if ( N->Floating && N->Tabs.size() == 1 ) // the window itself
                {
                    Doing = Act::MoveFloat;
                    GrabOff = PressAt - N->R.Pos();
                }
                else // tear the tab off into its own window under the cursor
                {
                    float W = std::clamp( N->R.w, 220.f, 460.f ), H = std::clamp( N->R.h, 160.f, 340.f );
                    Vec2 Grab = { std::min( GrabOff.x, W - 40 ), GrabOff.y };
                    Float( *ActPanel, { E.Pos.x - Bounds.x - Grab.x, E.Pos.y - Bounds.y - Grab.y, W, H } );
                    Doing = Act::MoveFloat;
                    ActNode = Floats.back().get();
                    GrabOff = Grab;
                    Arrange( Bounds );
                }
                [[fallthrough]];
            }
            case Act::MoveFloat:
            {
                (void)First;
                ActNode->FloatR.x = E.Pos.x - GrabOff.x - Bounds.x;
                ActNode->FloatR.y = E.Pos.y - GrabOff.y - Bounds.y;
                bool Hold = ( E.Mods & ModCtrl ) != 0; // Ctrl: just move, don't dock
                if ( Hold ) HasDrop = false, DropLeaf = nullptr;
                else PickDrop( E.Pos );
                Arrange( Bounds );
                Redraw();
                return true;
            }
            case Act::ResizeFloat:
            {
                Node* N = ActNode;
                N->FloatR.w = E.Pos.x + GrabOff.x - N->R.x;
                N->FloatR.h = E.Pos.y + GrabOff.y - N->R.y;
                Arrange( Bounds );
                Redraw();
                return true;
            }
            default: return true;
            }
        }
        case EventType::MouseUp:
        {
            if ( Doing == Act::None ) return false;
            Act Was = Doing;
            Doing = Act::None;
            if ( Was == Act::MoveFloat && HasDrop && ActNode )
            {
                std::vector<DockPanel*> Ts = ActNode->Tabs;
                Node* T = DropLeaf;
                DockSide Side = DropSide;
                DockPanel* Target = T ? T->Current() : nullptr;
                HasDrop = false;
                DropLeaf = nullptr;
                if ( T && !Target ) Target = nullptr; // an empty root: Dock( nullptr ) fills it
                if ( !T ) Target = nullptr;           // rim
                DockPanel* Lead = Ts.empty() ? nullptr : Ts[ size_t( ActNode->Active ) < Ts.size() ? size_t( ActNode->Active ) : 0 ];
                ActNode = nullptr;
                if ( Lead )
                {
                    Dock( *Lead, Target, Side, T ? 0.5f : 0.3f );
                    for ( DockPanel* P : Ts )
                        if ( P != Lead ) Dock( *P, Lead, DockSide::Center );
                    Activate( *Lead );
                }
            }
            HasDrop = false;
            DropLeaf = nullptr;
            ActNode = nullptr;
            ActPanel = nullptr;
            Invalidate();
            if ( Moved ) Changed();
            return true;
        }
        default: return false;
        }
    }

    // ---- save / load

    struct DockLayoutIO
    {
        using Node = DockSpace::Node;

        static void Write( const Node* N, std::string& O )
        {
            char B[ 64 ];
            if ( N->IsSplit )
            {
                std::snprintf( B, sizeof B, "split(%s,%.4g,", N->D == Dir::Row ? "row" : "col", double( N->Ratio ) );
                O += B;
                Write( N->A.get(), O );
                O += ',';
                Write( N->B.get(), O );
                O += ')';
                return;
            }
            O += "tabs(";
            for ( size_t i = 0; i < N->Tabs.size(); i++ ) O += ( i ? "," : "" ) + N->Tabs[ i ]->Id;
            O += "|" + std::to_string( N->Active ) + ")";
        }

        struct Parser
        {
            const std::string& S;
            size_t I = 0;
            DockSpace& D;
            std::vector<DockPanel*> Used;

            bool Eat( const char* T )
            {
                size_t L = std::strlen( T );
                if ( S.compare( I, L, T ) != 0 ) return false;
                I += L;
                return true;
            }
            std::string Word()
            {
                size_t B = I;
                while ( I < S.size() && !std::strchr( "(),|;", S[ I ] ) ) I++;
                return S.substr( B, I - B );
            }
            bool Num( float& V )
            {
                std::string W = Word();
                char* End = nullptr;
                V = std::strtof( W.c_str(), &End );
                return !W.empty() && End && *End == 0;
            }
            std::unique_ptr<Node> Parse( int Depth = 0 )
            {
                if ( Depth > 64 ) return nullptr;
                auto N = std::make_unique<Node>();
                if ( Eat( "split(" ) )
                {
                    N->IsSplit = true;
                    std::string Dw = Word();
                    if ( Dw != "row" && Dw != "col" ) return nullptr;
                    N->D = Dw == "row" ? Dir::Row : Dir::Column;
                    if ( !Eat( "," ) || !Num( N->Ratio ) || !Eat( "," ) ) return nullptr;
                    N->Ratio = std::clamp( N->Ratio, 0.02f, 0.98f );
                    N->A = Parse( Depth + 1 );
                    if ( !N->A || !Eat( "," ) ) return nullptr;
                    N->B = Parse( Depth + 1 );
                    if ( !N->B || !Eat( ")" ) ) return nullptr;
                    N->A->Parent = N.get();
                    N->B->Parent = N.get();
                    return N;
                }
                if ( !Eat( "tabs(" ) ) return nullptr;
                while ( I < S.size() && S[ I ] != '|' )
                {
                    std::string Id = Word();
                    DockPanel* P = Id.empty() ? nullptr : D.Find( Id );
                    if ( P && std::find( Used.begin(), Used.end(), P ) == Used.end() )
                    {
                        N->Tabs.push_back( P );
                        Used.push_back( P );
                    }
                    if ( !Eat( "," ) ) break;
                }
                float A = 0;
                if ( !Eat( "|" ) || !Num( A ) || !Eat( ")" ) ) return nullptr;
                N->Active = std::clamp( int( A ), 0, std::max( 0, int( N->Tabs.size() ) - 1 ) );
                return N;
            }
        };

        // drop empty leaves, collapsing their splits
        static void Clean( std::unique_ptr<Node>& Slot )
        {
            if ( !Slot->IsSplit ) return;
            Clean( Slot->A );
            Clean( Slot->B );
            Node* Up = Slot->Parent;
            if ( Slot->A->Empty() ) { auto K = std::move( Slot->B ); K->Parent = Up; Slot = std::move( K ); }
            else if ( Slot->B->Empty() ) { auto K = std::move( Slot->A ); K->Parent = Up; Slot = std::move( K ); }
        }
    };

    std::string DockSpace::SaveLayout() const
    {
        std::string O;
        DockLayoutIO::Write( Root.get(), O );
        for ( auto& F : Floats )
        {
            char B[ 96 ];
            std::snprintf( B, sizeof B, ";float(%.0f,%.0f,%.0f,%.0f,", double( F->FloatR.x ), double( F->FloatR.y ), double( F->FloatR.w ), double( F->FloatR.h ) );
            O += B;
            DockLayoutIO::Write( F.get(), O );
            O += ')';
        }
        return O;
    }

    bool DockSpace::LoadLayout( const std::string& L )
    {
        DockLayoutIO::Parser Ps{ L, 0, *this, {} };
        std::unique_ptr<Node> NewRoot = Ps.Parse();
        if ( !NewRoot ) return false;
        std::vector<std::unique_ptr<Node>> NewFloats;
        while ( Ps.I < L.size() )
        {
            Rect R;
            if ( !Ps.Eat( ";float(" ) || !Ps.Num( R.x ) || !Ps.Eat( "," ) || !Ps.Num( R.y ) || !Ps.Eat( "," ) || !Ps.Num( R.w ) || !Ps.Eat( "," ) ||
                 !Ps.Num( R.h ) || !Ps.Eat( "," ) )
                return false;
            auto F = Ps.Parse();
            if ( !F || F->IsSplit || !Ps.Eat( ")" ) ) return false;
            F->Floating = true;
            F->FloatR = R;
            if ( !F->Tabs.empty() ) NewFloats.push_back( std::move( F ) );
        }
        DockLayoutIO::Clean( NewRoot );
        NewRoot->Parent = nullptr;
        Doing = Act::None;
        ActNode = nullptr;
        HasDrop = false;
        DropLeaf = nullptr;
        Root = std::move( NewRoot );
        Floats = std::move( NewFloats );
        for ( DockPanel* P : Panels() )
        {
            bool In = std::find( Ps.Used.begin(), Ps.Used.end(), P ) != Ps.Used.end();
            P->Open = In;
            P->Visible = false;
        }
        Invalidate();
        return true;
    }
}
