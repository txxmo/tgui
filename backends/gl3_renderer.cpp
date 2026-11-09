#include "gl3_renderer.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

// ---- the slice of OpenGL we use, declared by hand so no GL headers/loaders are required ----
#if defined( _WIN32 ) && !defined( __CYGWIN__ )
#define TG_APIENTRY __stdcall
#else
#define TG_APIENTRY
#endif

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef std::ptrdiff_t GLsizeiptr;

#define TG_GL_FUNCS( X ) \
    X( void, GenTextures, GLsizei, GLuint* ) \
    X( void, DeleteTextures, GLsizei, const GLuint* ) \
    X( void, BindTexture, GLenum, GLuint ) \
    X( void, TexParameteri, GLenum, GLenum, GLint ) \
    X( void, TexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void* ) \
    X( void, PixelStorei, GLenum, GLint ) \
    X( void, ActiveTexture, GLenum ) \
    X( GLuint, CreateShader, GLenum ) \
    X( void, ShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint* ) \
    X( void, CompileShader, GLuint ) \
    X( void, GetShaderiv, GLuint, GLenum, GLint* ) \
    X( void, GetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar* ) \
    X( void, DeleteShader, GLuint ) \
    X( GLuint, CreateProgram, void ) \
    X( void, AttachShader, GLuint, GLuint ) \
    X( void, LinkProgram, GLuint ) \
    X( void, GetProgramiv, GLuint, GLenum, GLint* ) \
    X( void, GetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar* ) \
    X( void, DeleteProgram, GLuint ) \
    X( void, UseProgram, GLuint ) \
    X( GLint, GetUniformLocation, GLuint, const GLchar* ) \
    X( void, Uniform1i, GLint, GLint ) \
    X( void, Uniform1f, GLint, GLfloat ) \
    X( void, Uniform2f, GLint, GLfloat, GLfloat ) \
    X( void, Uniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat ) \
    X( void, Uniform4fv, GLint, GLsizei, const GLfloat* ) \
    X( void, UniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat* ) \
    X( void, GenVertexArrays, GLsizei, GLuint* ) \
    X( void, DeleteVertexArrays, GLsizei, const GLuint* ) \
    X( void, BindVertexArray, GLuint ) \
    X( void, GenBuffers, GLsizei, GLuint* ) \
    X( void, DeleteBuffers, GLsizei, const GLuint* ) \
    X( void, BindBuffer, GLenum, GLuint ) \
    X( void, BufferData, GLenum, GLsizeiptr, const void*, GLenum ) \
    X( void, EnableVertexAttribArray, GLuint ) \
    X( void, VertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void* ) \
    X( void, Enable, GLenum ) \
    X( void, Disable, GLenum ) \
    X( void, BlendEquation, GLenum ) \
    X( void, BlendFuncSeparate, GLenum, GLenum, GLenum, GLenum ) \
    X( void, Viewport, GLint, GLint, GLsizei, GLsizei ) \
    X( void, Scissor, GLint, GLint, GLsizei, GLsizei ) \
    X( void, DrawElements, GLenum, GLsizei, GLenum, const void* ) \
    X( void, ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void* ) \
    X( void, GenFramebuffers, GLsizei, GLuint* ) \
    X( void, DeleteFramebuffers, GLsizei, const GLuint* ) \
    X( void, BindFramebuffer, GLenum, GLuint ) \
    X( void, FramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint ) \
    X( GLenum, CheckFramebufferStatus, GLenum ) \
    X( void, CopyTexImage2D, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint ) \
    X( void, ClearColor, GLfloat, GLfloat, GLfloat, GLfloat ) \
    X( void, Clear, GLbitfield ) \
    X( void, GetIntegerv, GLenum, GLint* )

namespace
{
#define X( Ret, Name, ... ) typedef Ret( TG_APIENTRY* PFN_##Name )( __VA_ARGS__ ); PFN_##Name gl##Name = nullptr;
    TG_GL_FUNCS( X )
#undef X

