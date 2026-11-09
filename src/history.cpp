#include "tg/history.h"
#include "tg/context.h"
#include <cmath>

namespace tg
{
    static bool Same( const BindValue& A, const BindValue& B )
    {
        if ( A.V.size() != B.V.size() ) return false;
        for ( size_t i = 0; i < A.V.size(); i++ )
            if ( A.V[ i ] != B.V[ i ] && !( std::isnan( A.V[ i ] ) && std::isnan( B.V[ i ] ) ) ) return false;
        return true;
    }

    void History::Push( std::string Name, std::function<void()> U, std::function<void()> R )
    {
        Step S;
        S.Name = std::move( Name );
        S.Undo = std::move( U );
        S.Redo = std::move( R );
        S.Time = Ctx.Time();
        PushStep( std::move( S ) );
    }

    void History::PushStep( Step S )
    {
        Steps.resize( Pos ); // a new edit drops the redo branch
        Steps.push_back( std::move( S ) );
        if ( Steps.size() > Limit ) Steps.erase( Steps.begin() );
        Pos = Steps.size();
        if ( OnChange ) OnChange();
    }

    bool History::Undo()
    {
        if ( !CanUndo() ) return false;
        Busy = true;
        Step& S = Steps[ --Pos ];
        if ( S.Target ) S.Target->BindSet( S.Before );
        else if ( S.Undo ) S.Undo();
        Busy = false;
        Ctx.RequestFrame();
        if ( OnChange ) OnChange();
        return true;
    }

    bool History::Redo()
    {
        if ( !CanRedo() ) return false;
        Busy = true;
        Step& S = Steps[ Pos++ ];
        if ( S.Target ) S.Target->BindSet( S.After );
        else if ( S.Redo ) S.Redo();
        Busy = false;
        Ctx.RequestFrame();
        if ( OnChange ) OnChange();
        return true;
    }

    void History::Clear()
    {
        Steps.clear();
        Pos = 0;
        if ( OnChange ) OnChange();
    }

    void History::Begin( const std::vector<Widget*>& Candidates )
    {
        Pending.clear();
        if ( !Enabled || Busy ) return;
        for ( Widget* W : Candidates ) Pending.push_back( { W, W->BindGet() } );
    }

    void History::End()
    {
        auto P = std::move( Pending );
        Pending.clear();
        if ( !Enabled || Busy ) return;
        for ( auto& [ W, Before ] : P )
        {
            BindValue After = W->BindGet();
            if ( Same( Before, After ) ) continue;
            // repeated edits of the same control in quick succession (arrow keys held, several clicks
            // on a slider) become one step
            if ( Pos == Steps.size() && Pos > 0 && Steps[ Pos - 1 ].Target == W && Ctx.Time() - Steps[ Pos - 1 ].Time < 0.8 )
            {
                Steps[ Pos - 1 ].After = After;
                Steps[ Pos - 1 ].Time = Ctx.Time();
                if ( OnChange ) OnChange();
                continue;
            }
            Step S;
            std::string N = W->BindName();
            S.Name = N.empty() ? "Change" : "Change " + N;
            S.Target = W;
            S.Before = Before;
            S.After = After;
            S.Time = Ctx.Time();
            PushStep( std::move( S ) );
        }
    }

    void History::Forget( Widget* W )
    {
        for ( size_t i = 0; i < Steps.size(); )
            if ( Steps[ i ].Target == W )
            {
                Steps.erase( Steps.begin() + long( i ) );
                if ( Pos > i ) Pos--;
            }
            else i++;
        for ( size_t i = 0; i < Pending.size(); )
            if ( Pending[ i ].first == W ) Pending.erase( Pending.begin() + long( i ) );
            else i++;
    }
}
