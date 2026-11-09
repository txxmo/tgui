#include "tg/context.h"
#include "tg/renderer.h"
#include "tg/theme.h"
#include "tg/variants.h"
#include "tg/widgets/dialog.h"
#include "tg/widgets/menu.h"
#include "tg/widgets/popup.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>

namespace tg
{
    struct Context::PostQueue
    {
        std::mutex M;
        std::vector<std::function<void()>> Q;
    };

    namespace fonts
    {
        extern const unsigned char InterRegular[], InterMedium[], InterSemiBold[], MonoRegular[], MonoBold[], Icons[];
        extern const size_t InterRegularSize, InterMediumSize, InterSemiBoldSize, MonoRegularSize, MonoBoldSize, IconsSize;
    }

    // Overlay layer: popups are placed under their anchor (or above when there's no room),
    // anything else by Layout.Offset.
    class OverlayLayer : public Widget
    {
    public:
        void Arrange( Rect R ) override
        {
            Bounds = R;
            for ( auto& K : Children() )
            {
                if ( !K->Visible ) continue;
                // Full-screen layers (dialog backdrops, the toast area) fill the whole viewport.
                if ( K->Layout.Width.K == Size::Kind::Grow && K->Layout.Height.K == Size::Kind::Grow ) { K->Arrange( R ); continue; }
                auto* P = dynamic_cast<Popup*>( K.get() );
                if ( !P ) { Vec2 S = K->MeasureCached( R.w ); K->Arrange( { K->Layout.Offset, S } ); continue; }
                float W = P->Layout.Width.K == Size::Kind::Fixed ? P->Layout.Width.V : std::max( P->MinWidth, P->MeasureCached( R.w ).x );
                W = std::min( W, R.w - 8 );
                float H = std::min( P->MeasureCached( W ).y, R.h - 8 );
                float X = std::clamp( P->Anchor.x, 4.f, std::max( 4.f, R.w - W - 4 ) );
                float Y = P->Anchor.Bottom() + 4;
                if ( Y + H > R.h - 4 && P->Anchor.y - 4 - H >= 4 ) Y = P->Anchor.y - 4 - H; // flip above
                Y = std::clamp( Y, 4.f, std::max( 4.f, R.h - H - 4 ) );
                P->Arrange( { X, Y, W, H } );
            }
        }
    };

    double Context::Clock() const
    {
        using namespace std::chrono;
        return duration<double>( steady_clock::now().time_since_epoch() ).count() - Start;
    }

    Context::Context( Renderer& Rend ) : R( Rend ), RootWidget( std::make_unique<Widget>() ), OverlayWidget( std::make_unique<OverlayLayer>() ), Posts( std::make_unique<PostQueue>() )
    {
        RegisterBuiltinVariants();
        Start = 0;
        Start = Clock();
        RootWidget->Ctx = this;
        OverlayWidget->Ctx = this;
        Fonts.RequestFrame = [ this ] { FrameRequested = true; };
    }

    Context::~Context()
    {
        OverlayWidget.reset(); // popups first: they notify their owners, which live in the root
        RootWidget.reset();
        if ( Fonts.Texture ) R.DestroyTexture( Fonts.Texture );
    }

    // Mono fonts (embedded) + the system emoji font (FreeType builds) for every font setup.
    static void AddCommonFonts( FontAtlas& Fonts, Theme& T )
    {
        auto MonoR = [] { return std::vector<unsigned char>( fonts::MonoRegular, fonts::MonoRegular + fonts::MonoRegularSize ); };
        auto MonoB = [] { return std::vector<unsigned char>( fonts::MonoBold, fonts::MonoBold + fonts::MonoBoldSize ); };
        T.Mono = Fonts.AddFromMemory( MonoR(), 14 );
        T.MonoBold = Fonts.AddFromMemory( MonoB(), 14 );
        T.MonoItalic = Fonts.AddOblique( T.Mono, 0.18f );
        T.Italic = Fonts.AddOblique( T.Body );       // synthetic italics: slanted upright faces
        T.BoldItalic = Fonts.AddOblique( T.Bold );
        Fonts.AddIconFont( std::vector<unsigned char>( fonts::Icons, fonts::Icons + fonts::IconsSize ) ); // tg::Icon::* in any text
        Fonts.AddEmojiFont(); // no-op without FreeType
        Fonts.AddCjkFont();   // read on first use
    }

    bool Context::LoadDefaultFonts( float S )
    {
        Theme& T = GetTheme();
        auto Reg = [] { return std::vector<unsigned char>( fonts::InterRegular, fonts::InterRegular + fonts::InterRegularSize ); };
        auto Med = [] { return std::vector<unsigned char>( fonts::InterMedium, fonts::InterMedium + fonts::InterMediumSize ); };
        auto Semi = [] { return std::vector<unsigned char>( fonts::InterSemiBold, fonts::InterSemiBold + fonts::InterSemiBoldSize ); };
        T.Body = Fonts.AddFromMemory( Reg(), 15 );
        T.Medium = Fonts.AddFromMemory( Med(), 15 );
        T.Small = Fonts.AddFromMemory( Med(), 12.5f ); // Medium: small text thins out without hinting
        T.Bold = Fonts.AddFromMemory( Semi(), 15 );
        T.Title = Fonts.AddFromMemory( Semi(), 20 );
        AddCommonFonts( Fonts, T );
        Scale = S;
        return Fonts.Build( R, S );
    }

