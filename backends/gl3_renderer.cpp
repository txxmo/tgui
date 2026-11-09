#include "gl3_renderer.h"
#include "tg/draw.h"
#include <cstddef>
#include <algorithm>
#include <cstdio>
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
    X( void, DeleteProgram, GLuint ) \
    X( void, UseProgram, GLuint ) \
    X( GLint, GetUniformLocation, GLuint, const GLchar* ) \
    X( void, Uniform1i, GLint, GLint ) \
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
    X( void, ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void* )

namespace
{
#define X( Ret, Name, ... ) typedef Ret( TG_APIENTRY* PFN_##Name )( __VA_ARGS__ ); PFN_##Name gl##Name = nullptr;
    TG_GL_FUNCS( X )
#undef X

    enum : GLenum
    {
        GL_TEXTURE_2D = 0x0DE1, GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_MAG_FILTER = 0x2800, GL_LINEAR = 0x2601,
        GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803, GL_CLAMP_TO_EDGE = 0x812F, GL_RGBA = 0x1908, GL_RGBA8 = 0x8058,
        GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406, GL_UNPACK_ALIGNMENT = 0x0CF5, GL_TEXTURE0 = 0x84C0,
        GL_VERTEX_SHADER = 0x8B31, GL_FRAGMENT_SHADER = 0x8B30, GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82,
        GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893, GL_STREAM_DRAW = 0x88E0, GL_TRIANGLES = 0x0004,
        GL_BLEND = 0x0BE2, GL_FUNC_ADD = 0x8006, GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303, GL_ONE = 1,
        GL_SCISSOR_TEST = 0x0C11, GL_CULL_FACE = 0x0B44, GL_DEPTH_TEST = 0x0B71,
    };

    const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aCol;
uniform mat4 uProj;
out vec2 vUV;
out vec4 vCol;
void main() { vUV = aUV; vCol = aCol; gl_Position = uProj * vec4(aPos, 0.0, 1.0); }
)";

    const char* FS = R"(#version 330 core
in vec2 vUV;
in vec4 vCol;
uniform sampler2D uTex;
out vec4 oCol;
void main() { oCol = vCol * texture(uTex, vUV); }
)";

    GLuint Compile( GLenum Type, const char* Src )
    {
        GLuint S = glCreateShader( Type );
        glShaderSource( S, 1, &Src, nullptr );
        glCompileShader( S );
        GLint Ok = 0;
        glGetShaderiv( S, GL_COMPILE_STATUS, &Ok );
        if ( !Ok )
        {
            char Log[ 1024 ];
            glGetShaderInfoLog( S, sizeof( Log ), nullptr, Log );
            std::fprintf( stderr, "tg gl3: shader error: %s\n", Log );
        }
        return S;
    }
}

