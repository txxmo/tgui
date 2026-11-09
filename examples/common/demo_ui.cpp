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

    App.AddMessage( "mo", "deploying v2.3 to fra-01 \xF0\x9F\x87\xA9\xF0\x9F\x87\xAA then tyo-02 \xF0\x9F\x87\xAF\xF0\x9F\x87\xB5, should be quick", "10:41" );
    App.AddMessage( "rika", "nice \xF0\x9F\x9A\x80 is the websocket reconnect fix in there? the chat kept dropping for me on wifi yesterday", "10:42" );
    App.AddMessage( "mo", "yep, plus the backoff change. ping me if you still see drops \xF0\x9F\x91\x8D", "10:42" );
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

// Forms: menu bar, multi-line text, tags, search + drag-to-reorder list, date picker.
static void BuildForms( DemoApp& App, Container& Page )
{
    Page.Layout.Gap = 12;
    auto& Bar = Page.MenuBar();
    App.Bar = &Bar;
    auto Say = [ &App ]( std::string What ) { return [ &App, What ] { App.LastMenu = What; tg::Toast( *App.Ctx, What ); }; };
    Bar.Add( "File", [ Say ]( Menu& M ) {
        M.Item( "New note", Say( "New note" ), "Ctrl+N" );
        M.Item( "Open\xE2\x80\xA6", Say( "Open" ), "Ctrl+O" );
        M.Separator();
        M.Item( "Export as Markdown", Say( "Exported" ) );
    } );
    Bar.Add( "Edit", [ Say ]( Menu& M ) {
        M.Item( "Undo", Say( "Undo" ), "Ctrl+Z" );
        M.Item( "Redo", Say( "Redo" ), "Ctrl+Y" );
        M.Separator();
        M.Item( "Find", Say( "Find" ), "Ctrl+F" );
    } );
    Bar.Add( "View", [ &App ]( Menu& M ) { M.Check( "Word wrap", &App.WordWrap ); M.Check( "Compact layout", &App.Compact ); } );
    Bar.Add( "Help", [ Say ]( Menu& M ) { M.Item( "Documentation", Say( "Docs" ), "F1" ); M.Item( "About tg", Say( "tg v0.5" ) ); } );

    auto& Cols = Page.Columns( 12 );

    auto& Left = Cols.Group( "Notes" );
    App.NotesBox = &Left.TextArea( "Write something\xE2\x80\xA6", &App.Notes );
    App.NotesBox->Layout.Height = Grow();
    Left.Caption( "Multi-line, word-wrapped, scrolls. Ctrl+A/C/X/V, Home/End, Up/Down keep the column." );
    Left.Separator();
    Left.Text( "Chat-style composer" ).Style.TextColor = Tok::TextDim;
    App.Composer2 = &Left.TextArea( "Message (Enter sends, Shift+Enter = new line)" );
    App.Composer2->SubmitOnEnter = true;
    App.Composer2->ClearOnSubmit = true;
    App.Composer2->MinLines = 1;
    App.Composer2->MaxLines = 5; // grows as you type
    App.Composer2->OnSubmit = [ &App ]( const std::string& T ) { if ( !T.empty() ) { App.Sent.push_back( T ); tg::Toast( *App.Ctx, "Sent: " + T ); } };

    auto& Mid = Cols.Group( "Details" );
    Mid.LabelWidth = 64;
    Mid.Text( "Tags" ).Style.TextColor = Tok::TextDim;
    App.TagBox = &Mid.Tags( &App.Tags, "Add tag, Enter or comma" );
    App.ExpiresPicker = &Mid.Date( "Expires", &App.Expires );
    Mid.Caption( "Tags: type and press Enter or comma; Backspace on empty removes the last; x removes one." );

    auto& Right = Cols.Group( "Job queue" );
    App.SearchBoxW = &Right.Search( "Filter jobs", &App.Query );
    App.SearchBoxW->OnChange = [ &App ]( const std::string& ) { App.FilterQueue(); };
    App.Queue = &Right.List( App.QueueItems, SelectMode::Single );
    App.Queue->Layout.Height = Grow();
    App.Queue->Reorderable = true;
    App.Queue->OnReorder = [ &App ]( int, int ) {
        if ( !App.Query.empty() ) return;
        App.QueueItems.clear();
        for ( size_t i = 0; i < App.Queue->Count(); i++ ) App.QueueItems.push_back( App.Queue->GetItem( i ) );
    };
    Right.Caption( "Drag rows to reorder. The search box filters; Escape or x clears it." );
}

