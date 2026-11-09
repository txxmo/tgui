// Command palette — Ctrl+K / Ctrl+Shift+P style fuzzy launcher over everything your app can do.
//
//   tg::CommandList Cmds;
//   Cmds.Add( "Restart server", [&] { Restart(); }, tg::Icon::RotateCw, "Ctrl+R", "Server" );
//   Cmds.Add( "Toggle dark mode", [&] { ToggleTheme(); }, tg::Icon::Moon );
//   Ctx.Shortcut( "Ctrl+K", [&] { tg::OpenPalette( Ctx, Cmds ); } );
//
// Type to filter (fuzzy: "rst srv" finds "Restart server"), Up / Down, Enter runs, Escape closes.
#pragma once
#include "../widget.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Context;
    class TextInput;
    class VirtualList;

    struct Command
    {
        std::string Name, Icon, Shortcut, Group, Keywords; // Keywords: extra words that match
        std::function<void()> Run;
    };

    struct CommandList
    {
        std::vector<Command> Items;
        Command& Add( std::string Name, std::function<void()> Run, std::string Icon = {}, std::string Shortcut = {}, std::string Group = {} )
        {
            Items.push_back( { std::move( Name ), std::move( Icon ), std::move( Shortcut ), std::move( Group ), {}, std::move( Run ) } );
            return Items.back();
        }
    };

    // Fuzzy match: every pattern character in order (case-insensitive); higher = better (word starts,
    // consecutive runs). -1 = no match. Matched gets the byte indices of the matched characters.
    int FuzzyScore( const std::string& Pattern, const std::string& Text, std::vector<int>* Matched = nullptr );

    class CommandPalette : public Widget
    {
    public:
        CommandPalette( std::vector<Command> Commands, std::string Placeholder );
        float Width = 560;
        int MaxRows = 9;
        void Close();
        void Filter( const std::string& Query );
        size_t Matches() const { return Shown.size(); }
        void RunSelected();

        void Arrange( Rect R ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Widget* HitTest( Vec2 P ) override;

    private:
        std::vector<Command> Cmds;
        struct Hit { size_t Cmd; int Score; std::vector<int> Chars; };
        std::vector<Hit> Shown;
        TextInput* Input = nullptr;
        VirtualList* List = nullptr;
        Widget* Card = nullptr;
        bool Closed = false;
        float OpenT = 0;
    };

    CommandPalette& OpenPalette( Context& Ctx, const CommandList& Commands, std::string Placeholder = "Type a command..." );
}