    enum : GLenum
    {
        GL_TEXTURE_2D = 0x0DE1, GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_MAG_FILTER = 0x2800, GL_LINEAR = 0x2601,
        GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803, GL_CLAMP_TO_EDGE = 0x812F, GL_RGBA = 0x1908, GL_RGBA8 = 0x8058,
        GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406, GL_UNPACK_ALIGNMENT = 0x0CF5, GL_PACK_ALIGNMENT = 0x0D05,
        GL_TEXTURE0 = 0x84C0, GL_VERTEX_SHADER = 0x8B31, GL_FRAGMENT_SHADER = 0x8B30, GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82,
        GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893, GL_STREAM_DRAW = 0x88E0, GL_TRIANGLES = 0x0004,
        GL_BLEND = 0x0BE2, GL_FUNC_ADD = 0x8006, GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303, GL_ONE = 1,
        GL_SCISSOR_TEST = 0x0C11, GL_CULL_FACE = 0x0B44, GL_DEPTH_TEST = 0x0B71, GL_FRAMEBUFFER = 0x8D40, GL_COLOR_ATTACHMENT0 = 0x8CE0,
        GL_FRAMEBUFFER_COMPLETE = 0x8CD5, GL_COLOR_BUFFER_BIT = 0x4000, GL_FRAMEBUFFER_BINDING = 0x8CA6,
    };

    const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aCol;
uniform mat4 uProj;
uniform float uScale;
out vec2 vUV;
out vec4 vCol;
out vec2 vPos;
void main() { vUV = aUV; vCol = aCol; vPos = aPos * uScale; gl_Position = uProj * vec4(aPos, 0.0, 1.0); }
)";

    const char* FS = R"(#version 330 core
in vec2 vUV; in vec4 vCol; in vec2 vPos;
uniform sampler2D uTex;
out vec4 oCol;
void main() { oCol = vCol * texture(uTex, vUV); }
)";

    // separable Gaussian: 17 taps, uDir = step between taps (in UV)
    const char* BlurFS = R"(#version 330 core
in vec2 vUV; in vec4 vCol; in vec2 vPos;
uniform sampler2D uTex;
uniform vec2 uDir;
out vec4 oCol;
void main()
{
    vec4 C = vec4(0.0); float W = 0.0;
    for (int i = -8; i <= 8; i++) { float w = exp(-0.5 * float(i * i) / 10.0); C += texture(uTex, vUV + uDir * float(i)) * w; W += w; }
    oCol = C / W;
}
)";

    // the blurred backdrop, clipped to a rounded rect (antialiased edge)
    const char* MaskFS = R"(#version 330 core