void DemoApp::FilterQueue()
{
    std::vector<std::string> Shown;
    auto Lower = []( std::string S ) { for ( auto& c : S ) c = char( std::tolower( (unsigned char)c ) ); return S; };
    for ( auto& Q : QueueItems )
        if ( Query.empty() || Lower( Q ).find( Lower( Query ) ) != std::string::npos ) Shown.push_back( Q );
    Queue->SetItems( Shown );
}

// Charts: gauges, sparklines, bars, donut.
static void BuildCharts( DemoApp& App, Container& Page )
{
    Page.Layout.Gap = 12;
    auto& Top = Page.Columns( 12 );
    Top.Layout.Height = Fit();

    auto& Meters = Top.Group( "Usage" );
    Meters.Layout.Width = Grow( 2 );
    auto& GRow = Meters.Row( 24 );
    GRow.Layout.MainAlign = Align::Center;
    App.CpuGauge = &GRow.Gauge( "CPU", 0.42f );
    App.MemGauge = &GRow.Gauge( "Memory", 0.74f );
    App.DiskGauge = &GRow.Gauge( "Disk", 0.93f );

    auto& Spark = Top.Group( "Last minute" );
    Spark.Layout.Width = Grow( 1 );
    struct { const char* Name; tg::Sparkline** Out; Paint C; } S[] = { { "requests/s", &App.SparkA, Tok::Accent }, { "p99 latency", &App.SparkB, Tok::Warning }, { "errors", &App.SparkC, Tok::Danger } };
    for ( auto& X : S )
    {
        auto& R = Spark.Row( 10 );
        R.Layout.CrossAlign = Align::Center;
        auto& L = R.Text( X.Name );
        L.Style.TextColor = Tok::TextDim;
        L.Layout.Width = Grow();
        auto& Sp = R.Sparkline( 40 );
        Sp.LineColor = X.C;
        Sp.Width = 140;
        *X.Out = &Sp;
    }

    auto& Bottom = Page.Columns( 12 );
    auto& Req = Bottom.Group( "Requests per day" );
    Req.Layout.Width = Grow( 3 );
    App.Requests = &Req.BarChart();
    App.Requests->Layout.Height = Grow();
    App.Requests->Format = "%.0fk";
    App.Requests->SetBars( { { "Mon", 42 }, { "Tue", 58 }, { "Wed", 51 }, { "Thu", 77 }, { "Fri", 64 }, { "Sat", 23, Tok::TextDim }, { "Sun", 19, Tok::TextDim } } );

    auto& Mix = Bottom.Group( "Traffic by service" );
    Mix.Layout.Width = Grow( 2 );
    App.Traffic = &Mix.Donut();
    App.Traffic->Format = "%.0f%%";
    App.Traffic->SetSlices( { { "api", 46, Tok::Accent }, { "chat", 27, Tok::Success }, { "web", 18, Tok::Warning }, { "other", 9, Tok::BorderStrong } } );
    App.Traffic->CenterSub = "of traffic";
    App.Traffic->Center = "100%";
}

