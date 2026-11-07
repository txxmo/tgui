# Writing widgets

A widget is a class derived from `tg::Widget` that overrides the parts it needs. Most custom widgets are 20–60 lines.

## The smallest widget

```cpp
#include "tg/tg.h"

class Dot : public tg::Widget
{
public:
    explicit Dot( tg::Paint C, float Size = 8 ) : C( C )
    {
        Layout.Width = tg::Px( Size );
        Layout.Height = tg::Px( Size );
    }
    void Draw( tg::DrawList& D ) override
    {
        D.CircleFilled( Bounds.Center(), Bounds.w * 0.5f, C.Resolve() );
    }
    tg::Paint C;
};

Row.Add<Dot>( tg::Tok::Success );
```

## Override points

| Method | Override when | Default |
|---|---|---|
| `Vec2 Measure( float AvailWidth )` | your size depends on content (text, images) | measures children + padding |
| `void Arrange( Rect R )` | you position children yourself | sets `Bounds = R`, flex-lays out children |
| `void Draw( DrawList& D )` | always, unless you're a pure container | draws children |
| `bool OnEvent( const Event& E )` | you react to input; return `true` if handled | `false` (bubbles to the parent) |
| `Widget* HitTest( Vec2 P )` | you clip or overlap children (scroll areas, windows) | deepest `Interactive` child under P |
| `Cursor GetCursor() const` | you want a hand / I-beam / resize cursor | `Arrow` |

Flags to set in the constructor:
- `Interactive = true`: gets hover, press and click. Without it, mouse input falls through to the parent.
- `Focusable = true`: clicking focuses it, so it receives keys and text.
- `RaiseOnClick = true`: comes to the front of its siblings when clicked (windows).

## State helpers

```cpp
IsHovered(); IsPressed(); IsFocused();
Redraw();       // my look changed
Invalidate();   // my size changed: relayout + redraw
GetContext();   // the tg::Context this widget belongs to (nullptr until it's in a tree)
```

## Animation

```cpp
float HoverT = 0;   // member

void Draw( tg::DrawList& D ) override
{
    Animate( HoverT, IsHovered() ? 1.f : 0.f );      // eases toward the target, keeps requesting frames until settled
    tg::Color Fill = tg::Color::Lerp( tg::GetTheme()[ tg::Tok::Raised ], tg::GetTheme()[ tg::Tok::Accent ], HoverT );
    D.RectFilled( Bounds, Fill, 6 );
}
```

`Animate( Value, Target, Speed = 16 )`: a higher speed is snappier. For timed effects like a blinking caret or a pulse, call `GetContext()->RequestFrameIn( Seconds )` instead of redrawing every frame.

## Events

```cpp
bool OnEvent( const tg::Event& E ) override
{
    switch ( E.Type )
    {
    case tg::EventType::MouseDown: return E.Button == tg::MouseButton::Left;   // claim the press
    case tg::EventType::MouseMove: if ( IsPressed() ) { Drag( E.Pos ); return true; } return false;
    case tg::EventType::MouseUp:   if ( IsHovered() ) Click(); return true;    // click = release over me
    case tg::EventType::KeyDown:   return E.KeyCode == tg::Key::Enter && ( Activate(), true );
    default: return false;
    }
}
```

| Event type | Fields |
|---|---|
| `MouseMove`, `MouseDown`, `MouseUp` | `Pos`, `Button` |
| `MouseWheel` | `Pos`, `Wheel` (+y = up) |
| `KeyDown`, `KeyUp` | `KeyCode`, `Mods` (`ModShift`/`ModCtrl`/`ModAlt`/`ModSuper`), `Repeat` |
| `Text` | `Codepoint` |
| `MouseEnter`, `MouseLeave`, `FocusIn`, `FocusOut` | (sent by the context) |

Return `true` to stop the event bubbling to parents.

If a click handler might delete the widget it's running in, for example removing a list row from that row's own button, copy the callback before calling it: `auto Fn = OnClick; Fn();`. The built-in widgets already do this.

## Layout hooks

A widget with content-dependent size:

