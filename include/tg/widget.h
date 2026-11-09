// tg/widget.h — the base class every control derives from, plus the layout model.
//
// Retained mode: you build a tree once (like your old Window/Groupbox/Control setup), keep
// pointers/references to the widgets you care about, and change them when data changes.
// The tree is re-laid-out only when something calls Invalidate(), and redrawn only when
// something changed or an animation is running — so an idle dashboard uses ~0% CPU.
//
// Layout is a small flexbox:
//   Direction  Column (default) stacks children top->bottom, Row left->right, Stack on top of each other,
//              Free = each child sits at its own Layout.Offset (floating windows, popups).
//   Width/Height per child:  Fit() = as big as content, Px(n) = fixed, Grow(w) = share leftover space.
//   Padding, Gap, MainAlign (along Direction), CrossAlign (the other axis).
#pragma once
#include "core.h"
#include "input.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tg
{
    class Context;
    class DrawList;
    class Menu;
    class Container;

    // The value a key bind puts on a control (see tg/binds.h). Checkbox: {1} / {0}. Slider: {75}.
    // Combo: {index}. MultiCombo: one entry per item, NaN = leave that item alone. Color: {r, g, b, a}.
    struct BindValue
    {
        std::vector<double> V;
    };

    // What's being dragged (see Widget::OnDragStart). Type lets targets pick what they accept.
    struct DragData
    {
        std::string Type;           // "server", "file", "color", ... your choice
        std::string Text;           // payload as text (names, ids, paths)
        std::string Label;          // shown next to the cursor while dragging (default: Text)
        std::shared_ptr<void> Data; // anything else
        std::vector<std::string> Paths; // Type "files": files dragged in from the OS (Explorer, Finder, ...)
    };

    // Column / Row: flex lines. Stack: children on top of each other. Free: absolute (Offset).
    // Grid: cells in Columns (or as many ColumnMin-wide columns as fit), ColSpan / RowSpan on children.
    // Wrap: children left to right, wrapping onto new lines like words.
    enum class Dir : uint8_t { Column, Row, Stack, Free, Grid, Wrap };
    enum class Align : uint8_t { Start, Center, End, Stretch };

    struct Size
    {
        enum class Kind : uint8_t { Fit, Fixed, Grow } K = Kind::Fit;
        float V = 0;
    };
    inline Size Fit() { return { Size::Kind::Fit, 0 }; }
    inline Size Px( float v ) { return { Size::Kind::Fixed, v }; }
    inline Size Grow( float Weight = 1 ) { return { Size::Kind::Grow, Weight }; }

    struct LayoutSpec
    {
        Dir Direction = Dir::Column;
        Size Width, Height;
        Insets Padding;
        float Gap = 0;
        Align MainAlign = Align::Start;
        Align CrossAlign = Align::Stretch;
        float MinWidth = 0, MinHeight = 0;
        float MaxWidth = 0, MaxHeight = 0; // 0 = no limit
        float Aspect = 0;                  // width / height: the height follows the width (16:9 = 1.777)
        Vec2 Offset; // position inside a Dir::Free parent

        // Grid
        int Columns = 2;                   // fixed column count...
        float ColumnMin = 0;               // ...or, when > 0, as many columns at least this wide as fit (responsive)
        float RowGap = -1;                 // Grid / Wrap line spacing; -1 = Gap
        int ColSpan = 1, RowSpan = 1;      // on a child of a Grid

        // Responsive: a Row narrower than this lays its children out as a Column (cards side by side
        // on a wide window, stacked on a narrow one). 0 = never.
        float StackBelow = 0;
        // Children glide to their new place when the layout changes (reorder, show / hide, resize).
        bool AnimateChildren = false;
    };

    class Widget
    {
    public:
        Widget() = default;
        virtual ~Widget();
        Widget( const Widget& ) = delete;
        Widget& operator=( const Widget& ) = delete;

        // ---- tree ----
        // auto& Btn = Panel.Add<tg::Button>( "Save" );   (the parent owns it)
        template<typename T, typename... Args>
        T& Add( Args&&... A )
        {
            auto P = std::make_unique<T>( std::forward<Args>( A )... );
            T& Ref = *P;
            AddChild( std::move( P ) );
            return Ref;
        }
        Widget& AddChild( std::unique_ptr<Widget> W );
        void Remove( Widget* W );
        void Clear();
        void BringToFront(); // draw last among siblings (and get hit first)
        void MoveChild( size_t From, size_t To ); // reorder children (relayouts; AnimateChildren makes them glide)
        // Take a child out without destroying it (to AddChild it somewhere else, even in another window).
        std::unique_ptr<Widget> Detach( Widget* Child );
        Widget* GetParent() const { return Parent; }
        const std::vector<std::unique_ptr<Widget>>& Children() const { return Kids; }

        // ---- public state ----
        LayoutSpec Layout;
        Rect Bounds;           // where layout put us, in UI units
        Vec2 Slide;          // AnimateChildren: drawn offset easing back to 0
        float LastResizeW = -1;
        bool Visible = true;
        bool Enabled = true;
        bool Interactive = false; // gets hover/press/click (otherwise mouse falls through to the parent)
        bool Focusable = false;   // can take keyboard focus
        bool RaiseOnClick = false; // move to the top of its parent when clicked (windows)

        // Hover text, shown after a short delay. Works on any widget (labels too).
        std::string Tooltip;
        // Right-click menu. Fill the menu when it opens:  W.ContextMenu = []( tg::Menu& M ) { M.Item( "Copy", ... ); };
        // The nearest ancestor with a ContextMenu answers, so set it on a row/card to cover its children.
        std::function<void( Menu& )> ContextMenu;

        // ---- override points ----
        // Natural size (including Layout.Padding) when given at most AvailWidth. Used for Fit().
        virtual Vec2 Measure( float AvailWidth );
        // Take Bounds = R and position children. Default = flex layout described above.
        virtual void Arrange( Rect R );
        // Draw yourself. Default draws children.
        virtual void Draw( DrawList& D );
        // Return true if handled. Unhandled mouse/key events bubble to the parent.
        virtual bool OnEvent( const Event& E );
        // Deepest Interactive widget under P (or nullptr).
        virtual Widget* HitTest( Vec2 P );
        virtual Cursor GetCursor() const { return Cursor::Arrow; }
        // Enter / Space on the focused widget (or Enter on a highlighted popup item) clicks it.
        bool KeyActivates = false;
        // True while focused for typing (text fields, terminal): global hotkeys and key binds stand back.
        virtual bool WantsKeyboard() const { return false; }
        // Text widgets: where the caret is (UI units). IME candidate windows and composition text go here.
        virtual Rect CaretRect() const { return {}; }

        // ---- key binds: right-click a control -> "Add key bind". Controls opt in by overriding these.
        virtual bool CanBind() const { return false; }
        virtual BindValue BindGet() const { return {}; }               // current value
        virtual BindValue BindDefault() const { return BindGet(); }    // what a new bind starts with in the editor
        virtual void BindSet( const BindValue& V ) { (void)V; }       // apply (fires OnChange like a user edit)
        virtual void BindEditor( Container& Into, BindValue& V );     // controls to pick the bound value
        virtual std::string BindText( const BindValue& V ) const;     // "on", "75", "High" (for menus)
        virtual std::string BindName() const;                         // its label; default: the label next to it
        virtual bool BindIsAction() const { return false; }           // buttons: a key press clicks it
        bool Bindable = true;                                         // false: no bind menu on this control

        // Called by the parent's layout when this widget's width changes (responsive layouts).
        std::function<void( Widget& Self, float Width )> OnResize;

        // ---- drag and drop between any widgets
        //   Card.OnDragStart = [&]( tg::DragData& D ) { D.Type = "server"; D.Text = Name; return true; };
        //   Bin.AcceptDrop = []( const tg::DragData& D ) { return D.Type == "server"; };
        //   Bin.OnDrop = [&]( const tg::DragData& D, tg::Vec2 At ) { Remove( D.Text ); };
        std::function<bool( DragData& )> OnDragStart;               // fill it in; false = don't drag
        std::function<bool( const DragData& )> AcceptDrop;
        std::function<void( const DragData&, Vec2 At )> OnDrop;

        // Cached Measure(): call this (not Measure) when laying out children.
        Vec2 MeasureCached( float AvailWidth );
        Vec2 MeasureFlow( const std::vector<Widget*>& Visible, float InnerW, const Rect* Place ); // Grid / Wrap

        // ---- helpers ----
        bool IsHovered() const;
        bool IsPressed() const;
        bool IsFocused() const;
        void Invalidate();   // needs re-layout + redraw (size may have changed)
        void Redraw();       // needs redraw only
        Context* GetContext() const;

    protected:
        void DrawChildren( DrawList& D );
        void ArrangeChildren( Rect Inner ); // the flex algorithm, on the area inside padding
        void ArrangeChildrenImpl( Rect Inner );
        Vec2 MeasureChildren( float AvailInnerWidth );

        // Exponential ease toward Target. Keeps requesting frames until it settles.
        //   Animate( HoverT, IsHovered() ? 1.f : 0.f );
        float Animate( float& Value, float Target, float Speed = 16.f );

    private:
        friend class Context;
        Widget* Parent = nullptr;
        Context* Ctx = nullptr; // only set on roots
        std::vector<std::unique_ptr<Widget>> Kids;

        // Measure cache, valid for one layout generation (bumped on every full relayout).
        struct CacheEntry { float Avail = -1; Vec2 Size; };
        CacheEntry Cache[ 2 ];
        uint32_t CacheGen = 0;
        static inline uint32_t LayoutGeneration = 1;
    };

    // Call Fn( Wide ) whenever W's width crosses Breakpoint (and once at the first layout).
    //   tg::Responsive( Page, 760, [&]( bool Wide ) { Sidebar->Visible = Wide; } );
    void Responsive( Widget& W, float Breakpoint, std::function<void( bool Wide )> Fn );
}
