#include "tg/widgets/menu.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/box.h"
#include "tg/widgets/label.h"
#include "tg/widgets/listitem.h"
#include <cmath>

namespace tg
{
    MenuItem::MenuItem( std::string T, std::function<void()> Fn, std::string S ) : Text( std::move( T ) ), Shortcut( std::move( S ) ), OnClick( std::move( Fn ) )
    {
        Interactive = true;
    }

    Vec2 MenuItem::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        float W = 24 + std::ceil( F.Width( Text ) ) + 12;
        if ( !Shortcut.empty() ) W += 24 + std::ceil( GetTheme().Small->Width( Shortcut ) );
        return { W, F.LineHeight + 12 };
    }

    void MenuItem::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& F = *T.Body;
        Animate( HoverT, Enabled && IsHovered() ? 1.f : 0.f, 24.f );
        D.RectFilled( Bounds, Color( 255, 255, 255, 16 ).Fade( HoverT ), 5 );

        float Cy = Bounds.y + Bounds.h * 0.5f;
        if ( Checked && *Checked )
            DrawCheckMark( D, { Bounds.x + 6, std::floor( Cy - 7 ), 14, 14 }, T[ Tok::Accent ], 1.8f );

        Color Tc = Danger ? T[ Tok::Danger ] : T[ Tok::Text ];
        D.Text( F, { Bounds.x + 24, std::floor( Cy - F.LineHeight * 0.5f ) }, Text, Tc.Fade( Enabled ? 1 : 0.4f ) );
        if ( !Shortcut.empty() )
        {
            const Font& Sf = *T.Small;
            D.Text( Sf, { Bounds.Right() - 10 - Sf.Width( Shortcut ), std::floor( Cy - Sf.LineHeight * 0.5f ) }, Shortcut, T[ Tok::TextDim ] );
        }
    }

    bool MenuItem::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type != EventType::MouseUp || !IsHovered() ) return E.Type == EventType::MouseUp;
        if ( Checked ) *Checked = !*Checked;
        auto Fn = OnClick; // copy: closing the menu deletes us
        for ( Widget* W = GetParent(); W; W = W->GetParent() )
            if ( auto* P = dynamic_cast<Popup*>( W ) ) { P->Close(); break; }
        if ( Fn ) Fn();
        return true;
    }

    Menu::Menu()
    {
        MinWidth = 180;
        Layout.Gap = 1;
    }

    MenuItem& Menu::Item( std::string Text, std::function<void()> Fn, std::string Shortcut )
    {
        return Add<MenuItem>( std::move( Text ), std::move( Fn ), std::move( Shortcut ) );
    }

    MenuItem& Menu::Check( std::string Text, bool* Value, std::string Shortcut )
    {
        auto& I = Item( std::move( Text ), {}, std::move( Shortcut ) );
        I.Checked = Value;
        return I;
    }

    void Menu::Separator()
    {
        auto& Wrap = Add<Box>( Dir::Column, 0, Insets( 6, 4 ) );
        auto& Line = Wrap.Add<Box>();
        Line.Layout.Height = Px( 1 );
        Line.Style.Fill = Tok::Border;
    }

    void Menu::Header( std::string Text )
    {
        auto& L = Add<Label>( std::move( Text ) );
        L.Style.TextFont = GetTheme().Small;
        L.Style.TextColor = Tok::TextDim;
        L.Layout.Padding = Insets( 10, 6, 10, 2 );
    }

    Menu& OpenMenu( Widget& Owner, Rect Anchor )
    {
        Context* C = Owner.GetContext();
        auto& M = C->Overlay().Add<Menu>();
        M.Owner = &Owner;
        M.Anchor = Anchor;
        return M;
    }

    Menu& OpenMenu( Widget& Owner, Vec2 Pos ) { return OpenMenu( Owner, Rect{ Pos.x, Pos.y - 4, 0, 0 } ); }
}
