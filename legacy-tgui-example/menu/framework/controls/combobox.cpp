#include "controls.hpp"

using namespace TGUI;

TGUI::Combobox::Combobox( std::string Name, int* Value, std::vector<std::string> Items )
{
	this->Name = Name;
	this->Value = Value;
	this->Items = Items;
	this->IsOpen = false;
}

void TGUI::Combobox::Render()
{

}

void Combobox::Run()
{
	static Vector2D ComboSize = { 107, 20 };
	static Vector2D ItemSize = { 107, 20 };

	bool hoverMain = Input::IsHovering(this->Pos, ComboSize);
	if (hoverMain && Input::KeyPress(VK_LBUTTON)) {
		IsOpen = !IsOpen;
		if (IsOpen)
			TGUI::Focus::ActiveControl = this;
		else if (TGUI::Focus::ActiveControl == this)
			TGUI::Focus::ActiveControl = nullptr;

		Input::MouseConsumed = true; 
	}

	if (IsOpen) {
		bool clickedInside = false;

		// Items
		for (int i = 0; i < (int)Items.size(); i++) {
			Vector2D itemPos = this->Pos + Vector2D(0, ComboSize.y * (i + 1));
			if (Input::IsHovering(itemPos, ItemSize)) {
				clickedInside = true;
				if (Input::KeyPress(VK_LBUTTON)) {
					*Value = i;
					IsOpen = false;
					TGUI::Focus::ActiveControl = nullptr;
					Input::MouseConsumed = true;
				}
			}
		}

		// Outside click closes
		if (Input::KeyPress(VK_LBUTTON) && !hoverMain && !clickedInside) {
			IsOpen = false;
			TGUI::Focus::ActiveControl = nullptr;
			Input::MouseConsumed = true;
		}
	}
	
}

void TGUI::Combobox::Render(float tabAlpha)
{
	static Vector2D ComboSize = { 107, 20 };

	auto ApplyAlpha = [tabAlpha](Color c) {
		return Color((int)(c.a() * tabAlpha), c.r(), c.g(), c.b());
		};

	// Draw main box
	Draw->Rect(this->Pos, ComboSize, ApplyAlpha(Style::LightOutline));
	Draw->Rect(this->Pos + Vector2D(1, 1), ComboSize - Vector2D(2, 2), ApplyAlpha(Style::Outline));
	Draw->RectFilled(this->Pos + Vector2D(2, 2), ComboSize - Vector2D(3, 3), ApplyAlpha(Style::MenuBG));

	// Current item text
	auto Wid = Draw->GetTextWidth(this->Items[*this->Value].c_str(), Draw->F.Segoe);
	Draw->Text(this->Pos + Vector2D(ComboSize.x / 2 - Wid / 2, 4),
		Draw->F.Segoe,
		this->Items[*this->Value],
		ApplyAlpha(Style::Text));

	// Dropdown arrow
	Draw->Text(this->Pos + Vector2D(ComboSize.x - 12, 4), Draw->F.SegoeBold, "▼", ApplyAlpha(Style::TextDim));

	// Label
	Draw->Text(this->Pos + Vector2D(0, ComboSize.y + 2), Draw->F.Segoe, this->Name, ApplyAlpha(Style::Text));
}

void TGUI::Combobox::RenderDropDown(float tabAlpha, Vector2D drawPos)
{
	static Vector2D ItemSize = { 107, 20 };

	auto ApplyAlpha = [tabAlpha](Color c) {
		return Color((int)(c.a() * tabAlpha), c.r(), c.g(), c.b());
		};

	for (int i = 0; i < (int)Items.size(); i++)
	{
		Vector2D itemPos = drawPos + Vector2D(0, ItemSize.y * (i + 1));

		// background + outline
		Draw->Rect(itemPos, ItemSize, ApplyAlpha(Style::LightOutline));
		Draw->Rect(itemPos + Vector2D(1, 1), ItemSize - Vector2D(2, 2), ApplyAlpha(Style::Outline));
		Draw->RectFilled(itemPos + Vector2D(2, 2), ItemSize - Vector2D(3, 3), ApplyAlpha(Style::MenuBG));

		// item text centered
		auto itemWid = Draw->GetTextWidth(Items[i].c_str(), Draw->F.Segoe);
		Draw->Text(itemPos + Vector2D(ItemSize.x / 2 - itemWid / 2, 4),
			Draw->F.Segoe,
			Items[i],
			ApplyAlpha(Style::Text));
	}
}
