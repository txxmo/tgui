// Effects: shaders that come with tg, gradients on anything, frosted glass, render-to-texture.
//
//   tg::FillGradient( D, Ctx, R, tg::Gradient::Linear( 90, Accent, Purple ), 8 );   // in a Draw()
//   auto& Glass = W.Add<tg::Backdrop>();      // frosted panel: blurs what's behind it
//   auto& Fx = W.Add<tg::Effect>( tg::Effects::Grayscale(), { 1 } );  // children drawn through a shader
//   auto& Cache = W.Add<tg::CachedPanel>();   // children rendered once to a texture, redrawn on MarkDirty()
//
// Your own shader: a ShaderDesc with GLSL (GL3), HLSL (Direct3D 11) and/or C++ (software renderer):
//   static tg::ShaderDesc Tint{ "vec4 effect(vec4 c, vec2 uv, vec2 p) { return tex(uv) * c * Params[0]; }",
//                               "float4 effect(float4 c, float2 uv, float2 p) { return tex(uv) * c * Params[0]; }" };
//   D.PushShader( Ctx.Shader( Tint ), Rgba, 4 ); ... D.PopShader();
#pragma once
#include "draw.h"
#include "renderer.h"
#include "widgets/container.h"
#include <memory>

namespace tg
{
    class Context;

    struct Gradient
    {
        enum class Kind : uint8_t { Linear, Radial, Conic } K = Kind::Linear;
        float Angle = 90;          // Linear: degrees, 0 = left -> right, 90 = top -> bottom. Conic: start angle
        Vec2 Center = { 0.5f, 0.5f }; // Radial / Conic: in the rect, 0..1
        int Count = 2;             // 2..4 stops
        Color Stops[ 4 ];
        float At[ 4 ] = { 0, 1, 1, 1 };

        static Gradient Linear( float Angle, Color A, Color B ) { Gradient G; G.Angle = Angle; G.Stops[ 0 ] = A; G.Stops[ 1 ] = B; return G; }
        static Gradient Radial( Color Inner, Color Outer ) { Gradient G; G.K = Kind::Radial; G.Stops[ 0 ] = Inner; G.Stops[ 1 ] = Outer; return G; }
        static Gradient Conic( Color A, Color B, Color C )
        {
            Gradient G;
            G.K = Kind::Conic;
            G.Count = 4;
            G.Stops[ 0 ] = A; G.Stops[ 1 ] = B; G.Stops[ 2 ] = C; G.Stops[ 3 ] = A;
            G.At[ 0 ] = 0; G.At[ 1 ] = 1 / 3.f; G.At[ 2 ] = 2 / 3.f; G.At[ 3 ] = 1;
            return G;
        }
        Gradient& Stop( float Pos, Color C ) { if ( Count < 4 ) { At[ Count ] = Pos; Stops[ Count ] = C; Count++; } return *this; }
    };

    namespace Effects
    {
        const ShaderDesc& Gradient();  // used by FillGradient
        const ShaderDesc& Shimmer();   // a moving highlight (loading skeletons): Params[0] = color, Params[1].x = speed
        const ShaderDesc& Grayscale(); // Params[0].x = amount 0..1 (disabled look)
    }

    // A gradient-filled (rounded) rect. Exact with shader support; vertex-colored fallback otherwise.
    void FillGradient( DrawList& D, Context& Ctx, Rect R, const Gradient& G, float Radius = 0 );

    // Children drawn through a shader. Params: up to 32 floats (Params[0..7] in the shader).
    class Effect : public Container
    {
    public:
        Effect( const ShaderDesc& Shader, std::vector<float> Params = {} );
        const ShaderDesc* Desc;
        std::vector<float> Params;
        void Draw( DrawList& D ) override;
    };

    // Frosted glass: blurs whatever is behind it, then a translucent fill. Put content inside.
    class Backdrop : public Container
    {
    public:
        Backdrop();
        float Blur = 14, Radius = 12;
        Paint Fill = Color( 22, 25, 32, 150 );
        Paint Stroke = Color( 255, 255, 255, 28 );
        void Draw( DrawList& D ) override;
    };

    // Children rendered into a texture, reused until MarkDirty() (or every RefreshEvery seconds).
    // For expensive, mostly static content (big charts, long rich text). Falls back to normal drawing.
    class CachedPanel : public Container
    {
    public:
        CachedPanel();
        ~CachedPanel() override;
        void MarkDirty() { Dirty = true; Redraw(); }
        double RefreshEvery = 0;
        bool IsCached() const { return Tex != 0 && !Dirty; }
        void Draw( DrawList& D ) override;

    private:
        TextureId Tex = 0;
        int TexW = 0, TexH = 0;
        bool Dirty = true;
        double LastRender = -1;
    };
}
