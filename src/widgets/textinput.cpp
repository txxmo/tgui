#include "tg/widgets/textinput.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/menu.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    static const char* Bullet = "\xE2\x80\xA2"; // U+2022, 3 bytes

    static bool IsCont( char c ) { return ( static_cast<unsigned char>( c ) & 0xC0 ) == 0x80; }
    static size_t PrevCp( const std::string& s, size_t i ) { return PrevGrapheme( s, i ); } // whole emoji sequences
    static size_t NextCp( const std::string& s, size_t i ) { return NextGrapheme( s, i ); }
    static size_t CountCps( const std::string& s, size_t End ) { size_t n = 0; for ( size_t i = 0; i < End; i++ ) n += !IsCont( s[ i ] ); return n; }

    static void AppendUtf8( std::string& s, uint32_t c )
    {
        if ( c < 0x80 ) s += char( c );
        else if ( c < 0x800 ) { s += char( 0xC0 | c >> 6 ); s += char( 0x80 | ( c & 0x3F ) ); }
        else if ( c < 0x10000 ) { s += char( 0xE0 | c >> 12 ); s += char( 0x80 | ( ( c >> 6 ) & 0x3F ) ); s += char( 0x80 | ( c & 0x3F ) ); }
        else { s += char( 0xF0 | c >> 18 ); s += char( 0x80 | ( ( c >> 12 ) & 0x3F ) ); s += char( 0x80 | ( ( c >> 6 ) & 0x3F ) ); s += char( 0x80 | ( c & 0x3F ) ); }
    }

    TextInput::TextInput( std::string Ph ) : Placeholder( std::move( Ph ) )
    {
        Interactive = true;
        Focusable = true;
        // right-click: the usual edit menu (runs the same code as the shortcuts)
        ContextMenu = [ this ]( Menu& M ) {
            bool Sel = Caret != Anchor, Pw = Password;
            auto K = [ this ]( Key Kc ) { return [ this, Kc ] { Event E; E.Type = EventType::KeyDown; E.KeyCode = Kc; E.Mods = ModCtrl; OnEvent( E ); }; };
            M.Item( "Cut", K( Key::X ), "Ctrl+X" ).Enabled = Sel && !Pw;
            M.Item( "Copy", K( Key::C ), "Ctrl+C" ).Enabled = Sel && !Pw;
            M.Item( "Paste", K( Key::V ), "Ctrl+V" );
            M.Separator();
            M.Item( "Select All", K( Key::A ), "Ctrl+A" ).Enabled = !Text.empty();
        };
    }

    void TextInput::SetText( std::string T )
    {
        Text = std::move( T );
        Caret = Anchor = Text.size();
        Changed();
    }

    std::string TextInput::Shown() const
    {
        if ( !Password ) return Text;
        std::string S;
        for ( size_t i = 0, n = CountCps( Text, Text.size() ); i < n; i++ ) S += Bullet;
        return S;
    }

    // Byte offset in Text -> byte offset in Shown().
    static size_t ToShown( const std::string& Text, size_t Off, bool Password ) { return Password ? CountCps( Text, Off ) * 3 : Off; }

    size_t TextInput::ByteAt( float LocalX ) const
    {
        const Font& F = *GetTheme().Body;
        size_t S = F.HitTest( Shown(), LocalX );
        if ( !Password ) return S;
        size_t Cp = S / 3, i = 0;
        while ( Cp-- && i < Text.size() ) i = NextCp( Text, i );
        return i;
    }

    void TextInput::Changed()
    {
        if ( auto* C = GetContext() ) BlinkStart = C->Time();
        if ( OnChange ) OnChange( Text );
        Redraw();
    }

    void TextInput::EraseSelection()
    {
        size_t A = std::min( Caret, Anchor ), B = std::max( Caret, Anchor );
        Text.erase( A, B - A );
        Caret = Anchor = A;
    }

    void TextInput::Insert( const std::string& S )
    {
        EraseSelection();
        Text.insert( Caret, S );
        Caret = Anchor = Caret + S.size();
        Changed();
    }

    Vec2 TextInput::Measure( float Avail )
    {
        return { std::min( Avail, 200.f ), GetTheme().Body->LineHeight + Style.Padding.Vertical() };
    }

    void TextInput::Draw( DrawList& D )
    {
        const TextInputStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Context* C = GetContext();
        bool Foc = IsFocused();

        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.RectStroke( Bounds, ( Foc ? S.StrokeFocused : S.Stroke ).Resolve(), S.Radius );

        Rect In = Bounds.Shrink( S.Padding );
        std::string Disp = Shown();
        std::string_view DV = Disp;
        float CaretX = F.Width( DV.substr( 0, ToShown( Text, Caret, Password ) ) );

        // keep caret in view
        if ( CaretX - ScrollX > In.w - 2 ) ScrollX = CaretX - In.w + 2;
        if ( CaretX - ScrollX < 0 ) ScrollX = CaretX;
        if ( F.Width( DV ) < In.w ) ScrollX = 0;

        float X = In.x - ScrollX, Y = std::floor( In.y + ( In.h - F.LineHeight ) * 0.5f );
        D.PushClip( { In.x - 1, Bounds.y, In.w + 2, Bounds.h } );

        if ( Foc && Caret != Anchor )
        {
            float A = F.Width( DV.substr( 0, ToShown( Text, std::min( Caret, Anchor ), Password ) ) );
            float B = F.Width( DV.substr( 0, ToShown( Text, std::max( Caret, Anchor ), Password ) ) );
            D.RectFilled( { X + A, Y, B - A, F.LineHeight }, S.Selection.Resolve(), 2 );
        }

        if ( Text.empty() ) D.Text( F, { X, Y }, Placeholder, S.Placeholder.Resolve() );
        else D.Text( F, { X, Y }, Disp, S.TextColor.Resolve() );

        // caret blinks at ~1Hz; schedule exactly the frames we need instead of redrawing nonstop
        if ( Foc && C )
        {
            const double Period = 1.06, Phase = std::fmod( C->Time() - BlinkStart, Period );
            if ( Phase < Period * 0.5 ) D.RectFilled( { std::floor( X + CaretX ), Y + 1, 1.5f, F.LineHeight - 2 }, S.TextColor.Resolve() );
            C->RequestFrameIn( ( Phase < Period * 0.5 ? Period * 0.5 : Period ) - Phase );
        }
        D.PopClip();
    }

    bool TextInput::OnEvent( const Event& E )
    {
        Context* C = GetContext();
        const float LocalX = E.Pos.x - ( Bounds.x + Style.Padding.Left ) + ScrollX;
        bool Shift = E.Mods & ModShift, Ctrl = E.Mods & ( ModCtrl | ModSuper );

        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            Caret = ByteAt( LocalX );
            Anchor = Caret;
            if ( E.Clicks >= 2 ) // double: word, triple: everything
            {
                if ( E.Clicks >= 3 || Password ) { Anchor = 0; Caret = Text.size(); }
                else WordAt( Text, Caret, Anchor, Caret );
                Redraw();
                return true;
            }
            Dragging = true;
            if ( C ) BlinkStart = C->Time();
            return true;
        case EventType::MouseMove:
            if ( !Dragging ) return false;
            Caret = ByteAt( LocalX );
            return true;
        case EventType::MouseUp:
            Dragging = false;
            return E.Button == MouseButton::Left;
        case EventType::FocusIn:
            if ( C ) BlinkStart = C->Time();
            return true;
        case EventType::FocusOut:
            Anchor = Caret;
            Dragging = false;
            if ( OnBlur ) { auto Fn = OnBlur; Fn(); }
            return true;
        case EventType::Text:
            if ( E.Codepoint < 32 || E.Codepoint == 127 ) return false;
            { std::string S; AppendUtf8( S, E.Codepoint ); Insert( S ); }
            return true;
        case EventType::KeyDown:
            break;
        default:
            return false;
        }

        // KeyDown
        auto Move = [ & ]( size_t To ) { Caret = To; if ( !Shift ) Anchor = Caret; if ( C ) BlinkStart = C->Time(); Redraw(); };
        bool HasSel = Caret != Anchor;
        switch ( E.KeyCode )
        {
        case Key::Left:  Move( HasSel && !Shift ? std::min( Caret, Anchor ) : PrevCp( Text, Caret ) ); return true;
        case Key::Right: Move( HasSel && !Shift ? std::max( Caret, Anchor ) : NextCp( Text, Caret ) ); return true;
        case Key::Home:  Move( 0 ); return true;
        case Key::End:   Move( Text.size() ); return true;
        case Key::Backspace:
            if ( Text.empty() ) return false; // nothing to delete: let the parent react (TagInput)
            if ( !HasSel ) Anchor = PrevCp( Text, Caret );
            EraseSelection(); Changed(); return true;
        case Key::Delete:
            if ( !HasSel ) Anchor = NextCp( Text, Caret );
            EraseSelection(); Changed(); return true;
        case Key::Enter:
        case Key::KpEnter:
            if ( OnSubmit ) OnSubmit( Text );
            if ( ClearOnSubmit ) SetText( {} );
            return true;
        case Key::Escape:
            if ( !Text.empty() && GetParent() && GetParent()->OnEvent( E ) ) return true; // e.g. SearchBox clears first
            if ( C ) C->SetFocus( nullptr );
            return true;
        case Key::A: if ( !Ctrl ) return false; Anchor = 0; Caret = Text.size(); Redraw(); return true;
        case Key::C:
        case Key::X:
            if ( !Ctrl ) return false;
            if ( HasSel && !Password && C && C->SetClipboard )
                C->SetClipboard( Text.substr( std::min( Caret, Anchor ), std::max( Caret, Anchor ) - std::min( Caret, Anchor ) ) );
            if ( E.KeyCode == Key::X && HasSel ) { EraseSelection(); Changed(); }
            return true;
        case Key::V:
            if ( !Ctrl ) return false;
            if ( C && C->GetClipboard )
            {
                std::string S = C->GetClipboard();
                S.erase( std::remove_if( S.begin(), S.end(), []( char ch ) { return ch == '\n' || ch == '\r'; } ), S.end() );
                Insert( S );
            }
            return true;
        default:
            return false;
        }
    }
}
