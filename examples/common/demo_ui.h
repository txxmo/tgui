// The demo app used by both examples: a VPS dashboard, a chat with a live "screen share", and a controls page.
#pragma once
#include "tg/tg.h"
#include <string>
#include <vector>

struct DemoApp
{
    void Build( tg::Context& Ctx );
    void ShowPage( int Index );

    // Simulated live data. In a real app these come from your network/stats threads via Ctx.Post().
    void TickStats( double T );
    void TickScreenShare( double T );
    void AddMessage( const std::string& Who, const std::string& Text, const std::string& Time );

    tg::Context* Ctx = nullptr;
    int Page = 0;

    // things we update later
    std::vector<tg::Widget*> Pages;
    std::vector<tg::Button*> NavButtons;
    tg::Label* CpuValue = nullptr;
    tg::Label* RamValue = nullptr;
    tg::Label* DiskValue = nullptr;
    tg::Label* NetValue = nullptr;
    tg::ProgressBar *CpuBar = nullptr, *RamBar = nullptr, *DiskBar = nullptr, *NetBar = nullptr;
    tg::LineChart *CpuChart = nullptr, *NetChart = nullptr;
    tg::ScrollView* Messages = nullptr;
    tg::TextInput* Composer = nullptr;
    tg::Image* Share = nullptr;
    tg::TextureId ShareTex = 0;
    std::vector<uint32_t> SharePixels;
    tg::Label* SliderValue = nullptr;

    // plain app state bound to controls, like the old tgui did with bool*
    bool AutoRestart = true, Notifications = false, Compact = false, LightTheme = false;
    float Volume = 65;
};
