
#include "controls.hpp"

using namespace TGUI;

TGUI::Button::Button( std::string Name, ButtonFn Func )
{
	this->Name = Name;
	this->Func = Func;
}

void TGUI::Button::Render()
{

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
}

void TGUI::Button::Render(float tabAlpha)
{
	Vector2D ButtonSize = { 151, 20 };

	// Helper to apply tab alpha to any color
	auto ApplyAlpha = [tabAlpha](Color c) {
		return Color((int)(c.a() * tabAlpha), c.r(), c.g(), c.b());
		};

	// Outlines
	Draw->Rect(this->Pos, ButtonSize, ApplyAlpha(Style::LightOutline));
	Draw->Rect(this->Pos + Vector2D(1, 1), ButtonSize - Vector2D(2, 2), ApplyAlpha(Style::Outline));

	// Background
	Draw->RectFilled(this->Pos + Vector2D(2, 2),
		ButtonSize - Vector2D(3, 3),
		ApplyAlpha(Style::DarkBG));

	// Centered text
	auto Wid = Draw->GetTextWidth(this->Name.c_str(), Draw->F.Segoe);
	Draw->Text(this->Pos + Vector2D(ButtonSize.x / 2 - Wid / 2, 3),
		Draw->F.Segoe,
		this->Name,
		ApplyAlpha(Style::Text));
}
