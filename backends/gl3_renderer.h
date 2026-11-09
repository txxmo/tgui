// OpenGL 3.3 core renderer. No GL loader library needed: pass a GetProcAddress function
// (glfwGetProcAddress, SDL_GL_GetProcAddress, wglGetProcAddress wrapper, ...).
#pragma once
#include "tg/renderer.h"

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

    private:
        unsigned Program = 0, Vao = 0, Vbo = 0, Ebo = 0;
        int ProjLoc = -1, TexLoc = -1;
    };
}