    bool Context::LoadFonts( const std::string& Regular, const std::string& Bold, float S, const std::string& Medium )
    {
        Theme& T = GetTheme();
        const std::string& Mid = Medium.empty() ? Regular : Medium;
        T.Body = Fonts.AddFromFile( Regular, 15 );
        T.Medium = Fonts.AddFromFile( Mid, 15 );
        T.Small = Fonts.AddFromFile( Mid, 12.5f );
        T.Bold = Fonts.AddFromFile( Bold, 15 );
        T.Title = Fonts.AddFromFile( Bold, 20 );
        if ( !T.Body || !T.Medium || !T.Small || !T.Bold || !T.Title ) return false;
        AddCommonFonts( Fonts, T );
        Scale = S;
        return Fonts.Build( R, S );
    }

    bool Context::LoadDefaultFonts( const std::string& Dir, float S )
    {
        Theme& T = GetTheme();
        T.Body = Fonts.AddFromFile( Dir + "/Inter-Regular.otf", 15 );
        T.Medium = Fonts.AddFromFile( Dir + "/Inter-Medium.otf", 15 );
        T.Small = Fonts.AddFromFile( Dir + "/Inter-Medium.otf", 12.5f );
        T.Bold = Fonts.AddFromFile( Dir + "/Inter-SemiBold.otf", 15 );
        T.Title = Fonts.AddFromFile( Dir + "/Inter-SemiBold.otf", 20 );
        if ( !T.Body || !T.Medium || !T.Small || !T.Bold || !T.Title ) return false;
        AddCommonFonts( Fonts, T );
        Scale = S;
        return Fonts.Build( R, S );
    }

    // ------------------------------------------------------------ viewport

    void Context::SetViewport( Vec2 Size, float S )
    {
        if ( S != Scale && Fonts.Texture )
            Fonts.Build( R, S ); // re-rasterize for the new DPI
        if ( Size == Viewport && S == Scale ) return;
        Viewport = Size;
        Scale = S;
        InvalidateLayout();
    }

    // ------------------------------------------------------------ input

    bool Context::Dispatch( Widget* T, const Event& E )
    {
        for ( Widget* W = T; W; W = W->GetParent() )
        {
            if ( W->Enabled && W->OnEvent( E ) )
            {
                RequestFrame();
                return true;
            }
        }
        return false;
    }

    Widget* Context::HitTestAll( Vec2 P )
    {
        if ( Widget* H = OverlayWidget->HitTest( P ) ) return H;
        return RootWidget->HitTest( P );
    }

    bool Context::HasPopups() const
    {
        for ( auto& K : OverlayWidget->Children() )
            if ( auto* P = dynamic_cast<Popup*>( K.get() ); P && !P->IsClosed() ) return true;
        return false;
    }

    void Context::ClosePopups()
    {
        for ( auto& K : OverlayWidget->Children() )
            if ( auto* P = dynamic_cast<Popup*>( K.get() ) ) P->Close();
    }

    static Widget* DeepestIn( Widget* W, Vec2 P )
    {
        if ( !W->Visible || !W->Bounds.Contains( P ) ) return nullptr;
        for ( auto It = W->Children().rbegin(); It != W->Children().rend(); ++It )
            if ( Widget* D = DeepestIn( It->get(), P ) ) return D;
        return W;
    }

    Widget* Context::DeepestAt( Vec2 P )
    {
        // Prefer what the hit test says (respects scroll clipping), then dig into non-interactive children.
        Widget* Start = HitTestAll( P );
        if ( Start )
            if ( Widget* D = DeepestIn( Start, P ) ) return D;
        if ( Widget* D = DeepestIn( OverlayWidget.get(), P ); D && D != OverlayWidget.get() ) return D;
        return DeepestIn( RootWidget.get(), P );
    }

    void Context::UpdateTooltip()
    {
        Widget* T = nullptr;
        for ( Widget* W = DeepestAt( Mouse ); W; W = W->GetParent() )
            if ( !W->Tooltip.empty() ) { T = W; break; }
        if ( T == TipW ) return;
        bool WasShown = TipW && !TipSuppressed && Now - TipSince >= TooltipDelay;
        TipW = T;
        TipSince = FixedDt > 0 ? Now : Clock();
        TipSuppressed = false;
        if ( T ) RequestFrameIn( TooltipDelay + 0.01 );
        if ( WasShown ) RequestFrame();
    }

