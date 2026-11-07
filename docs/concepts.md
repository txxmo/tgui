# Core concepts

## Retained, not immediate

In ImGui you call `Checkbox(...)` every frame. In tg you **create** a checkbox once and it lives in a tree until you remove it:

```cpp
auto& Cb = Panel.Add<tg::Checkbox>( "Enabled", &Enabled ); // Panel owns it; keep the reference if you need it
Cb.OnChange = []( bool On ) { /* ... */ };
```

This is the same model as the old tgui (`Window → Groupbox → Control`), but with real ownership, layout and events. It's what lets tg skip drawing when nothing changed: an idle window uses no CPU.

## The pieces

```
Context ─── Root (Widget tree: your UI)
        ├── Overlay (popups: dropdowns, color pickers, menus)
        ├── FontAtlas, Theme fonts
        ├── input state: hover / press / focus / capture, key state
        └── DrawList ──► Renderer (OpenGL 3, Direct3D 11, Direct3D 9, software)
```

| Type | Header | Job |
|---|---|---|
| `tg::Context` | `tg/context.h` | Owns the tree, routes input, runs layout, builds the frame's `DrawList` |
| `tg::Widget` | `tg/widget.h` | Base class of every control: children, layout spec, bounds, events, drawing |
| `tg::Container` | `tg/widgets/container.h` | A `Box` with one-line helpers (`W.Checkbox(...)`). Windows, groupboxes, tab pages and cards are all Containers |
| `tg::DrawList` | `tg/draw.h` | Anti-aliased shapes, text and images, recorded as triangles |
| `tg::Renderer` | `tg/renderer.h` | Four functions a graphics backend implements |
| `tg::Theme` | `tg/theme.h` | Color palette + fonts |
| `tg::App` | `tg/app.h` | Optional: window + renderer + loop in one object |

## The tree

```cpp
auto& Row = Parent.Add<tg::Box>( tg::Dir::Row, 8 );   // Add<T>( constructor args... ) returns T&
auto& Btn = Row.Add<tg::Button>( "Save" );
Row.Remove( &Btn );        // destroys it
Row.Clear();               // destroys all children
Btn.BringToFront();        // draw last among siblings (and get hit first)
Btn.Visible = false;       // hidden: takes no space, gets no input
Btn.Enabled = false;       // drawn dimmed, ignores input
```

The parent owns its children (`std::unique_ptr`), and references stay valid until the widget is removed. Destroying a widget is always safe: the context forgets any hover, press or focus pointing at it.

## The frame loop

`tg::App::Run()` does this for you. If you bring your own loop:

```cpp
// every iteration
Platform.FeedInput( Ctx );            // OnMouseMove / OnMouseButton / OnKey / OnText ...
Ctx.SetViewport( SizeInUnits, DpiScale );

if ( Ctx.NeedsFrame() )
{
    Ctx.Frame();                       // run posted tasks, layout if dirty, build the DrawList
    Ctx.Render( FramebufferSizeInPixels );
    Present();
}
else
    WaitForEvents( Ctx.SecondsUntilNextFrame() );   // sleep: nothing to do
```

`NeedsFrame()` is true when:
- input changed hover, press or focus, or a widget handled an event
- a widget called `Redraw()` (look changed) or `Invalidate()` (size changed, so layout runs again)
- an animation is still running (`Animate()` keeps requesting frames until it settles)
- a timed redraw is due (`RequestFrameIn(seconds)`, e.g. the text caret blink)
- a task was posted with `Post()`

Games and overlays that render every frame anyway can just call `Frame()` and `Render()` every frame, or set `AppConfig::Continuous = true`.

## Input

Platform glue converts native events into calls on the context. All positions are in UI units:

```cpp
Ctx.OnMouseMove( { X, Y } );
Ctx.OnMouseButton( tg::MouseButton::Left, /*Down*/ true );   // Left, Right, Middle, Back, Forward
Ctx.OnMouseWheel( { 0, 1 } );                                 // +y = scroll up, in "lines"
Ctx.OnKey( tg::Key::Enter, /*Down*/ true, Mods, Repeat );
Ctx.OnText( Codepoint );                                      // typed characters, after keyboard layout / IME
```

How events travel:
- **Mouse** events go to the deepest `Interactive` widget under the cursor and bubble up through the parents until one returns `true` from `OnEvent`.
- While a mouse button is held, moves go to the **pressed** widget. That's how sliders and window drags keep working outside their bounds.
- **Keys** and text go to the **focused** widget. Clicking a `Focusable` widget, like a `TextInput` or `Keybind`, focuses it. Clicking anything else clears focus.

