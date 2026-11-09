# tg documentation

tg is a small retained-mode C++20 GUI framework. You build a tree of widgets once, keep references to the ones you care about, and change them when your data changes. tg lays them out, routes input, and redraws only when something actually changed.

| Page | What's in it |
|---|---|
| [Getting started](getting-started.md) | Build, run the examples, put tg in your own project, your first window |
| [Core concepts](concepts.md) | The widget tree, `Context`, the frame loop, input, focus, popups, threads |
| [Layout](layout.md) | Rows, columns, stacks, free placement; `Fit` / `Px` / `Grow`; padding, gap, alignment |
| [Styling](styling.md) | Theme palette, `Paint`, per-type defaults, per-instance styles, editing `Draw()` |
| [Controls reference](widgets.md) | Every widget: how to create it, its API, its style fields, its one-line helper |
| [App systems](systems.md) | Keyboard navigation, shortcuts, undo/redo, saved settings & profiles, inspector and stats |
| [Writing widgets](custom-widgets.md) | Subclassing `Widget`, the `DrawList` API, events, animation, popups |
| [Rendering](rendering.md) | Custom shaders, gradients, frosted glass, render to texture, SVG, italics & font families |
| [Platforms & renderers](backends.md) | `tg::App`, GLFW/OpenGL, Win32 + Direct3D 11 / 9, overlays, writing a backend |
| [Testing](testing.md) | The headless test, scripted input, screenshots, cross-compiling + Wine |

## The 30-second version

```cpp
#include "tg/app.h"

int main()
{
    tg::App App( "My tool" );

    static bool Enabled = true;
    static float Volume = 0.5f;

    auto& W = App.Window( "Settings" );         // floating, draggable window
    W.Checkbox( "Enabled", &Enabled );           // bound to your bool
    W.Slider( "Volume", &Volume, 0, 1 );
    W.Button( "Apply", [] { /* ... */ } );

    return App.Run();
}
```

- **Where things live:** each widget is one header in `include/tg/widgets/` and one `.cpp` in `src/widgets/`. Its look is the `Draw()` function in that `.cpp`, and its tunables are the `XxxStyle` struct in the header.
- **Platforms:** Windows (native Win32 + Direct3D 11, or Direct3D 9), plus anything GLFW + OpenGL 3.3 runs on.
- **Assets:** none to ship. The Inter font is compiled into the library.

## Conventions used in the code

- `PascalCase` for types, functions and members, same as the original tgui.
- Every widget has a `Style` member (for `Window` it's `Chrome`, for `Groupbox` it's `Frame`). Most also have `static XxxStyle& Defaults()`.
- Widgets that edit a value take an optional pointer to *your* variable (`bool*`, `float*`, `int*`, `tg::Color*`, `tg::Key*`). If you pass `nullptr`, the widget stores the value itself. Read it with `GetValue()`/`IsChecked()`/..., or listen with `OnChange`.
- Units are **UI units**, not pixels. At 100% display scaling they're the same; at 150% one unit is 1.5 pixels. Fonts are rasterized at the real scale, so text stays sharp.
