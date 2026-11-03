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
    Page.Layout.Padding = Insets( 24 );
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

static void BuildControls( DemoApp& App, Container& Page )
{
    Theme& T = GetTheme();
    Page.Layout.Padding = Insets( 24 );
    Page.Layout.Gap = 16;
    AddText( Page, "Controls", T.Title );

    auto& Row = AddRow( Page, 16 );

    auto& Left = AddCard( Row, Dir::Column, 14 );
    Left.Layout.Width = Grow();
    AddText( Left, "Toggles", T.Bold );
    Left.Add<Checkbox>( "Auto-restart crashed services", &App.AutoRestart );
    Left.Add<Checkbox>( "Desktop notifications", &App.Notifications );
    Left.Add<Checkbox>( "Compact message layout", &App.Compact );

    // The thing that was painful in imgui: restyling ONE checkbox. Here it's just its Style.
    auto& Custom = Left.Add<Checkbox>( "Custom styled (square, red)" );
    Custom.Style.Radius = 0;
    Custom.Style.Fill = Tok::Danger;
    Custom.Style.BoxHoverStroke = Tok::Danger;
    Custom.SetChecked( true );

    AddDivider( Left );
    Left.Add<Toggle>( "Light theme", &App.LightTheme ).OnChange = [ &App ]( bool On ) {
        GetTheme().UseColors( On ? Theme::Light() : Theme::Dark() );
        App.Ctx->RequestFrame();
    };

    auto& Mid = AddCard( Row, Dir::Column, 14 );
    Mid.Layout.Width = Grow();
    AddText( Mid, "Inputs", T.Bold );
    auto& VolRow = AddRow( Mid, 12 );
    VolRow.Layout.CrossAlign = Align::Center;
    AddText( VolRow, "Volume", nullptr, Tok::TextDim );
    auto& Sl = VolRow.Add<Slider>( &App.Volume, 0, 100, 1 );
    Sl.Layout.Width = Grow();
    App.SliderValue = &AddText( VolRow, "65" );
    App.SliderValue->Layout.MinWidth = 28;
    Sl.OnChange = [ &App ]( float V ) { App.SliderValue->SetText( std::to_string( int( V ) ) ); };
    Mid.Add<TextInput>( "Server address" ).SetText( "ssh://root@10.0.4.21" );
    Mid.Add<TextInput>( "Password" ).Password = true;
    auto& Prog = Mid.Add<ProgressBar>( 0.42f );
    Prog.Style.Height = 6;

    auto& Right = AddCard( Row, Dir::Column, 12 );
    Right.Layout.Width = Grow();
    AddText( Right, "Buttons", T.Bold );
    Right.Add<Button>( "Default" );
    Right.Add<Button>( "Primary" ).Style = Button::Primary();
    Right.Add<Button>( "Danger" ).Style = Button::Danger();
    Right.Add<Button>( "Ghost" ).Style = Button::Ghost();
    auto& Dis = Right.Add<Button>( "Disabled" );
    Dis.Enabled = false;

    // ---- pickers + list (built with the Container helpers: one line per control)
    auto& Row2 = Page.Row( 16 );
    auto& Pick = Row2.Card( "Pickers" );
    Pick.Layout.Width = Grow();
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

    auto& Lst = Row2.Card( "Servers" );
    Lst.Layout.Width = Grow();
    App.ServerList = &Lst.List( { "vps-fra-01  \xC2\xB7  Frankfurt", "vps-ams-02  \xC2\xB7  Amsterdam", "vps-nyc-01  \xC2\xB7  New York",
                                  "vps-sgp-01  \xC2\xB7  Singapore", "vps-syd-01  \xC2\xB7  Sydney", "db-fra-01  \xC2\xB7  Frankfurt",
                                  "cache-ams-01  \xC2\xB7  Amsterdam" }, SelectMode::Single, &App.Server );
    App.ServerList->Layout.Height = Px( 150 );
    auto& Opts = Lst.Row( 18 );
    auto Apply = [ &App ] { App.ServerList->SetMode( !App.ListSelectable ? SelectMode::None : App.ListMulti ? SelectMode::Multi : SelectMode::Single ); };
    Opts.Toggle( "Selectable", &App.ListSelectable ).OnChange = [ Apply ]( bool ) { Apply(); };
    Opts.Toggle( "Multi-select", &App.ListMulti ).OnChange = [ Apply ]( bool ) { Apply(); };

    auto& Note = AddCard( Page, Dir::Column, 6 );
    AddText( Note, "Where's the code?", T.Bold );
    AddText( Note, "Every widget is one header + one .cpp in src/widgets. Its look lives in Draw(), its tunables in a Style struct "
                   "next to it, and colors come from the Theme palette. Change one checkbox (Style), every checkbox (Checkbox::Defaults()), "
                   "every accent (Theme), or just edit Draw().", nullptr, Tok::TextDim, true );
}