in vec2 vUV; in vec4 vCol; in vec2 vPos;
uniform sampler2D uTex;
uniform vec4 uRect;   // pixels: x, y, w, h
uniform float uRadius;
out vec4 oCol;
void main()
{
    vec2 H = uRect.zw * 0.5, Q = abs(vPos - (uRect.xy + H)) - (H - vec2(uRadius));
    float D = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - uRadius;
    vec4 C = texture(uTex, vUV);
    oCol = vec4(C.rgb, 1.0) * clamp(0.5 - D, 0.0, 1.0);
}
)";

    std::string CustomFS( const std::string& Body )
    {
        return std::string( R"(#version 330 core
in vec2 vUV; in vec4 vCol; in vec2 vPos;
uniform sampler2D uTex;
uniform vec4 Params[8];
uniform float Time;
uniform vec2 Resolution;
out vec4 oCol;
vec4 tex(vec2 uv) { return texture(uTex, uv); }
#line 1
)" ) + Body + "\nvoid main() { oCol = effect(vCol, vUV, vPos); }\n";
    }

    GLuint Compile( GLenum Type, const char* Src )
    {
        GLuint S = glCreateShader( Type );
        glShaderSource( S, 1, &Src, nullptr );
        glCompileShader( S );
        GLint Ok = 0;
        glGetShaderiv( S, GL_COMPILE_STATUS, &Ok );
        if ( !Ok )
        {
            char Log[ 2048 ];
            glGetShaderInfoLog( S, sizeof( Log ), nullptr, Log );
            std::fprintf( stderr, "tg gl3: shader error: %s\n", Log );
            glDeleteShader( S );
            return 0;
        }
        return S;
    }

    GLuint MakeTex( int W, int H )
    {
        GLuint T = 0;
        glGenTextures( 1, &T );
        glBindTexture( GL_TEXTURE_2D, T );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
        return T;
    }

    void SetupVao( GLuint& Vao, GLuint& Vbo, GLuint& Ebo )
    {
        glGenVertexArrays( 1, &Vao );
        glGenBuffers( 1, &Vbo );
        glGenBuffers( 1, &Ebo );
        glBindVertexArray( Vao );
        glBindBuffer( GL_ARRAY_BUFFER, Vbo );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Ebo );
        glEnableVertexAttribArray( 0 );
        glEnableVertexAttribArray( 1 );
        glEnableVertexAttribArray( 2 );
        glVertexAttribPointer( 0, 2, GL_FLOAT, 0, sizeof( tg::Vertex ), (void*)offsetof( tg::Vertex, Pos ) );
        glVertexAttribPointer( 1, 2, GL_FLOAT, 0, sizeof( tg::Vertex ), (void*)offsetof( tg::Vertex, UV ) );
        glVertexAttribPointer( 2, 4, GL_UNSIGNED_BYTE, 1, sizeof( tg::Vertex ), (void*)offsetof( tg::Vertex, Col ) );
        glBindVertexArray( 0 );
    }
}

namespace tg
{
    bool GL3Renderer::Link( Prog& P, const char* Fs )
    {
        GLuint V = Compile( GL_VERTEX_SHADER, VS ), F = Compile( GL_FRAGMENT_SHADER, Fs );
        if ( !V || !F ) { if ( V ) glDeleteShader( V ); if ( F ) glDeleteShader( F ); return false; }
        P.Id = glCreateProgram();
        glAttachShader( P.Id, V );
        glAttachShader( P.Id, F );
        glLinkProgram( P.Id );
        glDeleteShader( V );
        glDeleteShader( F );
        GLint Ok = 0;
        glGetProgramiv( P.Id, GL_LINK_STATUS, &Ok );
        if ( !Ok )
        {
            char Log[ 1024 ];
            glGetProgramInfoLog( P.Id, sizeof( Log ), nullptr, Log );
            std::fprintf( stderr, "tg gl3: link error: %s\n", Log );
            glDeleteProgram( P.Id );
            P.Id = 0;
            return false;
        }
        P.Proj = glGetUniformLocation( P.Id, "uProj" );
        P.Tex = glGetUniformLocation( P.Id, "uTex" );
        P.Scale = glGetUniformLocation( P.Id, "uScale" );
        P.Params = glGetUniformLocation( P.Id, "Params" );
        P.Time = glGetUniformLocation( P.Id, "Time" );
        P.Res = glGetUniformLocation( P.Id, "Resolution" );
        P.Extra = glGetUniformLocation( P.Id, "uDir" ) >= 0 ? glGetUniformLocation( P.Id, "uDir" ) : glGetUniformLocation( P.Id, "uRect" );
        P.Extra2 = glGetUniformLocation( P.Id, "uRadius" );
        return true;
    }

    bool GL3Renderer::Init( GetProcFn GetProc )
    {
#define X( Ret, Name, ... ) gl##Name = reinterpret_cast<PFN_##Name>( GetProc( "gl" #Name ) ); if ( !gl##Name ) { std::fprintf( stderr, "tg gl3: missing gl" #Name "\n" ); return false; }
        TG_GL_FUNCS( X )
#undef X
        if ( !Link( Default, FS ) || !Link( Blur, BlurFS ) || !Link( Mask, MaskFS ) ) return false;
        SetupVao( Vao, Vbo, Ebo );
        SetupVao( QVao, QVbo, QEbo );
        return true;
    }

