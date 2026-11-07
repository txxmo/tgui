#include "demo_ui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

using namespace tg;

// ---------------------------------------------------------------- small custom widgets
// Writing your own widget = subclass, set Layout size, draw. That's it.

// Round avatar with initials.
class Avatar : public Widget
{
public:
    Avatar( std::string Initials, Color Bg ) : Initials( std::move( Initials ) ), Bg( Bg )
    {
        Layout.Width = Px( 34 );
        Layout.Height = Px( 34 );
    }
    void Draw( DrawList& D ) override
    {
        const Font& F = *GetTheme().Bold;
        D.CircleFilled( Bounds.Center(), Bounds.w * 0.5f, Bg );
        float W = F.Width( Initials );
        D.Text( F, { std::floor( Bounds.Center().x - W * 0.5f ), std::floor( Bounds.Center().y - F.LineHeight * 0.5f ) }, Initials, Color( 255, 255, 255 ) );
    }
    std::string Initials;
    Color Bg;
};

// Colored status dot (service up/down, "LIVE" indicator...).
class Dot : public Widget
{
public:
    explicit Dot( Paint C, float Size = 8 ) : C( C ) { Layout.Width = Px( Size ); Layout.Height = Px( Size ); }
    void Draw( DrawList& D ) override { D.CircleFilled( Bounds.Center(), Bounds.w * 0.5f, C.Resolve() ); }
    Paint C;
};

static Label& AddText( Widget& Parent, std::string Text, Font* F = nullptr, Paint C = Tok::Text, bool Wrap = false )
{
    auto& L = Parent.Add<Label>( std::move( Text ), Wrap );
    L.Style.TextFont = F;
    L.Style.TextColor = C;
    return L;
}

static Box& AddCard( Widget& Parent, Dir D = Dir::Column, float Gap = 10 )
{
    auto& B = Parent.Add<Box>( D, Gap, Insets( 16 ) );
    B.Style = Box::Card();
    return B;
}

// ---------------------------------------------------------------- pages

static Box& BuildStatCard( Widget& Parent, const char* Title, Label*& Value, ProgressBar*& Bar )
{
    Theme& T = GetTheme();
    auto& Card = AddCard( Parent, Dir::Column, 8 );
    Card.Layout.Width = Grow();
    AddText( Card, Title, T.Small, Tok::TextDim );
    Value = &AddText( Card, "--", T.Title );
    Bar = &Card.Add<ProgressBar>();
    Bar->Style.Thresholds = true;
    return Card;
}

static void BuildDashboard( DemoApp& App, Container& Page )
{
    Theme& T = GetTheme();
    Page.Layout.Gap = 16;

    // header
    auto& Header = AddRow( Page, 12 );
    Header.Layout.CrossAlign = Align::Center;
    auto& Titles = AddColumn( Header, 2 );
    auto& NameRow = AddRow( Titles, 8 );
    NameRow.Layout.CrossAlign = Align::Center;
    NameRow.Add<Dot>( Tok::Success, 9 );
    AddText( NameRow, "vps-fra-01", T.Title );
    AddText( Titles, "Ubuntu 24.04  \xC2\xB7  4 vCPU  \xC2\xB7  8 GB RAM  \xC2\xB7  10.0.4.21", T.Small, Tok::TextDim );
    AddSpacer( Header );
    Header.Add<Button>( "SSH" );
    Header.Add<Button>( "Restart" ).Style = Button::Primary();

    // stat cards
    auto& Stats = AddRow( Page, 16 );
    BuildStatCard( Stats, "CPU", App.CpuValue, App.CpuBar );
    BuildStatCard( Stats, "MEMORY", App.RamValue, App.RamBar );
    BuildStatCard( Stats, "DISK", App.DiskValue, App.DiskBar );
    BuildStatCard( Stats, "NETWORK", App.NetValue, App.NetBar );

    // charts
    auto& Charts = AddRow( Page, 16 );
    auto& CpuCard = AddCard( Charts );
    CpuCard.Layout.Width = Grow( 2 );
    AddText( CpuCard, "CPU usage", T.Bold );
    App.CpuChart = &CpuCard.Add<LineChart>( 90 );
    App.CpuChart->Unit = "%";
    App.CpuChart->Style.Height = 150;

    auto& NetCard = AddCard( Charts );
    NetCard.Layout.Width = Grow( 1 );
    AddText( NetCard, "Bandwidth", T.Bold );
    App.NetChart = &NetCard.Add<LineChart>( 60 );
    App.NetChart->AutoRange = true;
    App.NetChart->Style.Line = Tok::Success;
    App.NetChart->Style.Height = 150;

    // services
    auto& Svc = AddCard( Page, Dir::Column, 0 );
    AddText( Svc, "Services", T.Bold ).Layout.Padding = Insets( 0, 0, 0, 8 );
    struct S { const char* Name; const char* Desc; bool Up; };
    static bool States[ 4 ] = { true, true, false, true };
    S List[] = { { "nginx", "reverse proxy  \xC2\xB7  :80, :443", true }, { "postgres", "database  \xC2\xB7  :5432", true },
                 { "redis", "cache  \xC2\xB7  stopped 2h ago", false }, { "chat-server", "websocket  \xC2\xB7  :8443", true } };
    for ( int i = 0; i < 4; i++ )
    {
        if ( i ) AddDivider( Svc );
        auto& Row = AddRow( Svc, 12, Insets( 0, 10 ) );
        Row.Layout.CrossAlign = Align::Center;
        Row.Add<Dot>( List[ i ].Up ? Tok::Success : Tok::Danger );
        auto& Col = AddColumn( Row, 2 );
        Col.Layout.Width = Grow();
        AddText( Col, List[ i ].Name, T.Bold );
        AddText( Col, List[ i ].Desc, T.Small, Tok::TextDim );
        Row.Add<Button>( "Logs" ).Style = Button::Ghost();
        Row.Add<Toggle>( "", &States[ i ] );
    }
}