    void Context::DrawTooltip()
    {
        if ( !TipW || TipSuppressed || PressW || Now - TipSince < TooltipDelay ) return;
        const Font* F = GetTheme().Small;
        if ( !F ) return;
        const float MaxW = 280, Pad = 8;
        Vec2 S = F->Measure( TipW->Tooltip, MaxW );
        Rect R = { Mouse.x + 12, Mouse.y + 18, S.x + Pad * 2, S.y + Pad * 1.5f };
        if ( R.Right() > Viewport.x - 4 ) R.x = Viewport.x - 4 - R.w;
        if ( R.Bottom() > Viewport.y - 4 ) R.y = Mouse.y - 8 - R.h;
        R.x = std::max( 4.f, R.x );
        R.y = std::max( 4.f, R.y );
        const Theme& T = GetTheme();
        List.Shadow( R, 6, Color( 0, 0, 0, 100 ), 8, { 0, 3 } );
        List.RectFilled( R, T[ Tok::Raised ], 6 );
        List.RectStroke( R, T[ Tok::BorderStrong ], 6 );
        List.Text( *F, { R.x + Pad, R.y + Pad * 0.75f }, TipW->Tooltip, T[ Tok::Text ], MaxW );
    }

    bool Context::OpenContextMenu()
    {
        for ( Widget* W = DeepestAt( Mouse ); W; W = W->GetParent() )
        {
            bool Bind = Binds.Enabled && W->Bindable && W->CanBind();
            if ( W->ContextMenu || Bind )
            {
                ClosePopups(); // one menu at a time
                Menu& M = OpenMenu( *W, Mouse );
                M.OwnerToggles = false; // the owner can be a whole tree/table: clicking it elsewhere must close the menu
                if ( W->ContextMenu ) W->ContextMenu( M );
                if ( Bind ) Binds.AddMenuItems( *W, M ); // "Add key bind..." on every bindable control
                return true;
            }
        }
        return false;
    }

    void Context::UpdateHover()
    {
        Widget* H = HitTestAll( Mouse );
        if ( H == HoverW ) return;
        Widget* Old = HoverW;
        HoverW = H;
        Event E{};
        E.Pos = Mouse;
        if ( Old ) { E.Type = EventType::MouseLeave; Old->OnEvent( E ); }
        if ( H ) { E.Type = EventType::MouseEnter; H->OnEvent( E ); }
        RequestFrame();
    }

    uint8_t Context::Mods() const
    {
        auto K = [ this ]( Key A, Key B ) { return IsKeyDown( A ) || IsKeyDown( B ); };
        return uint8_t( ( K( Key::LeftShift, Key::RightShift ) ? ModShift : 0 ) | ( K( Key::LeftCtrl, Key::RightCtrl ) ? ModCtrl : 0 ) |
                        ( K( Key::LeftAlt, Key::RightAlt ) ? ModAlt : 0 ) | ( K( Key::LeftSuper, Key::RightSuper ) ? ModSuper : 0 ) );
    }

    void Context::OnMouseMove( Vec2 P )
    {
        if ( KeyHotW && P != Mouse ) { KeyHotW = nullptr; RequestFrame(); } // the mouse takes over from arrow keys
        Mouse = P;
        UpdateHover();
        // drag and drop: start once the press moves far enough from a draggable widget
        if ( !DragOn && PressW && IsKeyDown( Key::Mouse1 ) )
        {
            Vec2 Dv = P - PressAt;
            if ( Dv.x * Dv.x + Dv.y * Dv.y > DragThreshold * DragThreshold )
                for ( Widget* W = PressW; W; W = W->GetParent() )
                    if ( W->OnDragStart )
                    {
                        DragData D;
                        if ( W->OnDragStart( D ) )
                        {
                            if ( D.Label.empty() ) D.Label = D.Text;
                            DragOn = true;
                            DragNow = std::move( D );
                            DragSrc = W;
                            PressW = nullptr; // the press became a drag: no click
                        }
                        break;
                    }
        }
        if ( DragOn )
        {
            DropW = nullptr;
            for ( Widget* W = DeepestAt( Mouse ); W; W = W->GetParent() ) // targets needn't be interactive (a panel, a card)
                if ( W->AcceptDrop && W->OnDrop && W->AcceptDrop( DragNow ) ) { DropW = W; break; }
            RequestFrame();
            return;
        }
        Event E; E.Type = EventType::MouseMove;
        E.Pos = P;
        E.Mods = Mods();
        Dispatch( PressW ? PressW : HoverW, E );
        UpdateTooltip(); // after the move: widgets may set per-part tooltips (chart bars) while handling it
    }

    bool Context::TrackKey( Key K, bool Down )
    {
        size_t i = size_t( K );
        if ( i >= size_t( Key::Count ) ) return false;
        if ( Down && !KeyState[ i ] ) PressCount[ i ]++;
        KeyState[ i ] = Down;
        if ( CaptureW ) return false; // a Keybind is listening: it gets the key, nothing fires
        bool IsMouse = K >= Key::Mouse1 && K <= Key::Mouse5;
        // typing in a text field / terminal: letters are text, not hotkeys. Key releases always go through
        // so a Hold bind never gets stuck because focus moved while the key was down.
        if ( !IsMouse && Down && FocusW && FocusW->WantsKeyboard() ) return false;
        Binds.OnKey( K, Down );
        if ( !OnKeyEvent ) return false;
        return OnKeyEvent( K, Down );
    }