// Terminal: a real shell in a pseudo-terminal, and a log view you feed with Write().
static void BuildTerminal( DemoApp& App, Container& Page )
{
    Page.Layout.Gap = 12;
    auto& Cols = Page.Columns( 12 );

    auto& ShellG = Cols.Group( "Shell" );
    ShellG.Layout.Width = Grow( 3 );
    auto& Tools = ShellG.Row( 8 );
    Tools.Layout.CrossAlign = Align::Center;
    App.ShellStatus = &Tools.Text( "not started" );
    App.ShellStatus->Style.TextColor = Tok::TextDim;
    App.ShellStatus->Layout.Width = Grow();
    DemoApp* A = &App;
    Tools.Button( "Restart", [ A ] { A->StartShell(); } );
    Tools.Button( "Clear", [ A ] { A->Shell->Clear(); } );
    App.Shell = &ShellG.Terminal();
    App.Shell->OnTitle = [ A ]( const std::string& T ) { A->ShellStatus->SetText( T ); };
    App.Shell->OnExit = [ A ]( int Code ) { A->ShellStatus->SetText( "exited (" + std::to_string( Code ) + ")" ); };

    auto& LogG = Cols.Group( "Deploy log" );
    LogG.Layout.Width = Grow( 2 );
    App.Log = &LogG.Terminal();
    App.Log->Style.Background = Color::Hex( 0x0E1116 );
    App.Log->OnInput = [ A ]( std::string_view K ) { if ( K == "\r" ) A->Log->Write( "\r\n" ); }; // no process: just echo Enter
    App.Log->Write(
        "\x1b[1m\xE2\x94\x8C\xE2\x94\x80 deploy \x1b[36mapi v2.3\x1b[39m \xE2\x86\x92 fra-01 \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\x1b[0m\r\n"
        "\x1b[2m10:41:02\x1b[22m \x1b[32m\xE2\x9C\x93\x1b[0m pulled image          \x1b[2m1.2s\x1b[0m\r\n"
        "\x1b[2m10:41:04\x1b[22m \x1b[32m\xE2\x9C\x93\x1b[0m migrations (3)        \x1b[2m0.8s\x1b[0m\r\n"
        "\x1b[2m10:41:05\x1b[22m \x1b[33m!\x1b[0m redis warmup slow     \x1b[33m2.4s\x1b[0m\r\n"
        "\x1b[2m10:41:08\x1b[22m \x1b[32m\xE2\x9C\x93\x1b[0m health checks \x1b[38;5;48m\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\x1b[38;5;240m\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\x1b[0m 7/10\r\n"
        "\x1b[2m10:41:09\x1b[22m \x1b[41;97m FAIL \x1b[0m \x1b[31mfra-01-c: timeout\x1b[0m\r\n"
        "\x1b[2m10:41:12\x1b[22m \x1b[32m\xE2\x9C\x93\x1b[0m retried, healthy \xF0\x9F\x8E\x89\r\n"
        "\x1b[2m10:41:13\x1b[22m regions \xF0\x9F\x87\xA9\xF0\x9F\x87\xAA \xF0\x9F\x87\xAF\xF0\x9F\x87\xB5 \xF0\x9F\x87\xBA\xF0\x9F\x87\xB8 \xF0\x9F\x8F\xB4\xE2\x80\x8D\xE2\x98\xA0\xEF\xB8\x8F\r\n"
        "\r\n\x1b[1mcpu\x1b[0m  \x1b[38;2;110;183;212m\xE2\xA3\x80\xE2\xA3\xA0\xE2\xA3\xB4\xE2\xA3\xBE\xE2\xA3\xBF\xE2\xA3\xB7\xE2\xA3\xA6\xE2\xA3\x84\xE2\xA3\x80\xE2\xA3\xA0\xE2\xA3\xB6\xE2\xA3\xBF\x1b[0m  42%\r\n"
        "\x1b[1mmem\x1b[0m  \x1b[48;5;236m\x1b[38;5;214m\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x8C     \x1b[0m  74%\r\n"
        "\r\n\x1b[7m inverse \x1b[0m \x1b[4munderline\x1b[0m \x1b[9mstrike\x1b[0m \x1b[3mitalic\x1b[0m \x1b[1;35mbold\x1b[0m\r\n"
        "\xE2\x95\x94\xE2\x95\x90\xE2\x95\x90\xE2\x95\xA6\xE2\x95\x90\xE2\x95\x90\xE2\x95\x97 \xE2\x95\xAD\xE2\x94\x80\xE2\x94\x80\xE2\x95\xAE \xE6\xBC\xA2\xE5\xAD\x97 \xF0\x9F\x90\xA7\r\n"
        "\xE2\x95\x9A\xE2\x95\x90\xE2\x95\x90\xE2\x95\xA9\xE2\x95\x90\xE2\x95\x90\xE2\x95\x9D \xE2\x95\xB0\xE2\x94\x80\xE2\x94\x80\xE2\x95\xAF\r\n" );
}