static void BuildChat( DemoApp& App, Container& Page )
{
    Theme& T = GetTheme();
    Page.Layout.Direction = Dir::Row;
    Page.Style = Box::Card(); // frame the chat like a panel

    // conversation column
    auto& Conv = AddColumn( Page, 0 );
    Conv.Layout.Width = Grow();

    auto& Head = AddRow( Conv, 8, Insets( 20, 14 ) );
    Head.Layout.CrossAlign = Align::Center;
    AddText( Head, "# general", T.Title );
    AddText( Head, "builds, deploys & memes", T.Small, Tok::TextDim );
    AddDivider( Conv );

    App.Messages = &Conv.Add<ScrollView>();
    App.Messages->StickToBottom = true;
    App.Messages->Content().Layout.Padding = Insets( 20, 12 );
    App.Messages->Content().Layout.Gap = 14;

    auto& Bottom = AddRow( Conv, 10, Insets( 20, 14 ) );
    App.Composer = &Bottom.Add<TextInput>( "Message #general" );
    App.Composer->Layout.Width = Grow();
    App.Composer->ClearOnSubmit = true;
    DemoApp* A = &App;
    App.Composer->OnSubmit = [ A ]( const std::string& Text ) { if ( !Text.empty() ) A->AddMessage( "you", Text, "now" ); };
    Bottom.Add<Button>( "Send", [ A ]() {
        if ( !A->Composer->GetText().empty() ) { A->AddMessage( "you", A->Composer->GetText(), "now" ); A->Composer->SetText( {} ); }
    } ).Style = Button::Primary();

    AddDivider( Page, true );

    // side panel: screen share + members
    auto& Side = AddColumn( Page, 14, Insets( 16 ) );
    Side.Layout.Width = Px( 340 );
    auto& ShareCard = AddCard( Side, Dir::Column, 10 );
    auto& ShareHead = AddRow( ShareCard, 8 );
    ShareHead.Layout.CrossAlign = Align::Center;
    ShareHead.Add<Dot>( Tok::Danger );
    AddText( ShareHead, "LIVE", T.Small, Tok::Danger );
    AddText( ShareHead, "alex is sharing their screen", T.Small, Tok::TextDim );

    const int W = 320, H = 180;
    App.SharePixels.assign( W * H, 0xFF000000u );
    App.ShareTex = App.Ctx->GetRenderer().CreateTexture( W, H, App.SharePixels.data() );
    App.Share = &ShareCard.Add<Image>( App.ShareTex, W, H );
    App.Share->Style.Radius = 6;
    auto& ShareBtns = AddRow( ShareCard, 8 );
    ShareBtns.Add<Button>( "Watch" ).Layout.Width = Grow();
    ShareBtns.Add<Button>( "Pop out" ).Style = Button::Ghost();

    AddText( Side, "ONLINE \xE2\x80\x94 4", T.Small, Tok::TextDim );
    struct M { const char* Init; const char* Name; uint32_t Col; const char* Status; };
    M Members[] = { { "AL", "alex", 0x8B5CF6, "streaming" }, { "MO", "mo", 0x0EA5E9, "deploying v2.3" }, { "RK", "rika", 0xF97316, "idle" }, { "YO", "you", 0x22C55E, "" } };
    for ( auto& Mb : Members )
    {
        auto& R = AddRow( Side, 10 );
        R.Layout.CrossAlign = Align::Center;
        R.Add<Avatar>( Mb.Init, Color::Hex( Mb.Col ) );
        auto& C = AddColumn( R, 1 );
        AddText( C, Mb.Name, T.Bold );
        if ( *Mb.Status ) AddText( C, Mb.Status, T.Small, Tok::TextDim );
    }

    App.AddMessage( "mo", "deploying v2.3 to fra-01, should be quick", "10:41" );
    App.AddMessage( "rika", "nice. is the websocket reconnect fix in there? the chat kept dropping for me on wifi yesterday", "10:42" );
    App.AddMessage( "mo", "yep, plus the backoff change. ping me if you still see drops", "10:42" );
    App.AddMessage( "alex", "sharing my screen \xE2\x80\x94 check the latency graph on the right, p99 went from 180ms to 40ms after the redis change", "10:44" );
}

