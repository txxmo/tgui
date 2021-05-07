#include "controls.hpp"

using namespace TGUI;

void TGUI::Groupbox::AddControl( Combobox* Combo )
{
	if ( this->Visible )
	{
		Combo->CurrentIndex = Menu::Index;

		D3DVIEWPORT9 OldViewport = Draw->GetViewport( );
		Draw->SetViewport( { ( DWORD )this->Pos.x, ( DWORD )this->Pos.y + 25, ( DWORD )this->Size.x, ( DWORD )this->Size.y - 25, 0.f, 1.f } );

		Combo->Pos = Vector2D( this->ControlOffset[ this->CurrentIndex ].x + 1, this->ControlOffset[ this->CurrentIndex ].y );
		Combo->Run( );

		this->ControlOffset[ this->CurrentIndex ].y += 40;
		this->MaxElementsY[ this->CurrentIndex ] += 40;
		Menu::Index++;

		Draw->SetViewport( OldViewport );
	}
}

TGUI::Combobox::Combobox( std::string Name, int* Value, std::vector<std::string> Items )
{
	this->Name = Name;
	this->Value = Value;
	this->Items = Items;
}

void TGUI::Combobox::Run( )
{
	static int LastHoverIndex = 1;
	static int ShouldBeOpen = false;
	static Vector2D ComboSize = { 107, 20 };

	bool HoverSub = Input::IsHovering( this->Pos, { 20, 20 } );
	bool HoverAdd = Input::IsHovering( this->Pos + Vector2D( ComboSize.x + 24, 0 ), { 20, 20 } );

	if ( *this->Value > 0 && HoverSub && Input::KeyPress( VK_LBUTTON ) )
		*this->Value -= 1;

	if ( *this->Value < this->Items.size( ) - 1 && HoverAdd && Input::KeyPress( VK_LBUTTON ) )
		*this->Value += 1;

	/* Main */
	Draw->Rect( this->Pos + Vector2D( 22, 0 ), ComboSize, Style::LightOutline );
	Draw->Rect( this->Pos + Vector2D( 23, 1 ), ComboSize - Vector2D( 2, 2 ), Style::Outline );
	Draw->RectFilled( this->Pos + Vector2D( 24, 2 ), ComboSize - Vector2D( 3, 3 ), Style::MenuBG );

	/* Sub */
	Draw->Rect( this->Pos, { 20, 20 }, Style::LightOutline );
	Draw->Rect( this->Pos + Vector2D( 1, 1 ), Vector2D( 18, 18 ), Style::Outline );
	Draw->RectFilled( this->Pos + Vector2D( 2, 2 ), Vector2D( 17, 17 ), Style::MenuBG );
	Draw->Text( this->Pos + Vector2D( 9, 3 ), Draw->F.SegoeBold, "-", Style::TextDim );

	/* Add */
	Draw->Rect( this->Pos + Vector2D( ComboSize.x + 24, 0 ), { 20, 20 }, Style::LightOutline );
	Draw->Rect( this->Pos + Vector2D( ComboSize.x + 25, 1 ), Vector2D( 18, 18 ), Style::Outline );
	Draw->RectFilled( this->Pos + Vector2D( ComboSize.x + 26, 2 ), Vector2D( 17, 17 ), Style::MenuBG );
	Draw->Text( this->Pos + Vector2D( ComboSize.x + 31, 3 ), Draw->F.SegoeBold, "+", Style::TextDim );

	Draw->Text( this->Pos + Vector2D( 0, 21 ), Draw->F.Segoe, this->Name, Style::Text );
	auto Wid = Draw->GetTextWidth( this->Items[ *this->Value ].c_str( ), Draw->F.Segoe );
	Draw->Text( this->Pos + Vector2D( 22 + ComboSize.x / 2 - Wid / 2, 4 ), Draw->F.Segoe, this->Items[ *this->Value ], Style::Text );
}