// More widgets: icons, rich text, toolbar, virtual list, palette, dials, steps, pickers, editor, drag & drop.
static void BuildWidgetsPlus( DemoApp& App, Container& Page )
{
    DemoApp* A = &App;
    Page.Layout.Gap = 10;
    App.Tools = &Page.Toolbar();
    App.Tools->Tool( Icon::Save, "Save (Ctrl+S)", [ A ] { A->LastMenu = "Save"; } );
    App.Tools->Tool( Icon::Undo2, "Undo", [ A ] { A->Ctx->History.Undo(); } );
    App.Tools->Separator();
    App.Tools->ToolToggle( Icon::Wifi, "Live updates", &App.Live );
    App.Tools->ToolToggle( Icon::Bell, "Notifications", &App.Notifications );
    App.Tools->Spacer();
    App.Tools->Tool( Icon::Command, "Command palette (Ctrl+K)", [ A ] { A->OpenCommands(); } );

    auto& Cols = Page.Columns( 12 );
    // ---- column 1: rich text, virtual list, drag & drop
    auto& C1 = Cols.Group( "Rich text  \xC2\xB7  100k-row list  \xC2\xB7  drag & drop" );
    C1.Layout.Width = Grow( 4 );
    App.Rich = &C1.Rich( Icon::Rocket + " **Deploy** of `api v2.3` finished on <success>fra-01</> in *42s*. "
                         "See [the build log](https://ci.example/123), @mo. <warning>2 warnings</>, <danger>0 errors</>." );
    App.Rich->OnLink = [ A ]( const std::string& U ) { A->LastLink = U; };
    App.BigList = &C1.List( 100000, []( size_t i ) { return "log line " + std::to_string( i + 1 ) + "  \xC2\xB7  GET /api/v2/servers 200 " + std::to_string( 3 + i % 97 ) + "ms"; } );
    App.BigList->Layout.Height = Px( 150 );
    auto& Dnd = C1.Row( 8 );
    for ( const char* N : { "vps-fra-01", "vps-ams-02", "db-fra-01" } )
    {
        auto& Chip = Dnd.Add<Badge>( std::string( Icon::Server ) + " " + N, Tok::Raised );
        std::string Name = N;
        Chip.OnDragStart = [ Name ]( DragData& D ) { D.Type = "server"; D.Text = Name; return true; };
        Chip.Interactive = true;
    }
    auto& Bin = C1.Add<Box>( Dir::Column, 4, Insets( 10 ) );
    Bin.Style = Box::Card();
    Bin.Layout.Height = Px( 46 );
    App.DropLabel = &AddText( Bin, Icon::Trash2 + "  drop a server here", nullptr, Tok::TextDim );
    Bin.AcceptDrop = []( const DragData& D ) { return D.Type == "server"; };
    Bin.OnDrop = [ A ]( const DragData& D, Vec2 ) { A->Dropped = D.Text; A->DropLabel->SetText( Icon::Trash2 + "  removed " + D.Text ); };
    App.DropBin = &Bin;

    // ---- column 2: dials, pickers, steps
    auto& C2 = Cols.Group( "Dials  \xC2\xB7  pickers  \xC2\xB7  steps" );
    C2.Layout.Width = Grow( 3 );
    auto& Dials = C2.Row( 18 );
    App.GainKnob = &Dials.Knob( "Gain", &App.Gain, -24, 24, 1 );
    App.SensKnob = &Dials.Knob( "Sens", &App.Sens, 0, 10, 0.1f );
    App.SensKnob->Format = "%.1f";
    App.Pad = &Dials.XYPad( "Crosshair", &App.Cross, { -1, -1 }, { 1, 1 } );
    App.Pad->Size = 96;
    App.TimeW = &C2.Time( "Backup at", &App.BackupAt );
    App.RangeW = &C2.DateRange( "Report", &App.From, &App.To );
    auto& Sw = C2.Row( 8 );
    Sw.Layout.CrossAlign = Align::Center;
    App.SwatchW = &Sw.Swatches( &App.Accent );
    App.SwatchW->Columns = 8;
    App.Dropper = &Sw.Eyedropper( &App.Accent );
    App.Pager = &C2.Pagination( &App.PageNo, 20 );
    App.Steps = &C2.Stepper( { "Account", "Server", "Confirm" }, &App.StepNo );

    // ---- column 3: code editor, property grid, accordion
    auto& C3 = Cols.Group( "Code editor  \xC2\xB7  inspector" );
    C3.Layout.Width = Grow( 4 );
    App.Editor = &C3.Add<CodeEditor>( "// tg::CodeEditor\n#include <cstdio>\n\nint main()\n{\n    /* hello */\n    std::printf( \"%d\\n\", 42 );\n    return 0;\n}\n", CodeLang::Cpp );
    App.Editor->Layout.Height = Px( 170 );
    App.Props = &C3.PropertyGrid();
    App.Props->Category( "Server" );
    App.Props->Text( "Hostname", &App.Host );
    App.Props->Int( "Port", &App.SrvPort, 1, 65535 );
    App.Props->Bool( "TLS", &App.Tls );
    App.Props->Category( "Look", false );
    App.Props->Enum( "Density", { "Compact", "Comfy" }, &App.Density );
    App.Props->Color( "Accent", &App.Accent );
    App.Acc = &C3.Accordion();
    App.Acc->Section( "General", true ).Text( "Accordion: one section open at a time." );
    App.Acc->Section( "Advanced" ).Text( "Opening this closes General." );

    auto& Sb = Page.StatusBar();
    Sb.Item( Icon::Wifi + "  connected" );
    Sb.Separator();
    Sb.Item( "3 servers" );
    Sb.Spacer();
    Sb.Item( "F12 inspector  \xC2\xB7  Ctrl+K commands" );
}

