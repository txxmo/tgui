# Platforms & renderers

tg's core (`tg::tg`) has no platform code. Two small adapters connect it to the world:

- a **platform layer** feeds input into `tg::Context` and tells it the window size and DPI
- a **renderer** draws the `DrawList` that `Context::Frame()` produces

| | Platform | Renderer | Target |
|---|---|---|---|
| `tg::App` on Windows (default) | Win32 | Direct3D 11 | `tg::app` |
| `tg::App` elsewhere | GLFW | OpenGL 3.3 | `tg::app` |
| Your own Windows app / game / overlay | `tg::win32` | `tg::d3d11` or `tg::d3d9` | link both |
| Headless | none | software (PNG) | `tg::soft` |

## tg::App

```cpp
tg::AppConfig Cfg;
Cfg.Title = "Server Monitor";
Cfg.Width = 1280; Cfg.Height = 800;   // UI units (scaled by display DPI)
Cfg.VSync = true;
Cfg.Continuous = false;   // true = redraw every frame (games, overlays)
Cfg.Light = false;        // start with the light palette
tg::App App( Cfg );

App.Main();               // full-window Container
App.Window( "Tools" );    // floating window
App.OnUpdate = [] { /* poll your stuff, on the UI thread, every loop iteration */ };
App.GetContext();         // the tg::Context
App.GetRenderer();        // e.g. to create textures
return App.Run();         // returns when the window closes; App.Quit() closes it
```

On Windows the window is per-monitor DPI aware: moving between a 100% and a 150% monitor re-rasterizes the fonts. If no GPU driver is available (VMs, CI), Direct3D 11 falls back to Windows' software rasterizer.

To use GLFW + OpenGL on Windows too: `-DTG_APP_BACKEND=glfw`.

## Windows: putting tg in your own window

This is the shape of `examples/win32_d3d11` and `examples/win32_d3d9`. It's how you'd add a tg menu to a game, an overlay or an existing D3D app:

```cpp
#include "tg/tg.h"
#include "d3d11_renderer.h"     // or d3d9_renderer.h
#include "win32_platform.h"

// --- once
tg::D3D11Renderer Dx;
Dx.Init( Device, DeviceContext );               // D3D9: Dx.Init( Device9 )
tg::Context Ctx( Dx );
Ctx.LoadDefaultFonts( tg::win32::GetDpiScale( Hwnd ) );
tg::win32::Install( Hwnd, Ctx );                // clipboard, wake-up, cursor
BuildMenu( Ctx );

// --- your WndProc
LRESULT CALLBACK WndProc( HWND H, UINT M, WPARAM W, LPARAM L )
{
    bool UsedByTg = tg::win32::HandleMessage( H, M, W, L );
    if ( UsedByTg && M == WM_SETCURSOR ) return TRUE;
    // overlay: if UsedByTg, don't pass this input on to the game
    ...
}

// --- your frame (D3D9: between BeginScene and EndScene)
tg::win32::Sync( Hwnd, Ctx );                   // viewport + DPI + cursor
Ctx.Frame();
Ctx.Render( { float( BackBufferWidth ), float( BackBufferHeight ) } );
```

`HandleMessage` returns `true` when the input belongs to tg: the mouse is over a tg widget, a popup is open, or a text field or keybind has focus.

### Direct3D 11 notes

- Shaders compile at startup with `d3dcompiler_47.dll`, which ships with Windows 10 and 11.
- `Render()` saves and restores the pipeline state it touches (shaders, buffers, blend, raster, depth, viewport, scissor, sampler, SRV). The host's rendering isn't disturbed.
- Bind your back buffer as the render target before calling `Ctx.Render(...)`.

### Direct3D 9 notes

- Same API as the old tgui setup: draw between `BeginScene` and `EndScene`.
- State is saved and restored with a state block.
- **Device reset:** call `Dx.InvalidateDeviceObjects()` before `IDirect3DDevice9::Reset` and `Dx.CreateDeviceObjects()` after it.
- Textures are `D3DPOOL_MANAGED`, so they survive resets.

### Win32 platform API