// Floating windows, ImGui-style: every control is one line.
static void BuildWindows( DemoApp& App, Container& Page )
{
    Page.Layout.Direction = Dir::Free;

    auto& Hint = Page.Add<Label>( "Drag the title bars, resize from the corner, close with the X.  Code: examples/common/demo_ui.cpp \xE2\x86\x92 BuildWindows()" );
    Hint.Style.TextColor = Tok::TextDim;
    Hint.Style.TextFont = GetTheme().Small;
    Hint.Layout.Offset = { 24, 20 };

    auto& W = Page.Add<Window>( "Overlay settings", Vec2{ 24, 52 }, Vec2{ 360, 0 } );
    App.SettingsWin = &W;
    W.Checkbox( "Enable overlay", &App.Overlay );
    W.Checkbox( "Show FPS counter", &App.ShowFps );
    W.Checkbox( "Crosshair", &App.Crosshair );
    W.Separator();
    W.Slider( "Opacity", &App.Opacity, 0, 1 );
    W.SliderInt( "FPS cap", &App.FpsCap, 30, 240 );
    W.Combo( "Position", { "Top left", "Top right", "Bottom left", "Bottom right" }, &App.Mode );
    W.ColorPicker( "Tint", &App.Tint );
    auto& Btns = W.Row();
    Btns.Button( "Apply" ).Style = Button::Primary();
    Btns.Button( "Reset" );

    auto& P = Page.Add<Window>( "Players", Vec2{ 420, 90 }, Vec2{ 300, 360 } );
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
    Page.Layout.Padding = Insets( 24 );
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

// ---------------------------------------------------------------- app

void DemoApp::Build( Context& C, Widget& Root )
{
    Ctx = &C;
    Theme& T = GetTheme();
    Root.Layout.Direction = Dir::Row;

    auto& Side = Root.Add<Box>( Dir::Column, 4, Insets( 14, 18 ) );
    Side.Style = Box::Panel();
    Side.Layout.Width = Px( 210 );
    auto& Brand = AddRow( Side, 8, Insets( 8, 0, 0, 14 ) );
    Brand.Layout.CrossAlign = Align::Center;
    Brand.Add<Dot>( Tok::Accent, 12 );
    AddText( Brand, "tg", T.Title );
    AddText( Brand, "demo", T.Small, Tok::TextDim );

    const char* Names[] = { "Dashboard", "Chat", "Controls", "Tables", "Windows" };
    for ( int i = 0; i < 5; i++ )
    {
        auto& B = Side.Add<Button>( Names[ i ], [ this, i ]() { ShowPage( i ); } );
        B.Style = Button::Ghost();
        NavButtons.push_back( &B );
    }
    AddSpacer( Side );
    AddText( Side, "redraws only when something changes", T.Small, Tok::TextDim, true );

    AddDivider( Root, true );

    auto& Stack = Root.Add<Box>( Dir::Stack );
    Stack.Layout.Width = Grow();
    for ( int i = 0; i < 5; i++ ) Pages.push_back( &Stack.Add<Container>( Dir::Column, 0 ) );
    BuildDashboard( *this, *Pages[ 0 ] );
    BuildChat( *this, *Pages[ 1 ] );
    BuildControls( *this, *Pages[ 2 ] );
    BuildTables( *this, *Pages[ 3 ] );
    BuildWindows( *this, *Pages[ 4 ] );

    for ( int i = 0; i < 90; i++ ) TickStats( i * 1.0 - 90 );
    TickScreenShare( 0 );
    ShowPage( 0 );
}

void DemoApp::ShowPage( int Index )
{
    Page = Index;
    for ( size_t i = 0; i < Pages.size(); i++ )
    {
        Pages[ i ]->Visible = int( i ) == Index;
        NavButtons[ i ]->Style = int( i ) == Index ? Button::Defaults() : Button::Ghost();
        NavButtons[ i ]->Style.Stroke = Color( 0, 0, 0, 0 );
    }
    Ctx->InvalidateLayout();
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
