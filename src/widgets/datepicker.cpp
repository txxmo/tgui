#include "tg/widgets/datepicker.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/popup.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace tg
{
    // ------------------------------------------------------------ Date

    Date Date::Today()
    {
        std::time_t T = std::time( nullptr );
        std::tm L{};
#ifdef _WIN32
        localtime_s( &L, &T );
#else
        localtime_r( &T, &L );
#endif
        return { L.tm_year + 1900, L.tm_mon + 1, L.tm_mday };
    }

    int Date::DaysInMonth( int Y, int M )
    {
        static const int D[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        bool Leap = ( Y % 4 == 0 && Y % 100 != 0 ) || Y % 400 == 0;
        return M == 2 && Leap ? 29 : D[ ( M - 1 + 12 ) % 12 ];
    }

    int Date::Weekday() const
    {
        // Sakamoto: 0 = Sunday; shift to Monday-first
        static const int T[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
        int Y = Year - ( Month < 3 );
        int W = ( Y + Y / 4 - Y / 100 + Y / 400 + T[ Month - 1 ] + Day ) % 7;
        return ( W + 6 ) % 7;
    }

    std::string Date::ToString() const
    {
        char B[ 16 ];
        std::snprintf( B, sizeof( B ), "%04d-%02d-%02d", Year, Month, Day );
        return B;
    }

    static const char* MonthNames[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    // ------------------------------------------------------------ Calendar

    Calendar::Calendar( tg::Date S ) : Selected( S ), ViewYear( S.Year ), ViewMonth( S.Month ) { Interactive = true; }

    void Calendar::Shift( int N )
    {
        int M = ViewYear * 12 + ( ViewMonth - 1 ) + N;
        ViewYear = M / 12;
        ViewMonth = M % 12 + 1;
        Redraw();
    }

    Vec2 Calendar::Measure( float ) { return { Cell * 7, Head + 22 + Cell * 6 + 30 }; }

    Rect Calendar::DayRect( int Day ) const
    {
        int Offset = tg::Date{ ViewYear, ViewMonth, 1 }.Weekday();
        int Idx = Offset + Day - 1;
        return { Bounds.x + ( Idx % 7 ) * Cell, Bounds.y + Head + 22 + ( Idx / 7 ) * Cell, Cell, Cell };
    }

    int Calendar::DayAt( Vec2 P ) const
    {
        for ( int D = 1; D <= tg::Date::DaysInMonth( ViewYear, ViewMonth ); D++ )
            if ( DayRect( D ).Contains( P ) ) return D;
        return -1;
    }

    void Calendar::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& F = *T.Body;
        const Font& B = *T.Bold;
        const Font& S = *T.Small;

        // header: < November 2026 >
        char Title[ 32 ];
        std::snprintf( Title, sizeof( Title ), "%s %d", MonthNames[ ViewMonth - 1 ], ViewYear );
        float Tw = B.Width( Title );
        D.Text( B, { std::floor( Bounds.x + ( Bounds.w - Tw ) * 0.5f ), Bounds.y + 8 }, Title, T[ Tok::Text ] );
        PrevR = { Bounds.x, Bounds.y, Head, Head };
        NextR = { Bounds.Right() - Head, Bounds.y, Head, Head };
        for ( float Dir : { -1.f, 1.f } ) // < and >
        {
            Vec2 C = ( Dir < 0 ? PrevR : NextR ).Center();
            Vec2 P[ 3 ] = { { C.x - 2.5f * Dir, C.y - 5 }, { C.x + 2.5f * Dir, C.y }, { C.x - 2.5f * Dir, C.y + 5 } };
            D.Polyline( P, 3, T[ Tok::TextDim ], 1.6f );
        }

        // weekday initials
        const char* Wd[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
        for ( int i = 0; i < 7; i++ )
        {
            float W = S.Width( Wd[ i ] );
            D.Text( S, { std::floor( Bounds.x + i * Cell + ( Cell - W ) * 0.5f ), Bounds.y + Head + 2 }, Wd[ i ], T[ Tok::TextDim ] );
        }

        // days
        tg::Date Today = tg::Date::Today();
        for ( int Dd = 1; Dd <= tg::Date::DaysInMonth( ViewYear, ViewMonth ); Dd++ )
        {
            Rect R = DayRect( Dd ).Shrink( 2 );
            tg::Date This = { ViewYear, ViewMonth, Dd };
            bool Sel = !Range && This == Selected, IsToday = This == Today;
            if ( Range )
            {
                tg::Date A = RangeStart, B = PickingEnd && HoverDay > 0 ? tg::Date{ ViewYear, ViewMonth, HoverDay } : RangeEnd;
                if ( B < A ) std::swap( A, B );
                bool In = !( This < A ) && !( B < This ), End = This == A || This == B;
                if ( In && !End ) D.RectFilled( DayRect( Dd ).Shrink( { 0, 2, 0, 2 } ), T[ Tok::Accent ].Fade( 0.22f ) );
                if ( End ) { D.RectFilled( R, T[ Tok::Accent ], 6 ); Sel = true; }
            }
            if ( Sel ) D.RectFilled( R, T[ Tok::Accent ], 6 );
            else if ( Dd == HoverDay && IsHovered() ) D.RectFilled( R, Color( 255, 255, 255, 16 ), 6 );
            if ( IsToday && !Sel ) D.RectStroke( R, T[ Tok::Accent ], 6, 1.2f );
            char N[ 12 ];
            std::snprintf( N, sizeof( N ), "%d", Dd );
            float W = F.Width( N );
            int Wk = This.Weekday();
            Color Tc = Sel ? T[ Tok::TextOnAccent ] : Wk >= 5 ? T[ Tok::TextDim ] : T[ Tok::Text ];
            D.Text( F, { std::floor( R.x + ( R.w - W ) * 0.5f ), std::floor( R.y + ( R.h - F.LineHeight ) * 0.5f ) }, N, Tc );
        }

        // "Today" shortcut
        float W = S.Width( "Today" );
        TodayR = { Bounds.x + ( Bounds.w - W - 20 ) * 0.5f, Bounds.Bottom() - 26, W + 20, 22 };
        D.RectFilled( TodayR, T[ Tok::Raised ], 5 );
        D.Text( S, { TodayR.x + 10, std::floor( TodayR.y + ( TodayR.h - S.LineHeight ) * 0.5f ) }, "Today", T[ Tok::Text ] );
    }

    bool Calendar::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove: { int H = DayAt( E.Pos ); if ( H != HoverDay ) { HoverDay = H; Redraw(); } return false; }
        case EventType::MouseLeave: HoverDay = -1; Redraw(); return false;
        case EventType::MouseWheel: Shift( E.Wheel.y > 0 ? -1 : 1 ); return true;
        case EventType::MouseDown: return E.Button == MouseButton::Left;
        case EventType::MouseUp:
        {
            if ( E.Button != MouseButton::Left ) return false;
            if ( PrevR.Contains( E.Pos ) ) { Shift( -1 ); return true; }
            if ( NextR.Contains( E.Pos ) ) { Shift( 1 ); return true; }
            tg::Date Pick;
            if ( TodayR.Contains( E.Pos ) ) Pick = tg::Date::Today();
            else
            {
                int Dd = DayAt( E.Pos );
                if ( Dd < 0 ) return true;
                Pick = { ViewYear, ViewMonth, Dd };
            }
            if ( Range )
            {
                if ( !PickingEnd ) { RangeStart = RangeEnd = Pick; PickingEnd = true; Redraw(); return true; }
                RangeEnd = Pick;
                if ( RangeEnd < RangeStart ) std::swap( RangeStart, RangeEnd );
                PickingEnd = false;
                auto Fr = OnRange;
                if ( Fr ) Fr( RangeStart, RangeEnd );
                return true;
            }
            Selected = Pick;
            auto Fn = OnPick;
            if ( Fn ) Fn( Pick );
            return true;
        }
        default:
            return false;
        }
    }

    // ------------------------------------------------------------ DatePicker

    DatePicker::DatePicker( tg::Date* B ) : Bind( B ) { Interactive = true; Focusable = true; KeyActivates = true; } // Tab to it, Enter / Space clicks

    DatePicker::~DatePicker()
    {
        if ( Open ) { Open->OnClose = nullptr; Open->Close(); }
    }

    void DatePicker::SetDate( tg::Date D )
    {
        if ( D == GetDate() ) return;
        ( Bind ? *Bind : Own ) = D;
        if ( OnChange ) OnChange( D );
        Redraw();
    }

    void DatePicker::OpenCalendar()
    {
        if ( Open ) return;
        Popup& P = OpenPopup( *this, Bounds );
        P.MinWidth = 0;
        P.Layout.Padding = Insets( 10 );
        Open = &P;
        P.OnClose = [ this ] { Open = nullptr; Redraw(); };
        auto& C = P.Add<Calendar>( GetDate() );
        C.OnPick = [ this ]( const tg::Date& D ) { SetDate( D ); if ( Open ) Open->Close(); };
    }

    Vec2 DatePicker::Measure( float Avail )
    {
        const Font& F = *GetTheme().Body;
        return { std::min( Avail, std::ceil( F.Width( "0000-00-00" ) ) + 52 ), F.LineHeight + 16 };
    }

    void DatePicker::Draw( DrawList& D )
    {
        const Font& F = *GetTheme().Body;
        D.RectFilled( Bounds, Fill.Resolve(), 6 );
        D.RectStroke( Bounds, ( Open ? StrokeOpen : Stroke ).Resolve(), 6 );
        D.Text( F, { Bounds.x + 10, std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) }, GetDate().ToString(), GetTheme()[ Tok::Text ] );
        // little calendar glyph
        Rect G = { Bounds.Right() - 26, Bounds.y + Bounds.h * 0.5f - 7, 14, 13 };
        Color Gc = GetTheme()[ Tok::TextDim ];
        D.RectStroke( G, Gc, 2, 1.3f );
        D.RectFilled( { G.x, G.y + 3, G.w, 1.3f }, Gc );
        D.RectFilled( { G.x + 3, G.y - 2, 1.3f, 3 }, Gc );
        D.RectFilled( { G.Right() - 4.3f, G.y - 2, 1.3f, 3 }, Gc );
    }

    bool DatePicker::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() ) { if ( Open ) Open->Close(); else OpenCalendar(); }
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------ DateRangePicker

    DateRangePicker::DateRangePicker( tg::Date* A, tg::Date* B ) : StartBind( A ), EndBind( B ) { Interactive = true; Focusable = true; KeyActivates = true; }

    DateRangePicker::~DateRangePicker()
    {
        if ( Open ) { Open->OnClose = nullptr; Open->Close(); }
    }

    void DateRangePicker::SetRange( tg::Date A, tg::Date B )
    {
        if ( B < A ) std::swap( A, B );
        if ( A == GetStart() && B == GetEnd() ) return;
        ( StartBind ? *StartBind : OwnA ) = A;
        ( EndBind ? *EndBind : OwnB ) = B;
        if ( OnChange ) OnChange( A, B );
        Redraw();
    }

    BindValue DateRangePicker::BindGet() const
    {
        tg::Date A = GetStart(), B = GetEnd();
        return { { double( A.Year ), double( A.Month ), double( A.Day ), double( B.Year ), double( B.Month ), double( B.Day ) } };
    }

    void DateRangePicker::BindSet( const BindValue& V )
    {
        if ( V.V.size() >= 6 )
            SetRange( { int( V.V[ 0 ] ), int( V.V[ 1 ] ), int( V.V[ 2 ] ) }, { int( V.V[ 3 ] ), int( V.V[ 4 ] ), int( V.V[ 5 ] ) } );
    }

    void DateRangePicker::OpenCalendar()
    {
        if ( Open ) return;
        Popup& P = OpenPopup( *this, Bounds );
        P.MinWidth = 0;
        P.Layout.Padding = Insets( 10 );
        Open = &P;
        P.OnClose = [ this ] { Open = nullptr; Redraw(); };
        auto& C = P.Add<Calendar>( GetStart() );
        C.Range = true;
        C.RangeStart = GetStart();
        C.RangeEnd = GetEnd();
        C.OnRange = [ this ]( const tg::Date& A, const tg::Date& B ) { SetRange( A, B ); if ( Open ) Open->Close(); };
    }

    Vec2 DateRangePicker::Measure( float Avail )
    {
        const Font& F = *GetTheme().Body;
        return { std::min( Avail, std::ceil( F.Width( "0000-00-00  \xE2\x86\x92  0000-00-00" ) ) + 52 ), F.LineHeight + 16 };
    }

    void DateRangePicker::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& F = *T.Body;
        D.RectFilled( Bounds, T[ Tok::SurfaceAlt ], 6 );
        D.RectStroke( Bounds, Open ? T[ Tok::Accent ] : T[ Tok::Border ], 6 );
        std::string S = GetStart().ToString() + "  \xE2\x86\x92  " + GetEnd().ToString();
        D.Text( F, { Bounds.x + 10, std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) }, S, T[ Tok::Text ] );
    }

    bool DateRangePicker::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() ) { if ( Open ) Open->Close(); else OpenCalendar(); }
            return true;
        }
        return false;
    }
}
