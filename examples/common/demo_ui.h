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

    // "Forms" tab
    std::string Notes = "Maintenance window: Sunday 02:00 UTC.\nRemember to rotate the API keys before the upgrade, and check that the backups from last night actually finished.";
    std::string Query;
    std::vector<std::string> Tags = { "prod", "eu-west" };
    tg::Date Expires = { 2026, 12, 31 };
    std::string LastMenu;
    bool WordWrap = true;
    std::vector<std::string> Sent;
    tg::MenuBar* Bar = nullptr;
    tg::TextArea* NotesBox = nullptr;
    tg::TextArea* Composer2 = nullptr;
    tg::TagInput* TagBox = nullptr;
    tg::SearchBox* SearchBoxW = nullptr;
    tg::ListBox* Queue = nullptr;
    tg::DatePicker* ExpiresPicker = nullptr;
    std::vector<std::string> QueueItems = { "deploy api v2.3", "migrate users table", "rotate TLS certs", "warm redis cache", "purge CDN", "notify #general" };

    // "More widgets" tab
    tg::Toolbar* Tools = nullptr;
    tg::RichText* Rich = nullptr;
    tg::VirtualList* BigList = nullptr;
    tg::Label* DropLabel = nullptr;
    tg::Box* DropBin = nullptr;
    tg::Knob *GainKnob = nullptr, *SensKnob = nullptr;
    tg::XYPad* Pad = nullptr;
    tg::TimePicker* TimeW = nullptr;
    tg::DateRangePicker* RangeW = nullptr;
    tg::Swatches* SwatchW = nullptr;
    tg::Eyedropper* Dropper = nullptr;
    tg::Pagination* Pager = nullptr;
    tg::Stepper* Steps = nullptr;
    tg::CodeEditor* Editor = nullptr;
    tg::PropertyGrid* Props = nullptr;
    tg::Accordion* Acc = nullptr;
    tg::CommandPalette* Palette = nullptr;
    bool Live = true;
    float Gain = 0, Sens = 2.5f;
    tg::Vec2 Cross = { 0, 0 };
    tg::Time BackupAt = { 3, 30 };
    tg::Date From = { 2026, 11, 1 }, To = { 2026, 11, 7 };
    int PageNo = 0, StepNo = 1, SrvPort = 443, Density = 0;
    bool Tls = true;
    std::string Host = "vps-fra-01", LastLink, Dropped;
    void OpenCommands();

    // "Layout & theme" tab
    tg::Container *GridBox = nullptr, *WrapBox = nullptr, *StackRow = nullptr;
    tg::ThemeEditor* ThemeEd = nullptr;

    // "Terminal" tab
    tg::Terminal* Shell = nullptr;
    tg::Terminal* Log = nullptr;
    tg::Label* ShellStatus = nullptr;
    std::string ShellCommand; // empty = the user's shell
    bool ShellStarted = false;
    void StartShell();

    // "Charts" tab
    tg::BarChart* Requests = nullptr;
    tg::DonutChart* Traffic = nullptr;
    tg::Gauge *CpuGauge = nullptr, *MemGauge = nullptr, *DiskGauge = nullptr;
    tg::Sparkline *SparkA = nullptr, *SparkB = nullptr, *SparkC = nullptr;
    void FilterQueue();

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