// tgui-style: two rows of titled groupboxes that fill the page. Every control is one line.
static void BuildControls( DemoApp& App, Container& Page )
{
    Page.Layout.Gap = 12;

    auto& Top = Page.Columns( 12 );

    auto& Toggles = Top.Group( "Toggles" );
    Toggles.Checkbox( "Auto-restart crashed services", &App.AutoRestart );
    Toggles.Checkbox( "Desktop notifications", &App.Notifications );
    Toggles.Checkbox( "Compact message layout", &App.Compact );
    // The thing that was painful in imgui: restyling ONE checkbox. Here it's just its Style.
    auto& Custom = Toggles.Checkbox( "Custom styled (square, red)" );
    Custom.Style.Radius = 0;
    Custom.Style.Fill = Tok::Danger;
    Custom.Style.BoxHoverStroke = Tok::Danger;
    Custom.SetChecked( true );
    Toggles.Separator();
    Toggles.Toggle( "Light theme", &App.LightTheme ).OnChange = [ &App ]( bool On ) {
        GetTheme().UseColors( On ? Theme::Light() : Theme::Dark() );
        App.Ctx->RequestFrame();
    };

    auto& Inputs = Top.Group( "Inputs" );
    Inputs.LabelWidth = 64;
    Inputs.Slider( "Volume", &App.Volume, 0, 100, 1 );
    Inputs.Input( "Server address" ).SetText( "ssh://root@10.0.4.21" );
    Inputs.Input( "Password" ).Password = true;
    Inputs.Progress( 0.42f ).Style.Height = 6;

    auto& Buttons = Top.Group( "Buttons" );
    Buttons.Button( "Default" );
    Buttons.Button( "Primary" ).Style = Button::Primary();
    Buttons.Button( "Danger" ).Style = Button::Danger();
    Buttons.Button( "Ghost" ).Style = Button::Ghost();
    Buttons.Button( "Disabled" ).Enabled = false;

    auto& Bottom = Page.Columns( 12 );

    auto& Pick = Bottom.Group( "Pickers" );
    Pick.LabelWidth = 80;
    App.RegionCombo = &Pick.Combo( "Region", { "Frankfurt", "Amsterdam", "London", "New York", "Toronto", "S\xC3\xA3o Paulo",
                                               "Singapore", "Tokyo", "Sydney", "Mumbai", "Johannesburg" }, &App.Region );
    App.AlertCombo = &Pick.MultiCombo( "Alerts", { "CPU > 90%", "Disk > 85%", "Service down", "New SSH login", "Backup failed" }, &App.Alerts );
    App.AccentPicker = &Pick.ColorPicker( "Accent", &App.Accent, false );
    App.AccentPicker->OnChange = [ &App ]( Color C ) {
        GetTheme()[ Tok::Accent ] = C;
        GetTheme()[ Tok::AccentHover ] = C;
        App.Ctx->RequestFrame();
    };
    Pick.ColorPicker( "Highlight", &App.Tint );

    auto& Servers = Bottom.Group( "Servers" );
    App.ServerList = &Servers.List( { "vps-fra-01  \xC2\xB7  Frankfurt", "vps-ams-02  \xC2\xB7  Amsterdam", "vps-nyc-01  \xC2\xB7  New York",
                                      "vps-sgp-01  \xC2\xB7  Singapore", "vps-syd-01  \xC2\xB7  Sydney", "db-fra-01  \xC2\xB7  Frankfurt",
                                      "cache-ams-01  \xC2\xB7  Amsterdam" }, SelectMode::Single, &App.Server );
    App.ServerList->Layout.Height = Grow();
    App.ServerList->Layout.MinHeight = 120;
    auto& Opts = Servers.Row( 18 );
    auto Apply = [ &App ] { App.ServerList->SetMode( !App.ListSelectable ? SelectMode::None : App.ListMulti ? SelectMode::Multi : SelectMode::Single ); };
    Opts.Toggle( "Selectable", &App.ListSelectable ).OnChange = [ Apply ]( bool ) { Apply(); };
    Opts.Toggle( "Multi-select", &App.ListMulti ).OnChange = [ Apply ]( bool ) { Apply(); };

    Page.Caption( "Every widget is one header + one .cpp in src/widgets: tunables in its Style struct, look in its Draw(). "
                  "Change one checkbox (Style), every checkbox (Checkbox::Defaults()), every accent (Theme), or edit Draw()." );
}

