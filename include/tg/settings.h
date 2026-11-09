// tg/settings.h — save and restore what's set in the UI: every control's value and the key binds,
// as named profiles ("Default", "Ranked", "Streaming"...).
//
//   tg::Profiles P( Ctx, "profiles" );             // a folder (created if missing)
//   P.Save( "Ranked" );                            // values of every bindable control + key binds
//   P.Load( "Ranked" );
//   for ( auto& Name : P.List() ) ...
//
// Lower level: tg::SaveValues( Root ) / tg::LoadValues( Text, Root ) — one "label <tab> values" line per
// control (the same labels key binds use).
#pragma once
#include <string>
#include <vector>

namespace tg
{
    class Context;
    class Widget;

    std::string SaveValues( Widget& Root );
    int LoadValues( const std::string& Text, Widget& Root ); // returns how many controls were set

    class Profiles
    {
    public:
        Profiles( Context& Ctx, std::string Folder );
        std::vector<std::string> List() const;   // sorted names
        bool Save( const std::string& Name ) const;
        bool Load( const std::string& Name ) const;
        bool Remove( const std::string& Name ) const;
        bool Exists( const std::string& Name ) const;
        std::string Path( const std::string& Name ) const;
        std::string Export() const;              // the current state as text (share / paste a config)
        bool Import( const std::string& Text ) const;

    private:
        Context& Ctx;
        std::string Folder;
    };
}