    void Context::OnMouseButton( MouseButton B, bool Down )
    {
        UpdateHover();
        Event E; E.Type = Down ? EventType::MouseDown : EventType::MouseUp;
        E.Pos = Mouse;
        E.Button = B;
        E.Mods = Mods();
        if ( Down )
        {
            double T = FixedDt > 0 ? Now : Clock(); // fixed timestep (tests): frame time, deterministic
            Vec2 D = Mouse - LastClickPos;
            bool Again = B == LastClickButton && T - LastClickTime <= DoubleClickTime && D.x * D.x + D.y * D.y <= 16;
            ClickCount = Again ? uint8_t( ClickCount % 3 + 1 ) : 1; // 1, 2, 3, then back to 1
            LastClickTime = T;
            LastClickPos = Mouse;
            LastClickButton = B;
            E.Clicks = ClickCount;
        }
        bool Swallowed = TrackKey( MouseKey( B ), Down );
        if ( CaptureW ) // a widget is listening for "any button": it gets this one, nothing else does
        {
            CaptureW->OnEvent( E );
            RequestFrame();
            return;
        }
        if ( Swallowed ) return;
        if ( Down && TipW ) { TipSuppressed = true; RequestFrame(); } // clicking hides the tooltip until you move elsewhere
        if ( Down ) FocusRing = false; // the ring is for keyboard users
        if ( Down && B == MouseButton::Left ) PressAt = Mouse;
        if ( DragOn && B == MouseButton::Left && !Down )
        {
            Widget* T = DropW;
            DragData D = std::move( DragNow );
            CancelDrag();
            if ( T && T->OnDrop ) T->OnDrop( D, Mouse );
            History.End();
            return;
        }
        if ( B == MouseButton::Left && Down )
        {
            // undo: snapshot the control under the mouse and the owners of open dropdowns / pickers
            std::vector<Widget*> Cand;
            for ( Widget* W = HoverW; W; W = W->GetParent() )
                if ( W->CanBind() && !W->BindIsAction() ) { Cand.push_back( W ); break; }
            for ( auto& K : OverlayWidget->Children() )
                if ( auto* P = dynamic_cast<Popup*>( K.get() ); P && !P->IsClosed() && P->Owner && P->Owner->CanBind() ) Cand.push_back( P->Owner );
            History.Begin( Cand );
        }
        if ( B == MouseButton::Back || B == MouseButton::Forward ) { Dispatch( HoverW, E ); return; }
        if ( Down )
        {
            // Click outside a popup closes it (unless it's on the popup's owner, which toggles it itself).
            for ( auto& K : OverlayWidget->Children() )
            {
                auto* P = dynamic_cast<Popup*>( K.get() );
                if ( !P || P->IsClosed() ) continue;
                bool Inside = false;
                for ( Widget* W = HoverW; W; W = W->GetParent() )
                    if ( W == P || ( W == P->Owner && P->OwnerToggles ) ) { Inside = true; break; }
                if ( !Inside ) P->Close();
            }
            for ( Widget* W = HoverW; W; W = W->GetParent() )
                if ( W->RaiseOnClick ) W->BringToFront();

            if ( B == MouseButton::Left )
            {
                PressW = HoverW && HoverW->Enabled ? HoverW : nullptr;
                Widget* F = PressW;
                while ( F && !F->Focusable ) F = F->GetParent();
                SetFocus( F );
            }
            Dispatch( HoverW, E );
        }
        else
        {
            bool Handled = Dispatch( B == MouseButton::Left && PressW ? PressW : HoverW, E );
            if ( B == MouseButton::Left ) { PressW = nullptr; History.End(); }
            if ( B == MouseButton::Right && !Handled ) OpenContextMenu();
        }
        RequestFrame();
    }

    void Context::OnMouseWheel( Vec2 Delta )
    {
        Event E; E.Type = EventType::MouseWheel;
        E.Pos = Mouse;
        E.Mods = Mods();
        E.Wheel = Delta;
        Dispatch( HoverW, E );
        // Content moved under the cursor: re-evaluate hover.
        UpdateHover();
    }

