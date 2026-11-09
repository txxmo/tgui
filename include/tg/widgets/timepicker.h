// TimePicker — "14:30" field; click for hour / minute (/ second) columns. Arrows / wheel change the
// part under the mouse.
//
//   static tg::Time At{ 14, 30 };
//   W.Time( "Backup at", &At );
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <functional>
#include <string>

namespace tg
{
    class Popup;

    struct Time
    {
        int H = 0, M = 0, S = 0;
        std::string ToString( bool Seconds = false, bool H24 = true ) const;
        bool operator==( const Time& ) const = default;
    };

    class TimePicker : public Widget
    {
    public:
        explicit TimePicker( tg::Time* Bind = nullptr );
        ~TimePicker() override;
        tg::Time* Bind;
        bool Seconds = false;
        bool H24 = true;       // false: 2:30 PM
        int MinuteStep = 1;    // the popup's minute list (e.g. 5 or 15)
        std::function<void( const tg::Time& )> OnChange;

        tg::Time GetTime() const { return Bind ? *Bind : Own; }
        void SetTime( tg::Time T );
        bool IsOpen() const { return Open != nullptr; }
        void OpenList();

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
        bool CanBind() const override { return true; }
        BindValue BindGet() const override { tg::Time T = GetTime(); return { { double( T.H ), double( T.M ), double( T.S ) } }; }
        void BindSet( const BindValue& V ) override;

    private:
        tg::Time Own;
        Popup* Open = nullptr;
        int Part( Vec2 P ) const; // 0 hour, 1 minute, 2 second under the mouse
        void Bump( int Part, int D );
    };
}