// Layout & theme: responsive grid, wrap, StackBelow, animated reflow, the live theme editor.
static void BuildLayoutTheme( DemoApp& App, Container& Page )
{
    DemoApp* A = &App;
    auto& Cols = Page.Columns( 12 );
    auto& Left = Cols.Group( "Grid (ColumnMin 170: resize the window)  \xC2\xB7  wrap  \xC2\xB7  animated reflow" );
    Left.Layout.Width = Grow( 3 );
    auto& Grid = Left.Add<Container>( Dir::Grid, 10 );
    Grid.Layout.ColumnMin = 170;
    Grid.Layout.AnimateChildren = true;
    App.GridBox = &Grid;
    const char* Names[] = { "fra-01", "ams-02", "nyc-01", "sgp-01", "syd-01", "db-fra-01" };
    for ( int i = 0; i < 6; i++ )
    {
        auto& Card = AddCard( Grid, Dir::Column, 6 );
        if ( i == 0 ) Card.Layout.ColSpan = 2; // the first card is twice as wide
        AddText( Card, Icon::Server + "  " + Names[ i ], GetTheme().Bold );
        AddText( Card, i % 3 == 2 ? "degraded" : "healthy", nullptr, i % 3 == 2 ? Tok::Warning : Tok::Success );
        Card.Add<ProgressBar>( 0.2f + 0.13f * i );
    }
    auto& Wrap = Left.Add<Container>( Dir::Wrap, 6 );
    Wrap.Layout.AnimateChildren = true;
    App.WrapBox = &Wrap;
    for ( const char* T : { "prod", "eu-west", "k8s", "postgres", "redis", "nginx", "grafana", "backup", "tls", "ipv6" } ) Wrap.Badge( T, Tok::Raised );
    auto& Shuffle = Left.Row( 8 );
    Shuffle.Button( Icon::Shuffle + "  Shuffle cards", [ A ] {
        size_t N = A->GridBox->Children().size();
        if ( N > 2 ) A->GridBox->MoveChild( 1, N - 1 ); // reorder: the cards glide to their new cells
    } );
    auto& Split = Left.Row( 10 );
    Split.Layout.StackBelow = 520; // side by side when wide, stacked when narrow
    App.StackRow = &Split;
    auto& Va = AddCard( Split, Dir::Column, 4 );
    Va.Layout.Width = Grow();
    AddText( Va, "StackBelow = 520", GetTheme().Bold );
    AddText( Va, "These two cards sit side by side, or stack when the row is narrow.", nullptr, Tok::TextDim, true );
    auto& Vb = AddCard( Split, Dir::Column, 4 );
    Vb.Layout.Width = Grow();
    Vb.Layout.Aspect = 0; // (Aspect = width / height pins a ratio)
    AddText( Vb, "Variants", GetTheme().Bold );
    auto& Vr = AddRow( Vb, 6 );
    for ( const char* V : { "default", "primary", "danger", "ghost" } ) UseVariant( Vr.Add<Button>( V ), V );

    auto& Right = Cols.Group( "Theme editor (live)" );
    Right.Layout.Width = Grow( 2 );
    App.ThemeEd = &Right.Add<ThemeEditor>();
}

