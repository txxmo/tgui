#include "tg/widgets/textarea.h"
#include "../internal.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/menu.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    using internal::PrevCp; // whole emoji sequences
    using internal::NextCp;
    using internal::AppendUtf8;

    TextArea::TextArea( std::string Ph ) : Placeholder( std::move( Ph ) )
    {
        Interactive = true;
        Focusable = true;
        Lines.push_back( { 0, 0 } );
        ContextMenu = [ this ]( Menu& M ) {
            bool Sel = Caret != Anchor;
            auto K = [ this ]( Key Kc ) { return [ this, Kc ] { Event E; E.Type = EventType::KeyDown; E.KeyCode = Kc; E.Mods = ModCtrl; OnEvent( E ); }; };
            M.Item( "Cut", K( Key::X ), "Ctrl+X" ).Enabled = Sel && !ReadOnly;
            M.Item( "Copy", K( Key::C ), "Ctrl+C" ).Enabled = Sel;
            M.Item( "Paste", K( Key::V ), "Ctrl+V" ).Enabled = !ReadOnly;
            M.Separator();
            M.Item( "Select All", K( Key::A ), "Ctrl+A" ).Enabled = !Text.empty();
        };
    }

    void TextArea::SetText( std::string T )
    {
        Text = std::move( T );
        Caret = Anchor = Text.size();
        Changed();
    }

    // ------------------------------------------------------------ layout of the text

    void TextArea::Relayout()
    {
        Lines.clear();
        const Font& F = *GetTheme().Body;
        std::string_view S = Text;
        F.ForEachLine( S, WrapW > 1 ? WrapW : 0, [ & ]( std::string_view L, float ) {
            size_t Start = size_t( L.data() - S.data() );
            Lines.push_back( { Start, Start + L.size() } );
        } );
        if ( Lines.empty() ) Lines.push_back( { 0, 0 } );
    }

    size_t TextArea::LineCount() { Relayout(); return Lines.size(); }

    size_t TextArea::LineOf( size_t B ) const
    {
        // the last line whose start <= B (a caret at a wrap point belongs to the next line's start)
        size_t L = 0;
        for ( size_t i = 0; i < Lines.size(); i++ )
            if ( Lines[ i ].Start <= B ) L = i;
        return L;
    }

    Vec2 TextArea::PosOf( size_t B ) const
    {
        const Font& F = *GetTheme().Body;
        size_t L = LineOf( B );
        size_t E = std::min( B, Lines[ L ].End );
        return { F.Width( std::string_view( Text ).substr( Lines[ L ].Start, E - Lines[ L ].Start ) ), L * F.LineHeight };
    }

    size_t TextArea::ByteAt( Vec2 P ) const
    {
        const Font& F = *GetTheme().Body;
        int L = int( std::floor( P.y / F.LineHeight ) );
        L = std::clamp( L, 0, int( Lines.size() ) - 1 );
        const Line& Ln = Lines[ size_t( L ) ];
        return Ln.Start + F.HitTest( std::string_view( Text ).substr( Ln.Start, Ln.End - Ln.Start ), P.x );
    }

    Rect TextArea::TextRect() const { return Bounds.Shrink( Style.Padding ); }

    Vec2 TextArea::Measure( float Avail )
    {
        const Font& F = *GetTheme().Body;
        float W = std::min( Avail, 320.f );
        int N = std::max( 1, MinLines );
        if ( MaxLines > 0 )
        {
            WrapW = std::max( 1.f, Avail - Style.Padding.Horizontal() - 8 );
            Relayout();
            N = std::clamp( int( Lines.size() ), N, std::max( N, MaxLines ) );
        }
        return { W, N * F.LineHeight + Style.Padding.Vertical() };
    }

    void TextArea::Arrange( Rect R )
    {
        Bounds = R;
        WrapW = std::max( 1.f, TextRect().w - 8 ); // leave room for the scrollbar
        Relayout();
        const Font& F = *GetTheme().Body;
        ScrollY = Clamp( ScrollY, 0, std::max( 0.f, Lines.size() * F.LineHeight - TextRect().h ) );
    }

    void TextArea::KeepCaretVisible()
    {
        const Font& F = *GetTheme().Body;
        Rect In = TextRect();
        Vec2 C = PosOf( Caret );
        if ( C.y - ScrollY < 0 ) ScrollY = C.y;
        if ( C.y + F.LineHeight - ScrollY > In.h ) ScrollY = C.y + F.LineHeight - In.h;
        float MaxS = std::max( 0.f, Lines.size() * F.LineHeight - In.h );
        ScrollY = Clamp( ScrollY, 0, MaxS );
    }

    // ------------------------------------------------------------ editing

    void TextArea::Changed()
    {
        size_t Before = Lines.size();
        Relayout();
        if ( Bind ) *Bind = Text;
        if ( auto* C = GetContext() ) BlinkStart = C->Time();
        if ( OnChange ) OnChange( Text );
        if ( Bounds.h > 0 ) KeepCaretVisible(); // not laid out yet: nothing to scroll
        if ( MaxLines > 0 && Lines.size() != Before ) Invalidate(); // auto-growing: our height changed
        else Redraw();
    }

    void TextArea::EraseSelection()
    {
        size_t A = std::min( Caret, Anchor ), B = std::max( Caret, Anchor );
        Text.erase( A, B - A );
        Caret = Anchor = A;
    }

    void TextArea::Insert( const std::string& S )
    {
        if ( ReadOnly ) return;
        EraseSelection();
        Text.insert( Caret, S );
        Caret = Anchor = Caret + S.size();
        WantX = -1;
        Changed();
    }

    // ------------------------------------------------------------ drawing

    void TextArea::Draw( DrawList& D )
    {
        const TextAreaStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Context* C = GetContext();
        if ( Bind && *Bind != Text ) { Text = *Bind; Caret = Anchor = std::min( Caret, Text.size() ); Relayout(); }
        bool Foc = IsFocused();

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.RectStroke( Bounds, ( Foc ? S.StrokeFocused : S.Stroke ).Resolve(), S.Radius );

        Rect In = TextRect();
        D.PushClip( { In.x - 2, In.y, In.w + 4, In.h } );
        Vec2 O = { In.x, In.y - ScrollY };

        if ( Text.empty() ) D.Text( F, O, Placeholder, S.Placeholder.Resolve(), WrapW );

        // selection, one rect per line
        if ( Foc && Caret != Anchor )
        {
            size_t A = std::min( Caret, Anchor ), B = std::max( Caret, Anchor );
            for ( size_t i = 0; i < Lines.size(); i++ )
            {
                const Line& L = Lines[ i ];
                if ( L.End < A || L.Start > B ) continue;
                size_t S0 = std::max( A, L.Start ), S1 = std::min( B, L.End );
                float X0 = F.Width( std::string_view( Text ).substr( L.Start, S0 - L.Start ) );
                float X1 = F.Width( std::string_view( Text ).substr( L.Start, S1 - L.Start ) );
                if ( B > L.End ) X1 += 4; // selection continues past the line end
                D.RectFilled( { O.x + X0, O.y + i * F.LineHeight, std::max( 2.f, X1 - X0 ), F.LineHeight }, S.Selection.Resolve(), 2 );
            }
        }

        // text, line by line (only the visible ones)
        int First = std::max( 0, int( ScrollY / F.LineHeight ) );
        int Last = std::min( int( Lines.size() ), int( ( ScrollY + In.h ) / F.LineHeight ) + 2 );
        for ( int i = First; i < Last; i++ )
        {
            const Line& L = Lines[ size_t( i ) ];
            D.Text( F, { O.x, O.y + i * F.LineHeight }, std::string_view( Text ).substr( L.Start, L.End - L.Start ), S.TextColor.Resolve() );
        }

        // caret
        {
            Vec2 P = PosOf( Caret );
            CaretAt = { std::floor( O.x + P.x ), O.y + P.y + 1, 1.5f, F.LineHeight - 2 };
        }
        if ( Foc && C && !ReadOnly )
        {
            const double Period = 1.06, Phase = std::fmod( C->Time() - BlinkStart, Period );
            if ( Phase < Period * 0.5 )
            {
                Vec2 P = PosOf( Caret );
                D.RectFilled( { std::floor( O.x + P.x ), O.y + P.y + 1, 1.5f, F.LineHeight - 2 }, S.TextColor.Resolve() );
            }
            C->RequestFrameIn( ( Phase < Period * 0.5 ? Period * 0.5 : Period ) - Phase );
        }
        D.PopClip();

        // scrollbar
        float Content = Lines.size() * F.LineHeight;
        if ( Content > In.h + 0.5f )
        {
            float Track = Bounds.h - 8, H = std::max( 16.f, Track * In.h / Content );
            float T = ScrollY / std::max( 1.f, Content - In.h );
            D.RectFilled( { Bounds.Right() - 7, Bounds.y + 4 + ( Track - H ) * T, 4, H }, S.Thumb.Resolve(), 2 );
        }
    }

    // ------------------------------------------------------------ input

    bool TextArea::OnEvent( const Event& E )
    {
        Context* C = GetContext();
        const Font& F = *GetTheme().Body;
        Rect In = TextRect();
        Vec2 Local = { E.Pos.x - In.x, E.Pos.y - In.y + ScrollY };
        bool Shift = E.Mods & ModShift, Ctrl = E.Mods & ( ModCtrl | ModSuper );

        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            Caret = ByteAt( Local );
            if ( !Shift ) Anchor = Caret;
            if ( E.Clicks >= 2 ) // double: word, triple: the (logical) line
            {
                if ( E.Clicks >= 3 )
                {
                    size_t A = Text.rfind( '\n', Caret ? Caret - 1 : 0 ), B = Text.find( '\n', Caret );
                    Anchor = ( A == std::string::npos || Caret == 0 ) ? 0 : A + 1;
                    Caret = B == std::string::npos ? Text.size() : B;
                }
                else WordAt( Text, Caret, Anchor, Caret );
                WantX = -1;
                Redraw();
                return true;
            }
            Dragging = true;
            WantX = -1;
            if ( C ) BlinkStart = C->Time();
            return true;
        case EventType::MouseMove:
            if ( !Dragging ) return false;
            Caret = ByteAt( Local );
            KeepCaretVisible();
            Redraw();
            return true;
        case EventType::MouseUp:
            Dragging = false;
            return E.Button == MouseButton::Left;
        case EventType::MouseWheel:
        {
            float MaxS = std::max( 0.f, Lines.size() * F.LineHeight - In.h );
            if ( MaxS <= 0 ) return false;
            ScrollY = Clamp( ScrollY - E.Wheel.y * F.LineHeight * 3, 0, MaxS );
            Redraw();
            return true;
        }
        case EventType::FocusIn: if ( C ) BlinkStart = C->Time(); return true;
        case EventType::FocusOut: Anchor = Caret; Dragging = false; return true;
        case EventType::Text:
            if ( E.Codepoint < 32 || E.Codepoint == 127 ) return false;
            { std::string S; AppendUtf8( S, E.Codepoint ); Insert( S ); }
            return true;
        case EventType::KeyDown: break;
        default: return false;
        }

        auto Move = [ & ]( size_t To, bool KeepX = false ) {
            Caret = To;
            if ( !Shift ) Anchor = Caret;
            if ( !KeepX ) WantX = -1;
            if ( C ) BlinkStart = C->Time();
            KeepCaretVisible();
            Redraw();
        };
        auto Vertical = [ & ]( int Dir ) {
            Vec2 P = PosOf( Caret );
            if ( WantX < 0 ) WantX = P.x;
            int L = int( LineOf( Caret ) ) + Dir;
            if ( L < 0 ) return Move( 0 );
            if ( L >= int( Lines.size() ) ) return Move( Text.size() );
            Move( ByteAt( { WantX, ( L + 0.5f ) * F.LineHeight } ), true );
        };
        bool HasSel = Caret != Anchor;
        const Line& Cur = Lines[ LineOf( Caret ) ];

        switch ( E.KeyCode )
        {
        case Key::Left:  Move( HasSel && !Shift ? std::min( Caret, Anchor ) : PrevCp( Text, Caret ) ); return true;
        case Key::Right: Move( HasSel && !Shift ? std::max( Caret, Anchor ) : NextCp( Text, Caret ) ); return true;
        case Key::Up:    Vertical( -1 ); return true;
        case Key::Down:  Vertical( 1 ); return true;
        case Key::Home:  Move( Ctrl ? 0 : Cur.Start ); return true;
        case Key::End:   Move( Ctrl ? Text.size() : Cur.End ); return true;
        case Key::Backspace:
            if ( ReadOnly ) return true;
            if ( !HasSel ) Anchor = PrevCp( Text, Caret );
            EraseSelection(); Changed(); return true;
        case Key::Delete:
            if ( ReadOnly ) return true;
            if ( !HasSel ) Anchor = NextCp( Text, Caret );
            EraseSelection(); Changed(); return true;
        case Key::Enter:
        case Key::KpEnter:
            if ( Ctrl || ( SubmitOnEnter && !Shift ) )
            {
                if ( OnSubmit ) OnSubmit( Text );
                if ( ClearOnSubmit ) SetText( {} );
                return true;
            }
            Insert( "\n" );
            return true;
        case Key::Escape:
            if ( C ) C->SetFocus( nullptr );
            return true;
        case Key::A: if ( !Ctrl ) return false; Anchor = 0; Caret = Text.size(); Redraw(); return true;
        case Key::C:
        case Key::X:
            if ( !Ctrl ) return false;
            if ( HasSel && C && C->SetClipboard )
                C->SetClipboard( Text.substr( std::min( Caret, Anchor ), std::max( Caret, Anchor ) - std::min( Caret, Anchor ) ) );
            if ( E.KeyCode == Key::X && HasSel && !ReadOnly ) { EraseSelection(); Changed(); }
            return true;
        case Key::V:
            if ( !Ctrl ) return false;
            if ( C && C->GetClipboard )
            {
                std::string S = C->GetClipboard();
                S.erase( std::remove( S.begin(), S.end(), '\r' ), S.end() );
                Insert( S );
            }
            return true;
        default:
            return false;
        }
    }
}
