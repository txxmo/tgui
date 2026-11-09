#include "tg/binds.h"
#include "tg/context.h"
#include "tg/widgets/button.h"
#include "tg/widgets/choice.h"
#include "tg/widgets/container.h"
#include "tg/widgets/dialog.h"
#include "tg/widgets/keybind.h"
#include "tg/widgets/label.h"
#include "tg/widgets/menu.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace tg
{
    const char* BindTriggerName( BindTrigger T )
    {
        switch ( T )
        {
        case BindTrigger::Hold: return "hold";
        case BindTrigger::Toggle: return "toggle";
        case BindTrigger::OffHold: return "off hold";
        case BindTrigger::Press: return "press";
        }
        return "?";
    }

    std::string BindNumber( double V, float Step )
    {
        char B[ 32 ];
        if ( Step >= 1 || V == std::floor( V ) ) std::snprintf( B, sizeof( B ), "%.0f", V );
        else
        {
            std::snprintf( B, sizeof( B ), "%.3f", V );
            std::string S = B;
            while ( !S.empty() && S.back() == '0' ) S.pop_back();
            if ( !S.empty() && S.back() == '.' ) S.pop_back();
            return S;
        }
        return B;
    }

    static const char* Explain( BindTrigger T )
    {
        switch ( T )
        {
        case BindTrigger::Hold: return "While the key is held. Letting go puts the old value back.";
        case BindTrigger::Toggle: return "Press to switch it on, press again to put the old value back.";
        case BindTrigger::OffHold: return "On all the time, except while the key is held.";
        case BindTrigger::Press: return "Every press sets the value. Nothing is put back.";
        }
        return "";
    }

    KeyBinds::~KeyBinds() = default;

    // ------------------------------------------------------------ the binds

    Binding& KeyBinds::Add( Widget& Target, Key K, BindTrigger T, BindValue V )
    {
        auto B = std::make_unique<Binding>();
        B->Id = NextId++;
        B->Target = &Target;
        B->K = K;
        B->Trigger = T;
        B->Value = std::move( V );
        List.push_back( std::move( B ) );
        Start( *List.back() );
        return *List.back();
    }

    void KeyBinds::Update( Binding& B, Key K, BindTrigger T, BindValue V )
    {
        Deactivate( B );
        B.K = K;
        B.Trigger = T;
        B.Value = std::move( V );
        Start( B );
    }

    void KeyBinds::Start( Binding& B )
    {
        if ( !Enabled || B.Target->BindIsAction() ) return;
        bool Held = Ctx.IsKeyDown( B.K );
        if ( ( B.Trigger == BindTrigger::OffHold && !Held ) || ( B.Trigger == BindTrigger::Hold && Held ) ) Activate( B );
    }

    void KeyBinds::Remove( uint32_t Id )
    {
        for ( size_t i = 0; i < List.size(); i++ )
            if ( List[ i ]->Id == Id )
            {
                Deactivate( *List[ i ] );
                List.erase( List.begin() + long( i ) );
                return;
            }
    }

    void KeyBinds::RemoveAll( Widget& T )
    {
        for ( Binding* B : For( T ) ) Remove( B->Id );
    }

    void KeyBinds::Clear()
    {
        while ( !List.empty() ) Remove( List.back()->Id );
    }

    std::vector<Binding*> KeyBinds::For( const Widget& T ) const
    {
        std::vector<Binding*> Out;
        for ( auto& B : List )
            if ( B->Target == &T ) Out.push_back( B.get() );
        return Out;
    }

    void KeyBinds::Activate( Binding& B )
    {
        if ( B.Active ) return;
        B.Saved = B.Target->BindGet();
        // only put back what the bind changes (multicombo items it leaves alone stay untouched)
        for ( size_t i = 0; i < B.Saved.V.size() && i < B.Value.V.size(); i++ )
            if ( std::isnan( B.Value.V[ i ] ) ) B.Saved.V[ i ] = NAN;
        B.Active = true;
        B.Target->BindSet( B.Value );
        Ctx.RequestFrame();
    }

    void KeyBinds::Deactivate( Binding& B )
    {
        if ( !B.Active ) return;
        B.Active = false;
        B.Target->BindSet( B.Saved );
        Ctx.RequestFrame();
    }

    void KeyBinds::OnKey( Key K, bool Down )
    {
        if ( !Enabled ) return;
        std::vector<uint32_t> Ids; // callbacks may add/remove binds: walk ids, re-find each
        for ( auto& B : List )
            if ( B->K == K ) Ids.push_back( B->Id );
        for ( uint32_t Id : Ids )
        {
            auto It = std::find_if( List.begin(), List.end(), [ Id ]( auto& P ) { return P->Id == Id; } );
            if ( It == List.end() ) continue;
            Binding& B = **It;
            if ( B.Target->BindIsAction() || B.Trigger == BindTrigger::Press )
            {
                if ( Down ) B.Target->BindSet( B.Value );
                continue;
            }
            switch ( B.Trigger )
            {
            case BindTrigger::Hold: Down ? Activate( B ) : Deactivate( B ); break;
            case BindTrigger::OffHold: Down ? Deactivate( B ) : Activate( B ); break;
            case BindTrigger::Toggle: if ( Down ) { B.Active ? Deactivate( B ) : Activate( B ); } break;
            default: break;
            }
        }
    }

    void KeyBinds::Forget( Widget* W )
    {
        List.erase( std::remove_if( List.begin(), List.end(), [ W ]( auto& B ) { return B->Target == W; } ), List.end() );
    }

    std::string KeyBinds::Describe( const Binding& B ) const
    {
        std::string S = KeyName( B.K );
        if ( B.Target->BindIsAction() ) return S + "  \xC2\xB7  click";
        S += std::string( "  \xC2\xB7  " ) + BindTriggerName( B.Trigger );
        std::string V = B.Target->BindText( B.Value );
        if ( !V.empty() ) S += "  \xE2\x86\x92  " + V;
        return S;
    }

    // ------------------------------------------------------------ UI

    void KeyBinds::AddMenuItems( Widget& T, Menu& M )
    {
        if ( !Enabled || !T.Bindable || !T.CanBind() ) return;
        auto Binds = For( T );
        M.Header( "Key binds" );
        for ( Binding* B : Binds )
        {
            uint32_t Id = B->Id;
            M.Item( Describe( *B ), [ this, Id ] {
                for ( auto& P : List )
                    if ( P->Id == Id ) { OpenEditor( *P->Target, P.get() ); return; }
            } );
        }
        Widget* Tp = &T;
        M.Item( Binds.empty() ? "Add key bind..." : "Add another key bind...", [ this, Tp ] { OpenEditor( *Tp ); } );
        if ( !Binds.empty() )
            M.Item( Binds.size() > 1 ? "Remove key binds" : "Remove key bind", [ this, Tp ] {
                RemoveAll( *Tp );
                if ( OnEdit ) OnEdit();
            } ).Danger = true;
    }

    void KeyBinds::OpenEditor( Widget& T, Binding* Ex )
    {
        // the editor's state lives as long as the dialog (its buttons hold it)
        struct State
        {
            Key K = Key::None;
            int Trigger = 0;
            BindValue V;
            uint32_t Id = 0;
        };
        auto S = std::make_shared<State>();
        if ( Ex )
        {
            S->K = Ex->K;
            S->Trigger = int( Ex->Trigger );
            S->V = Ex->Value;
            S->Id = Ex->Id;
        }
        else S->V = T.BindDefault();

        std::string Name = T.BindName();
        Dialog& D = OpenDialog( Ctx, Name.empty() ? "Key bind" : "Key bind  \xC2\xB7  " + Name );
        Container& B = D.Body();
        B.Layout.Gap = 10;
        auto& Kb = B.Keybind( "Key", &S->K );
        Kb.AllowModes = false;
        Kb.Tooltip = "Click, then press a key or mouse button. Escape clears it.";
        if ( T.BindIsAction() ) B.Caption( "Pressing the key clicks it." );
        else
        {
            auto& Row = B.Row( 10 );
            Row.Layout.CrossAlign = Align::Center;
            auto& L = Row.Text( "Trigger" );
            L.Style.TextColor = Tok::TextDim;
            L.Layout.Width = Grow();
            auto& Sg = Row.Segmented( { "Hold", "Toggle", "Off hold", "Press" }, &S->Trigger );
            auto& Why = B.Caption( Explain( BindTrigger( S->Trigger ) ) );
            Sg.OnChange = [ &Why ]( int i ) { Why.SetText( Explain( BindTrigger( i ) ) ); };
            auto& H = B.Text( "Value while active" );
            H.Style.TextColor = Tok::TextDim;
            T.BindEditor( B, S->V );
        }

        Widget* Tp = &T;
        // a target that dies while the dialog is open is noticed through Forget(): look it up by id / pointer
        auto Alive = [ this, Tp ] {
            std::vector<Widget*> Stack = { &Ctx.Root() };
            while ( !Stack.empty() )
            {
                Widget* W = Stack.back();
                Stack.pop_back();
                if ( W == Tp ) return true;
                for ( auto& K : W->Children() ) Stack.push_back( K.get() );
            }
            return false;
        };
        if ( Ex )
            D.AddButton( "Remove", [ this, S ] {
                Remove( S->Id );
                if ( OnEdit ) OnEdit();
            } ).Style = Button::Danger();
        D.AddButton( "Cancel" );
        D.AddButton( "Save", [ this, S, Tp, Alive ] {
            if ( S->K == Key::None || !Alive() ) return;
            Binding* Existing = nullptr;
            for ( auto& P : List )
                if ( P->Id == S->Id ) Existing = P.get();
            if ( Existing ) Update( *Existing, S->K, BindTrigger( S->Trigger ), S->V );
            else Add( *Tp, S->K, BindTrigger( S->Trigger ), S->V );
            if ( OnEdit ) OnEdit();
        } ).Style = Button::Primary();
        D.OnResult = [ S ]( int ) {}; // keeps the state alive with the dialog
    }

    // ------------------------------------------------------------ save / load

    std::string KeyBinds::Save() const
    {
        std::string Out;
        for ( auto& B : List )
        {
            std::string N = B->Target->BindName();
            for ( char& C : N )
                if ( C == '\t' || C == '\n' || C == '\r' ) C = ' ';
            char Buf[ 96 ];
            std::snprintf( Buf, sizeof( Buf ), "\t%u\t%s\t%d\t", unsigned( B->K ), KeyName( B->K ), int( B->Trigger ) );
            Out += N + Buf;
            for ( size_t i = 0; i < B->Value.V.size(); i++ )
            {
                if ( i ) Out += ',';
                double V = B->Value.V[ i ];
                if ( std::isnan( V ) ) Out += '-';
                else { std::snprintf( Buf, sizeof( Buf ), "%.9g", V ); Out += Buf; }
            }
            Out += '\n';
        }
        return Out;
    }

    int KeyBinds::Load( const std::string& Text, Widget& Root )
    {
        Clear();
        std::vector<Widget*> Targets, Stack = { &Root };
        while ( !Stack.empty() )
        {
            Widget* W = Stack.back();
            Stack.pop_back();
            if ( W->CanBind() && W->Bindable ) Targets.push_back( W );
            auto& K = W->Children();
            for ( auto It = K.rbegin(); It != K.rend(); ++It ) Stack.push_back( It->get() );
        }
        int N = 0;
        std::istringstream In( Text );
        std::string Line;
        while ( std::getline( In, Line ) )
        {
            std::vector<std::string> F;
            std::istringstream Ls( Line );
            for ( std::string X; std::getline( Ls, X, '\t' ); ) F.push_back( X );
            if ( F.size() < 4 ) continue;
            Key K = Key( std::atoi( F[ 1 ].c_str() ) );
            int T = std::atoi( F[ 3 ].c_str() );
            if ( size_t( K ) >= size_t( Key::Count ) || T < 0 || T > 3 ) continue;
            BindValue V;
            if ( F.size() > 4 )
            {
                std::istringstream Vs( F[ 4 ] );
                for ( std::string X; std::getline( Vs, X, ',' ); ) V.V.push_back( X == "-" ? NAN : std::atof( X.c_str() ) );
            }
            for ( Widget* W : Targets )
                if ( W->BindName() == F[ 0 ] )
                {
                    Add( *W, K, BindTrigger( T ), V );
                    N++;
                    break;
                }
        }
        return N;
    }
}
