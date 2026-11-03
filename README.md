# tg

A small retained-mode C++20 GUI framework for real apps (chat clients with screen shares, server dashboards, tools, overlays), built so that **you can find and change anything in one place**, and so that making a window with a checkbox takes about as much code as in ImGui.

```cpp
#include "tg/app.h"
#include <cstdio>

int main()
{
    tg::App App( "hello tg", 900, 600 );

    static bool Enabled = true;
    static float Volume = 0.5f;
    static int Quality = 1;
    static tg::Color Tint = tg::Color::Hex( 0x6EB7D4 );

    auto& W = App.Window( "Settings" );
    W.Checkbox( "Enabled", &Enabled );
    W.Slider( "Volume", &Volume, 0, 1 );
    W.Combo( "Quality", { "Low", "Medium", "High", "Ultra" }, &Quality );
    W.ColorPicker( "Tint", &Tint );
    W.Button( "Print", [] { std::printf( "volume=%.2f\n", Volume ); } );

    return App.Run();
}
```

<img src="docs/hello.png" width="420">

That's the whole program (`examples/hello`). There are no asset files to ship, because the font is compiled into the library.

## Screenshots

These come from `tg_headless`, which renders with the built-in software rasterizer. The OpenGL backend produces the same pixels.

<img src="docs/dashboard.png" width="640">

<img src="docs/tables.png" width="640">

| | |
|---|---|
| <img src="docs/chat.png" width="400"> | <img src="docs/windows.png" width="400"> |
| <img src="docs/combo_open.png" width="400"> | <img src="docs/multicombo_open.png" width="400"> |
| <img src="docs/colorpicker_open.png" width="400"> | <img src="docs/controls_light.png" width="400"> |

## Use it in your project

**CMake, via FetchContent:**

```cmake
include( FetchContent )
FetchContent_Declare( tg GIT_REPOSITORY https://github.com/txxmo/tgui.git GIT_TAG <branch-or-commit> )
FetchContent_MakeAvailable( tg )

add_executable( myapp main.cpp )
target_link_libraries( myapp PRIVATE tg::app )   # tg::app = tg + GLFW + OpenGL 3 + tg::App
```

You can also copy the repo in and use `add_subdirectory( tg )`. When tg is pulled in this way, its examples are off by default. GLFW is used if it's installed. Otherwise CMake fetches it.

**Targets:**

| target | what | deps |
|---|---|---|
| `tg::tg` | widgets, layout, text, DrawList | none (stb is vendored) |
| `tg::app` | `tg::App`: window + GL3 renderer + event loop | GLFW, OpenGL 3.3 |
| `tg::soft` | software renderer → PNG (tests, screenshots) | none |

