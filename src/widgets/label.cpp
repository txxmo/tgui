#include "tg/widgets/label.h"
#include "tg/draw.h"
#include "tg/context.h"
#include "tg/font.h"
#include "tg/widgets/menu.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Label::Label( std::string T, bool W ) : Wrap( W ), Text( std::move( T ) ) {}

    const Font& Label::GetFont() const { return Style.TextFont ? *Style.TextFont : *GetTheme().Body; }

    void Label::SetText( std::string T )
    {
        if ( T == Text ) return;
        SelA = SelB = 0;
        // Same width? Then only a redraw is needed (common for ticking numbers in a dashboard).
        bool SameSize = !Wrap && GetFont().Width( T ) == GetFont().Width( Text );
        Text = std::move( T );
        if ( SameSize ) Redraw();
        else Invalidate();
    }

    Vec2 Label::Measure( float Avail )
    {
        const Insets& P = Layout.Padding;
        Vec2 S = GetFont().Measure( Text, Wrap ? std::fmax( 1.f, Avail - P.Horizontal() ) : 0 );
        return { std::ceil( S.x ) + P.Horizontal(), S.y + P.Vertical() };
    }

    Vec2 Label::Origin() const
    {
        const Font& F = GetFont();
        Rect In = Bounds.Shrink( Layout.Padding );
        Vec2 Sz = F.Measure( Text, Wrap ? In.w : 0 );
        float X = In.x, Y = In.y;
        if ( Style.HAlign == Align::Center ) X += std::floor( ( In.w - Sz.x ) * 0.5f );
        if ( Style.HAlign == Align::End ) X += In.w - Sz.x;
        if ( Style.VAlign == Align::Center ) Y += std::floor( ( In.h - Sz.y ) * 0.5f );
        if ( Style.VAlign == Align::End ) Y += In.h - Sz.y;
        return { X, Y };
    }

    void Label::Draw( DrawList& D )
    {
        const Font& F = GetFont();
        Rect In = Bounds.Shrink( Layout.Padding );
        Vec2 O = Origin();
        if ( Selectable && SelA != SelB ) // highlight per wrapped line
        {
            size_t A = std::min( SelA, SelB ), B = std::max( SelA, SelB );
            F.ForEachLine( Text, Wrap ? In.w : 0, [ & ]( std::string_view Ln, float Y ) {
                size_t L0 = size_t( Ln.data() - Text.data() ), L1 = L0 + Ln.size();
                if ( B <= L0 || A > L1 ) return;
                size_t S0 = std::max( A, L0 ) - L0, S1 = std::min( B, L1 ) - L0;
                float X0 = F.Width( Ln.substr( 0, S0 ) ), X1 = F.Width( Ln.substr( 0, S1 ) );
                if ( B > L1 ) X1 += F.Find( ' ' ).Advance * 0.5f; // the selection continues past the line end
                D.RectFilled( { O.x + X0, O.y + Y, std::max( 1.f, X1 - X0 ), F.LineHeight }, Style.SelectionFill.Resolve() );
            } );
        }
        D.Text( F, O, Text, Style.TextColor.Resolve().Fade( Enabled ? 1.f : 0.5f ), Wrap ? In.w : 0 );
    }

    void Label::SetSelectable( bool On )
    {
        Selectable = On;
        Interactive = On;
        Focusable = On;
        SelA = SelB = 0;
        if ( On )
            ContextMenu = [ this ]( Menu& M ) {
                bool Sel = SelA != SelB;
                M.Item( Sel ? "Copy" : "Copy text", [ this, Sel ] {
                    Context* C = GetContext();
                    if ( C && C->SetClipboard ) C->SetClipboard( Sel ? SelectedText() : Text );
                }, Sel ? "Ctrl+C" : "" );
                M.Item( "Select All", [ this ] { SelectAll(); } );
            };
        else ContextMenu = nullptr;
        Redraw();
    }

    std::string Label::SelectedText() const
    {
        size_t A = std::min( SelA, SelB ), B = std::min( std::max( SelA, SelB ), Text.size() );
        return A < B ? Text.substr( A, B - A ) : std::string();
    }

    size_t Label::ByteAt( Vec2 P ) const
    {
        const Font& F = GetFont();
        Rect In = Bounds.Shrink( Layout.Padding );
        Vec2 O = Origin();
        size_t Best = Text.size();
        bool Found = false;
        F.ForEachLine( Text, Wrap ? In.w : 0, [ & ]( std::string_view Ln, float Y ) {
            if ( Found ) return;
            size_t L0 = size_t( Ln.data() - Text.data() );
            if ( P.y < O.y + Y + F.LineHeight || L0 + Ln.size() >= Text.size() )
            {
                Best = L0 + F.HitTest( Ln, P.x - O.x );
                Found = true;
            }
        } );
        return P.y < O.y ? 0 : Best;
    }

    bool Label::OnEvent( const Event& E )
    {
        if ( !Selectable ) return false;
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            SelA = SelB = ByteAt( E.Pos );
            if ( E.Clicks == 2 ) WordAt( Text, SelA, SelA, SelB );
            else if ( E.Clicks >= 3 ) { SelA = 0; SelB = Text.size(); }
            else Dragging = true;
            Redraw();
            return true;
        case EventType::MouseMove:
            if ( !Dragging ) return false;
            SelB = ByteAt( E.Pos );
            Redraw();
            return true;
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left ) return false;
            Dragging = false;
            return true;
        case EventType::KeyDown:
            if ( !( E.Mods & ModCtrl ) ) return false;
            if ( E.KeyCode == Key::A ) { SelectAll(); return true; }
            if ( E.KeyCode == Key::C && SelA != SelB )
            {
                Context* C = GetContext();
                if ( C && C->SetClipboard ) C->SetClipboard( SelectedText() );
                return true;
            }
            return false;
        case EventType::FocusOut:
            SelA = SelB = 0; // like a browser: clicking elsewhere drops the selection
            Redraw();
            return false;
        default:
            return false;
        }
    }
}