    void Context::OnKey( Key K, bool Down, uint8_t Mods, bool Repeat )
    {
        bool Swallowed = Repeat ? false : TrackKey( K, Down );
        if ( CaptureW )
        {
            Event E; E.Type = Down ? EventType::KeyDown : EventType::KeyUp;
            E.KeyCode = K;
            E.Mods = Mods;
            E.Repeat = Repeat;
            CaptureW->OnEvent( E );
            RequestFrame();
            return;
        }
        if ( Swallowed ) return;
        if ( Down && !Repeat && DevToolKeys && K == Key::F12 )
        {
            if ( Mods & ModCtrl ) ShowStats = !ShowStats;
            else ShowInspector = !ShowInspector;
            RequestFrame();
            return;
        }
        if ( Down && HasPopups() )
        {
            bool Handled = false;
            PopupKeys( K, Handled ); // arrows / Enter inside the newest menu or dropdown
            if ( Handled ) { RequestFrame(); return; }
        }
        if ( Down && K == Key::Escape && DragOn ) { CancelDrag(); return; }
        if ( Down && K == Key::Escape && HasPopups() )
        {
            // Escape closes the newest popup first.
            auto& Kids = OverlayWidget->Children();
            for ( auto It = Kids.rbegin(); It != Kids.rend(); ++It )
                if ( auto* P = dynamic_cast<Popup*>( It->get() ); P && !P->IsClosed() ) { P->Close(); break; }
            RequestFrame();
            return;
        }
        Event E; E.Type = Down ? EventType::KeyDown : EventType::KeyUp;
        E.KeyCode = K;
        E.Mods = Mods;
        E.Repeat = Repeat;
        if ( !Down ) { Dispatch( FocusW, E ); return; }

        bool Typing = FocusW && FocusW->WantsKeyboard();
        if ( FocusW && FocusW->CanBind() && !FocusW->BindIsAction() ) History.Begin( { FocusW } );
        bool Handled = Dispatch( FocusW, E );
        History.End();
        if ( Handled ) return;

        // not used by the focused widget: navigation, activation, shortcuts, undo
        if ( K == Key::Tab && TabNavigation && !( Mods & ( ModCtrl | ModAlt ) ) )
        {
            FocusNext( Mods & ModShift );
            return;
        }
        if ( ( K == Key::Enter || K == Key::Space ) && FocusW && FocusW->KeyActivates && !Typing && !Mods )
        {
            Click( *FocusW );
            return;
        }
        if ( !Repeat && RunShortcuts( K, Mods ) ) return;
        if ( History.Enabled && !Typing && ( Mods & ModCtrl ) && !Repeat )
        {
            if ( K == Key::Z && !( Mods & ModShift ) ) { History.Undo(); return; }
            if ( K == Key::Y || ( K == Key::Z && ( Mods & ModShift ) ) ) { History.Redo(); return; }
        }
    }

    // ------------------------------------------------------------ keyboard navigation

    static bool Shown( const Widget* W )
    {
        for ( ; W; W = W->GetParent() )
            if ( !W->Visible ) return false;
        return true;
    }

    void Context::FocusNext( bool Backward )
    {
        // inside the open dialog if there is one, else the main tree
        Widget* Scope = RootWidget.get();
        for ( auto& K : OverlayWidget->Children() )
            if ( auto* D = dynamic_cast<Dialog*>( K.get() ); D && !D->IsClosed() ) Scope = D;
        std::vector<Widget*> Stops, Stack = { Scope };
        while ( !Stack.empty() )
        {
            Widget* W = Stack.back();
            Stack.pop_back();
            if ( !W->Visible || !W->Enabled ) continue;
            if ( W != Scope && W->Focusable ) Stops.push_back( W );
            auto& Kids = W->Children();
            for ( auto It = Kids.rbegin(); It != Kids.rend(); ++It ) Stack.push_back( It->get() );
        }
        if ( Stops.empty() ) return;
        auto It = std::find( Stops.begin(), Stops.end(), FocusW );
        size_t N = Stops.size(), I;
        if ( It == Stops.end() ) I = Backward ? N - 1 : 0;
        else I = ( size_t( It - Stops.begin() ) + ( Backward ? N - 1 : 1 ) ) % N;
        SetFocus( Stops[ I ] );
        FocusRing = true;
        RequestFrame();
    }

    void Context::Click( Widget& W )
    {
        Widget* OldHover = HoverW;
        HoverW = &W;
        PressW = &W;
        Event E;
        E.Type = EventType::MouseDown;
        E.Pos = W.Bounds.Center();
        E.Button = MouseButton::Left;
        W.OnEvent( E );
        E.Type = EventType::MouseUp;
        Dispatch( &W, E );
        PressW = nullptr;
        HoverW = OldHover;
        UpdateHover();
        RequestFrame();
    }

    void Context::PopupKeys( Key K, bool& Handled )
    {
        Popup* Top = nullptr;
        for ( auto& C : OverlayWidget->Children() )
            if ( auto* P = dynamic_cast<Popup*>( C.get() ); P && !P->IsClosed() ) Top = P;
        if ( !Top ) return;
        if ( FocusW && FocusW->WantsKeyboard() && Top->Owner != FocusW ) return; // typing somewhere else
        std::vector<Widget*> Items, Stack = { Top };
        while ( !Stack.empty() )
        {
            Widget* W = Stack.back();
            Stack.pop_back();
            if ( !W->Visible || !W->Enabled ) continue;
            if ( W != Top && W->KeyActivates ) Items.push_back( W );
            auto& Kids = W->Children();
            for ( auto It = Kids.rbegin(); It != Kids.rend(); ++It ) Stack.push_back( It->get() );
        }
        if ( Items.empty() ) return;
        auto It = std::find( Items.begin(), Items.end(), KeyHotW );
        bool Has = It != Items.end();
        size_t I = Has ? size_t( It - Items.begin() ) : 0, N = Items.size();
        switch ( K )
        {
        case Key::Down: KeyHotW = Items[ Has ? ( I + 1 ) % N : 0 ]; Handled = true; break;
        case Key::Up: KeyHotW = Items[ Has ? ( I + N - 1 ) % N : N - 1 ]; Handled = true; break;
        case Key::Home: KeyHotW = Items.front(); Handled = true; break;
        case Key::End: KeyHotW = Items.back(); Handled = true; break;
        case Key::Enter:
        case Key::Space:
            if ( !Has ) return;
            {
                Widget* W = KeyHotW;
                KeyHotW = nullptr;
                Click( *W );
            }
            Handled = true;
            break;
        default: break;
        }
    }

