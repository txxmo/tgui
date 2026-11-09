// tg/history.h — undo / redo. Ctx.History records edits to controls by itself (a checkbox click,
// a slider drag, a combo pick, arrow keys on a focused slider...); Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z)
// walk it. Push your own steps for anything else:
//
//   Ctx.History.Push( "Delete server", [=] { Restore( S ); }, [=] { Delete( S ); } );
#pragma once
#include "widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Context;

    class History
    {
    public:
        explicit History( Context& C ) : Ctx( C ) {}
        bool Enabled = true;       // record control edits + Ctrl+Z / Ctrl+Y
        size_t Limit = 200;
        std::function<void()> OnChange; // after push / undo / redo (update an Edit menu)

        void Push( std::string Name, std::function<void()> Undo, std::function<void()> Redo );
        bool Undo();
        bool Redo();
        bool CanUndo() const { return Pos > 0; }
        bool CanRedo() const { return Pos < Steps.size(); }
        std::string UndoName() const { return CanUndo() ? Steps[ Pos - 1 ].Name : std::string(); }
        std::string RedoName() const { return CanRedo() ? Steps[ Pos ].Name : std::string(); }
        void Clear();
        bool Replaying() const { return Busy; }

        // ---- Context internals: automatic recording of control edits
        void Begin( const std::vector<Widget*>& Candidates );
        void End();
        void Forget( Widget* W );

    private:
        struct Step
        {
            std::string Name;
            std::function<void()> Undo, Redo;
            Widget* Target = nullptr; // for control edits: merge repeats, drop when the widget dies
            BindValue Before, After;
            double Time = 0;
        };
        Context& Ctx;
        std::vector<Step> Steps;
        size_t Pos = 0;
        bool Busy = false;
        std::vector<std::pair<Widget*, BindValue>> Pending;
        void PushStep( Step S );
    };
}
