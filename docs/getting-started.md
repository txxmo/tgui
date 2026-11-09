# Getting started

## Requirements

- CMake 3.16+
- A C++20 compiler: MSVC 2019+, GCC 10+ or Clang 12+ (MinGW-w64 works for Windows too)
- **Windows:** nothing else. `tg::App` uses Win32 + Direct3D 11, which ship with Windows.
- **Linux / macOS:** OpenGL 3.3. GLFW is used if it's installed, otherwise CMake downloads it.

## Build and run the examples

```sh
git clone https://github.com/thugexchange/tg.git
cd tg
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

| Executable | What it shows |
|---|---|
| `tg_hello` | The old tgui README example on tg: a window with tabs and groupboxes |
| `tg_showcase` | The full demo: dashboard, chat with a live "screen share", controls, tables, floating windows |
| `tg_headless` | The test suite: drives the demo with scripted input, checks results, writes screenshots |
| `tg_win32_d3d11` / `tg_win32_d3d9` | *(Windows only)* tg drawn into your own window + D3D device, overlay-style |

With Visual Studio, the executables land in `build\Release\`. With Ninja or Make, they land in `build/`.

On Windows you can also open the solution directly:

```bat
cmake -S . -B build -G "Visual Studio 17 2022"
start build\tg.sln
```

### Run the tests

```sh
./build/tg_headless out/        # Windows: build\Release\tg_headless.exe out
ctest --test-dir build          # same test, through CTest
```

It prints `headless: all checks passed` and writes PNG screenshots of every page into `out/`. CTest also runs the golden-image tests. See [Testing](testing.md) and [Packaging and tests](packaging.md).

## Use tg in your own project

### CMake, FetchContent (recommended)

```cmake
include( FetchContent )
FetchContent_Declare( tg GIT_REPOSITORY https://github.com/thugexchange/tg.git GIT_TAG main )
FetchContent_MakeAvailable( tg )

add_executable( myapp main.cpp )
target_link_libraries( myapp PRIVATE tg::app )
```

### CMake, vendored copy

Copy the repository into your project (for example `external/tg`):

```cmake
add_subdirectory( external/tg )
target_link_libraries( myapp PRIVATE tg::app )
```

When tg is a subproject, its examples and tests are off by default, so you only build the libraries.

### Installed package, vcpkg, Conan, single header

`cmake --install` gives you `find_package( tg )`. There are also a vcpkg overlay port, a Conan recipe, and a one-file `tg_single.h`. See [Packaging and tests](packaging.md).

### Which target to link

| Target | Use it when |
|---|---|
| `tg::app` | You want tg to make the window and run the loop (`tg::App`). Windows: Win32 + D3D11. Elsewhere: GLFW + GL3. |
| `tg::tg` | You bring your own window/renderer. Core only, no platform dependencies. |
| `tg::win32`, `tg::d3d11`, `tg::d3d9` | Windows pieces for putting tg into an existing app, game or overlay |
| `tg::soft` | Software renderer to PNG: tests, screenshots, servers rendering images |

### CMake options

| Option | Default | Meaning |
|---|---|---|
| `TG_BUILD_APP` | `ON` | Build `tg::app` |
| `TG_APP_BACKEND` | `win32` on Windows, else `glfw` | Which platform `tg::App` uses (`win32` or `glfw`) |
| `TG_BUILD_EXAMPLES` | `ON` when top-level | hello, showcase, Win32 examples |
| `TG_BUILD_HEADLESS` | `ON` when top-level | The headless test + `ctest` entry |
| `TG_FREETYPE` | `OFF` | Rasterize with FreeType (hinting) and enable color emoji. Uses the system FreeType, or downloads it. See [Fonts](styling.md#freetype-emoji-and-other-scripts) |
| `TG_WEBVIEW` | `OFF` | Build `tg::webview`: web pages inside tg (WebView2 / WebKitGTK / WKWebView). See [WebView](widgets.md#webview) |
| `TG_VULKAN` | `OFF` | Build `tg::vulkan`, the Vulkan renderer. Custom shaders need shaderc. See [Vulkan](backends.md#vulkan) |
| `TG_INSTALL` | `ON` when top-level | install rules, `find_package( tg )` config, `cpack` |
| `TG_HARFBUZZ` | `OFF` | Shape emoji sequences (flags, ZWJ families, skin tones, keycaps) with HarfBuzz. Implies `TG_FREETYPE`. Uses the system HarfBuzz, or downloads it. See [Emoji sequences](styling.md#emoji-sequences-harfbuzz) |

## Your first window

```cpp
#include "tg/app.h"
#include <cstdio>

int main()
{
    tg::App App( "hello", 900, 600 );            // title, size in UI units

    static bool Enabled = true;
    static float Volume = 0.5f;
    static int Quality = 1;
    static tg::Color Tint = tg::Color::Hex( 0x6EB7D4 );
    static tg::Key Hotkey = tg::Key::F;

    auto& W = App.Window( "Settings", { 40, 40 }, { 360, 0 } ); // height 0 = fit content
    W.Checkbox( "Enabled", &Enabled );
    W.Slider( "Volume", &Volume, 0, 1 );
    W.Combo( "Quality", { "Low", "Medium", "High" }, &Quality );
    W.ColorPicker( "Tint", &Tint );
    W.Keybind( "Hotkey", &Hotkey );
    W.Button( "Print", [] { std::printf( "volume %.2f\n", Volume ); } );

    return App.Run();
}
```

The `static` variables are just there so the lambdas and the widgets can reach them. In a real app they'd be members of your app's state.

### tgui-style: tabs and groupboxes

```cpp
auto& W = App.Window( "Test Window", { 40, 40 }, { 440, 320 } );
W.Chrome = tg::Window::Classic();      // centered title + accent line
W.Footer = "v.2.5";

auto& Tab1 = W.Tab( "Tab 1" );
W.Tab( "Tab 2" );

auto& Cols = Tab1.Columns();           // a full-height row
auto& G = Cols.Group( "Example Group" );
G.Checkbox( "Checkbox", &BooleanEx );
G.Slider( "Slider", &SliderEx, 0, 100 );
Cols.Group( "Example Group 2" ).Button( "Print", [] { /* ... */ } );
```

### A full-window app instead of floating windows

`App.Main()` is a `Container` that fills the whole window behind the floating windows. Build your app layout there:

```cpp
auto& Main = App.Main();
Main.Layout.Padding = tg::Insets( 16 );
auto& Header = Main.Row();
Header.Heading( "Servers" );
Header.Spacer();
Header.Button( "Add" ).Style = tg::Button::Primary();
auto& T = Main.Table( { "Name", "Region", "CPU" } );
T.AddRow( { "vps-fra-01", "Frankfurt", "12%" } );
```

### Updating the UI from your code

When your data changes, change the widget, and tg redraws:

```cpp
CpuLabel.SetText( "37%" );
CpuChart.Push( 37.f );
ServerTable.SetCell( Row, 2, "37%" );
```

From another thread (network, stats poller, decoder), **post** the change instead of touching widgets directly:

```cpp
App.GetContext().Post( [&, Cpu] { CpuChart.Push( Cpu ); } );   // runs on the UI thread, wakes it up
```

If you changed a variable that a widget is bound to (`&Enabled`) from your own code, ask for a redraw: `App.GetContext().RequestFrame()`.

Next: [Core concepts](concepts.md).
