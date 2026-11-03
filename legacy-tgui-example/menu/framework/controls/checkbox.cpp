#include "controls.hpp"

#include <algorithm>

using namespace TGUI;

int CurAlpha[264];
int TextColor[264];
int GradColor[264];

TGUI::Checkbox::Checkbox( std::string Name, bool* Value )
{
	this->Name = Name;
	this->Value = Value;

	this->Flags = CLICKABLE | ISACTIVE;
}

void TGUI::Checkbox::Run()
{
	if (Focus::ActiveControl && Focus::ActiveControl != this)
		return;

	auto Width = Draw->GetTextWidth(this->Name.c_str(), Draw->F.Segoe);
	bool Hovering = Input::IsHovering(this->Pos, Vector2D(15 + Width, 15));

	if (Hovering && Input::KeyPress(VK_LBUTTON))
		*this->Value = !*this->Value;
}

void TGUI::Checkbox::Render()
{
	// background box
	Draw->RectFilled(this->Pos, Vector2D(15, 15), Style::MenuBG);

	// inner fill with alpha = CurAlpha
	Draw->RectFilled(
		this->Pos + Vector2D(2, 2),
		Vector2D(12, 12),
		Color((int)CurAlpha[this->CurrentIndex], Style::Accent.r(), Style::Accent.g(), Style::Accent.b())
	);

	// gradient overlay
	Draw->Gradient(
		this->Pos + Vector2D(2, 2),
		Vector2D(12, 12),
		Color(0, 0, 0, 0),
		Color((int)GradColor[this->CurrentIndex], 5, 5, 5)
    );

    // outlines
    Draw->Rect(this->Pos, Vector2D(15, 15), Style::LightOutline);
    Draw->Rect(this->Pos + Vector2D(1, 1), Vector2D(13, 13), Style::Outline);

    // label text with alpha = TextColor
    Draw->Text(
        this->Pos + Vector2D(20, 1),
        Draw->F.Segoe,
        this->Name,
        Color((int)TextColor[this->CurrentIndex], Style::Text.r(), Style::Text.g(), Style::Text.b())
    );
}

void TGUI::Checkbox::Render(float tabAlpha)
{
	auto ApplyAlpha = [tabAlpha](int baseAlpha) {
		return (int)(baseAlpha * tabAlpha);
		};

	if (!*this->Value)
	{
		if (CurAlpha[this->CurrentIndex] > 0)
			CurAlpha[this->CurrentIndex] -= 16.09;
		CurAlpha[this->CurrentIndex] = std::clamp(CurAlpha[this->CurrentIndex], 0, 255);

		if (TextColor[this->CurrentIndex] > 125)
			TextColor[this->CurrentIndex] -= 16.09;
		TextColor[this->CurrentIndex] = std::clamp(TextColor[this->CurrentIndex], 125, 255);

		if (GradColor[this->CurrentIndex] > 0)
			GradColor[this->CurrentIndex] -= 16.09;
		GradColor[this->CurrentIndex] = std::clamp(GradColor[this->CurrentIndex], 0, 75);
	}
	else
	{
		if (CurAlpha[this->CurrentIndex] < 255)
			CurAlpha[this->CurrentIndex] += 12.09;
		CurAlpha[this->CurrentIndex] = std::clamp(CurAlpha[this->CurrentIndex], 0, 255);

		if (TextColor[this->CurrentIndex] < 255)
			TextColor[this->CurrentIndex] += 12.09;
		TextColor[this->CurrentIndex] = std::clamp(TextColor[this->CurrentIndex], 125, 255);

		if (GradColor[this->CurrentIndex] < 75)
			GradColor[this->CurrentIndex] += 12.09;
		GradColor[this->CurrentIndex] = std::clamp(GradColor[this->CurrentIndex], 0, 75);
	}

	// background box
	Draw->RectFilled(this->Pos, Vector2D(15, 15),
		Color(ApplyAlpha(255), Style::MenuBG.r(), Style::MenuBG.g(), Style::MenuBG.b()));

	// inner fill with animated alpha (CurAlpha is already an alpha value)
	Draw->RectFilled(
		this->Pos + Vector2D(2, 2),
		Vector2D(12, 12),
		Color(ApplyAlpha(CurAlpha[this->CurrentIndex]),
			Style::Accent.r(), Style::Accent.g(), Style::Accent.b())
	);

	// gradient overlay (GradColor is also an alpha value)
	Draw->Gradient(
		this->Pos + Vector2D(2, 2),
		Vector2D(12, 12),
		Color(0, 0, 0, 0),
		Color(ApplyAlpha(GradColor[this->CurrentIndex]), 5, 5, 5)
	);

	// outlines
	Draw->Rect(this->Pos, Vector2D(15, 15),
		Color(ApplyAlpha(255), Style::LightOutline.r(), Style::LightOutline.g(), Style::LightOutline.b()));
	Draw->Rect(this->Pos + Vector2D(1, 1), Vector2D(13, 13),
		Color(ApplyAlpha(255), Style::Outline.r(), Style::Outline.g(), Style::Outline.b()));

	// label text (TextColor is an alpha value)
	Draw->Text(
		this->Pos + Vector2D(20, 1),
		Draw->F.Segoe,
		this->Name,
		Color(ApplyAlpha(TextColor[this->CurrentIndex]),
			Style::Text.r(), Style::Text.g(), Style::Text.b())
	);
}
