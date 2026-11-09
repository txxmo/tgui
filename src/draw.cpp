#include "tg/draw.h"
#include "internal.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    using internal::Pi;

    void DrawList::Reset( Rect Vp, float S, TextureId Atlas, Vec2 White )
    {
        Vtx.clear();
        Idx.clear();
        Cmds.clear();
        ClipStack.clear();
        ShaderStack.clear();
        Params.clear();
        Viewport = Vp;
        Scale = S;
        AtlasTexture = Atlas;
        WhiteUV = White;
    }

    // ------------------------------------------------------------ state

    void DrawList::PushClip( Rect r, bool IntersectWithCurrent )
    {
        if ( IntersectWithCurrent ) r = r.Intersect( CurrentClip() );
        ClipStack.push_back( r );
    }

    void DrawList::PopClip()
    {
        if ( !ClipStack.empty() ) ClipStack.pop_back();
    }

    bool DrawList::Culled( const Rect& r ) const
    {
        Rect c = CurrentClip();
        return r.x > c.Right() || r.y > c.Bottom() || r.Right() < c.x || r.Bottom() < c.y || c.Empty();
    }

    void DrawList::SetTexture( TextureId Tex )
    {
        Rect Clip = CurrentClip();
        ShaderId Sh = ShaderStack.empty() ? 0 : ShaderStack.back().S;
        uint32_t Po = ShaderStack.empty() ? 0 : ShaderStack.back().Off;
        if ( !Cmds.empty() )
        {
            DrawCmd& Last = Cmds.back();
            bool SameClip = Last.Clip.x == Clip.x && Last.Clip.y == Clip.y && Last.Clip.w == Clip.w && Last.Clip.h == Clip.h;
            bool SameShader = Last.Shader == Sh && Last.ParamOffset == Po && Last.Kind == DrawCmd::Draw;
            if ( Last.Texture == Tex && SameClip && SameShader ) return;
            if ( Last.IdxCount == 0 && Last.Kind == DrawCmd::Draw ) { Last.Texture = Tex; Last.Clip = Clip; Last.Shader = Sh; Last.ParamOffset = Po; return; }
        }
        DrawCmd C{};
        C.Clip = Clip;
        C.Texture = Tex;
        C.IdxOffset = uint32_t( Idx.size() );
        C.Shader = Sh;
        C.ParamOffset = Po;
        Cmds.push_back( C );
    }

    void DrawList::PushShader( ShaderId S, const float* P, int N )
    {
        uint32_t Off = uint32_t( Params.size() );
        Params.resize( Params.size() + 32, 0.f );
        for ( int i = 0; i < N && i < 32; i++ ) Params[ Off + size_t( i ) ] = P[ i ];
        ShaderStack.push_back( { S, Off } );
    }

    void DrawList::PopShader()
    {
        if ( !ShaderStack.empty() ) ShaderStack.pop_back();
    }

    void DrawList::BackdropBlur( Rect R, float Amount, float Radius )
    {
        if ( R.Empty() || Culled( R ) ) return;
        DrawCmd C{};
        C.Clip = CurrentClip();
        C.IdxOffset = uint32_t( Idx.size() );
        C.Kind = DrawCmd::Backdrop;
        C.Area = R;
        C.Amount = Amount;
        C.Radius = Radius;
        Cmds.push_back( C );
    }

    void DrawList::Triangles( const Vec2* P, const Color* C, size_t N )
    {
        if ( N < 3 ) return;
        SetTexture( AtlasTexture );
        uint32_t Base = uint32_t( Vtx.size() );
        for ( size_t i = 0; i < N; i++ ) Vtx.push_back( { P[ i ], WhiteUV, C[ i ].Pack() } );
        for ( size_t i = 0; i + 2 < N; i += 3 ) Tri( Base + uint32_t( i ), Base + uint32_t( i + 1 ), Base + uint32_t( i + 2 ) );
    }

    void DrawList::ShadeLinear( size_t V0, Vec2 From, Vec2 To, Color A, Color B )
    {
        Vec2 D = To - From;
        float L2 = D.x * D.x + D.y * D.y;
        for ( size_t i = V0; i < Vtx.size(); i++ )
        {
            Vec2 P = Vtx[ i ].Pos - From;
            float T = L2 > 0 ? std::clamp( ( P.x * D.x + P.y * D.y ) / L2, 0.f, 1.f ) : 0.f;
            Color C = Color::Lerp( A, B, T );
            C.a = uint8_t( C.a * ( Color::Unpack( Vtx[ i ].Col ).a / 255.f ) );
            Vtx[ i ].Col = C.Pack();
        }
    }

    void DrawList::ShadeRadial( size_t V0, Vec2 Ctr, float R, Color In, Color Out )
    {
        for ( size_t i = V0; i < Vtx.size(); i++ )
        {
            Vec2 P = Vtx[ i ].Pos - Ctr;
            float T = R > 0 ? std::clamp( std::sqrt( P.x * P.x + P.y * P.y ) / R, 0.f, 1.f ) : 0.f;
            Color C = Color::Lerp( In, Out, T );
            C.a = uint8_t( C.a * ( Color::Unpack( Vtx[ i ].Col ).a / 255.f ) );
            Vtx[ i ].Col = C.Pack();
        }
    }

    void DrawList::Tri( uint32_t a, uint32_t b, uint32_t c )
    {
        Idx.push_back( a );
        Idx.push_back( b );
        Idx.push_back( c );
        Cmds.back().IdxCount += 3;
    }

    void DrawList::PrimQuadSkew( Vec2 a, Vec2 c, Vec2 uva, Vec2 uvc, uint32_t col, float DxTop, float DxBottom )
    {
        uint32_t Base = uint32_t( Vtx.size() );
        Vtx.push_back( { { a.x + DxTop, a.y }, uva, col } );
        Vtx.push_back( { { c.x + DxTop, a.y }, { uvc.x, uva.y }, col } );
        Vtx.push_back( { { c.x + DxBottom, c.y }, uvc, col } );
        Vtx.push_back( { { a.x + DxBottom, c.y }, { uva.x, uvc.y }, col } );
        Tri( Base, Base + 1, Base + 2 );
        Tri( Base, Base + 2, Base + 3 );
    }

    void DrawList::PrimQuadUV( Vec2 a, Vec2 c, Vec2 uva, Vec2 uvc, uint32_t col )
    {
        uint32_t Base = uint32_t( Vtx.size() );
        Vtx.push_back( { a, uva, col } );
        Vtx.push_back( { { c.x, a.y }, { uvc.x, uva.y }, col } );
        Vtx.push_back( { c, uvc, col } );
        Vtx.push_back( { { a.x, c.y }, { uva.x, uvc.y }, col } );
        Tri( Base, Base + 1, Base + 2 );
        Tri( Base, Base + 2, Base + 3 );
    }

    // ------------------------------------------------------------ paths

    void DrawList::PathArc( Vec2 C, float R, float A0, float A1, int Segments )
    {
        if ( Segments <= 0 )
            Segments = std::clamp( int( std::ceil( R * std::fabs( A1 - A0 ) * 0.5f ) ), 2, 64 );
        for ( int i = 0; i <= Segments; i++ )
        {
            float A = A0 + ( A1 - A0 ) * float( i ) / float( Segments );
            Path.push_back( { C.x + std::cos( A ) * R, C.y + std::sin( A ) * R } );
        }
    }

    void DrawList::PathRoundedRect( Rect r, float Rad, uint8_t Corners )
    {
        Path.clear();
        Rad = std::min( Rad, std::min( r.w, r.h ) * 0.5f );
        if ( Rad < 0.5f || Corners == 0 )
        {
            Path.push_back( { r.x, r.y } );
            Path.push_back( { r.Right(), r.y } );
            Path.push_back( { r.Right(), r.Bottom() } );
            Path.push_back( { r.x, r.Bottom() } );
            return;
        }
        // Clockwise on screen (y down): TL -> TR -> BR -> BL.
        if ( Corners & CornerTL ) PathArc( { r.x + Rad, r.y + Rad }, Rad, Pi, Pi * 1.5f );
        else Path.push_back( { r.x, r.y } );
        if ( Corners & CornerTR ) PathArc( { r.Right() - Rad, r.y + Rad }, Rad, Pi * 1.5f, Pi * 2.f );
        else Path.push_back( { r.Right(), r.y } );
        if ( Corners & CornerBR ) PathArc( { r.Right() - Rad, r.Bottom() - Rad }, Rad, 0, Pi * 0.5f );
        else Path.push_back( { r.Right(), r.Bottom() } );
        if ( Corners & CornerBL ) PathArc( { r.x + Rad, r.Bottom() - Rad }, Rad, Pi * 0.5f, Pi );
        else Path.push_back( { r.x, r.Bottom() } );
    }

    // Drop consecutive duplicate points (and a closing duplicate): zero-length segments have no normal
    // and would break the miter/AA math. Returns the cleaned count; writes into Out.
    static int Dedupe( const Vec2* P, int N, bool Closed, std::vector<Vec2>& Out )
    {
        Out.clear();
        for ( int i = 0; i < N; i++ )
        {
            if ( !Out.empty() )
            {
                Vec2 D = P[ i ] - Out.back();
                if ( D.x * D.x + D.y * D.y < 1e-6f ) continue;
            }
            Out.push_back( P[ i ] );
        }
        if ( Closed && Out.size() > 1 )
        {
            Vec2 D = Out.front() - Out.back();
            if ( D.x * D.x + D.y * D.y < 1e-6f ) Out.pop_back();
        }
        return int( Out.size() );
    }

    // ------------------------------------------------------------ fills

    void DrawList::ConvexFillImpl( const Vec2* Pin, int Nin, Color c, TextureId Tex, const Rect* UVRect, Vec2 UV0, Vec2 UV1 )
    {
        if ( Nin < 3 || c.a == 0 ) return;
        thread_local std::vector<Vec2> Clean;
        int N = Dedupe( Pin, Nin, true, Clean );
        const Vec2* P = Clean.data();
        if ( N < 3 ) return;
        SetTexture( Tex ? Tex : AtlasTexture );

        const uint32_t Col = c.Pack(), Clear = c.WithAlpha( 0 ).Pack();
        const uint32_t Base = uint32_t( Vtx.size() );
        const float Feather = 1.0f / Scale; // 1 device pixel of anti-aliasing

        Normals.resize( N );
        for ( int i = 0; i < N; i++ )
        {
            Vec2 D = P[ ( i + 1 ) % N ] - P[ i ];
            float L = std::sqrt( D.x * D.x + D.y * D.y );
            if ( L > 0 ) D = D * ( 1.f / L );
            Normals[ i ] = { D.y, -D.x }; // outward for clockwise-on-screen polygons
        }

        auto UVOf = [ & ]( Vec2 Pt ) -> Vec2
        {
            if ( !UVRect ) return WhiteUV;
            float u = ( Pt.x - UVRect->x ) / UVRect->w, v = ( Pt.y - UVRect->y ) / UVRect->h;
            return { Lerp( UV0.x, UV1.x, u ), Lerp( UV0.y, UV1.y, v ) };
        };

        for ( int i = 0; i < N; i++ )
        {
            Vec2 N0 = Normals[ ( i + N - 1 ) % N ], N1 = Normals[ i ];
            Vec2 Dm = ( N0 + N1 ) * 0.5f;
            float D2 = Dm.x * Dm.x + Dm.y * Dm.y;
            if ( D2 > 1e-6f ) Dm = Dm * std::min( 1.f / D2, 100.f );
            Dm = Dm * ( Feather * 0.5f );
            Vec2 In = P[ i ] - Dm, Out = P[ i ] + Dm;
            Vtx.push_back( { In, UVOf( In ), Col } );
            Vtx.push_back( { Out, UVOf( Out ), Clear } );
        }
        for ( int i = 2; i < N; i++ )
            Tri( Base, Base + uint32_t( i - 1 ) * 2, Base + uint32_t( i ) * 2 );
        for ( int i = 0; i < N; i++ )
        {
            uint32_t A = Base + i * 2, B = Base + ( ( i + 1 ) % N ) * 2;
            Tri( A, B, B + 1 );
            Tri( A, B + 1, A + 1 );
        }
    }

    void DrawList::ConvexFill( const Vec2* P, int N, Color c )
    {
        ConvexFillImpl( P, N, c, 0, nullptr, {}, {} );
    }

    void DrawList::AreaFill( const Vec2* P, int N, float BaseY, Color Top, Color Bottom )
    {
        if ( N < 2 ) return;
        SetTexture( AtlasTexture );
        size_t Start = Vtx.size();
        uint32_t Base = uint32_t( Start );
        for ( int i = 0; i < N; i++ )
        {
            Vtx.push_back( { P[ i ], WhiteUV, 0xFFFFFFFFu } );
            Vtx.push_back( { { P[ i ].x, BaseY }, WhiteUV, 0xFFFFFFFFu } );
        }
        for ( int i = 0; i + 1 < N; i++ )
        {
            uint32_t A = Base + i * 2, B = A + 2;
            Tri( A, B, B + 1 );
            Tri( A, B + 1, A + 1 );
        }
        float MinY = BaseY;
        for ( int i = 0; i < N; i++ ) MinY = std::min( MinY, P[ i ].y );
        ShadeVertical( Start, MinY, BaseY, Top, Bottom );
    }

    void DrawList::RectFilled( Rect r, Color c, float Radius, uint8_t Corners )
    {
        if ( c.a == 0 || r.Empty() || Culled( r ) ) return;
        if ( Radius < 0.5f )
        {
            SetTexture( AtlasTexture );
            PrimQuadUV( r.Pos(), r.Max(), WhiteUV, WhiteUV, c.Pack() );
            return;
        }
        PathRoundedRect( r, Radius, Corners );
        ConvexFill( Path.data(), int( Path.size() ), c );
    }

    void DrawList::ShadeVertical( size_t Start, float Y0, float Y1, Color Top, Color Bottom )
    {
        float H = Y1 - Y0;
        for ( size_t i = Start; i < Vtx.size(); i++ )
        {
            float t = H > 0 ? Clamp( ( Vtx[ i ].Pos.y - Y0 ) / H, 0, 1 ) : 0;
            Color Old = Color::Unpack( Vtx[ i ].Col );
            Color New = Color::Lerp( Top, Bottom, t );
            New.a = uint8_t( New.a * ( Old.a / 255.f ) );
            Vtx[ i ].Col = New.Pack();
        }
    }

    void DrawList::TransformFrom( size_t V0, size_t C0, Vec2 Ctr, float S, float A, Vec2 Off )
    {
        float Cs = std::cos( A ) * S, Sn = std::sin( A ) * S;
        for ( size_t i = V0; i < Vtx.size(); i++ )
        {
            Vec2 P = Vtx[ i ].Pos - Ctr;
            Vtx[ i ].Pos = { Ctr.x + Off.x + P.x * Cs - P.y * Sn, Ctr.y + Off.y + P.x * Sn + P.y * Cs };
        }
        for ( size_t i = C0; i < Cmds.size(); i++ )
        {
            Rect& R = Cmds[ i ].Clip;
            R = { Ctr.x + Off.x + ( R.x - Ctr.x ) * S, Ctr.y + Off.y + ( R.y - Ctr.y ) * S, R.w * S, R.h * S };
        }
    }

    void DrawList::ConvexFillInRounded( const Vec2* Pts, int Count, Color c, Rect R, float Radius )
    {
        if ( Count < 3 || R.Empty() ) return;
        PathRoundedRect( R, std::min( Radius, std::min( R.w, R.h ) * 0.5f ) );
        std::vector<Vec2> ClipPoly = Path, Out( Pts, Pts + Count ), In;
        // orientation of the clip polygon decides which side is "inside"
        float Area = 0;
        for ( size_t i = 0; i < ClipPoly.size(); i++ )
        {
            const Vec2 &P = ClipPoly[ i ], &Q = ClipPoly[ ( i + 1 ) % ClipPoly.size() ];
            Area += P.x * Q.y - Q.x * P.y;
        }
        float Sign = Area >= 0 ? 1.f : -1.f;
        // Sutherland–Hodgman: clip against each edge of the (convex) rounded rect
        for ( size_t e = 0; e < ClipPoly.size() && !Out.empty(); e++ )
        {
            Vec2 A = ClipPoly[ e ], B = ClipPoly[ ( e + 1 ) % ClipPoly.size() ];
            auto Side = [ & ]( Vec2 P ) { return Sign * ( ( B.x - A.x ) * ( P.y - A.y ) - ( B.y - A.y ) * ( P.x - A.x ) ); };
            In.swap( Out );
            Out.clear();
            for ( size_t i = 0; i < In.size(); i++ )
            {
                Vec2 P = In[ i ], Q = In[ ( i + 1 ) % In.size() ];
                float Sp = Side( P ), Sq = Side( Q );
                if ( Sp >= 0 ) Out.push_back( P );
                if ( ( Sp >= 0 ) != ( Sq >= 0 ) ) Out.push_back( P + ( Q - P ) * ( Sp / ( Sp - Sq ) ) );
            }
        }
        if ( Out.size() >= 3 ) ConvexFill( Out.data(), int( Out.size() ), c );
    }

    void DrawList::FadeFrom( size_t Start, float Alpha )
    {
        if ( Alpha >= 1 ) return;
        for ( size_t i = Start; i < Vtx.size(); i++ )
        {
            Color C = Color::Unpack( Vtx[ i ].Col );
            C.a = uint8_t( C.a * Clamp( Alpha, 0, 1 ) );
            Vtx[ i ].Col = C.Pack();
        }
    }

    void DrawList::RectGradient( Rect r, Color Top, Color Bottom, float Radius, uint8_t Corners )
    {
        if ( r.Empty() || Culled( r ) ) return;
        size_t Start = Vtx.size();
        RectFilled( r, Color( 255, 255, 255, 255 ), Radius, Corners );
        ShadeVertical( Start, r.y, r.Bottom(), Top, Bottom );
    }

    void DrawList::RectMultiColor( Rect r, Color TL, Color TR, Color BR, Color BL )
    {
        if ( r.Empty() || Culled( r ) ) return;
        SetTexture( AtlasTexture );
        uint32_t Base = uint32_t( Vtx.size() );
        Vtx.push_back( { r.Pos(), WhiteUV, TL.Pack() } );
        Vtx.push_back( { { r.Right(), r.y }, WhiteUV, TR.Pack() } );
        Vtx.push_back( { r.Max(), WhiteUV, BR.Pack() } );
        Vtx.push_back( { { r.x, r.Bottom() }, WhiteUV, BL.Pack() } );
        Tri( Base, Base + 1, Base + 2 );
        Tri( Base, Base + 2, Base + 3 );
    }

    void DrawList::CircleFilled( Vec2 C, float R, Color c )
    {
        if ( c.a == 0 || Culled( { C.x - R, C.y - R, R * 2, R * 2 } ) ) return;
        Path.clear();
        int Seg = std::clamp( int( R * 1.5f ) + 8, 12, 96 );
        PathArc( C, R, 0, Pi * 2.f * ( 1.f - 1.f / Seg ), Seg - 1 );
        ConvexFill( Path.data(), int( Path.size() ), c );
    }

    // ------------------------------------------------------------ strokes

    void DrawList::Polyline( const Vec2* Pin, int Nin, Color c, float Thickness, bool Closed )
    {
        if ( Nin < 2 || c.a == 0 ) return;
        thread_local std::vector<Vec2> CleanPts;
        std::vector<Vec2> Pts; // own copy: callers may pass our Path, and round joins reuse scratch buffers
        Dedupe( Pin, Nin, Closed, CleanPts );
        Pts = CleanPts;
        const Vec2* P = Pts.data();
        int N = int( Pts.size() );
        if ( N < 2 ) return;
        SetTexture( AtlasTexture );

        const float Feather = 1.0f / Scale;
        if ( Thickness < Feather ) { c = c.Fade( Thickness / Feather ); Thickness = Feather; }
        const float Core = ( Thickness - Feather ) * 0.5f;
        const uint32_t Col = c.Pack(), Clear = c.WithAlpha( 0 ).Pack();
        const int Segs = Closed ? N : N - 1;

        Normals.resize( N );
        for ( int i = 0; i < Segs; i++ )
        {
            Vec2 D = P[ ( i + 1 ) % N ] - P[ i ];
            float L = std::sqrt( D.x * D.x + D.y * D.y );
            if ( L > 0 ) D = D * ( 1.f / L );
            Normals[ i ] = { D.y, -D.x };
        }
        if ( !Closed ) Normals[ N - 1 ] = Normals[ N - 2 ];

        // Each point emits a 4-vertex cross-section: fringe, core, core, fringe.
        // Mild joins share one mitered cross-section. Sharp joins (spiky chart data) would make the
        // miter run along the line and collapse it, so they get two cross-sections + a round cap.
        auto Section = [ & ]( Vec2 Pt, Vec2 Dm ) {
            uint32_t First = uint32_t( Vtx.size() );
            Vec2 A = Dm * ( Core + Feather ), B = Dm * Core;
            Vtx.push_back( { Pt + A, WhiteUV, Clear } );
            Vtx.push_back( { Pt + B, WhiteUV, Col } );
            Vtx.push_back( { Pt - B, WhiteUV, Col } );
            Vtx.push_back( { Pt - A, WhiteUV, Clear } );
            return First;
        };

        thread_local std::vector<uint32_t> InIdx, OutIdx;
        thread_local std::vector<Vec2> RoundJoins;
        InIdx.resize( N );
        OutIdx.resize( N );
        RoundJoins.clear();

        for ( int i = 0; i < N; i++ )
        {
            if ( !Closed && ( i == 0 || i == N - 1 ) )
            {
                InIdx[ i ] = OutIdx[ i ] = Section( P[ i ], Normals[ i == 0 ? 0 : N - 2 ] );
                continue;
            }
            Vec2 N0 = Normals[ ( i + N - 1 ) % N ], N1 = Normals[ i ];
            Vec2 Sum = N0 + N1;
            float L = std::sqrt( Sum.x * Sum.x + Sum.y * Sum.y );
            float CosHalf = L * 0.5f; // cos of half the angle between the two normals
            if ( CosHalf > 0.5f )
            {
                Vec2 Dm = Sum * ( 1.f / L ) * ( 1.f / CosHalf ); // miter
                InIdx[ i ] = OutIdx[ i ] = Section( P[ i ], Dm );
            }
            else
            {
                InIdx[ i ] = Section( P[ i ], N0 );
                OutIdx[ i ] = Section( P[ i ], N1 );
                if ( Thickness > Feather * 1.5f ) RoundJoins.push_back( P[ i ] );
            }
        }

        for ( int i = 0; i < Segs; i++ )
        {
            uint32_t I0 = OutIdx[ i ], I1 = InIdx[ ( i + 1 ) % N ];
            for ( uint32_t k = 0; k < 3; k++ )
            {
                Tri( I0 + k, I0 + k + 1, I1 + k + 1 );
                Tri( I0 + k, I1 + k + 1, I1 + k );
            }
        }

        std::vector<Vec2> Joins = RoundJoins; // CircleFilled reuses scratch buffers
        for ( Vec2 J : Joins ) CircleFilled( J, Thickness * 0.5f, c );
    }

    void DrawList::Line( Vec2 a, Vec2 b, Color c, float Thickness )
    {
        Vec2 P[ 2 ] = { a, b };
        Polyline( P, 2, c, Thickness, false );
    }

    void DrawList::RectStroke( Rect r, Color c, float Radius, float Thickness, uint8_t Corners )
    {
        if ( c.a == 0 || r.Empty() || Culled( r ) ) return;
        // Keep the stroke inside r so a 1px border lands exactly on pixel centers.
        float H = Thickness * 0.5f;
        PathRoundedRect( r.Shrink( H ), std::max( 0.f, Radius - H ), Corners );
        std::vector<Vec2> Pts = Path; // Polyline uses Normals, not Path, but keep Path reusable by callers
        Polyline( Pts.data(), int( Pts.size() ), c, Thickness, true );
    }

    void DrawList::CircleStroke( Vec2 C, float R, Color c, float Thickness )
    {
        Path.clear();
        int Seg = std::clamp( int( R * 1.5f ) + 8, 12, 96 );
        PathArc( C, R - Thickness * 0.5f, 0, Pi * 2.f * ( 1.f - 1.f / Seg ), Seg - 1 );
        std::vector<Vec2> Pts = Path;
        Polyline( Pts.data(), int( Pts.size() ), c, Thickness, true );
    }

    void DrawList::Shadow( Rect r, float Radius, Color c, float Spread, Vec2 Offset )
    {
        const int Steps = 5;
        r = r.Offset( Offset );
        for ( int i = Steps; i >= 1; i-- )
        {
            float E = Spread * float( i ) / Steps;
            Rect Er = { r.x - E, r.y - E, r.w + E * 2, r.h + E * 2 };
            RectFilled( Er, c.Fade( 1.f / Steps ), Radius + E );
        }
    }

    // ------------------------------------------------------------ images & text

    void DrawList::Image( TextureId Tex, Rect r, Vec2 UV0, Vec2 UV1, Color Tint, float Radius )
    {
        if ( !Tex || r.Empty() || Culled( r ) ) return;
        if ( Radius >= 0.5f )
        {
            PathRoundedRect( r, Radius );
            ConvexFillImpl( Path.data(), int( Path.size() ), Tint, Tex, &r, UV0, UV1 );
            return;
        }
        SetTexture( Tex );
        PrimQuadUV( r.Pos(), r.Max(), UV0, UV1, Tint.Pack() );
    }

    void DrawList::Glyph( const Font& F, uint32_t Cp, Vec2 Pos, Color c ) { Glyph( F, F.Find( Cp ), Pos, c ); }

    void DrawList::Glyph( const Font& F, const tg::Glyph& G, Vec2 Pos, Color c )
    {
        if ( c.a == 0 || G.X1 <= G.X0 ) return;
        SetTexture( AtlasTexture );
        float Steps = Scale * F.SubpixelSteps;
        float X0 = std::round( ( Pos.x + G.X0 ) * Steps ) / Steps;
        float Y0 = std::round( ( Pos.y + F.Ascent ) * Scale ) / Scale + G.Y0;
        uint32_t C = G.Colored ? Color( 255, 255, 255, c.a ).Pack() : c.Pack();
        if ( F.Skew != 0 && !G.Colored )
        {
            float B = Y0 - G.Y0;
            PrimQuadSkew( { X0, Y0 }, { X0 + ( G.X1 - G.X0 ), Y0 + ( G.Y1 - G.Y0 ) }, { G.U0, G.V0 }, { G.U1, G.V1 }, C, F.Skew * ( B - Y0 ), F.Skew * ( B - ( Y0 + G.Y1 - G.Y0 ) ) );
        }
        else PrimQuadUV( { X0, Y0 }, { X0 + ( G.X1 - G.X0 ), Y0 + ( G.Y1 - G.Y0 ) }, { G.U0, G.V0 }, { G.U1, G.V1 }, C );
    }

    Vec2 DrawList::Text( const Font& F, Vec2 Pos, std::string_view Str, Color c, float WrapWidth )
    {
        Vec2 Covered;
        if ( c.a == 0 ) return F.Measure( Str, WrapWidth );
        SetTexture( AtlasTexture );
        const uint32_t Col = c.Pack();
        const Rect Clip = CurrentClip();
        auto Snap = [ this ]( float v ) { return std::round( v * Scale ) / Scale; };

        F.ForEachLine( Str, WrapWidth, [ & ]( std::string_view Line, float Y )
        {
            float Top = Pos.y + Y;
            Covered.y = Y + F.LineHeight;
            bool Visible = Top <= Clip.Bottom() && Top + F.LineHeight >= Clip.y;
            float X = Pos.x;
            float Baseline = Snap( Top + F.Ascent );
            // Glyphs land on 1/SubpixelSteps of a device pixel (the atlas is oversampled to match),
            // so spacing stays even instead of every glyph jumping to a whole pixel.
            const float Steps = Scale * F.SubpixelSteps;
            uint32_t Prev = 0;
            for ( size_t i = 0; i < Line.size(); )
            {
                uint32_t Cp;
                const tg::Glyph& G = F.Next( Line, i, Cp ); // an emoji sequence (flag, ZWJ...) comes back as one glyph
                if ( Cp ) X += F.Kern( Prev, Cp );
                Prev = Cp;
                if ( Visible && G.X1 > G.X0 && X <= Clip.Right() )
                {
                    float X0 = std::round( ( X + G.X0 ) * Steps ) / Steps, Y0 = Baseline + G.Y0;
                    // color glyphs (emoji) keep their own colors; the text color only contributes its alpha
                    uint32_t C = G.Colored ? Color( 255, 255, 255, c.a ).Pack() : Col;
                    if ( F.Skew != 0 && !G.Colored ) // synthetic italic: slant around the baseline
                        PrimQuadSkew( { X0, Y0 }, { X0 + ( G.X1 - G.X0 ), Y0 + ( G.Y1 - G.Y0 ) }, { G.U0, G.V0 }, { G.U1, G.V1 }, C,
                                      F.Skew * ( Baseline - Y0 ), F.Skew * ( Baseline - ( Y0 + G.Y1 - G.Y0 ) ) );
                    else PrimQuadUV( { X0, Y0 }, { X0 + ( G.X1 - G.X0 ), Y0 + ( G.Y1 - G.Y0 ) }, { G.U0, G.V0 }, { G.U1, G.V1 }, C );
                }
                X += G.Advance;
            }
            Covered.x = std::max( Covered.x, X - Pos.x );
        } );
        return Covered;
    }
}