| Function | |
|---|---|
| `win32::EnableDpiAwareness()` | per-monitor v2; call before creating windows |
| `win32::Install( Hwnd, Ctx )` | attach; any number of windows, one Context each |
| `win32::Uninstall( Hwnd )` / `Shutdown()` | detach one window / all of them |
| `win32::HandleMessage( Hwnd, Msg, W, L )` | feed a message; `true` = tg used it |
| `win32::Sync( Hwnd, Ctx )` | viewport, DPI scale, cursor; once per loop iteration |
| `win32::WaitForWork( Ctx )` | sleep until input, a posted task or a timed redraw |
| `win32::GetDpiScale( Hwnd )` | 1.0 = 96 DPI |

What it handles:
- every key, including left/right modifiers and the keypad
- mouse buttons including side buttons (`Mouse4` / `Mouse5`), with capture so drags work outside the window
- wheel and horizontal wheel, and UTF-16 text input including emoji surrogate pairs
- cursors and the clipboard
- files dragged in from Explorer (OLE drop target with hover highlight; `WM_DROPFILES` fallback)
- IME composition at the caret (return 0 for `WM_IME_STARTCOMPOSITION` / `COMPOSITION` / `ENDCOMPOSITION` when it says it used them)

## GLFW + OpenGL 3.3

`backends/glfw_platform.*` and `backends/gl3_renderer.*`. The GL renderer loads its ~40 functions through the `GetProcAddress` you give it (`glfwGetProcAddress`, `SDL_GL_GetProcAddress`...), so no GLEW or glad is needed.

```cpp
tg::GL3Renderer Gl;
Gl.Init( []( const char* N ) { return (void*)glfwGetProcAddress( N ); } );
tg::Context Ctx( Gl );
Ctx.LoadDefaultFonts( Scale );
tg::glfw::Install( Window, Ctx );
while ( !glfwWindowShouldClose( Window ) )
{
    tg::glfw::WaitForWork( Ctx );
    tg::glfw::Sync( Window, Ctx );
    if ( !Ctx.NeedsFrame() ) continue;
    Ctx.Frame();
    Ctx.Render( { float( FbW ), float( FbH ) } );
    glfwSwapBuffers( Window );
}
```

## Software renderer

`tg::SoftRenderer` rasterizes into `Pixels` (RGBA8) and can `SavePNG( Path )`. It's used by the test suite and can generate images on a server (status pages, thumbnails). It produces the same pixels as the GPU backends.

## Writing a renderer

Implement `tg::Renderer`:

```cpp
class MyRenderer : public tg::Renderer
{
public:
    tg::TextureId CreateTexture( int W, int H, const void* Rgba ) override;
    void UpdateTexture( tg::TextureId, int W, int H, const void* Rgba ) override;
    void DestroyTexture( tg::TextureId ) override;
    void Render( const tg::DrawList& L, tg::Vec2 FramebufferSize, float Scale ) override;
};
```

`Render()` walks `L.Cmds`. Each command is:
- a scissor rect (`Clip`, in UI units, so multiply by `Scale`)
- a texture
- an index range (`IdxOffset`, `IdxCount`) into `L.Idx`, a `uint32_t` triangle list over `L.Vtx`

The pipeline it needs:
- **Vertex format:** `float2 Pos` (UI units), `float2 UV`, `uint32 Col` (RGBA8, R in the low byte).
- **Blending:** straight alpha (`SrcAlpha, InvSrcAlpha`). Shapes sample a white texel in the font atlas, so a single "color × texture" shader covers everything.
- **Projection:** orthographic from `(0,0)` top-left to `FramebufferSize / Scale`.

`backends/gl3_renderer.cpp` (~220 lines) is the easiest template; `d3d11_renderer.cpp` shows state save/restore.

## Writing a platform layer

Call these on the context:
- `SetViewport( SizeInUnits, Scale )`
- `OnMouseMove`, `OnMouseButton`, `OnMouseWheel`
- `OnKey` (map your key codes to `tg::Key`) and `OnText` (Unicode codepoints)

Optionally:
- set `Ctx.SetClipboard`, `GetClipboard` and `WakeUp`
- apply `Ctx.GetCursor()`
- sleep for `Ctx.SecondsUntilNextFrame()` when `!Ctx.NeedsFrame()`, then call `Ctx.RunPumps()` (it caps the sleep at 10 ms while a pump is registered: the WebView on Linux uses one for GTK)
- set `Ctx.NativeWindow` (HWND / X11 Window / NSWindow*) so `WebView` can attach to your window

`backends/glfw_platform.cpp` is about 150 lines.