// Floating windows. The first one is the old tgui layout: classic chrome, tabs, groupboxes side by side.
static void BuildWindows( DemoApp& App, Container& Page )
{
    Page.Layout.Direction = Dir::Free;

    auto& Hint = Page.Add<Label>( "Drag the title bars, resize from the corner, close with the X.  Code: examples/common/demo_ui.cpp \xE2\x86\x92 BuildWindows()" );
    Hint.Style.TextColor = Tok::TextDim;
    Hint.Style.TextFont = GetTheme().Small;
    Hint.Layout.Offset = { 4, 4 };

    auto& W = Page.Add<Window>( "Test Window", Vec2{ 4, 32 }, Vec2{ 480, 380 } );
    App.SettingsWin = &W;
    W.Chrome = Window::Classic();
    W.Footer = "v.2.5";
    W.Layout.Padding = Insets( 8 );

    auto& Visuals = W.Tab( "Visuals" );
    auto& Cols = Visuals.Columns( 8 );
    auto& G1 = Cols.Group( "Example Group" );
    G1.LabelWidth = 56;
    G1.Checkbox( "Enable overlay", &App.Overlay );
    G1.Checkbox( "Show FPS counter", &App.ShowFps );
    G1.Checkbox( "Crosshair", &App.Crosshair );
    App.AimBind = &G1.Keybind( "Aim key", &App.AimKey );
    App.AimBind->ShowMode = true;
    G1.Slider( "Opacity", &App.Opacity, 0, 1 );
    G1.SliderInt( "FPS cap", &App.FpsCap, 30, 240 );
    G1.Combo( "", { "Top left", "Top right", "Bottom left", "Bottom right" }, &App.Mode );
    G1.Checkbox( "Outline text" );
    G1.Checkbox( "Shadow" );

    auto& G2 = Cols.Group( "Example Group 2" );
    G2.LabelWidth = 0;
    G2.ColorPicker( "Tint", &App.Tint );
    G2.ColorPicker( "Text", nullptr, false ).SetColor( Color( 230, 232, 238 ) );
    G2.MultiCombo( "", { "Players", "Items", "Vehicles", "Waypoints" } );
    G2.Toggle( "Streamer mode" );
    auto& Btns = G2.Row();
    Btns.Button( "Apply" ).Style = Button::Primary();
    Btns.Button( "Reset" );

    auto& Misc = W.Tab( "Misc" );
    auto& MCols = Misc.Columns( 8 );
    auto& G3 = MCols.Group( "Movement" );
    G3.Checkbox( "Auto run" );
    G3.Checkbox( "Bunny hop" );
    G3.Slider( "Speed", nullptr, 0, 10 );
    G3.Separator();
    App.MenuBind = &G3.Keybind( "Menu key", &App.MenuKey );
    App.MenuBind->AllowModes = false;
    G3.Caption( "Press it anywhere to show/hide this window (Escape while binding = none)." );
    auto& G4 = MCols.Group( "Chat" );
    G4.Checkbox( "Timestamps" );
    G4.Checkbox( "Filter profanity" );
    G4.Input( "Nickname", &App.Nickname );

    auto& Cfg = W.Tab( "Config" );
    auto& CCols = Cfg.Columns( 8 );
    auto& G5 = CCols.Group( "Configs" );
    G5.List( { "default", "legit", "streaming", "lan" }, SelectMode::Single ).Layout.Height = Grow();
    auto& G6 = CCols.Group( "Actions" );
    G6.Input( "Config name" );
    G6.Button( "Save" ).Style = Button::Primary();
    G6.Button( "Load" );
    G6.Button( "Delete" ).Style = Button::Danger();

    auto& P = Page.Add<Window>( "Players", Vec2{ 520, 60 }, Vec2{ 300, 360 } );
    App.PlayersWin = &P;
    P.Input( "Nickname", &App.Nickname );
    auto& L = P.List( { "alex", "mo", "rika", "you", "guest_4021", "guest_7734", "nightowl", "packetloss", "sudo_rm" }, SelectMode::Multi );
    L.Layout.Height = Grow();
    P.Caption( "Multi-select list in a resizable window." );
}

