// Combo (pick one) and MultiCombo (pick several) — a field that opens a scrollable dropdown.
//
//   auto& C = Parent.Add<tg::Combo>( std::vector<std::string>{ "Low", "Medium", "High" }, &Quality );
//   auto& M = Parent.Add<tg::MultiCombo>( Regions, &RegionMask );     // std::vector<bool>
#pragma once
#include "../theme.h"
#include "../widget.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Popup;
    class ScrollView;
    class ListItem;

    struct ComboStyle
    {
        Paint Fill = Tok::SurfaceAlt;
        Paint Stroke = Tok::Border;
        Paint StrokeOpen = Tok::Accent;
        Paint TextColor = Tok::Text;
        Paint Placeholder = Tok::TextDim;
        Paint Chevron = Tok::TextDim;
        float Radius = 6;
        Insets Padding = { 10, 7 };
        float MaxPopupHeight = 260; // longer lists scroll
    };

    // Shared field + dropdown behaviour. Subclass to make other pickers.
    class DropdownBase : public Widget
    {
    public:
        DropdownBase( std::vector<std::string> Items );
        ~DropdownBase() override;

        std::string Placeholder = "Select...";
        ComboStyle Style;
        static ComboStyle& Defaults();

        const std::vector<std::string>& GetItems() const { return Items; }
        void SetItems( std::vector<std::string> NewItems );

        bool IsOpen() const { return Open != nullptr; }
        void OpenList();
        void CloseList();

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;
        bool OnEvent( const Event& E ) override;
        Cursor GetCursor() const override { return Cursor::Hand; }

    protected:
        std::vector<std::string> Items;
        std::vector<ListItem*> Rows; // valid while open

        virtual std::string DisplayText( float MaxWidth ) const = 0;
        virtual bool StepSelection( int Delta ) { (void)Delta; return false; }
        virtual bool IsItemSelected( size_t i ) const = 0;
        virtual void OnItemClicked( size_t i ) = 0;
        virtual bool MultiSelect() const { return false; }
        void SyncRows();

    private:
        Popup* Open = nullptr;
        float OpenT = 0, HoverT = 0;
    };

    class Combo : public DropdownBase
    {
    public:
        explicit Combo( std::vector<std::string> Items = {}, int* Bind = nullptr );

        int* Bind;
        std::function<void( int )> OnChange;

        int GetIndex() const { return Bind ? *Bind : Own; }
        void SetIndex( int i );
        const std::string* GetSelectedText() const;
        bool CanBind() const override { return true; }
        BindValue BindGet() const override;
        void BindSet( const BindValue& V ) override;
        void BindEditor( Container& Into, BindValue& V ) override;
        std::string BindText( const BindValue& V ) const override;

    protected:
        std::string DisplayText( float MaxWidth ) const override;
        bool IsItemSelected( size_t i ) const override { return int( i ) == GetIndex(); }
        void OnItemClicked( size_t i ) override;
        bool StepSelection( int D ) override
        {
            if ( Items.empty() ) return false;
            SetIndex( std::clamp( GetIndex() + D, 0, int( Items.size() ) - 1 ) );
            return true;
        }

    private:
        int Own = -1;
    };

    class MultiCombo : public DropdownBase
    {
    public:
        explicit MultiCombo( std::vector<std::string> Items = {}, std::vector<bool>* Bind = nullptr );

        std::vector<bool>* Bind;
        std::function<void( const std::vector<bool>& )> OnChange;

        bool IsSelected( size_t i ) const;
        void SetSelected( size_t i, bool On );
        std::vector<int> SelectedIndices() const;
        // a bind can switch any item on or off and leave the rest alone
        bool CanBind() const override { return true; }
        BindValue BindGet() const override;
        BindValue BindDefault() const override;
        void BindSet( const BindValue& V ) override;
        void BindEditor( Container& Into, BindValue& V ) override;
        std::string BindText( const BindValue& V ) const override;

    protected:
        std::string DisplayText( float MaxWidth ) const override;
        bool IsItemSelected( size_t i ) const override { return IsSelected( i ); }
        void OnItemClicked( size_t i ) override { SetSelected( i, !IsSelected( i ) ); }
        bool MultiSelect() const override { return true; }

    private:
        std::vector<bool> Own;
        std::vector<bool>& Sel();
        const std::vector<bool>& Sel() const;
    };
}
