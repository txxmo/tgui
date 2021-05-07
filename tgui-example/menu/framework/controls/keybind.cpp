#include "controls.hpp"
std::string KeyName[264];

using namespace TGUI;

void TGUI::Groupbox::AddControl( Keybind* Bind )
{
	if ( this->Visible )
	{
		Bind->CurrentIndex = Menu::Index;

		D3DVIEWPORT9 OldViewport = Draw->GetViewport( );
		Draw->SetViewport( { ( DWORD )this->Pos.x, ( DWORD )this->Pos.y + 25, ( DWORD )this->Size.x, ( DWORD )this->Size.y - 25, 0.f, 1.f } );

		Bind->Pos = Vector2D( this->ControlOffset[ this->CurrentIndex ].x + 2, this->ControlOffset[ this->CurrentIndex ].y );
		Bind->Run( );

		this->ControlOffset[ this->CurrentIndex ] += Vector2D( 0, 25 );
		this->MaxElementsY[ this->CurrentIndex ] += 25;
		Menu::Index++;

		Draw->SetViewport( OldViewport );
	}
}

TGUI::Keybind::Keybind( std::string Name, int* Key )
{
	this->Name = Name;
	this->Key = Key;
}

void TGUI::Keybind::Run( )
{
	Vector2D Size = { 40, 15 };
	Vector2D Offset = { 111, 0 };
	static int LastIndex = -1;
	bool Hovered = Input::IsHovering( this->Pos + Offset, Size );
	bool Active = LastIndex == Menu::Index;
	
	if ( Hovered && Input::KeyPress( VK_LBUTTON ) )
		LastIndex = Menu::Index;

	Draw->Text( this->Pos, Draw->F.Segoe, this->Name, Style::TextDim );
	Draw->RectFilled( this->Pos + Offset, Size, Style::MenuBG );
	Draw->Rect( this->Pos + Offset, Size, Style::LightOutline );
	Draw->Rect( this->Pos + Offset + Vector2D( 1, 1 ), Size - Vector2D( 2, 2 ), Style::Outline );

	if ( Active )
	{
		KeyName[ Menu::Index ] = "~";

		for ( int i = 0; i < 255; i++ )
		{
			if ( Input::KeyPress( i ) )
			{
				if ( i == VK_ESCAPE )
				{
					*this->Key = -1;

					LastIndex = -1;

					return;
				}

				*this->Key = i;

				LastIndex = -1;

				return;
			}
		}
	}

	if ( *this->Key >= 0 )
	{
		if (!Active )
			KeyName[ Menu::Index ] = KeyNames[ *this->Key ];
		else
			KeyName[ Menu::Index ] = "~";

	}

	auto Width = Draw->GetTextWidth( KeyName[ Menu::Index ].c_str( ), Draw->F.Segoe );
	Draw->Text( this->Pos + Offset + Vector2D( 20, 1 ) - Vector2D( Width / 2, 0 ), Draw->F.Segoe, KeyName[ Menu::Index ], Style::Text );
}
