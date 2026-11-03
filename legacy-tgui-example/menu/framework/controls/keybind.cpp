#include "controls.hpp"
std::string KeyName[264];

using namespace TGUI;

TGUI::Keybind::Keybind( std::string Name, int* Key )
{
	this->Name = Name;
	this->Key = Key;
}

void TGUI::Keybind::Render()
{

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
}

void TGUI::Keybind::Render(float tabAlpha)
{
	Vector2D Size = { 40, 15 };
	Vector2D Offset = { 111, 0 };

	auto ApplyAlpha = [tabAlpha](Color c) {
		return Color((int)(c.a() * tabAlpha), c.r(), c.g(), c.b());
		};

	// Label
	Draw->Text(this->Pos, Draw->F.Segoe, this->Name, ApplyAlpha(Style::TextDim));

	// Box background
	Draw->RectFilled(this->Pos + Offset, Size, ApplyAlpha(Style::MenuBG));

	// Outlines
	Draw->Rect(this->Pos + Offset, Size, ApplyAlpha(Style::LightOutline));
	Draw->Rect(this->Pos + Offset + Vector2D(1, 1), Size - Vector2D(2, 2), ApplyAlpha(Style::Outline));

	// Key text
	auto Width = Draw->GetTextWidth(KeyName[Menu::Index].c_str(), Draw->F.Segoe);
	Draw->Text(this->Pos + Offset + Vector2D(20, 1) - Vector2D(Width / 2, 0),
		Draw->F.Segoe,
		KeyName[Menu::Index],
		ApplyAlpha(Style::Text));
}