// Effects: shader gradients, shimmer, grayscale, frosted glass, a cached (render-to-texture) panel.
class GradientTile : public Widget
{
public:
    GradientTile( Gradient G, std::string Label ) : G( G ), Label( std::move( Label ) ) { Layout.Height = Px( 90 ); Layout.Width = Grow(); }
    void Draw( DrawList& D ) override
    {
        if ( Context* C = GetContext() ) FillGradient( D, *C, Bounds, G, 10 );
        D.Text( *GetTheme().Bold, { Bounds.x + 12, Bounds.Bottom() - 26 }, Label, Color( 255, 255, 255 ) );
    }
    Gradient G;
    std::string Label;
};

// big colorful blobs to put frosted glass on
class Blobs : public Widget
{
public:
    Blobs() { Layout.Width = Grow(); Layout.Height = Grow(); }
    void Draw( DrawList& D ) override
    {
        Context* C = GetContext();
        D.RectFilled( Bounds, Color::Hex( 0x10131A ), 10 );
        D.PushClip( Bounds );
        if ( C )
        {
            FillGradient( D, *C, { Bounds.x + 20, Bounds.y + 30, 220, 220 }, Gradient::Radial( Color::Hex( 0xE5534B ), Color::Hex( 0xE5534B, 0 ) ), 110 );
            FillGradient( D, *C, { Bounds.x + 170, Bounds.y + 90, 260, 260 }, Gradient::Radial( Color::Hex( 0x4F8FE0 ), Color::Hex( 0x4F8FE0, 0 ) ), 130 );
            FillGradient( D, *C, { Bounds.Right() - 240, Bounds.y + 10, 220, 220 }, Gradient::Radial( Color::Hex( 0x3FB97A ), Color::Hex( 0x3FB97A, 0 ) ), 110 );
        }
        for ( int i = 0; i < 9; i++ ) D.Text( *GetTheme().Title, { Bounds.x + 30 + i * 52.f, Bounds.y + 140 }, "tg", Color( 255, 255, 255, 200 ) );
        D.PopClip();
        DrawChildren( D );
    }
};