To use your own window or renderer (Win32, SDL, D3D11, an existing game engine), link just `tg::tg` and see [Bring your own platform/renderer](#bring-your-own-platform--renderer).

## Controls

| widget | one-liner (`Container` helper) | notes |
|---|---|---|
| Label | `W.Text( "hi" )`, `W.Heading(..)`, `W.Caption(..)` | wrapping |
| Button | `W.Button( "Go", [] { ... } )` | `Button::Primary() / Danger() / Ghost()` styles |
| Checkbox | `W.Checkbox( "Enabled", &b )` | bind to your `bool`, or `OnChange` |
| Toggle | `W.Toggle( "Dark mode", &b )` | switch |
| Slider | `W.Slider( "Volume", &f, 0, 1 )`, `W.SliderInt( "FPS", &i, 30, 240 )` | label + slider + value |
| TextInput | `W.Input( "Nickname", &str )` | selection, clipboard, password mode |
| Combo | `W.Combo( "Region", items, &index )` | dropdown, scrolls when long |
| MultiCombo | `W.MultiCombo( "Alerts", items, &vectorOfBool )` | checkboxes in the dropdown; shows "a, b" or "3 selected" |
| ListBox | `W.List( items, SelectMode::Single, &index )` | `SelectMode::None` = plain list, `Single`, `Multi`; switch live with `SetMode()` |
| ColorPicker | `W.ColorPicker( "Tint", &color )` | swatch → popup: SV square, hue, alpha, hex input |
| ColorPanel | `W.ColorPanel( &color )` | the picker, inline |
| ProgressBar | `W.Progress( 0.4f )` | optional warn/hot thresholds |
| Table | `W.Table( { "Name", "CPU", "Memory" } )` | sortable headers (numbers sort as numbers), drag-to-resize columns, None/Single/Multi row select, custom cells, only visible rows drawn |
| Window | `App.Window( "Title", pos, size )` | drag, resize, close, raise on click |
| ScrollView, Image, LineChart, Box | `Add<T>(...)` | chat logs, live textures/screen share, charts, cards |

Layout helpers: `W.Row()`, `W.Column()`, `W.Card( "Title" )`, `W.Separator()`, `W.Spacer()`. They return Containers, so the helpers chain:

```cpp
auto& Btns = W.Row();
Btns.Button( "Apply" ).Style = tg::Button::Primary();
Btns.Button( "Reset" );
```

Every helper returns the real widget, so you can keep going: `W.Checkbox( "x", &b ).Style.Radius = 0;`.

## Styling: four levels, pick the closest one

```cpp
// 1. Palette: every widget follows.
tg::GetTheme()[ tg::Tok::Accent ] = tg::Color::Hex( 0xFF4F81 );
tg::GetTheme().UseColors( tg::Theme::Light() );           // runtime theme switch

// 2. Per type: every checkbox created after this.
tg::Checkbox::Defaults().Radius = 0;

// 3. Per instance: just this one.
auto& Cb = W.Checkbox( "Square & red", &MyBool );
Cb.Style.Radius = 0;
Cb.Style.Fill   = tg::Tok::Danger;             // a theme token...
Cb.Style.Check  = tg::Color( 255, 255, 255 );  // ...or a literal color

// 4. The code itself: open src/widgets/checkbox.cpp and change Draw(),
//    or subclass and override Draw().
```

Each widget is one header plus one `.cpp`. Its tunables are a `XxxStyle` struct at the top of the header, and its whole look is the `Draw()` function in its `.cpp`. Style colors are `Paint`s. A `Paint` is either a theme token (it follows theme changes) or a fixed `Color`.

## How it works (the 60-second version)

- **Retained tree.** You build widgets once, keep references to the ones you care about, and change them when data changes. This is the same model as the old tgui `Window → Groupbox → Control`, but with ownership, layout and events.
- **Flex layout.** `Dir::Column / Row / Stack / Free`, plus `Gap`, `Padding` and alignment. Each child sizes itself per axis with `Fit()`, `Px(n)` or `Grow(weight)`. `Dir::Free` places children at `Layout.Offset`, which is how floating windows work.
- **Redraws only on change.** An idle window does nothing. Animations (`Animate(value, target)` inside `Draw`) and the caret blink request exactly the frames they need. If you're making a game overlay, use `AppConfig::Continuous` to redraw every frame.
- **Popups** (dropdowns, pickers) live on an overlay layer drawn above everything. They close on an outside click or Escape.
- **Threads.** Never touch widgets from another thread. Post instead: `Ctx.Post( [&] { Chart->Push( Sample ); } );`. It's safe from any thread and wakes the UI.
- **Live textures** (screen share, video): `CreateTexture` once, `UpdateTexture` per frame, and show it with an `Image` widget.

## Tables

```cpp
auto& T = W.Table( { "PID", "Name", "CPU", "Status" } );
T.Column( 0 ).Width = tg::Px( 80 );
T.Column( 2 ).TextAlign = tg::Align::End;
T.AddRow( { "1234", "nginx", "3.2%", "running" } );
T.SortBy( 2, /*ascending*/ false );
T.SetCell( row, 2, "4.0%" );                    // live updates re-sort automatically
T.CellColor = [&]( size_t Row, size_t Col, tg::Color& Out ) { ... return true; };  // e.g. red "stopped"
T.DrawCell  = [&]( tg::DrawList& D, tg::Rect Cell, size_t Row, size_t Col ) { ... return true; }; // bars, icons
```

Row numbers are stable: sorting changes the display order, not the indices you use for `SetCell`, selection or `Bind`. Rows are painted directly instead of being widgets, and only the visible ones are drawn. With 100k rows, a scroll frame takes about 0.04 ms and a full re-sort about 0.1 s.

## Writing a widget

Subclass `Widget`. Set `Interactive = true` if it takes the mouse. Override `Measure` if its size depends on content, `Draw` to paint, and `OnEvent` to react. For example, the demo's avatar:

```cpp
class Avatar : public tg::Widget {
public:
    Avatar( std::string I, tg::Color Bg ) : Initials( std::move( I ) ), Bg( Bg ) { Layout.Width = tg::Px( 34 ); Layout.Height = tg::Px( 34 ); }
    void Draw( tg::DrawList& D ) override {
        D.CircleFilled( Bounds.Center(), Bounds.w / 2, Bg );
        D.Text( *tg::GetTheme().Bold, /*pos*/ ..., Initials, { 255, 255, 255 } );
    }
    std::string Initials; tg::Color Bg;
};
```

`DrawList` gives you anti-aliased rects and rounded rects (fill, stroke, gradient, per-corner color), circles, polylines, text, images, clipping and shadows. To build a new dropdown-style picker, subclass `DropdownBase` (see `combo.h`). For any other floating panel, call `tg::OpenPopup( *this, Bounds )`.

## Repo layout

```
include/tg/            public headers (tg.h = everything, app.h = tg::App)
  core.h draw.h font.h theme.h widget.h context.h renderer.h input.h
  widgets/*.h          one header per widget
src/                   one .cpp per header; default_fonts.cpp = embedded Inter (generated by tools/embed_fonts.py)
backends/
  app.cpp              tg::App on GLFW + GL3
  gl3_renderer.*       OpenGL 3.3 core renderer, no loader library needed
  glfw_platform.*      GLFW → tg input / cursor / clipboard / DPI, event-driven wait
  soft_renderer.*      CPU rasterizer → PNG
examples/
  hello/               the snippet at the top of this README
  showcase/            full demo: dashboard, chat + screen share, controls, floating windows
  headless/            same demo, software-rendered, scripted input + assertions (ctest)
assets/fonts/          Inter (OFL) source files
legacy-tgui-example/   the original DX9 tgui, kept for reference
```

## Build the examples

You need CMake 3.16+ and a C++20 compiler (MSVC 2019+, GCC 10+, or Clang 12+).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tg_hello
./build/tg_showcase
./build/tg_headless out/       # writes out/*.png and runs the checks
ctest --test-dir build
```

On Windows, run `cmake -S . -B build -G "Visual Studio 17 2022"` and open `build/tg.sln`.

**Build speed:** a full rebuild of the core library takes about 9s on 4 cores, most of it stb_truetype. Editing a widget and relinking takes about 2s.

## Bring your own platform / renderer

- **Renderer:** implement `tg::Renderer`, which has four functions: `CreateTexture`, `UpdateTexture`, `DestroyTexture` and `Render`. `Render` walks `DrawList::Cmds`; each command is a scissor rect, a texture and an index range over `Vtx`/`Idx`. The vertex format is `float2 pos, float2 uv, rgba8 color`, with straight-alpha blending. `backends/gl3_renderer.cpp` (about 220 lines) is the template.
- **Platform:** feed `Ctx.OnMouseMove / OnMouseButton / OnMouseWheel / OnKey / OnText` and `Ctx.SetViewport( size, dpiScale )`. Then loop: `if ( Ctx.NeedsFrame() ) { Ctx.Frame(); Ctx.Render( fb ); present(); } else wait( Ctx.SecondsUntilNextFrame() );`. `backends/glfw_platform.cpp` is the template.

## Roadmap

- [ ] Win32 + D3D11 backend (native Windows, no GLFW); Metal for macOS
- [ ] Single-header + single-.cpp bundle for copy-in use without CMake
- [ ] Tooltips, context menus, modal dialogs (the popup layer is in place)
- [ ] Keyboard navigation (Tab focus order, arrow keys in lists/combos), keybind widget from old tgui
- [ ] Virtualized lists for very long chat histories
- [ ] Multi-line text area, word-wise caret movement, undo
- [ ] Font fallback (emoji / CJK), IME
- [ ] Tabs, split panes
- [ ] Table: editable cells, column reordering, keyboard row navigation
