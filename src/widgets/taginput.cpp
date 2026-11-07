#include "tg/widgets/taginput.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/textinput.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    TagInput::TagInput( std::vector<std::string>* B, std::string Ph ) : Bind( B )
    {
        Interactive = true;
        Text = &Add<TextInput>( std::move( Ph ) );
        Text->Style.Fill = Color( 0, 0, 0, 0 );
        Text->Style.Stroke = Color( 0, 0, 0, 0 );
        Text->Style.StrokeFocused = Color( 0, 0, 0, 0 );
        Text->Style.Padding = Insets( 4, 3 );
        Text->OnSubmit = [ this ]( const std::string& T ) { AddTag( T ); Text->SetText( {} ); };
        Text->OnChange = [ this ]( const std::string& T ) {
            size_t C = T.find( ',' );
            if ( C == std::string::npos ) return;
            std::string Before = T.substr( 0, C ), After = T.substr( C + 1 );
            AddTag( Before );
            GetContext()->Post( [ this, After ] { Text->SetText( After ); } ); // don't re-enter SetText from OnChange
        };
    }

    void TagInput::AddTag( std::string T )
    {
        while ( !T.empty() && T.front() == ' ' ) T.erase( T.begin() );
        while ( !T.empty() && T.back() == ' ' ) T.pop_back();
        if ( T.empty() ) return;
        if ( !AllowDuplicates && std::find( Tags().begin(), Tags().end(), T ) != Tags().end() ) return;
        Tags().push_back( std::move( T ) );
        if ( OnChange ) OnChange( Tags() );
        Invalidate();
    }

    void TagInput::RemoveTag( size_t i )
    {
        if ( i >= Tags().size() ) return;
        Tags().erase( Tags().begin() + i );
        if ( OnChange ) OnChange( Tags() );
        Invalidate();
    }

    float TagInput::ChipWidth( const std::string& T ) const { return std::ceil( GetTheme().Small->Width( T ) ) + 10 + 18; }

    Vec2 TagInput::Flow( float Width, bool Place )
    {
        const TagStyle& S = Style;
        float X = 0, Y = 0, RowH = S.ChipHeight, MaxX = 0;
        if ( Place ) Chips.clear();
        Rect In = { Bounds.x + S.Padding.Left, Bounds.y + S.Padding.Top, Width, 0 };
        for ( auto& T : GetTags() )
        {
            float W = ChipWidth( T );
            if ( X > 0 && X + W > Width ) { X = 0; Y += RowH + S.Gap; }
            if ( Place ) Chips.push_back( { In.x + X, In.y + Y, W, S.ChipHeight } );
            X += W + S.Gap;
            MaxX = std::max( MaxX, X );
        }
        float FieldW = std::max( 80.f, Width - X );
        if ( X > 0 && Width - X < 80 ) { X = 0; Y += RowH + S.Gap; FieldW = Width; }
        if ( Place ) Text->Arrange( { In.x + X, In.y + Y - 1, FieldW, S.ChipHeight + 2 } );
        return { std::max( MaxX, 120.f ), Y + RowH };
    }

    Vec2 TagInput::Measure( float Avail )
    {
        Vec2 S = Flow( Avail - Style.Padding.Horizontal(), false );
        return { std::min( Avail, S.x + Style.Padding.Horizontal() ), S.y + Style.Padding.Vertical() };
    }

    void TagInput::Arrange( Rect R )
    {
        Bounds = R;
        Flow( R.w - Style.Padding.Horizontal(), true );
    }

    Rect TagInput::XRect( size_t i ) const { const Rect& C = Chips[ i ]; return { C.Right() - 18, C.y, 16, C.h }; }

    void TagInput::Draw( DrawList& D )
    {
        const TagStyle& S = Style;
        const Font& F = *GetTheme().Small;
        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.RectStroke( Bounds, ( Text->IsFocused() ? S.StrokeFocused : S.Stroke ).Resolve(), S.Radius );
        const auto& T = GetTags();
        for ( size_t i = 0; i < Chips.size() && i < T.size(); i++ )
        {
            const Rect& C = Chips[ i ];
            D.RectFilled( C, S.Chip.Resolve(), 5 );
            D.Text( F, { C.x + 8, std::floor( C.y + ( C.h - F.LineHeight ) * 0.5f ) }, T[ i ], S.ChipText.Resolve() );
            Vec2 Xc = XRect( i ).Center();
            Color Xcol = int( i ) == HoverX ? GetTheme()[ Tok::Danger ] : S.ChipX.Resolve();
            D.Line( Xc + Vec2( -3, -3 ), Xc + Vec2( 3, 3 ), Xcol, 1.4f );
            D.Line( Xc + Vec2( 3, -3 ), Xc + Vec2( -3, 3 ), Xcol, 1.4f );
        }
        Text->Draw( D );
    }

    Widget* TagInput::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        for ( size_t i = 0; i < Chips.size(); i++ )
            if ( XRect( i ).Contains( P ) ) return this;
        if ( Text->Bounds.Contains( P ) ) return Text;
        return this;
    }

    bool TagInput::OnEvent( const Event& E )
    {
        auto XAt = [ & ]( Vec2 P ) { for ( size_t i = 0; i < Chips.size(); i++ ) if ( XRect( i ).Contains( P ) ) return int( i ); return -1; };
        switch ( E.Type )
        {
        case EventType::MouseMove: { int H = XAt( E.Pos ); if ( H != HoverX ) { HoverX = H; Redraw(); } return false; }
        case EventType::MouseLeave: HoverX = -1; Redraw(); return false;
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            PressX = XAt( E.Pos );
            if ( PressX < 0 && GetContext() ) GetContext()->SetFocus( Text ); // click anywhere = type
            return true;
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left ) return false;
            if ( PressX >= 0 && XAt( E.Pos ) == PressX ) RemoveTag( size_t( PressX ) );
            PressX = -1;
            return true;
        case EventType::KeyDown: // bubbled from the field: Backspace on empty removes the last chip
            if ( E.KeyCode == Key::Backspace && Text->GetText().empty() && !GetTags().empty() ) { RemoveTag( GetTags().size() - 1 ); return true; }
            return false;
        default:
            return false;
        }
    }
}
