
#include "controls.hpp"

using namespace TGUI;

void TGUI::Groupbox::AddControl( Button* Butt )
{
	if ( this->Visible )
	{
		Butt->CurrentIndex = Menu::Index;

		D3DVIEWPORT9 OldViewport = Draw->GetViewport( );
		Draw->SetViewport( { ( DWORD )this->Pos.x, ( DWORD )this->Pos.y + 25, ( DWORD )this->Size.x, ( DWORD )this->Size.y - 25, 0.f, 1.f } );

		Butt->Pos = Vector2D( this->ControlOffset[ this->CurrentIndex ].x + 2, this->ControlOffset[ this->CurrentIndex ].y );
		Butt->Run( );

		this->ControlOffset[ this->CurrentIndex ].y += 30;
		this->MaxElementsY[ this->CurrentIndex ] += 30;
		Menu::Index++;

		Draw->SetViewport( OldViewport );
	}
}


TGUI::Button::Button( std::string Name, ButtonFn Func )
{
	this->Name = Name;
	this->Func = Func;
}

void TGUI::Button::Run( )
{
	if ( !Func )
		return;

	Vector2D ButtonSize = { 151, 20 };

	bool Hovering = Input::IsHovering( this->Pos, ButtonSize );

	bool Active = Hovering && Input::KeyPress( VK_LBUTTON );

	static bool Once;

	if ( Active && Input::KeyPress( VK_LBUTTON ) && !Once )
	{
		this->Func( );
		Once = true;
	}

	if ( !Input::KeyPress( VK_LBUTTON ) )
	{
		Once = false;
	}

	Draw->Rect( this->Pos, ButtonSize, Style::LightOutline );
	Draw->Rect( this->Pos + Vector2D( 1, 1 ), ButtonSize - Vector2D( 2, 2 ), Style::Outline );
	Draw->RectFilled( this->Pos + Vector2D( 2, 2 ), ButtonSize - Vector2D( 3, 3 ), Style::DarkBG );

	auto Wid = Draw->GetTextWidth( this->Name.c_str( ), Draw->F.Segoe );
	Draw->Text( this->Pos + Vector2D( ButtonSize.x / 2 - Wid / 2, 3 ), Draw->F.Segoe, this->Name, Style::Text );
}