// Tables: a live, sortable process list with custom cells, and a plain read-only table.
static void BuildTables( DemoApp& App, Container& Page )
{
    Theme& T = GetTheme();
    Page.Layout.Gap = 16;
    auto& Head = Page.Row( 10 );
    Head.Layout.CrossAlign = Align::Center;
    Head.Add<Label>( "Processes" ).Style.TextFont = T.Title;
    Head.Caption( "click a header to sort, drag a header edge to resize, rows update live" );

    auto& Body = Page.Row( 16 );
    Body.Layout.Height = Grow();
    Body.Layout.CrossAlign = Align::Stretch;

    // ---- processes: built by hand for full control over columns
    auto& Tbl = Body.Add<Table>();
    App.Procs = &Tbl;
    Tbl.Layout.Width = Grow( 2 );
    Tbl.AddColumn( "PID", Px( 80 ), Align::End );
    Tbl.AddColumn( "Name", Grow() );
    Tbl.AddColumn( "User", Px( 100 ) );
    Tbl.AddColumn( "CPU", Px( 80 ), Align::End );
    Tbl.AddColumn( "Memory", Px( 170 ) );
    Tbl.AddColumn( "Status", Px( 100 ) );
    Tbl.Bind = &App.ProcSel;

    const char* Names[] = { "nginx: worker", "postgres", "redis-server", "chat-server", "node", "dockerd", "containerd", "sshd",
                            "systemd-journald", "cron", "python3 stats.py", "prometheus", "grafana-server", "fail2ban", "rsyslogd",
                            "tg_showcase", "ffmpeg -i rtmp", "certbot", "unattended-upgr", "bash" };
    const char* Users[] = { "www-data", "postgres", "redis", "chat", "app", "root", "root", "root", "root", "root",
                            "app", "prometheus", "grafana", "root", "syslog", "you", "chat", "root", "root", "you" };
    for ( int i = 0; i < 60; i++ )
    {
        int k = i % 20;
        float Cpu = std::fmod( 37.f * i * 0.731f, 48.f ) * ( k < 6 ? 1.f : 0.15f );
        float Mem = std::fmod( 113.f * ( i + 3 ), 1900.f ) + 12;
        char C[ 16 ], M[ 24 ];
        std::snprintf( C, sizeof( C ), "%.1f%%", Cpu );
        std::snprintf( M, sizeof( M ), "%.0f MB", Mem );
        Tbl.AddRow( { std::to_string( 1200 + i * 37 ), Names[ k ], Users[ k ], C, M, i % 17 == 5 ? "stopped" : i % 7 == 3 ? "sleeping" : "running" } );
    }
    Tbl.SortBy( 3, false ); // CPU, highest first

    // Memory column: a bar + the number. Status column: colored text.
    Tbl.DrawCell = [ &Tbl ]( DrawList& D, Rect Cell, size_t Row, size_t Col ) {
        if ( Col != 4 ) return false;
        const Font& F = *GetTheme().Body;
        float Mb = float( std::atof( Tbl.GetCell( Row, Col ).c_str() ) );
        Rect In = Cell.Shrink( Insets( 12, 0 ) );
        Rect Bar = { In.x, std::floor( In.y + In.h * 0.5f - 3 ), In.w - 70, 6 };
        D.RectFilled( Bar, GetTheme()[ Tok::Raised ], 3 );
        D.RectFilled( { Bar.x, Bar.y, std::max( 6.f, Bar.w * std::min( 1.f, Mb / 2048 ) ), Bar.h }, GetTheme()[ Tok::Accent ], 3 );
        const std::string& Txt = Tbl.GetCell( Row, Col );
        D.Text( F, { In.Right() - F.Width( Txt ), std::floor( In.y + ( In.h - F.LineHeight ) * 0.5f ) }, Txt, GetTheme()[ Tok::TextDim ] );
        return true;
    };
    Tbl.CellColor = [ &Tbl ]( size_t Row, size_t Col, Color& Out ) {
        if ( Col == 0 || Col == 2 ) { Out = GetTheme()[ Tok::TextDim ]; return true; }
        if ( Col != 5 ) return false;
        const std::string& S = Tbl.GetCell( Row, Col );
        Out = GetTheme()[ S == "running" ? Tok::Success : S == "stopped" ? Tok::Danger : Tok::Warning ];
        return true;
    };

    // ---- connections: the one-line helper, plain (non-selectable) table with grid lines
    auto& Side = Body.Card( "Open connections" );
    Side.Layout.Width = Grow( 1 );
    auto& C = Side.Table( { "Remote", "Port", "State" }, SelectMode::None );
    App.Conns = &C;
    C.Column( 1 ).Width = Px( 64 );
    C.Column( 1 ).TextAlign = Align::End;
    C.Column( 2 ).Width = Px( 110 );
    C.Style.GridLines = true;
    const char* Ips[] = { "81.2.69.160", "203.0.113.7", "198.51.100.23", "192.0.2.44", "100.64.12.9", "172.16.4.2", "10.0.4.18", "45.33.32.156" };
    const char* Ports[] = { "443", "443", "22", "8443", "443", "5432", "6379", "80" };
    const char* States[] = { "ESTABLISHED", "ESTABLISHED", "ESTABLISHED", "TIME_WAIT", "ESTABLISHED", "LISTEN", "LISTEN", "CLOSE_WAIT" };
    for ( int i = 0; i < 8; i++ ) C.AddRow( { Ips[ i ], Ports[ i ], States[ i ] } );
    Side.Caption( "Built with W.Table( { \"Remote\", \"Port\", \"State\" }, SelectMode::None )" );
}

