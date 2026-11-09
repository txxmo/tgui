# Testing

## The headless test

`tg_headless` builds the full demo app, drives it with scripted input through the same `Context` API a real window uses, checks the results, and writes screenshots. It needs no GPU or window, so it runs in CI.

```sh
./build/tg_headless out/            # Windows: build\Release\tg_headless.exe out
ctest --test-dir build              # the same, via CTest
./build/tg_headless out/ 2          # render at 200% scale
```

It prints a line per screenshot and ends with `headless: all checks passed`. On failure it prints the failing check and line and exits with code 1:

```
CHECK failed: App.Region == 2 (line 154)
```

What it covers today:
- **Chat:** typing, Enter-to-send, stick-to-bottom scrolling.
- **Controls:** checkbox binding.
- **Combo:** pick, toggle closed, click outside, Escape. **MultiCombo:** stays open while ticking.
- **ListBox:** select, plain mode, multi mode. **Color picker:** drag in the square and the hue bar, hex entry.
- **Table:** numeric sort, reverse, selection follows its row, wheel, column resize.
- **Keybind:** keys, side buttons, M1, Escape unbinds, hold and toggle modes, the menu hotkey.
- **Windows:** drag, resize, raise, close. **Tabs** inside windows, and groupbox scrolling.
- **Light theme.**

## Writing checks

The test is plain C++ in `examples/headless/main.cpp`. The pattern:

```cpp
tg::SoftRenderer Soft;
tg::Context Ctx( Soft );
Ctx.LoadDefaultFonts();
Ctx.SetViewport( { 1280, 820 } );
Ctx.SetFixedTimestep( 1.f / 60.f );     // animations advance deterministically

auto& Cb = Ctx.Root().Add<tg::Checkbox>( "x", &Value );
Ctx.Frame();                            // layout

Ctx.OnMouseMove( Cb.Bounds.Center() );  // simulate a click
Ctx.OnMouseButton( tg::MouseButton::Left, true );
Ctx.OnMouseButton( tg::MouseButton::Left, false );
CHECK( Value == true );

Ctx.OnKey( tg::Key::Escape, true, 0 );  // keys
Ctx.OnText( 'a' );                      // typing

Ctx.Frame();
Ctx.Render( { 1280, 820 } );
Soft.SavePNG( "out/shot.png" );         // look at it
```

Helpers in the test file:
- `Click( Ctx, Widget )` and `ClickAt( Ctx, Pos )` click a widget or a point.
- `Drag( Ctx, From, To )` drags.
- `Settle( Ctx )` runs frames until animations finish.
- `Shot( Ctx, Name )` writes a screenshot.
- `Find<T>( Root, N )` finds the Nth widget of type `T`.
- `OpenPopup( Ctx )` returns the open popup.

## Golden images

`tg_golden` renders a few fixed scenes and compares them with the PNGs in `tests/golden/ref`. On failure it writes `.actual.png` and `.diff.png` files. After an intended visual change, refresh the references with `cmake --build build --target tg_golden_update`. Details are in [Packaging and tests](packaging.md#tests).

## Building and running the Windows version from Linux

```sh
sudo apt install g++-mingw-w64-x86-64-posix wine64
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
wine build-win/tg_headless.exe out     # the test, as a Windows exe
```

The executables are statically linked, so they run on a stock Windows machine with no DLLs to copy. Under Wine with a virtual display (`Xvfb`), the D3D11 and D3D9 examples render and accept real input. That's how the Windows backends in this repo were verified.
