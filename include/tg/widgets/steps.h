// Pagination, Stepper, Wizard, Accordion.
//
//   W.Pagination( &Page, 20 );                         // ‹ 1 … 6 7 8 … 20 ›
//   W.Stepper( { "Account", "Server", "Confirm" }, &Step );
//
//   auto& Wz = W.Wizard();                             // stepper + pages + Back / Next / Finish
//   auto& A = Wz.Step( "Account" );   A.Input( "Email", &Email );
//   auto& B = Wz.Step( "Server" );    B.Combo( "Region", Regions, &Region );
//   Wz.CanAdvance = [&]( int Step ) { return Step != 0 || !Email.empty(); };
//   Wz.OnFinish = [&] { Create(); };
//
//   auto& Acc = W.Accordion();                         // only one section open at a time
//   Acc.Section( "General", true ).Checkbox( ... );
#pragma once
#include "container.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Collapsible;
    class Button;

    class Pagination : public Widget
    {
    public:
        Pagination( int* Page, int PageCount ); // pages are 0-based; shown 1-based
        int* Bind;
        int PageCount;
        int Neighbours = 1; // pages shown around the current one
        std::function<void( int )> OnChange;
        int GetPage() const { return Bind ? *Bind : Own; }
        void SetPage( int P );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }
        bool CanBind() const override { return true; }
        BindValue BindGet() const override { return { { double( GetPage() ) } }; }
        void BindSet( const BindValue& V ) override { if ( !V.V.empty() ) SetPage( int( V.V[ 0 ] ) ); }

    private:
        int Own = 0, Hover = -3;
        struct Cell { int Page; Rect R; }; // Page: -1 prev, -2 next, -10 ellipsis
        std::vector<Cell> Cells;
        void Build();
    };

    class Stepper : public Widget
    {
    public:
        Stepper( std::vector<std::string> Steps, int* Current = nullptr );
        std::vector<std::string> Steps;
        int* Bind;
        bool AllowJumpBack = true; // click a finished step to go back to it
        std::function<void( int )> OnChange;
        int GetStep() const { return Bind ? *Bind : Own; }
        void SetStep( int S );

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;

    private:
        int Own = 0;
        float Shown = -1;
        Rect StepRect( size_t i ) const;
    };

    class Wizard : public Container
    {
    public:
        Wizard();
        Container& Step( std::string Title );     // the page for a step
        std::function<bool( int Step )> CanAdvance; // validate before Next
        std::function<void()> OnFinish;
        std::function<void( int )> OnStepChange;
        int Current() const { return Cur; }
        void Next();
        void Back();
        void Go( int Step );

    private:
        tg::Stepper* Bar = nullptr;
        Container* Pages = nullptr;
        tg::Button *BackB = nullptr, *NextB = nullptr;
        std::vector<Container*> Steps;
        std::vector<std::string> Titles;
        int Cur = 0;
        void Sync();
    };

    class Accordion : public Container
    {
    public:
        Accordion();
        bool Exclusive = true; // opening a section closes the others
        tg::Collapsible& Section( std::string Title, bool Open = false );

    private:
        std::vector<tg::Collapsible*> Sections;
    };
}