// More controls: choices, numbers, feedback (toasts, dialogs, tooltips, menus), tree + splitter + collapsibles.
static void BuildMore( DemoApp& App, Container& Page )
{
    Page.Layout.Gap = 12;
    auto& Top = Page.Columns( 12 );

    auto& Choices = Top.Group( "Choices" );
    App.PlanRadio = &Choices.Radio( { "Hobby  \xC2\xB7  1 vCPU", "Pro  \xC2\xB7  4 vCPU", "Team  \xC2\xB7  16 vCPU" }, &App.Plan );
    Choices.Separator();
    Choices.Caption( "Period" );
    App.PeriodSeg = &Choices.Segmented( { "1h", "24h", "7d", "30d" }, &App.Period );

    auto& Numbers = Top.Group( "Numbers" );
    Numbers.LabelWidth = 64;
    App.PortInput = &Numbers.NumberInt( "Port", &App.Port, 1, 65535 );
    App.PortInput->Tooltip = "Type a number, use -/+ , or Up/Down (Shift = x10)";
    Numbers.Number( "UI scale", &App.UiScale, 0.5f, 3, 0.25f, "%.2f" );
    App.Price = &Numbers.Range( "Price", &App.PriceLo, &App.PriceHi, 0, 1000, 10, "$%.0f" );

    auto& Feedback = Top.Group( "Feedback" );
    auto& Btns = Feedback.Row( 8 );
    App.ToastBtn = &Btns.Button( "Toast", [ &App ] { tg::Toast( *App.Ctx, "Settings saved", Tok::Success ); } );
    App.ToastBtn->Tooltip = "Shows a notification in the corner; it fades out on its own";
    Btns.Button( "Error", [ &App ] { tg::Toast( *App.Ctx, "Deploy failed: exit code 1", Tok::Danger, 6 ); } );
    App.DeleteBtn = &Btns.Button( "Delete\xE2\x80\xA6", [ &App ] {
        tg::Confirm( *App.Ctx, "Delete vps-fra-01?", "The server and its disks are destroyed. This can't be undone.", { "Cancel", "Delete" },
                     [ &App ]( int Choice ) { if ( Choice == 1 ) { App.Deleted++; tg::Toast( *App.Ctx, "vps-fra-01 deleted", Tok::Danger ); } } );
    } );
    App.DeleteBtn->Style = Button::Danger();
    App.DeleteBtn->Tooltip = "Opens a confirm dialog (Enter = Delete, Escape = cancel)";
    auto& Busy = Feedback.Row( 10 );
    Busy.Layout.CrossAlign = Align::Center;
    Busy.Spinner();
    Busy.Text( "Syncing 3 servers\xE2\x80\xA6" ).Style.TextColor = Tok::TextDim;
    auto& Tags = Feedback.Row( 6 );
    Tags.Badge( "LIVE", Tok::Danger );
    Tags.Badge( "v0.5", Tok::Accent );
    Tags.Badge( "healthy", Tok::Success );
    auto& Solid = Tags.Badge( "PRO", Tok::Warning );
    Solid.Soft = false;
    Feedback.Link( "Open the docs", [ &App ] { tg::Toast( *App.Ctx, "docs/README.md", Tok::Accent ); } );

    // ---- explorer: tree | details, in a resizable splitter
    auto& Explorer = Page.Columns( 12 );
    auto& G = Explorer.Group( "Explorer  \xC2\xB7  drag the divider, right-click a server" );
    App.Split = &G.Split( Dir::Row, 0.38f );
    auto& T = App.Split->First().Tree();
    App.Servers = &T;
    struct { const char* Region; std::vector<const char*> Hosts; } Regions[] = {
        { "Europe", { "vps-fra-01", "vps-ams-02", "db-fra-01" } },
        { "North America", { "vps-nyc-01", "vps-tor-01" } },
        { "Asia Pacific", { "vps-sgp-01", "vps-syd-01", "vps-tyo-01" } },
    };
    for ( auto& R : Regions )
    {
        auto& N = T.Add( R.Region );
        for ( const char* H : R.Hosts ) N.Add( H );
    }
    T.Root.Children[ 0 ]->Expanded = true;
    T.Root.Children[ 0 ]->Children[ 2 ]->Color = Tok::Warning;
    T.OnSelect = [ &App ]( TreeNode& N ) { App.ActionLabel->SetText( "Selected: " + N.Text ); };
    T.ContextMenu = [ &App ]( Menu& M ) {
        TreeNode* N = App.Servers->GetSelected();
        std::string Name = N ? N->Text : "server";
        M.Header( Name );
        M.Item( "Connect", [ &App, Name ] { App.LastAction = "Connect " + Name; App.ActionLabel->SetText( App.LastAction ); }, "Enter" );
        M.Item( "Restart", [ &App, Name ] { App.LastAction = "Restart " + Name; App.ActionLabel->SetText( App.LastAction ); }, "Ctrl+R" );
        M.Check( "Pinned", &App.Pinned );
        M.Separator();
        M.Item( "Delete", [ &App, Name ] { App.LastAction = "Delete " + Name; App.ActionLabel->SetText( App.LastAction ); } ).Danger = true;
    };

    auto& Right = App.Split->Second();
    Right.Layout.Padding = Insets( 0, 0, 0, 0 );
    App.ActionLabel = &Right.Text( "Selected: nothing" );
    auto& Details = Right.Collapsible( "Details", &App.DetailsOpen );
    Details.Text( "Ubuntu 24.04  \xC2\xB7  4 vCPU  \xC2\xB7  8 GB  \xC2\xB7  10.0.4.21" ).Style.TextColor = Tok::TextDim;
    Details.Progress( 0.62f ).Style.Thresholds = true;
    App.Advanced = &Right.Collapsible( "Advanced", &App.AdvancedOpen );
    App.Advanced->Checkbox( "Verbose logging", &App.Verbose );
    App.Advanced->NumberInt( "Workers", &App.FpsCap, 1, 64 );
    auto& Danger = Right.Collapsible( "Danger zone" );
    Danger.Button( "Reinstall OS" ).Style = Button::Danger();
}