    void GL3Renderer::Shutdown()
    {
        for ( GLuint* B : { &Vbo, &Ebo, &QVbo, &QEbo } ) if ( *B ) glDeleteBuffers( 1, B );
        for ( GLuint* A : { &Vao, &QVao } ) if ( *A ) glDeleteVertexArrays( 1, A );
        for ( Prog* P : { &Default, &Blur, &Mask } ) if ( P->Id ) glDeleteProgram( P->Id );
        for ( auto& [ Id, P ] : Custom ) glDeleteProgram( P.Id );
        Custom.clear();
        for ( auto& [ T, F ] : Targets ) { glDeleteFramebuffers( 1, &F.Fbo ); GLuint Tx = GLuint( T ); glDeleteTextures( 1, &Tx ); }
        Targets.clear();
        if ( BackTex ) glDeleteTextures( 1, &BackTex );
        for ( int i = 0; i < 2; i++ ) { if ( PingTex[ i ] ) glDeleteTextures( 1, &PingTex[ i ] ); if ( PingFbo[ i ] ) glDeleteFramebuffers( 1, &PingFbo[ i ] ); }
        Vbo = Ebo = Vao = QVbo = QEbo = QVao = BackTex = 0;
        Default = Blur = Mask = Prog{};
    }

    TextureId GL3Renderer::CreateTexture( int W, int H, const void* Pixels )
    {
        GLuint T = MakeTex( W, H );
        glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
        return TextureId( T );
    }

    void GL3Renderer::UpdateTexture( TextureId Tex, int W, int H, const void* Pixels )
    {
        glBindTexture( GL_TEXTURE_2D, GLuint( Tex ) );
        glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
    }

    void GL3Renderer::DestroyTexture( TextureId Tex )
    {
        auto It = Targets.find( Tex );
        if ( It != Targets.end() ) { glDeleteFramebuffers( 1, &It->second.Fbo ); Targets.erase( It ); }
        GLuint T = GLuint( Tex );
        glDeleteTextures( 1, &T );
    }

    ShaderId GL3Renderer::CreateShader( const ShaderDesc& D )
    {
        if ( D.Glsl.empty() ) return 0;
        Prog P;
        std::string Src = CustomFS( D.Glsl );
        if ( !Link( P, Src.c_str() ) ) return 0;
        Custom[ NextShader ] = P;
        return NextShader++;
    }

    void GL3Renderer::DestroyShader( ShaderId S )
    {
        auto It = Custom.find( S );
        if ( It == Custom.end() ) return;
        glDeleteProgram( It->second.Id );
        Custom.erase( It );
    }