    // ------------------------------------------------------------ drag and drop

    void Context::CancelDrag()
    {
        DragOn = false;
        DragNow = DragData{};
        DragSrc = DropW = nullptr;
        RequestFrame();
    }

    void Context::DrawDrag()
    {
        Theme& Th = GetTheme();
        if ( DropW && Shown( DropW ) )
        {
            List.RectFilled( DropW->Bounds, Th[ Tok::Accent ].Fade( 0.10f ), Th.Radius );
            List.RectStroke( DropW->Bounds, Th[ Tok::Accent ], Th.Radius, 2 );
        }
        if ( DragNow.Label.empty() ) return;
        const Font& F = *Th.Body;
        float W = F.Width( DragNow.Label ) + 20;
        Rect R = { Mouse.x + 14, Mouse.y + 10, W, F.LineHeight + 12 };
        List.Shadow( R, 8, Color( 0, 0, 0, 130 ), 10, { 0, 3 } );
        List.RectFilled( R, Th[ Tok::Raised ], 8 );
        List.RectStroke( R, DropW ? Th[ Tok::Accent ] : Th[ Tok::BorderStrong ], 8 );
        List.Text( F, { R.x + 10, R.y + 6 }, DragNow.Label, Th[ Tok::Text ] );
    }

    // ------------------------------------------------------------ shortcuts

    bool Context::ParseShortcut( const std::string& Combo, Key& K, uint8_t& Mods )
    {
        Mods = 0;
        K = Key::None;
        std::vector<std::string> Parts;
        std::string Cur;
        for ( size_t i = 0; i < Combo.size(); i++ )
        {
            char C = Combo[ i ];
            if ( C == '+' && !Cur.empty() ) { Parts.push_back( Cur ); Cur.clear(); }
            else if ( C != ' ' ) Cur += C;
        }
        if ( !Cur.empty() ) Parts.push_back( Cur );
        if ( Parts.empty() ) return false;
        auto Lower = []( std::string S ) { for ( char& C : S ) C = char( std::tolower( (unsigned char)C ) ); return S; };
        for ( size_t i = 0; i + 1 < Parts.size(); i++ )
        {
            std::string M = Lower( Parts[ i ] );
            if ( M == "ctrl" || M == "control" ) Mods |= ModCtrl;
            else if ( M == "shift" ) Mods |= ModShift;
            else if ( M == "alt" || M == "option" ) Mods |= ModAlt;
            else if ( M == "super" || M == "cmd" || M == "win" || M == "meta" ) Mods |= ModSuper;
            else return false;
        }
        std::string N = Lower( Parts.back() );
        static const std::pair<const char*, const char*> Alias[] = { { "escape", "esc" }, { "return", "enter" }, { "pageup", "pgup" },
            { "pagedown", "pgdn" }, { "del", "delete" }, { "ins", "insert" }, { "comma", "," }, { "period", "." }, { "minus", "-" },
            { "plus", "=" }, { "slash", "/" } };
        for ( auto& [ A, B ] : Alias )
            if ( N == A ) N = B;
        for ( size_t i = 1; i < size_t( Key::Count ); i++ )
            if ( Lower( KeyName( Key( i ) ) ) == N ) { K = Key( i ); return true; }
        return false;
    }

    std::string Context::ShortcutText( Key K, uint8_t Mods )
    {
        std::string S;
        if ( Mods & ModCtrl ) S += "Ctrl+";
        if ( Mods & ModAlt ) S += "Alt+";
        if ( Mods & ModShift ) S += "Shift+";
        if ( Mods & ModSuper ) S += "Super+";
        std::string N = KeyName( K );
        if ( N.size() > 1 && std::isalpha( (unsigned char)N[ 0 ] ) && !( N[ 0 ] == 'F' && std::isdigit( (unsigned char)N[ 1 ] ) ) )
            for ( size_t i = 1; i < N.size(); i++ ) N[ i ] = char( std::tolower( (unsigned char)N[ i ] ) ); // ENTER -> Enter
        return S + N;
    }

    uint32_t Context::Shortcut( const std::string& Combo, std::function<void()> Fn )
    {
        Key K;
        uint8_t M;
        if ( !ParseShortcut( Combo, K, M ) ) return 0;
        Shortcuts.push_back( { NextShortcut, K, M, std::move( Fn ) } );
        return NextShortcut++;
    }

    void Context::RemoveShortcut( uint32_t Id )
    {
        Shortcuts.erase( std::remove_if( Shortcuts.begin(), Shortcuts.end(), [ Id ]( auto& S ) { return S.Id == Id; } ), Shortcuts.end() );
    }

