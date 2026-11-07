// The demo app used by both examples: a VPS dashboard, a chat with a live "screen share", and a controls page.
#pragma once
#include "tg/tg.h"
#include <string>
#include <vector>

struct DemoApp
{
    void Build( tg::Context& Ctx, tg::Widget& Root );
    void ShowPage( int Index );

    // Simulated live data. In a real app these come from your network/stats threads via Ctx.Post().
    void TickStats( double T );
    void TickScreenShare( double T );
    void AddMessage( const std::string& Who, const std::string& Text, const std::string& Time );

    tg::Context* Ctx = nullptr;
    int Page = 0;

    // things we update later
    std::vector<tg::Container*> Pages;
    tg::TabView* Tabs = nullptr;
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
    tg::Combo* RegionCombo = nullptr;
    tg::MultiCombo* AlertCombo = nullptr;
    tg::ListBox* ServerList = nullptr;
    tg::ColorPicker* AccentPicker = nullptr;
    tg::Window* SettingsWin = nullptr;
    tg::Window* PlayersWin = nullptr;
    tg::Table* Procs = nullptr;
    tg::Table* Conns = nullptr;
    int ProcSel = -1;
    tg::Keybind* AimBind = nullptr;
    tg::Keybind* MenuBind = nullptr;
    tg::Key AimKey = tg::Key::Mouse5, MenuKey = tg::Key::Insert;

    // "More" tab
    int Plan = 1, Period = 1, Port = 8443;
    float UiScale = 1.f, PriceLo = 200, PriceHi = 800;
    bool DetailsOpen = true, AdvancedOpen = false, Verbose = false, Pinned = false;
    std::string LastAction;
    int Deleted = 0;
    tg::RadioGroup* PlanRadio = nullptr;
    tg::Segmented* PeriodSeg = nullptr;
    tg::NumberInput* PortInput = nullptr;
    tg::RangeSlider* Price = nullptr;
    tg::TreeView* Servers = nullptr;
    tg::Collapsible* Advanced = nullptr;
    tg::Splitter* Split = nullptr;
    tg::Button *ToastBtn = nullptr, *DeleteBtn = nullptr;
    tg::Label* ActionLabel = nullptr;

    // plain app state bound to controls, like the old tgui did with bool*
    bool AutoRestart = true, Notifications = false, Compact = false, LightTheme = false;
    float Volume = 65;
    int Region = 0, Server = 1, Mode = 1, FpsCap = 144;
    std::vector<bool> Alerts = { true, false, true, false, false };
    bool ListSelectable = true, ListMulti = false;
    tg::Color Accent = tg::Color::Hex( 0x6EB7D4 ), Tint = tg::Color::Hex( 0xE5534B, 200 );
    bool Overlay = true, ShowFps = true, Crosshair = false;
    float Opacity = 0.85f;
    std::string Nickname = "txxmo";
};
