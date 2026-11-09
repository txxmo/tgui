#include "tg/effects.h"
#include "tg/context.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace tg
{
    static float Clamp01( float V ) { return std::clamp( V, 0.f, 1.f ); }
    static Color Mul( Color A, Color B ) { return Color( uint8_t( A.r * B.r / 255 ), uint8_t( A.g * B.g / 255 ), uint8_t( A.b * B.b / 255 ), uint8_t( A.a * B.a / 255 ) ); }

    namespace Effects
    {
        // Params: [0..3] stop colors (0..1), [4] stop positions, [5] = ( kind, angle in radians, cx, cy ), [6] = rect (pixels), [7].x = count
        const ShaderDesc& Gradient()
        {
            static ShaderDesc D{ R"(
vec4 effect(vec4 color, vec2 uv, vec2 pos)
{
    vec4 R = Params[6];
    vec2 P = (pos - R.xy) / max(R.zw, vec2(1.0));
    float t;
    int K = int(Params[5].x + 0.5);
    if (K == 0) { vec2 Dir = vec2(cos(Params[5].y), sin(Params[5].y)); vec2 C = P - 0.5; float Ext = abs(Dir.x) * 0.5 * R.z / max(R.z, R.w) + abs(Dir.y) * 0.5 * R.w / max(R.z, R.w); t = dot(C * R.zw / max(R.z, R.w), Dir) / max(Ext, 0.0001) * 0.5 + 0.5; }
    else if (K == 1) { vec2 C = (P - Params[5].zw) * R.zw / max(R.z, R.w); t = length(C) * 2.0; }
    else { vec2 C = P - Params[5].zw; t = fract((atan(C.y, C.x) - Params[5].y) / 6.2831853 + 1.0); }
    t = clamp(t, 0.0, 1.0);
    int N = int(Params[7].x + 0.5);
    vec4 G = Params[0];
    for (int i = 1; i < 4; i++)
        if (i < N && t >= Params[4][i - 1]) G = mix(Params[i - 1], Params[i], clamp((t - Params[4][i - 1]) / max(Params[4][i] - Params[4][i - 1], 0.0001), 0.0, 1.0));
    return G * vec4(1.0, 1.0, 1.0, color.a * tex(uv).a);
}
)",
                                 R"(
float Atan2( float y, float x ) // portable: some HLSL compilers lack atan2
{
    float ax = abs( x ), ay = abs( y ), a = min( ax, ay ) / max( max( ax, ay ), 1e-6 ), s = a * a;
    float r = ( ( -0.0464964749 * s + 0.15931422 ) * s - 0.327622764 ) * s * a + a;
    if ( ay > ax ) r = 1.57079637 - r;
    if ( x < 0 ) r = 3.14159274 - r;
    if ( y < 0 ) r = -r;
    return r;
}
float4 effect(float4 color, float2 uv, float2 pos)
{
    float4 R = Params[6];
    float2 P = (pos - R.xy) / max(R.zw, float2(1, 1));
    float t;
    int K = (int)(Params[5].x + 0.5);
    if (K == 0) { float2 Dir = float2(cos(Params[5].y), sin(Params[5].y)); float2 C = P - 0.5; float M = max(R.z, R.w); float Ext = abs(Dir.x) * 0.5 * R.z / M + abs(Dir.y) * 0.5 * R.w / M; t = dot(C * R.zw / M, Dir) / max(Ext, 0.0001) * 0.5 + 0.5; }
    else if (K == 1) { float2 C = (P - Params[5].zw) * R.zw / max(R.z, R.w); t = length(C) * 2.0; }
    else { float2 C = P - Params[5].zw; t = frac((Atan2(C.y, C.x) - Params[5].y) / 6.2831853 + 1.0); }
    t = saturate(t);
    int N = (int)(Params[7].x + 0.5);
    float4 G = Params[0];
    [unroll] for (int i = 1; i < 4; i++)
        if (i < N && t >= Params[4][i - 1]) G = lerp(Params[i - 1], Params[i], saturate((t - Params[4][i - 1]) / max(Params[4][i] - Params[4][i - 1], 0.0001)));
    return G * float4(1, 1, 1, color.a * tex(uv).a);
}
)",
                                 []( Color In, Vec2 Uv, Vec2 Pos, const float* P, const std::function<Color( Vec2 )>& Tex ) {
                                     float Rx = P[ 24 ], Ry = P[ 25 ], Rw = std::max( 1.f, P[ 26 ] ), Rh = std::max( 1.f, P[ 27 ] ), M = std::max( Rw, Rh );
                                     float X = ( Pos.x - Rx ) / Rw, Y = ( Pos.y - Ry ) / Rh, T;
                                     int K = int( P[ 20 ] + 0.5f );
                                     if ( K == 0 )
                                     {
                                         float Dx = std::cos( P[ 21 ] ), Dy = std::sin( P[ 21 ] );
                                         float Ext = std::fabs( Dx ) * 0.5f * Rw / M + std::fabs( Dy ) * 0.5f * Rh / M;
                                         T = ( ( X - 0.5f ) * Rw / M * Dx + ( Y - 0.5f ) * Rh / M * Dy ) / std::max( Ext, 0.0001f ) * 0.5f + 0.5f;
                                     }
                                     else if ( K == 1 ) { float Cx = ( X - P[ 22 ] ) * Rw / M, Cy = ( Y - P[ 23 ] ) * Rh / M; T = std::sqrt( Cx * Cx + Cy * Cy ) * 2; }
                                     else { float A = ( std::atan2( Y - P[ 23 ], X - P[ 22 ] ) - P[ 21 ] ) / 6.2831853f + 1; T = A - std::floor( A ); }
                                     T = Clamp01( T );
                                     int N = int( P[ 28 ] + 0.5f );
                                     const float* S = P;
                                     float G[ 4 ] = { S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ] };
                                     for ( int i = 1; i < 4; i++ )
                                         if ( i < N && T >= P[ 16 + i - 1 ] )
                                         {
                                             float K2 = Clamp01( ( T - P[ 16 + i - 1 ] ) / std::max( P[ 16 + i ] - P[ 16 + i - 1 ], 0.0001f ) );
                                             for ( int c = 0; c < 4; c++ ) G[ c ] = S[ ( i - 1 ) * 4 + c ] + ( S[ i * 4 + c ] - S[ ( i - 1 ) * 4 + c ] ) * K2;
                                         }
                                     float A = G[ 3 ] * ( In.a / 255.f ) * ( Tex( Uv ).a / 255.f );
                                     return Color( uint8_t( G[ 0 ] * 255 ), uint8_t( G[ 1 ] * 255 ), uint8_t( G[ 2 ] * 255 ), uint8_t( Clamp01( A ) * 255 ) );
                                 } };
            return D;
        }

        const ShaderDesc& Shimmer()
        {
            static ShaderDesc D{ R"(
vec4 effect(vec4 color, vec2 uv, vec2 pos)
{
    vec4 Base = tex(uv) * color;
    float X = fract(pos.x / Resolution.x * 0.8 - Time * Params[1].x * 0.5);
    float Band = exp(-pow((X - 0.5) * 7.0, 2.0));
    return vec4(mix(Base.rgb, Params[0].rgb, Band * Params[0].a), Base.a);
}
)",
                                 R"(
float4 effect(float4 color, float2 uv, float2 pos)
{
    float4 Base = tex(uv) * color;
    float X = frac(pos.x / Resolution.x * 0.8 - Time * Params[1].x * 0.5);
    float Band = exp(-pow((X - 0.5) * 7.0, 2.0));
    return float4(lerp(Base.rgb, Params[0].rgb, Band * Params[0].a), Base.a);
}
)",
                                 []( Color In, Vec2 Uv, Vec2, const float*, const std::function<Color( Vec2 )>& Tex ) { return Mul( Tex( Uv ), In ); } };
            return D;
        }

        const ShaderDesc& Grayscale()
        {
            static ShaderDesc D{ R"(
vec4 effect(vec4 color, vec2 uv, vec2 pos)
{
    vec4 C = tex(uv) * color;
    float L = dot(C.rgb, vec3(0.299, 0.587, 0.114));
    return vec4(mix(C.rgb, vec3(L), Params[0].x), C.a);
}
)",
                                 R"(
float4 effect(float4 color, float2 uv, float2 pos)
{
    float4 C = tex(uv) * color;
    float L = dot(C.rgb, float3(0.299, 0.587, 0.114));
    return float4(lerp(C.rgb, float3(L, L, L), Params[0].x), C.a);
}
)",
                                 []( Color In, Vec2 Uv, Vec2, const float* P, const std::function<Color( Vec2 )>& Tex ) {
                                     Color C = Mul( Tex( Uv ), In );
                                     float L = C.r * 0.299f + C.g * 0.587f + C.b * 0.114f, K = Clamp01( P[ 0 ] );
                                     auto M = [ & ]( uint8_t V ) { return uint8_t( V + ( L - V ) * K ); };
                                     return Color( M( C.r ), M( C.g ), M( C.b ), C.a );
                                 } };
            return D;
        }
    }

    // the gradient's color at P (UI units), same math as the shader
    static Color EvalGradient( const Gradient& G, Rect R, Vec2 Pt )
    {
        float W = std::max( 1.f, R.w ), H = std::max( 1.f, R.h ), M = std::max( W, H );
        float X = ( Pt.x - R.x ) / W, Y = ( Pt.y - R.y ) / H, T;
        if ( G.K == Gradient::Kind::Linear )
        {
            float A = G.Angle * 0.0174533f, Dx = std::cos( A ), Dy = std::sin( A );
            float Ext = std::fabs( Dx ) * 0.5f * W / M + std::fabs( Dy ) * 0.5f * H / M;
            T = ( ( X - 0.5f ) * W / M * Dx + ( Y - 0.5f ) * H / M * Dy ) / std::max( Ext, 0.0001f ) * 0.5f + 0.5f;
        }
        else if ( G.K == Gradient::Kind::Radial ) { float Cx = ( X - G.Center.x ) * W / M, Cy = ( Y - G.Center.y ) * H / M; T = std::sqrt( Cx * Cx + Cy * Cy ) * 2; }
        else { float A = ( std::atan2( Y - G.Center.y, X - G.Center.x ) - G.Angle * 0.0174533f ) / 6.2831853f + 1; T = A - std::floor( A ); }
        T = Clamp01( T );
        Color C = G.Stops[ 0 ];
        for ( int i = 1; i < G.Count && i < 4; i++ )
            if ( T >= G.At[ i - 1 ] ) C = Color::Lerp( G.Stops[ i - 1 ], G.Stops[ i ], Clamp01( ( T - G.At[ i - 1 ] ) / std::max( G.At[ i ] - G.At[ i - 1 ], 0.0001f ) ) );
        return C;
    }

    void FillGradient( DrawList& D, Context& Ctx, Rect R, const Gradient& G, float Radius )
    {
        ShaderId S = Ctx.Shader( Effects::Gradient() );
        if ( S )
        {
            float P[ 32 ] = {};
            for ( int i = 0; i < 4; i++ )
            {
                Color C = G.Stops[ std::min( i, G.Count - 1 ) ];
                P[ i * 4 ] = C.r / 255.f; P[ i * 4 + 1 ] = C.g / 255.f; P[ i * 4 + 2 ] = C.b / 255.f; P[ i * 4 + 3 ] = C.a / 255.f;
                P[ 16 + i ] = G.At[ std::min( i, G.Count - 1 ) ];
            }
            float Sc = D.Scale;
            P[ 20 ] = float( int( G.K ) );
            P[ 21 ] = G.Angle * 0.0174533f;
            P[ 22 ] = G.Center.x;
            P[ 23 ] = G.Center.y;
            P[ 24 ] = R.x * Sc; P[ 25 ] = R.y * Sc; P[ 26 ] = R.w * Sc; P[ 27 ] = R.h * Sc;
            P[ 28 ] = float( G.Count );
            D.PushShader( S, P, 32 );
            D.RectFilled( R, Color( 255, 255, 255 ), Radius );
            D.PopShader();
            return;
        }
        // no shaders here (Direct3D 9, or a compiler that rejected it): a fine mesh with the gradient's
        // color at every vertex; vertices outside the rounded corners are pulled onto the edge
        const int N = G.K == Gradient::Kind::Linear && G.Count == 2 ? 2 : 28;
        Radius = std::min( Radius, std::min( R.w, R.h ) * 0.5f );
        auto Inside = [ & ]( Vec2 P ) {
            float Cx = std::clamp( P.x, R.x + Radius, R.Right() - Radius ), Cy = std::clamp( P.y, R.y + Radius, R.Bottom() - Radius );
            Vec2 Dv = { P.x - Cx, P.y - Cy };
            float L = std::sqrt( Dv.x * Dv.x + Dv.y * Dv.y );
            return L > Radius && L > 0 ? Vec2{ Cx + Dv.x / L * Radius, Cy + Dv.y / L * Radius } : P;
        };
        std::vector<Vec2> P( size_t( ( N + 1 ) * ( N + 1 ) ) );
        std::vector<Color> Cl( P.size() );
        for ( int y = 0; y <= N; y++ )
            for ( int x = 0; x <= N; x++ )
            {
                Vec2 Q = { R.x + R.w * x / N, R.y + R.h * y / N };
                Q = Inside( Q );
                P[ size_t( y * ( N + 1 ) + x ) ] = Q;
                Cl[ size_t( y * ( N + 1 ) + x ) ] = EvalGradient( G, R, Q );
            }
        std::vector<Vec2> Tp;
        std::vector<Color> Tc;
        for ( int y = 0; y < N; y++ )
            for ( int x = 0; x < N; x++ )
            {
                size_t A = size_t( y * ( N + 1 ) + x ), B = A + 1, C = A + size_t( N + 1 ), Dd = C + 1;
                for ( size_t k : { A, B, Dd, A, Dd, C } ) { Tp.push_back( P[ k ] ); Tc.push_back( Cl[ k ] ); }
            }
        D.Triangles( Tp.data(), Tc.data(), Tp.size() );
    }

    // ------------------------------------------------------------ widgets

    Effect::Effect( const ShaderDesc& S, std::vector<float> P ) : Container( Dir::Column, 0 ), Desc( &S ), Params( std::move( P ) ) {}

    void Effect::Draw( DrawList& D )
    {
        Context* C = GetContext();
        ShaderId S = C ? C->Shader( *Desc ) : 0;
        if ( S ) D.PushShader( S, Params.data(), int( Params.size() ) );
        DrawChildren( D );
        if ( S ) D.PopShader();
        if ( S && Desc == &Effects::Shimmer() && C ) C->RequestFrame(); // animated
    }

    Backdrop::Backdrop() : Container( Dir::Column, 8, Insets( 14 ) ) {}

    void Backdrop::Draw( DrawList& D )
    {
        D.BackdropBlur( Bounds, Blur, Radius );
        D.RectFilled( Bounds, Fill.Resolve(), Radius );
        D.RectStroke( Bounds, Stroke.Resolve(), Radius );
        DrawChildren( D );
    }

    CachedPanel::CachedPanel() : Container( Dir::Column, 8 ) {}

    CachedPanel::~CachedPanel()
    {
        if ( Tex )
            if ( Context* C = GetContext() ) C->GetRenderer().DestroyTexture( Tex );
    }

    void CachedPanel::Draw( DrawList& D )
    {
        Context* C = GetContext();
        if ( !C || Bounds.Empty() ) { DrawChildren( D ); return; }
        Renderer& R = C->GetRenderer();
        float S = D.Scale;
        int W = int( std::ceil( Bounds.w * S ) ), H = int( std::ceil( Bounds.h * S ) );
        if ( !Tex )
        {
            Tex = R.CreateRenderTarget( W, H );
            if ( !Tex ) { DrawChildren( D ); return; } // no render targets here: just draw
            TexW = W;
            TexH = H;
            Dirty = true;
        }
        if ( W != TexW || H != TexH ) { TexW = W; TexH = H; Dirty = true; }
        if ( RefreshEvery > 0 && C->Time() - LastRender >= RefreshEvery ) Dirty = true;
        if ( Dirty )
        {
            // draw the children into a list of their own, moved to the texture's origin
            auto L = std::make_unique<DrawList>();
            L->Reset( { 0, 0, Bounds.w, Bounds.h }, S, D.AtlasTexture, D.WhiteUV );
            L->Time = D.Time;
            L->PushClip( { Bounds.x, Bounds.y, Bounds.w, Bounds.h }, false );
            DrawChildren( *L );
            L->PopClip();
            L->TransformFrom( 0, 0, {}, 1, 0, { -Bounds.x, -Bounds.y } );
            C->RenderOffscreen( Tex, std::move( L ), { float( W ), float( H ) } );
            Dirty = false;
            LastRender = C->Time();
            if ( RefreshEvery > 0 ) C->RequestFrameIn( RefreshEvery );
        }
        D.Image( Tex, Bounds );
    }
}