    bool Context::RunShortcuts( Key K, uint8_t Mods )
    {
        Mods &= ModShift | ModCtrl | ModAlt | ModSuper;
        bool Typing = FocusW && FocusW->WantsKeyboard();
        for ( auto& S : Shortcuts )
            if ( S.K == K && S.Mods == Mods && ( !Typing || ( Mods & ( ModCtrl | ModAlt | ModSuper ) ) ) )
            {
                auto Fn = S.Fn; // may remove itself
                if ( Fn ) Fn();
                RequestFrame();
                return true;
            }
        return false;
    }

    void Context::OnText( uint32_t Cp )
    {
        if ( CaptureW ) return; // a listening Keybind doesn't want typed text anywhere
        Event E; E.Type = EventType::Text;
        E.Codepoint = Cp;
        Dispatch( FocusW, E );
    }

    void Context::SetFocus( Widget* W )
    {
        if ( W == FocusW ) return;
        Event E{};
        if ( FocusW ) { E.Type = EventType::FocusOut; FocusW->OnEvent( E ); }
        FocusW = W;
        if ( FocusW ) { E.Type = EventType::FocusIn; FocusW->OnEvent( E ); }
        RequestFrame();
    }

    void Context::Forget( Widget* W )
    {
        if ( HoverW == W ) HoverW = nullptr;
        if ( PressW == W ) PressW = nullptr;
        if ( CaptureW == W ) CaptureW = nullptr;
        if ( TipW == W ) TipW = nullptr;
        if ( FocusW == W ) FocusW = nullptr;
        if ( KeyHotW == W ) KeyHotW = nullptr;
        if ( DragSrc == W ) DragSrc = nullptr;
        if ( DropW == W ) DropW = nullptr;
        Binds.Forget( W );
        History.Forget( W );
    }

    Cursor Context::GetCursor() const
    {
        Widget* W = PressW ? PressW : HoverW;
        return W && W->Enabled ? W->GetCursor() : Cursor::Arrow;
    }

    // ------------------------------------------------------------ frame

    void Context::Post( std::function<void()> Fn )
    {
        {
            std::lock_guard<std::mutex> Lock( Posts->M );
            Posts->Q.push_back( std::move( Fn ) );
        }
        if ( WakeUp ) WakeUp();
    }

    void Context::RequestFrameIn( double Seconds )
    {
        double T = Clock() + Seconds;
        if ( NextTimedFrame < 0 || T < NextTimedFrame ) NextTimedFrame = T;
    }

    bool Context::NeedsFrame() const
    {
        if ( FrameRequested || LayoutDirty ) return true;
        if ( NextTimedFrame >= 0 && Clock() >= NextTimedFrame ) return true;
        std::lock_guard<std::mutex> Lock( Posts->M );
        return !Posts->Q.empty();
    }

    double Context::SecondsUntilNextFrame() const
    {
        if ( NeedsFrame() ) return 0;
        double T = NextTimedFrame >= 0 ? std::max( 0.0, NextTimedFrame - Clock() ) : 1e9;
        for ( auto& Tm : Timers ) T = std::min( T, std::max( 0.0, Tm.Due - TimerNow() ) );
        return Pumps.empty() ? T : std::min( T, 0.01 ); // pumps need regular turns
    }

    static void SetHook( std::vector<std::pair<const void*, std::function<void()>>>& V, const void* K, std::function<void()> Fn )
    {
        for ( auto& [ Key, F ] : V )
            if ( Key == K ) { F = std::move( Fn ); return; }
        V.emplace_back( K, std::move( Fn ) );
    }
    static void DropHook( std::vector<std::pair<const void*, std::function<void()>>>& V, const void* K )
    {
        V.erase( std::remove_if( V.begin(), V.end(), [ K ]( auto& P ) { return P.first == K; } ), V.end() );
    }
    void Context::AddFrameHook( const void* K, std::function<void()> Fn ) { SetHook( FrameHooks, K, std::move( Fn ) ); }
    void Context::RemoveFrameHook( const void* K ) { DropHook( FrameHooks, K ); }
    void Context::AddPump( const void* K, std::function<void()> Fn ) { SetHook( Pumps, K, std::move( Fn ) ); }
    void Context::RemovePump( const void* K ) { DropHook( Pumps, K ); }
    ShaderId Context::Shader( const ShaderDesc& D )
    {
        auto It = ShaderCache.find( &D );
        if ( It != ShaderCache.end() ) return It->second;
        ShaderId S = R.CanShade() ? R.CreateShader( D ) : 0;
        ShaderCache[ &D ] = S;
        return S;
    }

    void Context::RenderOffscreen( TextureId T, std::unique_ptr<DrawList> L, Vec2 Size )
    {
        PendingOffscreen.push_back( { T, std::move( L ), Size } );
    }

    double Context::TimerNow() const { return FixedDt > 0 ? Now : Clock(); }

    uint32_t Context::Every( double S, std::function<void()> Fn )
    {
        Timers.push_back( { NextTimer, TimerNow() + S, std::max( 0.001, S ), std::move( Fn ) } );
        if ( WakeUp ) WakeUp();
        return NextTimer++;
    }

