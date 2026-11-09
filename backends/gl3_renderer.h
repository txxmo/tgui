// OpenGL 3.3 core renderer. No GL loader library needed: pass a GetProcAddress function
// (glfwGetProcAddress, SDL_GL_GetProcAddress, wglGetProcAddress wrapper, ...).
// Supports custom shaders (ShaderDesc::Glsl), render targets, backdrop blur and frame capture.
#pragma once
#include "tg/renderer.h"
#include <unordered_map>

namespace tg
{
    class GL3Renderer : public Renderer
    {
    public:
        using GetProcFn = void* ( * )( const char* );

        // Call with a current GL 3.3+ context.
        bool Init( GetProcFn GetProc );
        void Shutdown();

        TextureId CreateTexture( int W, int H, const void* Pixels ) override;
        void UpdateTexture( TextureId Tex, int W, int H, const void* Pixels ) override;
        void DestroyTexture( TextureId Tex ) override;
        bool CanCapture() const override { return true; }
        void Render( const DrawList& List, Vec2 FramebufferSize, float Scale = 1.f ) override;

        ShaderId CreateShader( const ShaderDesc& Desc ) override;
        void DestroyShader( ShaderId S ) override;
        bool CanShade() const override { return true; }
        bool CanBackdrop() const override { return true; }
        TextureId CreateRenderTarget( int Width, int Height ) override;
        void RenderTo( TextureId Target, const DrawList& List, Vec2 Size, float Scale ) override;

    private:
        struct Prog { unsigned Id = 0; int Proj = -1, Tex = -1, Params = -1, Time = -1, Res = -1, Scale = -1, Extra = -1, Extra2 = -1; };
        Prog Default, Blur, Mask;
        std::unordered_map<ShaderId, Prog> Custom;
        ShaderId NextShader = 1;
        unsigned Vao = 0, Vbo = 0, Ebo = 0, QVao = 0, QVbo = 0, QEbo = 0;
        struct Target { unsigned Fbo = 0; int W = 0, H = 0; };
        std::unordered_map<TextureId, Target> Targets;
        unsigned BackTex = 0, PingTex[ 2 ] = {}, PingFbo[ 2 ] = {};
        int PingW = 0, PingH = 0;
        bool Link( Prog& P, const char* Fs );
        void Draw( const DrawList& L, Vec2 Fb, float Scale, bool FlipY, unsigned Fbo );
        void DoBackdrop( const struct DrawCmd& C, Vec2 Fb, float Scale, bool FlipY, unsigned Fbo );
        void Quad( Rect R, Vec2 Uv0, Vec2 Uv1 ); // draw one quad (UI units) with the bound program
    };
}
