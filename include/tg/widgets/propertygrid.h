// PropertyGrid — an inspector: name | editor rows, grouped in collapsible categories.
//
//   auto& G = W.PropertyGrid();
//   G.Category( "Server" );
//   G.Text( "Hostname", &Host );
//   G.Int( "Port", &Port, 1, 65535 );
//   G.Bool( "TLS", &Tls );
//   G.Category( "Look" );
//   G.Color( "Accent", &Accent );
//   G.Enum( "Theme", { "Dark", "Light" }, &ThemeIdx );
//   G.Vec2( "Offset", &Off );
//   G.ReadOnly( "Uptime", "4d 3h" );
#pragma once
#include "container.h"
#include <string>
#include <vector>

namespace tg
{
    class Collapsible;
    class Label;
    class NumberInput;

    class PropertyGrid : public Container
    {
    public:
        PropertyGrid();
        float NameWidth = 130;

        Container& Category( std::string Title, bool Open = true ); // later rows go into it
        tg::Checkbox& Bool( std::string Name, bool* Value );
        tg::NumberInput& Float( std::string Name, float* Value, float Min, float Max, float Step = 0.1f, const char* Format = "%.2f" );
        tg::NumberInput& Int( std::string Name, int* Value, int Min, int Max );
        tg::Slider& Range( std::string Name, float* Value, float Min, float Max, float Step = 0 );
        tg::TextInput& Text( std::string Name, std::string* Value );
        tg::Combo& Enum( std::string Name, std::vector<std::string> Options, int* Value );
        tg::ColorPicker& Color( std::string Name, tg::Color* Value );
        Container& Vec2( std::string Name, tg::Vec2* Value );
        tg::Label& ReadOnly( std::string Name, std::string Value );
        Container& Row( std::string Name ); // your own editor: add it to the returned row

        void Draw( DrawList& D ) override;

    private:
        Container* Into = this;
    };
}
