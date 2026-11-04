#include "tg/context.h"
#include "tg/renderer.h"
#include "tg/theme.h"
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
        extern const unsigned char InterRegular[], InterSemiBold[];
        extern const size_t InterRegularSize, InterSemiBoldSize;
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
    }

    Context::~Context()
    {
        OverlayWidget.reset(); // popups first: they notify their owners, which live in the root
        RootWidget.reset();
        if ( Fonts.Texture ) R.DestroyTexture( Fonts.Texture );
    }

    bool Context::LoadDefaultFonts( float S )
    {
        Theme& T = GetTheme();
        auto Reg = [] { return std::vector<unsigned char>( fonts::InterRegular, fonts::InterRegular + fonts::InterRegularSize ); };
        auto Semi = [] { return std::vector<unsigned char>( fonts::InterSemiBold, fonts::InterSemiBold + fonts::InterSemiBoldSize ); };
        T.Body = Fonts.AddFromMemory( Reg(), 15 );
        T.Small = Fonts.AddFromMemory( Reg(), 12.5f );
        T.Bold = Fonts.AddFromMemory( Semi(), 15 );
        T.Title = Fonts.AddFromMemory( Semi(), 20 );
        Scale = S;
        return Fonts.Build( R, S );
    }

    bool Context::LoadDefaultFonts( const std::string& Dir, float S )
    {
        Theme& T = GetTheme();
        T.Body = Fonts.AddFromFile( Dir + "/Inter-Regular.otf", 15 );
        T.Small = Fonts.AddFromFile( Dir + "/Inter-Regular.otf", 12.5f );
        T.Bold = Fonts.AddFromFile( Dir + "/Inter-SemiBold.otf", 15 );
        T.Title = Fonts.AddFromFile( Dir + "/Inter-SemiBold.otf", 20 );
        if ( !T.Body || !T.Small || !T.Bold || !T.Title ) return false;
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

    void Context::OnMouseMove( Vec2 P )
    {
        Mouse = P;
        UpdateHover();
        Event E; E.Type = EventType::MouseMove;
        E.Pos = P;
        Dispatch( PressW ? PressW : HoverW, E );
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
        bool Swallowed = TrackKey( MouseKey( B ), Down );
        if ( CaptureW ) // a widget is listening for "any button": it gets this one, nothing else does
        {
            CaptureW->OnEvent( E );
            RequestFrame();
            return;
        }
        if ( Swallowed ) return;
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
            Dispatch( B == MouseButton::Left && PressW ? PressW : HoverW, E );
            if ( B == MouseButton::Left ) PressW = nullptr;
        }
        RequestFrame();
    }

    void Context::OnMouseWheel( Vec2 Delta )
    {
        Event E; E.Type = EventType::MouseWheel;
        E.Pos = Mouse;
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
        if ( NextTimedFrame >= 0 ) return std::max( 0.0, NextTimedFrame - Clock() );
        return 1e9;
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

        List.Reset( { 0, 0, Viewport.x, Viewport.y }, Scale, Fonts.Texture, Fonts.WhiteUV );
        List.RectFilled( { 0, 0, Viewport.x, Viewport.y }, GetTheme()[ Tok::Background ] );
        RootWidget->Draw( List );
        OverlayWidget->Draw( List );
        return List;
    }

    void Context::Render( Vec2 Fb )
    {
        R.Render( List, Fb, Scale );
    }
}