// ---------------------------------------------------------------- app

void DemoApp::Build( Context& C, Widget& Root )
{
    Ctx = &C;
    Theme& T = GetTheme();
    Root.Layout.Direction = Dir::Column;

    // ---- header bar (tgui): centered title, accent line underneath
    auto& Header = Root.Add<Box>( Dir::Stack );
    Header.Style = Box::Panel();
    Header.Layout.Height = Px( 46 );
    auto& Bar = AddRow( Header, 8, Insets( 18, 0 ) );
    Bar.Layout.CrossAlign = Align::Center;
    Bar.Add<Dot>( Tok::Accent, 10 );
    AddText( Bar, "tg", T.Title );
    AddSpacer( Bar );
    AddText( Bar, "redraws only when something changes  \xC2\xB7  v0.4", T.Small, Tok::TextDim );
    auto& Title = AddText( Header, "tg showcase", T.Bold );
    Title.Style.HAlign = Align::Center;
    auto& Accent = Root.Add<Box>();
    Accent.Style.Fill = Tok::Accent;
    Accent.Layout.Height = Px( 2 );

    // ---- tabs across the top, one page each
    auto& Body = Root.Add<Container>( Dir::Column, 0, Insets( 16, 14, 16, 16 ) );
    Body.Layout.Height = Grow();
    const char* Names[] = { "Dashboard", "Chat", "Controls", "Tables", "Windows", "More" };
    for ( const char* N : Names ) Pages.push_back( &Body.Tab( N ) );
    Tabs = Body.Tabs();
    Tabs->OnChange = [ this ]( int i ) { Page = i; };

    BuildDashboard( *this, *Pages[ 0 ] );
    BuildChat( *this, *Pages[ 1 ] );
    BuildControls( *this, *Pages[ 2 ] );
    BuildTables( *this, *Pages[ 3 ] );
    BuildWindows( *this, *Pages[ 4 ] );
    BuildMore( *this, *Pages[ 5 ] );

    // Global hotkey, like the old tgui Insert key: show/hide the Test Window.
    C.OnKeyEvent = [ this ]( Key K, bool Down ) {
        if ( !Down || K != MenuKey || K == Key::None || !SettingsWin ) return false;
        if ( SettingsWin->Visible ) SettingsWin->Close();
        else SettingsWin->Show();
        return true;
    };

    for ( int i = 0; i < 90; i++ ) TickStats( i * 1.0 - 90 );
    TickScreenShare( 0 );
}

void DemoApp::ShowPage( int Index )
{
    Tabs->Select( Index );
    Page = Index;
}