namespace tg
{
    bool GL3Renderer::Init( GetProcFn GetProc )
    {
#define X( Ret, Name, ... ) gl##Name = reinterpret_cast<PFN_##Name>( GetProc( "gl" #Name ) ); if ( !gl##Name ) { std::fprintf( stderr, "tg gl3: missing gl" #Name "\n" ); return false; }
        TG_GL_FUNCS( X )
#undef X
        GLuint V = Compile( GL_VERTEX_SHADER, VS ), F = Compile( GL_FRAGMENT_SHADER, FS );
        Program = glCreateProgram();
        glAttachShader( Program, V );
        glAttachShader( Program, F );
        glLinkProgram( Program );
        glDeleteShader( V );
        glDeleteShader( F );
        GLint Ok = 0;
        glGetProgramiv( Program, GL_LINK_STATUS, &Ok );
        if ( !Ok ) return false;
        ProjLoc = glGetUniformLocation( Program, "uProj" );
        TexLoc = glGetUniformLocation( Program, "uTex" );

        glGenVertexArrays( 1, &Vao );
        glGenBuffers( 1, &Vbo );
        glGenBuffers( 1, &Ebo );
        glBindVertexArray( Vao );
        glBindBuffer( GL_ARRAY_BUFFER, Vbo );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Ebo );
        glEnableVertexAttribArray( 0 );
        glEnableVertexAttribArray( 1 );
        glEnableVertexAttribArray( 2 );
        glVertexAttribPointer( 0, 2, GL_FLOAT, 0, sizeof( Vertex ), (void*)offsetof( Vertex, Pos ) );
        glVertexAttribPointer( 1, 2, GL_FLOAT, 0, sizeof( Vertex ), (void*)offsetof( Vertex, UV ) );
        glVertexAttribPointer( 2, 4, GL_UNSIGNED_BYTE, 1, sizeof( Vertex ), (void*)offsetof( Vertex, Col ) );
        glBindVertexArray( 0 );
        return true;
    }

    void GL3Renderer::Shutdown()
    {
        if ( Vbo ) glDeleteBuffers( 1, &Vbo );
        if ( Ebo ) glDeleteBuffers( 1, &Ebo );
        if ( Vao ) glDeleteVertexArrays( 1, &Vao );
        if ( Program ) glDeleteProgram( Program );
        Vbo = Ebo = Vao = Program = 0;
    }

    TextureId GL3Renderer::CreateTexture( int W, int H, const void* Pixels )
    {
        GLuint T = 0;
        glGenTextures( 1, &T );
        glBindTexture( GL_TEXTURE_2D, T );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
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
        GLuint T = GLuint( Tex );
        glDeleteTextures( 1, &T );
    }

    void GL3Renderer::Render( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( Fb.x <= 0 || Fb.y <= 0 ) return;
        glViewport( 0, 0, GLsizei( Fb.x ), GLsizei( Fb.y ) );
        glEnable( GL_BLEND );
        glBlendEquation( GL_FUNC_ADD );
        glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
        glDisable( GL_CULL_FACE );
        glDisable( GL_DEPTH_TEST );
        glEnable( GL_SCISSOR_TEST );

        // Orthographic projection in UI units: (0,0) top-left.
        float W = Fb.x / Scale, H = Fb.y / Scale;
        const float Proj[ 16 ] = { 2.f / W, 0, 0, 0, 0, -2.f / H, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1 };
        glUseProgram( Program );
        glUniformMatrix4fv( ProjLoc, 1, 0, Proj );
        glUniform1i( TexLoc, 0 );
        glActiveTexture( GL_TEXTURE0 );

        glBindVertexArray( Vao );
        glBindBuffer( GL_ARRAY_BUFFER, Vbo );
        glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( L.Vtx.size() * sizeof( Vertex ) ), L.Vtx.data(), GL_STREAM_DRAW );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Ebo );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( L.Idx.size() * sizeof( uint32_t ) ), L.Idx.data(), GL_STREAM_DRAW );

        for ( const DrawCmd& C : L.Cmds )
        {
            if ( !C.IdxCount ) continue;
            GLint X = GLint( C.Clip.x * Scale ), Y = GLint( Fb.y - C.Clip.Bottom() * Scale );
            GLsizei Sw = GLsizei( C.Clip.w * Scale + 0.5f ), Sh = GLsizei( C.Clip.h * Scale + 0.5f );
            if ( Sw <= 0 || Sh <= 0 ) continue;
            glScissor( X, Y, Sw, Sh );
            glBindTexture( GL_TEXTURE_2D, GLuint( C.Texture ) );
            glDrawElements( GL_TRIANGLES, GLsizei( C.IdxCount ), GL_UNSIGNED_INT, (void*)( size_t( C.IdxOffset ) * sizeof( uint32_t ) ) );
        }

        glDisable( GL_SCISSOR_TEST );
        glBindVertexArray( 0 );
        if ( CaptureNext )
        {
            CaptureNext = false;
            int Cw = int( Fb.x ), Ch = int( Fb.y );
            std::vector<uint32_t> Up( size_t( Cw ) * Ch );
            glPixelStorei( 0x0D05 /* GL_PACK_ALIGNMENT */, 1 );
            glReadPixels( 0, 0, Cw, Ch, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, Up.data() );
            Captured.W = Cw;
            Captured.H = Ch;
            Captured.Px.resize( Up.size() );
            for ( int y = 0; y < Ch; y++ ) // GL rows go bottom-up
                std::copy( Up.begin() + size_t( Ch - 1 - y ) * Cw, Up.begin() + size_t( Ch - y ) * Cw, Captured.Px.begin() + size_t( y ) * Cw );
        }
    }
}
