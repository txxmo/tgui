#include "../tgui.hpp"
#include "controls.hpp"

using namespace TGUI;

TGUI::Window::Window(std::string Name, Vector2D Pos, Vector2D Size, int* Tab)
{
    this->Name = Name;
    this->Pos = Pos;
    this->Size = Size;
    this->Tab = Tab;
    this->CurrentTab = *Tab;
    this->TargetTab = *Tab;
}

void TGUI::Window::End()
{
    auto TabSize = Size.x / Tabs.size() - 1;

    // background and accent header
    Draw->RectFilled(this->Pos, this->Size, Style::MenuBG);
    Draw->RectFilled(this->Pos + Vector2D(0, 20), Vector2D(this->Size.x, 2), Style::Accent);
    Draw->RectFilled(this->Pos + Vector2D(0, 21), Vector2D(this->Size.x, 1),
        Color(Style::Accent.r() - 15, Style::Accent.g() - 15, Style::Accent.b() - 15));

    Draw->Rect(this->Pos, this->Size, Style::Outline);
    Draw->Rect(this->Pos + Vector2D(1, 1), this->Size - Vector2D(2, 2), Style::LightOutline);

    auto Wid = Draw->GetTextWidth(this->Name.c_str(), Draw->F.Segoe);
    Draw->Text(this->Pos + Vector2D(this->Size.x / 2 - Wid / 2, 3), Draw->F.Segoe, this->Name, Style::Text);

    Draw->Text(this->Pos + Vector2D(this->Size.x - 28, this->Size.y - 16), Draw->F.Segoe, "v.2.5", Style::TextDim);

    // --- Draw and handle tabs ---
    for (int i = 0; i < this->Tabs.size(); i++)
    {
        auto TabName = this->Tabs[i];

        RECT TabPos;
        TabPos.left = (LONG)(Pos.x + (i * TabSize) + 2);
        TabPos.top = (LONG)(Pos.y + 23);
        TabPos.right = TabPos.left + TabSize;
        TabPos.bottom = TabPos.top + 20;

        bool Hovered = Input::IsHovering(
            { (float)TabPos.left, (float)TabPos.top },
            { (float)(TabPos.right - TabPos.left), (float)(TabPos.bottom - TabPos.top) });

        if (Hovered && Input::KeyPress(VK_LBUTTON))
        {
            if (i != CurrentTab)   // only animate if switching to a different tab
            {
                TargetTab = i;
                TabAnimProgress = 0.0f;
                Animating = true;
            }
        }

        // Tab colors (no fade here)
        Color TabColor = *this->Tab != i
            ? (Hovered ? Color(53, 53, 53) : Color(43, 43, 43))
            : Color(244, 244, 244);

        // Draw tab visuals
        Draw->Rect(Vector2D(TabPos.left, TabPos.top),
            Vector2D(TabPos.right - TabPos.left, TabPos.bottom - TabPos.top),
            Style::LightOutline);
        Draw->Rect(Vector2D(TabPos.left + 1, TabPos.top + 1),
            Vector2D(TabPos.right - TabPos.left - 2, TabPos.bottom - TabPos.top - 2),
            Style::Outline);
        Draw->RectFilled(Vector2D(TabPos.left + 2, TabPos.top + 2),
            Vector2D(TabPos.right - TabPos.left - 3, TabPos.bottom - TabPos.top - 3),
            Style::DarkBG);

        Draw->Text(Vector2D(TabPos.left + (TabSize / 2), TabPos.top + 3),
            Draw->F.Segoe, TabName,
            *this->Tab == i ? Style::Text : Style::TextDim, true);
    }

    // --- Animate tab transition ---
    if (Animating)
    {
        TabAnimProgress += 0.08f; // speed factor
        if (TabAnimProgress >= 1.0f)
        {
            TabAnimProgress = 1.0f;
            CurrentTab = TargetTab;
            *Tab = CurrentTab;   // commit tab change
            Animating = false;
        }
    }
    else
    {
        CurrentTab = *Tab;
    }

    // --- Render groups with fade ---
    for (auto& group : groups)
    {
        const bool isCurrent = (group->Tab == CurrentTab);
        const bool isTarget = (Animating && group->Tab == TargetTab);

        if (isCurrent || isTarget)
        {
            group->Pos = this->Pos + Vector2D(5, 21) + group->RelativePos;
            group->Visible = true; // <-- ensure group renders

            float alpha = isCurrent
                ? (Animating ? (1.0f - TabAnimProgress) : 1.0f)
                : TabAnimProgress;

            group->Render(alpha);
            group->Run();
            group->Scroll();
        }
        else
        {
            group->Visible = false; // <-- hide non-active tab groups
        }
    }
}

void TGUI::Window::AddTab(std::string Name)
{
    this->Tabs.push_back(Name);
}

void TGUI::Window::HandlePos()
{
    static Vector2D DragOffset;
    static bool ShouldDrag = false;
    bool Clicked = (GetAsyncKeyState(VK_LBUTTON) & 0x8000);

    if (ShouldDrag && !Clicked)
        ShouldDrag = false;

    if (ShouldDrag && Clicked)
    {
        this->Pos = Input::MousePos - DragOffset;
        Menu::WindowPos = this->Pos; // keep global in sync if needed
    }

    if (Input::IsHovering(Pos, { Size.x, 20 }) && Clicked)
    {
        ShouldDrag = true;
        DragOffset = Input::MousePos - this->Pos;
    }
}

Vector2D TGUI::Window::GetPos()
{
    return this->Pos;
}

void TGUI::Window::SetPos(Vector2D Pos)
{
    this->Pos = Pos;
}
