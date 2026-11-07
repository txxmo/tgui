#include "tg/context.h"
#include "tg/renderer.h"
#include "tg/theme.h"
#include "tg/widgets/menu.h"
#include "tg/widgets/popup.h"
#include <algorithm>
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
        extern const unsigned char InterRegular[], InterMedium[], InterSemiBold[], MonoRegular[], MonoBold[];
        extern const size_t InterRegularSize, InterMediumSize, InterSemiBoldSize, MonoRegularSize, MonoBoldSize;
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
            if ( W->ContextMenu )
            {
                ClosePopups(); // one menu at a time
                Menu& M = OpenMenu( *W, Mouse );
                W->ContextMenu( M );
                return true;
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
        Mouse = P;
        UpdateHover();
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
        if ( CaptureW || !OnKeyEvent ) return false;
        bool IsMouse = K >= Key::Mouse1 && K <= Key::Mouse5;
        if ( !IsMouse && FocusW ) return false; // typing in a text field: letters are text, not hotkeys
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
                    if ( W == P || W == P->Owner ) { Inside = true; break; }
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
            if ( B == MouseButton::Left ) PressW = nullptr;
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
        Dispatch( FocusW, E );
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
    void Context::RunPumps()
    {
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

        if ( NextTimedFrame >= 0 && Now >= NextTimedFrame ) NextTimedFrame = -1;
        FrameRequested = false;

        if ( LayoutDirty )
        {
            LayoutDirty = false;
            Widget::LayoutGeneration++;
            RootWidget->Arrange( { 0, 0, Viewport.x, Viewport.y } );
            OverlayWidget->Arrange( { 0, 0, Viewport.x, Viewport.y } );
            UpdateHover(); // things moved
            FrameRequested = false;
        }

        Fonts.Maintain( R ); // grow the glyph atlas now if on-demand glyphs ran out of room last frame
        List.Reset( { 0, 0, Viewport.x, Viewport.y }, Scale, Fonts.Texture, Fonts.WhiteUV );
        List.RectFilled( { 0, 0, Viewport.x, Viewport.y }, GetTheme()[ Tok::Background ] );
        RootWidget->Draw( List );
        OverlayWidget->Draw( List );
        DrawTooltip();
        auto Hooks = FrameHooks; // hooks may add/remove hooks; they see the same FrameIndex() as Draw did
        for ( auto& [ K, Fn ] : Hooks ) Fn();
        Frames++;
        return List;
    }

    void Context::Render( Vec2 Fb )
    {
        Fonts.Upload( R ); // glyphs rasterized on demand this frame (emoji, CJK...)
        R.Render( List, Fb, Scale );
    }
}