### Polling keys and global hotkeys

```cpp
Ctx.IsKeyDown( tg::Key::LeftShift );      // any keyboard key or Key::Mouse1..Mouse5
Ctx.KeyPressCount( tg::Key::F5 );         // number of presses since start (edge detection)

Ctx.OnKeyEvent = [&]( tg::Key K, bool Down ) {
    if ( Down && K == tg::Key::Insert ) { Menu.Visible ? Menu.Close() : Menu.Show(); return true; } // true = swallow
    return false;
};
```

`OnKeyEvent` sees every press before widgets do. It does not fire for keyboard keys while a text field has focus, so typing "i" in a chat box won't toggle your menu. It also doesn't fire while a `Keybind` is listening.

### Capture

A widget can claim all mouse-button and key input with `Ctx.Capture(this)` until it calls `ReleaseCapture(this)`. `Keybind` uses this while it waits for "press any key".

## Popups (the overlay layer)

Dropdowns, color pickers and the keybind mode menu are `tg::Popup`s on `Ctx.Overlay()`. They draw above everything and:
- close when you click outside them, unless the click is on the widget that opened them (that widget toggles them itself)
- close the newest one first when you press Escape
- flip above their anchor when there's no room below

```cpp
tg::Popup& P = tg::OpenPopup( *this, Bounds );   // anchored under this widget
P.Add<tg::ListItem>( "Rename" ).OnClick = [&] { /* ... */ P.Close(); };
P.OnClose = [&] { /* opener forgets its pointer */ };
```

## Tooltips and context menus

Every widget has two hooks that work without subclassing:

```cpp
Btn.Tooltip = "Builds and ships to every region";
Card.ContextMenu = []( tg::Menu& M ) { M.Item( "Copy", ... ); M.Separator(); M.Item( "Delete", ... ).Danger = true; };
```

For both, the context looks up the **deepest widget under the mouse**, even a non-interactive one like a Label, then walks up its parents to the first one that has the hook. Setting it on a row or card therefore covers everything inside.

## Modal dialogs and toasts

`tg::Confirm(...)` / `tg::OpenDialog(...)` and `tg::Toast(...)` add full-screen layers to `Ctx.Overlay()`. A dialog swallows input until it's answered. The toast layer is click-through except on the toasts themselves. See [Controls reference](widgets.md#feedback).

## Threads

**Only the UI thread may touch widgets.** Everything else posts:

```cpp
std::thread Net( [&] {
    for ( auto& Msg : Socket )
        Ctx.Post( [&, Msg] { Chat.AddMessage( Msg ); } );   // queued, thread-safe, wakes the UI
} );
```

`Post` is safe from any thread. The task runs at the start of the next `Frame()`. The platform's `WakeUp` hook (set by the GLFW and Win32 glue) interrupts the idle wait so it runs right away.

## Live textures (screen share, video, camera)

```cpp
tg::TextureId Tex = Ctx.GetRenderer().CreateTexture( W, H, nullptr );
auto& View = Panel.Add<tg::Image>( Tex, W, H );
// per decoded frame, on the UI thread (Post from your decoder thread):
Ctx.GetRenderer().UpdateTexture( Tex, W, H, RgbaPixels );
View.Redraw();
```

Pixels are tightly packed RGBA8. If the size changes, call `View.SetSource( Tex, NewW, NewH )` so layout updates.

## Units, DPI and fonts

- Layout and drawing use **UI units**; `Ctx.SetViewport( Size, Scale )` tells tg how many pixels one unit is.
- When the scale changes, the font atlas is re-rasterized at the new scale, so text stays crisp at 125%, 150% or 200%.
- `Ctx.LoadDefaultFonts( Scale )` loads the built-in Inter at four sizes and sets the theme fonts: `Body` 15, `Bold` 15, `Small` 12.5, `Title` 20.
- Use `Ctx.Fonts.AddFromFile( Path, Size )` / `AddFromMemory(...)` before the atlas is built to add your own fonts.

## Timing

| Call | Meaning |
|---|---|
| `Ctx.Time()` | seconds since the context started |
| `Ctx.DeltaTime()` | seconds since the previous frame (clamped to 0.1) |
| `Ctx.SetFixedTimestep( 1.f / 60 )` | every `Frame()` advances exactly this much: deterministic tests and recordings |