static void BuildEffects( DemoApp& App, Container& Page )
{
    DemoApp* A = &App;
    auto& Cols = Page.Columns( 12 );
    auto& L = Cols.Group( "Gradients (shader)  \xC2\xB7  shimmer  \xC2\xB7  grayscale" );
    L.Layout.Width = Grow();
    auto& G1 = L.Row( 10 );
    G1.Add<GradientTile>( Gradient::Linear( 0, Color::Hex( 0x6EB7D4 ), Color::Hex( 0xB277D6 ) ), "Linear" );
    G1.Add<GradientTile>( Gradient::Radial( Color::Hex( 0xF5C25B ), Color::Hex( 0xE5534B ) ), "Radial" );
    G1.Add<GradientTile>( Gradient::Conic( Color::Hex( 0xE5534B ), Color::Hex( 0x3FB97A ), Color::Hex( 0x4F8FE0 ) ), "Conic" );
    auto& G2 = L.Row( 10 );
    Gradient Three = Gradient::Linear( 0, Color::Hex( 0xE5534B ), Color::Hex( 0xF5C25B ) );
    Three.At[ 1 ] = 0.5f;
    G2.Add<GradientTile>( Three.Stop( 1, Color::Hex( 0x3FB97A ) ), "3 stops" );
    G2.Add<GradientTile>( Gradient::Linear( 45, Color::Hex( 0x3FB97A ), Color::Hex( 0x4DB6C8 ) ), "45\xC2\xB0" );
    // shimmer skeleton (loading placeholder)
    auto& Sk = L.Add<Effect>( Effects::Shimmer(), std::vector<float>{ 1, 1, 1, 0.10f, 1.0f } );
    Sk.Layout.Gap = 8;
    for ( float W : { 0.9f, 0.6f, 0.75f } )
    {
        auto& Bar = Sk.Add<Box>();
        Bar.Style.Fill = Tok::Raised;
        Bar.Style.Radius = 6;
        Bar.Layout.Height = Px( 14 );
        Bar.Layout.Width = Px( 360 * W );
    }
    // grayscale toggle over real controls
    auto& Gs = L.Add<Effect>( Effects::Grayscale(), std::vector<float>{ 0 } );
    App.GrayFx = &Gs;
    auto& Gr = Gs.Row( 8 );
    Gr.Button( Icon::Rocket + "  Deploy" ).Style = Button::Primary();
    Gr.Button( Icon::Trash2 + "  Delete" ).Style = Button::Danger();
    Gr.Toggle( "Grayscale", &App.Gray ).OnChange = [ A ]( bool On ) { A->GrayFx->Params[ 0 ] = On ? 1.f : 0.f; };

    auto& R = Cols.Group( "Frosted glass  \xC2\xB7  cached panel" );
    R.Layout.Width = Grow();
    auto& Bg = R.Add<Blobs>();
    Bg.Layout.Direction = Dir::Free;
    Bg.Layout.Height = Px( 300 );
    auto& Glass = Bg.Add<Backdrop>();
    Glass.Layout.Offset = { 60, 60 };
    Glass.Layout.Width = Px( 300 );
    App.Glass = &Glass;
    Glass.Heading( Icon::Sparkles + "  Frosted glass" );
    Glass.Caption( "BackdropBlur blurs what's behind; a translucent fill goes on top." );
    Glass.Slider( "Blur", &Glass.Blur, 0, 30, 1 );
    auto& Cache = R.Add<CachedPanel>();
    App.Cached = &Cache;
    Cache.Heading( "CachedPanel" );
    Cache.Caption( "Rendered once into a texture, redrawn on MarkDirty(). Good for heavy, mostly static content." );
    auto& Lc = Cache.Add<LineChart>( 60 );
    std::vector<float> Pts;
    for ( int i = 0; i < 60; i++ ) Pts.push_back( 50 + 30 * std::sin( i * 0.25f ) + 10 * std::sin( i * 0.9f ) );
    Lc.SetData( Pts );
}