    uint32_t Context::After( double S, std::function<void()> Fn )
    {
        Timers.push_back( { NextTimer, TimerNow() + S, 0, std::move( Fn ) } );
        if ( WakeUp ) WakeUp();
        return NextTimer++;
    }

    void Context::CancelTimer( uint32_t Id )
    {
        Timers.erase( std::remove_if( Timers.begin(), Timers.end(), [ Id ]( auto& T ) { return T.Id == Id; } ), Timers.end() );
    }

    void Context::RunTimers()
    {
        double N = TimerNow();
        std::vector<std::function<void()>> Due;
        for ( auto It = Timers.begin(); It != Timers.end(); )
        {
            if ( It->Due > N ) { ++It; continue; }
            Due.push_back( It->Fn );
            if ( It->Period > 0 ) { It->Due = std::max( It->Due + It->Period, N ); ++It; }
            else It = Timers.erase( It );
        }
        for ( auto& Fn : Due ) Fn();
    }

    void Context::WatchTheme( const std::string& Path )
    {
        auto Stamp = std::make_shared<std::string>( ThemeFileStamp( Path ) );
        LoadThemeFile( Path );
        InvalidateLayout();
        Every( 0.5, [ this, Path, Stamp ] {
            std::string S = ThemeFileStamp( Path );
            if ( S == *Stamp || S.empty() ) return;
            *Stamp = S;
            if ( LoadThemeFile( Path ) ) InvalidateLayout();
        } );
    }

    void Context::RunPumps()
    {
        RunTimers();
        auto Copy = Pumps; // a pump may remove itself
        for ( auto& [ K, Fn ] : Copy ) Fn();
    }

    const DrawList& Context::Frame()
    {
        if ( FixedDt > 0 )
        {
            Dt = FixedDt;
            Now += FixedDt;
        }
        else
        {
            double T = Clock();
            Dt = Now == 0 ? 1.f / 60.f : float( std::clamp( T - Now, 0.0, 0.1 ) );
            Now = T;
        }

        std::vector<std::function<void()>> Run;
        {
            std::lock_guard<std::mutex> Lock( Posts->M );
            Run.swap( Posts->Q );
        }
        for ( auto& Fn : Run ) Fn();
        RunTimers();

        if ( NextTimedFrame >= 0 && Now >= NextTimedFrame ) NextTimedFrame = -1;
        FrameRequested = false;

        const double T0 = Clock();
        if ( LayoutDirty )
        {
            LayoutDirty = false;
            Widget::LayoutGeneration++;
            RootWidget->Arrange( { 0, 0, Viewport.x, Viewport.y } );
            OverlayWidget->Arrange( { 0, 0, Viewport.x, Viewport.y } );
            // OnResize: widgets whose width changed in this layout pass
            std::vector<Widget*> St = { RootWidget.get(), OverlayWidget.get() }, Fire;
            while ( !St.empty() )
            {
                Widget* W = St.back();
                St.pop_back();
                if ( W->OnResize && W->Bounds.w != W->LastResizeW ) Fire.push_back( W );
                for ( auto& K : W->Children() ) St.push_back( K.get() );
            }
            for ( Widget* W : Fire )
            {
                W->LastResizeW = W->Bounds.w;
                auto Fn = W->OnResize;
                Fn( *W, W->Bounds.w );
            }
            UpdateHover(); // things moved
            FrameRequested = false;
        }

        LayoutCpu = Clock() - T0;
        Fonts.Maintain( R ); // grow the glyph atlas now if on-demand glyphs ran out of room last frame
        List.Reset( { 0, 0, Viewport.x, Viewport.y }, Scale, Fonts.Texture, Fonts.WhiteUV );
        List.Time = float( Now );
        List.RectFilled( { 0, 0, Viewport.x, Viewport.y }, GetTheme()[ Tok::Background ] );
        RootWidget->Draw( List );
        OverlayWidget->Draw( List );
        if ( FocusRing && FocusW && Shown( FocusW ) && !FocusW->WantsKeyboard() )
        {
            Theme& Th = GetTheme();
            Rect B = FocusW->Bounds;
            List.RectStroke( { B.x - 3, B.y - 3, B.w + 6, B.h + 6 }, Th[ Tok::Accent ].Fade( 0.85f ), Th.Radius + 3, 2 );
        }
        DrawTooltip();
        if ( DragOn ) DrawDrag();
        FrameCpu = Clock() - T0;
        if ( ShowInspector || ShowStats ) DrawDevTools();
        auto Hooks = FrameHooks; // hooks may add/remove hooks; they see the same FrameIndex() as Draw did
        for ( auto& [ K, Fn ] : Hooks ) Fn();
        Frames++;
        return List;
    }

    void Context::Render( Vec2 Fb )
    {
        Fonts.Upload( R ); // glyphs rasterized on demand this frame (emoji, CJK...)
        for ( auto& O : PendingOffscreen ) R.RenderTo( O.Target, *O.List, O.Size, Scale ); // cached panels first
        PendingOffscreen.clear();
        R.Render( List, Fb, Scale );
    }
}
