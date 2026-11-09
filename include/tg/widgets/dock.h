// DockSpace — IDE-style docking: panels live as tabs in a tree of splits, can be dragged out to
// float, dropped on any side of another panel (or the edge of the whole space), resized with the
// dividers, closed and reopened, and the whole arrangement saved to a string and restored.
//
//   auto& Dock = Page.Add<tg::DockSpace>();
//   auto& Files = Dock.AddPanel( "Files", "files" );      // a Container: all the helpers work
//   auto& Code  = Dock.AddPanel( "Editor", "editor" );
//   auto& Log   = Dock.AddPanel( "Log", "log" );
//   Dock.Dock( Files, &Code, tg::DockSide::Left, 0.25f ); // Files left of the editor, 25% wide
//   Dock.Dock( Log, &Code, tg::DockSide::Bottom, 0.3f );
//   std::string Saved = Dock.SaveLayout();  ...  Dock.LoadLayout( Saved );
//
// Mouse: drag a tab to move it (past a few pixels it floats under the cursor; hover a panel to see
// where it will land — centre = add as a tab, an edge = split; the space's outer rim docks to that
// whole side). Drag a floating window's title to move it, its corner to resize, a divider to resize
// a split. Middle-click or the × closes a tab. Double-click a floating title to dock it back.
#pragma once
#include "container.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tg
{
    enum class DockSide : uint8_t { Center, Left, Right, Top, Bottom };

    struct DockStyle
    {
        float TabHeight = 28, TabPad = 12, Handle = 5, MinPane = 60, RimZone = 26;
        float FloatRadius = 8, Grip = 14;
        Paint TabBar = Tok::SurfaceAlt;
        Paint Tab = Tok::Surface;
        Paint ActiveTab = Tok::Raised;
        Paint Body = Tok::Surface;
        Paint Line = Tok::Border;
        Paint Text = Tok::TextDim;
        Paint ActiveText = Tok::Text;
        Paint Accent = Tok::Accent;
    };

    class DockPanel : public Container
    {
    public:
        DockPanel( std::string Title, std::string Id );
        std::string Title;
        std::string Id;           // stable name for Save/LoadLayout (no spaces or ( ) , | ; characters)
        bool Closable = true;
        std::function<void( DockPanel& )> OnClose; // after it was closed (Dock.Show( P ) brings it back)

    private:
        friend class DockSpace;
        bool Open = true, WasFloat = false;
        std::string ReturnTo; // where Show() puts it back
        DockSide ReturnSide = DockSide::Center;
        float ReturnRatio = 0.5f;
        Rect LastFloat;
    };

    class DockSpace : public Widget
    {
    public:
        DockSpace();
        ~DockSpace() override;

        DockStyle Style;

        // New panel, added as a tab next to the most recently added one (or the first leaf).
        DockPanel& AddPanel( std::string Title, std::string Id = {} );
        // Move P next to Target (nullptr = the whole space). Center = as a tab; a side = split, P taking Ratio.
        void Dock( DockPanel& P, DockPanel* Target, DockSide Side = DockSide::Center, float Ratio = 0.5f );
        void Float( DockPanel& P, Rect R );        // pop it out into a floating window at R (DockSpace coords)
        void Close( DockPanel& P );                // hide it (still owned; Show brings it back where it was)
        void Show( DockPanel& P );                 // reopen a closed panel (docked where it was, or as a tab)
        void Activate( DockPanel& P );             // bring its tab to the front
        bool IsOpen( const DockPanel& P ) const;
        bool IsFloating( const DockPanel& P ) const;
        DockPanel* Find( const std::string& Id ) const;
        std::vector<DockPanel*> Panels() const;
        Rect TabOf( const DockPanel& P ) const; // where its tab is (empty when closed)

        // "split(row,0.25,tabs(files|0),split(col,0.7,tabs(editor,preview|0),tabs(log|0)));float(40,40,300,200,tabs(props|0))"
        std::string SaveLayout() const;
        bool LoadLayout( const std::string& Layout ); // unknown ids are skipped; panels not mentioned get closed

        std::function<void()> OnLayoutChange; // after any user drag / drop / resize / close

        Vec2 Measure( float AvailWidth ) override;
        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;
        Cursor GetCursor() const override;

        struct Node;

    private:
        std::unique_ptr<Node> Root;
        std::vector<std::unique_ptr<Node>> Floats; // leaves with their own rect; last = on top
        DockPanel* LastAdded = nullptr;

        enum class Act : uint8_t { None, Tab, MoveFloat, ResizeFloat, Split };
        Act Doing = Act::None;
        Node* ActNode = nullptr;
        DockPanel* ActPanel = nullptr;
        Vec2 PressAt, GrabOff;
        bool Moved = false;
        Vec2 Mouse;
        bool HasDrop = false;
        Node* DropLeaf = nullptr; // nullptr + HasDrop = the outer rim
        DockSide DropSide = DockSide::Center;
        Rect DropPreview;
        float PreviewT = 0;
        Rect PreviewAnim;

        Node* LeafOf( const DockPanel* P, Node** FloatRoot = nullptr ) const;
        void Unlink( DockPanel* P );             // take P out of whatever leaf holds it (keeps it as a child)
        void Prune( Node* Leaf );                // remove an empty docked leaf, collapsing its split
        Node* FirstLeaf( Node* N ) const;
        void Place( Node* N, Rect R );
        void DrawLeaf( DrawList& D, Node* N, bool Floating );
        Rect TabRect( Node* N, size_t i ) const;
        Rect CloseRect( Node* N, size_t i ) const;
        bool PickDrop( Vec2 P );
        void Changed();
        friend struct DockLayoutIO;
    };
}