    TextureId GL3Renderer::CreateRenderTarget( int W, int H )
    {
        GLuint T = MakeTex( W, H );
        Target Tg;
        Tg.W = W;
        Tg.H = H;
        glGenFramebuffers( 1, &Tg.Fbo );
        glBindFramebuffer( GL_FRAMEBUFFER, Tg.Fbo );
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, T, 0 );
        bool Ok = glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
        if ( !Ok ) { glDeleteFramebuffers( 1, &Tg.Fbo ); glDeleteTextures( 1, &T ); return 0; }
        Targets[ TextureId( T ) ] = Tg;
        return TextureId( T );
    }

    void GL3Renderer::RenderTo( TextureId Tex, const DrawList& L, Vec2 Size, float Scale )
    {
        auto It = Targets.find( Tex );
        if ( It == Targets.end() ) return;
        if ( It->second.W != int( Size.x ) || It->second.H != int( Size.y ) )
        {
            glBindTexture( GL_TEXTURE_2D, GLuint( Tex ) );
            glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, int( Size.x ), int( Size.y ), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
            It->second.W = int( Size.x );
            It->second.H = int( Size.y );
        }
        GLint Prev = 0;
        glGetIntegerv( GL_FRAMEBUFFER_BINDING, &Prev );
        glBindFramebuffer( GL_FRAMEBUFFER, It->second.Fbo );
        glDisable( GL_SCISSOR_TEST );
        glClearColor( 0, 0, 0, 0 );
        glClear( GL_COLOR_BUFFER_BIT );
        Draw( L, Size, Scale, true, It->second.Fbo ); // flipped: texture rows stay top-down
        glBindFramebuffer( GL_FRAMEBUFFER, GLuint( Prev ) );
    }

    void GL3Renderer::Render( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( Fb.x <= 0 || Fb.y <= 0 ) return;
        GLint Cur = 0;
        glGetIntegerv( GL_FRAMEBUFFER_BINDING, &Cur );
        Draw( L, Fb, Scale, false, GLuint( Cur ) );
        if ( CaptureNext )
        {
            CaptureNext = false;
            int Cw = int( Fb.x ), Ch = int( Fb.y );
            std::vector<uint32_t> Up( size_t( Cw ) * Ch );
            glPixelStorei( GL_PACK_ALIGNMENT, 1 );
            glReadPixels( 0, 0, Cw, Ch, GL_RGBA, GL_UNSIGNED_BYTE, Up.data() );
            Captured.W = Cw;
            Captured.H = Ch;
            Captured.Px.resize( Up.size() );
            for ( int y = 0; y < Ch; y++ ) // GL rows go bottom-up
                std::copy( Up.begin() + size_t( Ch - 1 - y ) * Cw, Up.begin() + size_t( Ch - y ) * Cw, Captured.Px.begin() + size_t( y ) * Cw );
        }
    }

    void GL3Renderer::Quad( Rect R, Vec2 Uv0, Vec2 Uv1 )
    {
        Vertex V[ 4 ] = { { R.Pos(), Uv0, 0xFFFFFFFFu }, { { R.Right(), R.y }, { Uv1.x, Uv0.y }, 0xFFFFFFFFu },
                          { R.Max(), Uv1, 0xFFFFFFFFu }, { { R.x, R.Bottom() }, { Uv0.x, Uv1.y }, 0xFFFFFFFFu } };
        const uint32_t I[ 6 ] = { 0, 1, 2, 0, 2, 3 };
        glBindVertexArray( QVao );
        glBindBuffer( GL_ARRAY_BUFFER, QVbo );
        glBufferData( GL_ARRAY_BUFFER, sizeof( V ), V, GL_STREAM_DRAW );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, QEbo );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( I ), I, GL_STREAM_DRAW );
        glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr );
    }

    // Frosted glass: copy what's behind the area, blur it (two passes at half resolution), draw it back
    // through a rounded-rect mask. The panel itself is drawn after, by the widget.
    void GL3Renderer::DoBackdrop( const DrawCmd& C, Vec2 Fb, float Scale, bool FlipY, unsigned Fbo )
    {
        Rect A = C.Area.Intersect( C.Clip );
        if ( A.Empty() ) return;
        float Pad = C.Amount * 1.5f;
        Rect Src = { A.x - Pad, A.y - Pad, A.w + Pad * 2, A.h + Pad * 2 };
        int X0 = std::max( 0, int( Src.x * Scale ) ), Y0 = std::max( 0, int( Src.y * Scale ) );
        int X1 = std::min( int( Fb.x ), int( std::ceil( Src.Right() * Scale ) ) ), Y1 = std::min( int( Fb.y ), int( std::ceil( Src.Bottom() * Scale ) ) );
        int W = X1 - X0, H = Y1 - Y0;
        if ( W <= 0 || H <= 0 ) return;
        Rect Got = { X0 / Scale, Y0 / Scale, W / Scale, H / Scale }; // what we copied, in UI units
        // 1) copy (GL's y is bottom-up on screen; render targets are already flipped)
        if ( !BackTex ) BackTex = MakeTex( 1, 1 );
        glBindTexture( GL_TEXTURE_2D, BackTex );
        glCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, X0, FlipY ? Y0 : int( Fb.y ) - Y1, W, H, 0 );
        // 2) blur at half size, ping-pong
        int Hw = std::max( 1, W / 2 ), Hh = std::max( 1, H / 2 );
        if ( Hw > PingW || Hh > PingH )
        {
            PingW = std::max( PingW, Hw );
            PingH = std::max( PingH, Hh );
            for ( int i = 0; i < 2; i++ )
            {
                if ( PingTex[ i ] ) glDeleteTextures( 1, &PingTex[ i ] );
                if ( !PingFbo[ i ] ) glGenFramebuffers( 1, &PingFbo[ i ] );
                PingTex[ i ] = MakeTex( PingW, PingH );
                glBindFramebuffer( GL_FRAMEBUFFER, PingFbo[ i ] );
                glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PingTex[ i ], 0 );
            }
        }
        glDisable( GL_SCISSOR_TEST );
        glDisable( GL_BLEND );
        glUseProgram( Blur.Id );
        const float Id[ 16 ] = { 2.f / PingW, 0, 0, 0, 0, 2.f / PingH, 0, 0, 0, 0, -1, 0, -1, -1, 0, 1 }; // pixels, y up
        glUniformMatrix4fv( Blur.Proj, 1, 0, Id );
        glUniform1f( Blur.Scale, 1 );
        glUniform1i( Blur.Tex, 0 );
        glActiveTexture( GL_TEXTURE0 );
        float Step = std::max( 1.f, C.Amount * Scale / 2 / 8 ); // 17 taps across the radius (half-res texels)
        glViewport( 0, 0, PingW, PingH );
        // H pass: BackTex -> Ping0 ; V pass: Ping0 -> Ping1
        glBindFramebuffer( GL_FRAMEBUFFER, PingFbo[ 0 ] );
        glBindTexture( GL_TEXTURE_2D, BackTex );
        glUniform2f( Blur.Extra, Step * 2 / W, 0 );
        Quad( { 0, 0, float( Hw ), float( Hh ) }, { 0, 0 }, { 1, 1 } );
        glBindFramebuffer( GL_FRAMEBUFFER, PingFbo[ 1 ] );
        glBindTexture( GL_TEXTURE_2D, PingTex[ 0 ] );
        float U = float( Hw ) / PingW, V = float( Hh ) / PingH;
        glUniform2f( Blur.Extra, 0, Step / PingH );
        Quad( { 0, 0, float( Hw ), float( Hh ) }, { 0, 0 }, { U, V } );
        // 3) back to the target, draw through the rounded mask
        glBindFramebuffer( GL_FRAMEBUFFER, Fbo );
        glViewport( 0, 0, GLsizei( Fb.x ), GLsizei( Fb.y ) );
        glEnable( GL_BLEND );
        glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA ); // premultiplied
        glUseProgram( Mask.Id );
        float Wu = Fb.x / Scale, Hu = Fb.y / Scale;
        const float Proj[ 16 ] = { 2.f / Wu, 0, 0, 0, 0, FlipY ? 2.f / Hu : -2.f / Hu, 0, 0, 0, 0, -1, 0, -1, FlipY ? -1.f : 1.f, 0, 1 };
        glUniformMatrix4fv( Mask.Proj, 1, 0, Proj );
        glUniform1f( Mask.Scale, Scale );
        glUniform1i( Mask.Tex, 0 );
        glUniform4f( Mask.Extra, A.x * Scale, A.y * Scale, A.w * Scale, A.h * Scale );
        glUniform1f( Mask.Extra2, C.Radius * Scale );
        glBindTexture( GL_TEXTURE_2D, PingTex[ 1 ] );
        // Ping1 holds the copied region with y up (screen) or y down (render target)
        Vec2 Uv0 = { ( A.x - Got.x ) / Got.w * U, 0 }, Uv1 = { ( A.Right() - Got.x ) / Got.w * U, 0 };
        float T0 = ( A.y - Got.y ) / Got.h, T1 = ( A.Bottom() - Got.y ) / Got.h;
        if ( !FlipY ) { Uv0.y = ( 1 - T0 ) * V; Uv1.y = ( 1 - T1 ) * V; }
        else { Uv0.y = T0 * V; Uv1.y = T1 * V; }
        Quad( A, Uv0, Uv1 );
        glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
        glEnable( GL_SCISSOR_TEST );
    }

    void GL3Renderer::Draw( const DrawList& L, Vec2 Fb, float Scale, bool FlipY, unsigned Fbo )
    {
        glViewport( 0, 0, GLsizei( Fb.x ), GLsizei( Fb.y ) );
        glEnable( GL_BLEND );
        glBlendEquation( GL_FUNC_ADD );
        glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
        glDisable( GL_CULL_FACE );
        glDisable( GL_DEPTH_TEST );
        glEnable( GL_SCISSOR_TEST );

        // Orthographic projection in UI units: (0,0) top-left (render targets: flipped so rows stay top-down).
        float W = Fb.x / Scale, H = Fb.y / Scale;
        const float Proj[ 16 ] = { 2.f / W, 0, 0, 0, 0, FlipY ? 2.f / H : -2.f / H, 0, 0, 0, 0, -1, 0, -1, FlipY ? -1.f : 1.f, 0, 1 };
        ShaderId Bound = ~ShaderId( 0 );
        auto Use = [ & ]( ShaderId S, const float* Params ) {
            auto It = S ? Custom.find( S ) : Custom.end();
            const Prog& P = It != Custom.end() ? It->second : Default;
            if ( S != Bound )
            {
                glUseProgram( P.Id );
                glUniformMatrix4fv( P.Proj, 1, 0, Proj );
                glUniform1f( P.Scale, Scale );
                glUniform1i( P.Tex, 0 );
                if ( P.Time >= 0 ) glUniform1f( P.Time, L.Time );
                if ( P.Res >= 0 ) glUniform2f( P.Res, Fb.x, Fb.y );
                Bound = S;
            }
            if ( P.Params >= 0 && Params ) glUniform4fv( P.Params, 8, Params );
        };
        glActiveTexture( GL_TEXTURE0 );
        auto Upload = [ & ] {
            glBindVertexArray( Vao );
            glBindBuffer( GL_ARRAY_BUFFER, Vbo );
            glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( L.Vtx.size() * sizeof( Vertex ) ), L.Vtx.data(), GL_STREAM_DRAW );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Ebo );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( L.Idx.size() * sizeof( uint32_t ) ), L.Idx.data(), GL_STREAM_DRAW );
        };
        Upload();
        for ( const DrawCmd& C : L.Cmds )
        {
            if ( C.Kind == DrawCmd::Backdrop )
            {
                DoBackdrop( C, Fb, Scale, FlipY, Fbo );
                Bound = ~ShaderId( 0 );
                glBindVertexArray( Vao ); // the main buffers are still uploaded
                glBindBuffer( GL_ARRAY_BUFFER, Vbo );
                glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Ebo );
                continue;
            }
            if ( !C.IdxCount ) continue;
            GLint X = GLint( C.Clip.x * Scale ), Y = FlipY ? GLint( C.Clip.y * Scale ) : GLint( Fb.y - C.Clip.Bottom() * Scale );
            GLsizei Sw = GLsizei( C.Clip.w * Scale + 0.5f ), Sh = GLsizei( C.Clip.h * Scale + 0.5f );
            if ( Sw <= 0 || Sh <= 0 ) continue;
            Use( C.Shader, C.Shader && C.ParamOffset + 32 <= L.Params.size() ? L.Params.data() + C.ParamOffset : nullptr );
            glScissor( X, Y, Sw, Sh );
            glBindTexture( GL_TEXTURE_2D, GLuint( C.Texture ) );
            glDrawElements( GL_TRIANGLES, GLsizei( C.IdxCount ), GL_UNSIGNED_INT, (void*)( size_t( C.IdxOffset ) * sizeof( uint32_t ) ) );
        }
        glDisable( GL_SCISSOR_TEST );
        glBindVertexArray( 0 );
    }
}
