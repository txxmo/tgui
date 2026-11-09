// Shared bits of the tg::App backends (GLFW + GL3, Win32 + D3D11).
#pragma once
#include "tg/app.h"
#include <algorithm>
#include <cmath>

namespace tg::detail
{
    // Root = Stack[ main area, floating window layer ]
    inline void BuildRoot( Context& C, Container*& MainArea, Widget*& WindowLayer )
    {
        Widget& Root = C.Root();
        Root.Layout.Direction = Dir::Stack;
        MainArea = &Root.Add<Container>( Dir::Column, 0 );
        WindowLayer = &Root.Add<Widget>();
        WindowLayer->Layout.Direction = Dir::Free;
    }

    // Move W into Into; returns what's needed to put it back.
    struct PoppedOut
    {
        Widget* W = nullptr;
        Widget* Parent = nullptr;
        size_t Index = 0;
        LayoutSpec Was;
        bool WasVisible = true;
    };

    inline PoppedOut PopOutInto( Widget& W, Container& Into )
    {
        PoppedOut P;
        P.W = &W;
        P.Parent = W.GetParent();
        if ( !P.Parent ) return P;
        auto& K = P.Parent->Children();
        P.Index = size_t( std::find_if( K.begin(), K.end(), [ & ]( auto& C ) { return C.get() == &W; } ) - K.begin() );
        P.Was = W.Layout;
        P.WasVisible = W.Visible;
        auto Owned = P.Parent->Detach( &W );
        W.Layout.Width = Grow();
        W.Layout.Height = Grow();
        W.Layout.Offset = {};
        W.Visible = true;
        Into.AddChild( std::move( Owned ) );
        return P;
    }

    inline void PutBack( PoppedOut& P, Container& From )
    {
        if ( !P.W || !P.Parent ) return;
        auto Owned = From.Detach( P.W );
        if ( !Owned ) return;
        P.W->Layout = P.Was;
        P.W->Visible = P.WasVisible;
        P.Parent->AddChild( std::move( Owned ) );
        size_t Last = P.Parent->Children().size() - 1;
        if ( P.Index < Last ) P.Parent->MoveChild( Last, P.Index );
        P.W = nullptr;
    }

    inline Vec2 PopOutSize( Widget& W, int Width, int Height )
    {
        float Wd = Width > 0 ? float( Width ) : std::max( 320.f, std::ceil( W.Bounds.w ) );
        float Ht = Height > 0 ? float( Height ) : std::max( 240.f, std::ceil( W.Bounds.h ) );
        return { Wd, Ht };
    }
}
