// DatePicker — a date field with a month calendar popup.
//
//   static tg::Date Expires = tg::Date::Today();
//   W.Date( "Expires", &Expires );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class Popup;

    struct Date
    {
        int Year = 2000, Month = 1, Day = 1; // Month 1..12

        static Date Today();
        static int DaysInMonth( int Year, int Month );
        int Weekday() const;                 // 0 = Monday ... 6 = Sunday
        std::string ToString() const;        // "2026-11-07"
        bool operator==( const Date& ) const = default;
        bool operator<( const Date& O ) const { return Year != O.Year ? Year < O.Year : Month != O.Month ? Month < O.Month : Day < O.Day; }
    };

    class DatePicker : public Widget
    {
    public:
        explicit DatePicker( tg::Date* Bind = nullptr );
        ~DatePicker() override;

        tg::Date* Bind;
        std::function<void( const tg::Date& )> OnChange;
        Paint Fill = Tok::SurfaceAlt, Stroke = Tok::Border, StrokeOpen = Tok::Accent;

        tg::Date GetDate() const { return Bind ? *Bind : Own; }
        void SetDate( tg::Date D );
        void OpenCalendar();
        bool IsOpen() const { return Open != nullptr; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        tg::Date Own = tg::Date::Today();
        Popup* Open = nullptr;
    };

    // The month grid on its own (used by DatePicker's popup; embeddable).
    class Calendar : public Widget
    {
    public:
        explicit Calendar( tg::Date Selected );
        tg::Date Selected;
        int ViewYear, ViewMonth;
        std::function<void( const tg::Date& )> OnPick;

        Rect DayRect( int Day ) const; // where a day of the shown month is drawn
        void Shift( int Months );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    private:
        int HoverDay = -1;
        Rect PrevR, NextR, TodayR;
        static constexpr float Cell = 34, Head = 34;
        int DayAt( Vec2 P ) const;
    };
}