void DemoApp::AddMessage( const std::string& Who, const std::string& Text, const std::string& Time )
{
    Theme& T = GetTheme();
    uint32_t Col = Who == "alex" ? 0x8B5CF6 : Who == "mo" ? 0x0EA5E9 : Who == "rika" ? 0xF97316 : 0x22C55E;
    std::string Init = Who.substr( 0, 2 );
    for ( auto& Ch : Init ) Ch = char( std::toupper( (unsigned char)Ch ) );

    auto& Row = AddRow( Messages->Content(), 12 );
    Row.Layout.CrossAlign = Align::Start;
    Row.Add<Avatar>( Init, Color::Hex( Col ) );
    auto& Col2 = AddColumn( Row, 3 );
    Col2.Layout.Width = Grow();
    auto& Meta = AddRow( Col2, 8 );
    Meta.Layout.CrossAlign = Align::End;
    AddText( Meta, Who, T.Bold );
    AddText( Meta, Time, T.Small, Tok::TextDim );
    AddText( Col2, Text, nullptr, Tok::Text, true );
}

void DemoApp::TickStats( double T )
{
    float Cpu = 38 + 22 * std::sin( T * 0.21 ) + 9 * std::sin( T * 1.3 ) + 5 * std::sin( T * 3.1 );
    float Ram = 0.62f + 0.05f * float( std::sin( T * 0.05 ) );
    float Net = 18 + 12 * std::sin( T * 0.4 ) + 6 * std::sin( T * 2.2 );
    if ( Cpu < 2 ) Cpu = 2;
    if ( Net < 0.5f ) Net = 0.5f;

    char B[ 64 ];
    std::snprintf( B, sizeof( B ), "%.0f%%", Cpu );               CpuValue->SetText( B );
    std::snprintf( B, sizeof( B ), "%.1f / 8 GB", Ram * 8 );      RamValue->SetText( B );
    DiskValue->SetText( "71.2 / 80 GB" );
    std::snprintf( B, sizeof( B ), "%.1f MB/s", Net );            NetValue->SetText( B );
    CpuBar->SetValue( Cpu / 100 );
    RamBar->SetValue( Ram );
    DiskBar->SetValue( 71.2f / 80 );
    NetBar->SetValue( Net / 50 );
    CpuChart->Push( Cpu );
    NetChart->Push( Net );
    if ( Procs )
        for ( size_t r = 0; r < 6 && r < Procs->RowCount(); r++ )
        {
            char C[ 16 ];
            std::snprintf( C, sizeof( C ), "%.1f%%", std::fabs( 30 * std::sin( T * 0.3 + r * 1.7 ) + 8 * std::sin( T * 1.1 + r ) ) );
            Procs->SetCell( r, 3, C );
        }
}

void DemoApp::TickScreenShare( double T )
{
    // Fake "desktop" frame. A real app would copy a decoded video frame here.
    const int W = 320, H = 180;
    for ( int y = 0; y < H; y++ )
        for ( int x = 0; x < W; x++ )
        {
            float g = float( y ) / H;
            uint8_t r = uint8_t( 20 + 30 * g ), gg = uint8_t( 24 + 40 * g ), b = uint8_t( 48 + 70 * g );
            SharePixels[ y * W + x ] = Color( r, gg, b ).Pack();
        }
    auto Fill = [ & ]( int X, int Y, int Ww, int Hh, Color C ) {
        for ( int y = std::max( 0, Y ); y < std::min( H, Y + Hh ); y++ )
            for ( int x = std::max( 0, X ); x < std::min( W, X + Ww ); x++ ) SharePixels[ y * W + x ] = C.Pack();
    };
    // a "window" with a live latency graph
    Fill( 24, 20, 272, 140, Color( 236, 238, 242 ) );
    Fill( 24, 20, 272, 14, Color( 200, 204, 212 ) );
    for ( int x = 0; x < 256; x++ )
    {
        double t = T * 2 + x * 0.08;
        float v = x < 128 ? 0.75f + 0.1f * float( std::sin( t ) ) : 0.22f + 0.05f * float( std::sin( t * 1.7 ) );
        int y = 150 - int( v * 100 );
        Fill( 32 + x, y, 1, 150 - y, Color( 110, 183, 212 ) );
    }
    // a moving cursor
    int Cx = 60 + int( 180 * ( 0.5 + 0.5 * std::sin( T * 0.9 ) ) ), Cy = 60 + int( 50 * std::cos( T * 1.3 ) );
    Fill( Cx, Cy, 4, 4, Color( 20, 20, 20 ) );

    Ctx->GetRenderer().UpdateTexture( ShareTex, W, H, SharePixels.data() );
    Share->Redraw();
}