```cpp
tg::Vec2 Measure( float AvailWidth ) override
{
    const tg::Font& F = *tg::GetTheme().Body;
    tg::Vec2 S = F.Measure( Text, /*WrapWidth*/ AvailWidth );
    return { S.x + 16, S.y + 8 };
}
```

A container that places its children itself:

```cpp
void Arrange( tg::Rect R ) override
{
    Bounds = R;
    // ArrangeChildren( Inner ) runs the standard flex layout inside any rect,
    // MeasureChildren( W ) measures the children as the flex layout would.
    ArrangeChildren( R.Shrink( Layout.Padding ) );
}
```

When measuring children, always use `Child.MeasureCached( Width )`, not `Measure`, so results are cached for the layout pass.

## DrawList API

All coordinates are UI units. Fills and strokes are anti-aliased.

| Call | Draws |
|---|---|
| `RectFilled( Rect, Color, Radius = 0, Corners = CornersAll )` | (rounded) rectangle; `Corners` picks which corners are rounded |
| `RectStroke( Rect, Color, Radius = 0, Thickness = 1 )` | outline, kept inside the rect |
| `RectGradient( Rect, Top, Bottom, Radius = 0 )` | vertical gradient |
| `RectMultiColor( Rect, TL, TR, BR, BL )` | a color per corner |
| `CircleFilled( Center, Radius, Color )`, `CircleStroke( ... , Thickness )` | |
| `Line( A, B, Color, Thickness )`, `Polyline( Pts, N, Color, Thickness, Closed )` | mitered, with round joins on sharp angles |
| `ConvexFill( Pts, N, Color )` | any convex polygon, clockwise on screen |
| `AreaFill( Pts, N, BaseY, Top, Bottom )` | area under a line (charts) |
| `Shadow( Rect, Radius, Color, Spread, Offset )` | soft drop shadow |
| `Image( Tex, Rect, UV0, UV1, Tint, Radius )` | texture, optionally rounded |
| `Text( Font, Pos, Utf8, Color, WrapWidth = 0 )` | returns the size it covered |
| `PushClip( Rect )`, `PopClip()`, `CurrentClip()` | scissor (nests by intersecting) |
| `FadeFrom( VtxStart, Alpha )` | fade everything drawn since `D.Vtx.size()` was `VtxStart` |

Fonts: `tg::GetTheme().Body / Bold / Small / Title`, plus `Font::Measure( Text, Wrap )`, `Font::Width( Text )`, `Font::LineHeight`, `Font::HitTest( Text, X )` (byte offset nearest X).

## Popups

```cpp
void OpenMenu()
{
    tg::Popup& P = tg::OpenPopup( *this, Bounds );     // under me; flips above if needed
    P.MinWidth = 160;
    Menu = &P;
    P.OnClose = [ this ] { Menu = nullptr; };          // always clear your pointer here
    for ( const char* Item : { "Rename", "Duplicate", "Delete" } )
    {
        auto& Row = P.Add<tg::ListItem>( Item );
        Row.OnClick = [ this, Item ] { Run( Item ); if ( Menu ) Menu->Close(); };
    }
}

~MyWidget() override { if ( Menu ) { Menu->OnClose = nullptr; Menu->Close(); } }
```

The context closes popups on an outside click or Escape. Closing removes the popup at the start of the next frame, so it's safe from inside the popup's own handlers.

To build a new dropdown-style picker, derive from `tg::DropdownBase` (see `combo.h`) and implement `DisplayText`, `IsItemSelected` and `OnItemClicked`.

## Input capture

To grab every key and button for a moment, call `GetContext()->Capture( this )` and later `ReleaseCapture( this )`. `Keybind` does this while listening.

## Adding a widget to the library

1. Create `include/tg/widgets/mywidget.h` and `src/widgets/mywidget.cpp`.
2. Add the `.cpp` to the `tg` target in `CMakeLists.txt`, and the header to `include/tg/tg.h`.
3. Optionally add a one-liner to `Container` (`container.h` / `container.cpp`).
4. Add a check to `examples/headless/main.cpp` so it's tested.
