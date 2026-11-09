// Style variants by name: register a style once, apply it anywhere by name (themes, data files, plugins).
//
//   tg::SetVariant<tg::ButtonStyle>( "pill", Pill );           // register (Primary/Danger/Ghost come built in)
//   tg::UseVariant( MyButton, "pill" );                        // MyButton.Style = that style
//   tg::UseVariant( Card, "card" );                            // Box: "card", "panel"
//
// Works for any widget with a `Style` member: the registry is per style type.
#pragma once
#include <string>
#include <unordered_map>

namespace tg
{
    template<typename S> std::unordered_map<std::string, S>& VariantRegistry()
    {
        static std::unordered_map<std::string, S> M;
        return M;
    }
    template<typename S> void SetVariant( const std::string& Name, const S& Style ) { VariantRegistry<S>()[ Name ] = Style; }
    template<typename S> const S* GetVariant( const std::string& Name )
    {
        auto& M = VariantRegistry<S>();
        auto It = M.find( Name );
        return It == M.end() ? nullptr : &It->second;
    }
    // W.Style = the variant; false if no variant of that name is registered for W's style type.
    template<typename W> bool UseVariant( W& Widget, const std::string& Name )
    {
        using S = std::decay_t<decltype( Widget.Style )>;
        if ( const S* V = GetVariant<S>( Name ) ) { Widget.Style = *V; Widget.Redraw(); return true; }
        return false;
    }
    void RegisterBuiltinVariants(); // done by Context; call again after editing the theme's defaults
}