void DemoApp::OpenCommands()
{
    CommandList L;
    DemoApp* A = this;
    L.Add( "Restart server", [ A ] { A->LastMenu = "Restart server"; }, Icon::RotateCw, "Ctrl+R", "Server" );
    L.Add( "Open terminal", [ A ] { A->Tabs->Select( 8 ); }, Icon::Terminal, "", "View" );
    L.Add( "Toggle light theme", [ A ] { A->LastMenu = "Toggle light theme"; }, Icon::Sun, "", "View" );
    L.Add( "Deploy to production", [ A ] { A->LastMenu = "Deploy to production"; }, Icon::Rocket, "Ctrl+Shift+D", "Release" ).Keywords = "ship release";
    L.Add( "Show inspector", [ A ] { A->Ctx->ShowInspector = true; }, Icon::Search, "F12", "Debug" );
    L.Add( "Clear logs", [ A ] { A->LastMenu = "Clear logs"; }, Icon::Trash2, "", "Server" );
    Palette = &OpenPalette( *Ctx, L, "Type a command" );
}

void DemoApp::StartShell()
{
    if ( !Shell ) return;
    Shell->Reset();
    if ( !tg::Pty::Supported() ) { Shell->Write( "pseudo-terminals aren't available here (Windows 10 1809+ needed)\r\n" ); return; }
    bool Ok = Shell->Spawn( ShellCommand );
    ShellStatus->SetText( Ok ? ( ShellCommand.empty() ? "shell" : ShellCommand ) : "failed to start" );
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
    const char* Names[] = { "Dashboard", "Chat", "Controls", "Tables", "Windows", "More", "Forms", "Charts", "Terminal", "More widgets", "Layout & theme", "Effects" };
    for ( const char* N : Names ) Pages.push_back( &Body.Tab( N ) );
    Tabs = Body.Tabs();
    Tabs->OnChange = [ this ]( int i ) {
        Page = i;
        if ( i == 8 && !ShellStarted ) { ShellStarted = true; StartShell(); } // only start a shell once you look at it
    };

    BuildDashboard( *this, *Pages[ 0 ] );
    BuildChat( *this, *Pages[ 1 ] );
    BuildControls( *this, *Pages[ 2 ] );
    BuildTables( *this, *Pages[ 3 ] );
    BuildWindows( *this, *Pages[ 4 ] );
    BuildMore( *this, *Pages[ 5 ] );
    BuildForms( *this, *Pages[ 6 ] );
    BuildCharts( *this, *Pages[ 7 ] );
    BuildTerminal( *this, *Pages[ 8 ] );
    BuildWidgetsPlus( *this, *Pages[ 9 ] );
    BuildLayoutTheme( *this, *Pages[ 10 ] );
    BuildEffects( *this, *Pages[ 11 ] );
    C.Shortcut( "Ctrl+K", [ this ] { OpenCommands(); } );

    // Global hotkey, like the old tgui Insert key: show/hide the Test Window.
    C.OnKeyEvent = [ this ]( Key K, bool Down ) {
        if ( !Down || K != MenuKey || K == Key::None || !SettingsWin ) return false;
        if ( SettingsWin->IsOpen() ) SettingsWin->Close();
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
    AddText( Col2, Text, nullptr, Tok::Text, true ).SetSelectable( true ); // drag / double-click to select, Ctrl+C or right-click to copy
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
    if ( CpuGauge )
    {
        CpuGauge->SetValue( Cpu / 100 );
        MemGauge->SetValue( Ram );
        SparkA->Push( 900 + 300 * float( std::sin( T * 0.7 ) ) + 80 * float( std::sin( T * 2.3 ) ) );
        SparkB->Push( 40 + 12 * float( std::sin( T * 0.4 + 1 ) ) + 6 * float( std::sin( T * 3.1 ) ) );
        SparkC->Push( std::max( 0.f, 2 + 3 * float( std::sin( T * 0.9 ) ) ) );
    }
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
