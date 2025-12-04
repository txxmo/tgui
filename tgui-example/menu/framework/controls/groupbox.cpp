#include "controls.hpp"
using namespace TGUI;

static short int OuterPos[MAXCONTROLS];
int Alphpapa[MAXCONTROLS];
int headerHeight = 20;

void TGUI::Window::AddGroup(std::unique_ptr<Groupbox> group)
{
    // Position groupbox inside window (window header offset likely 21)
    group->Pos = this->Pos + Vector2D(5, 21) + group->Pos;

    // Initialize per-group scroll/alpha using the group’s stable id
    const int gid = group->GroupId; // see Groupbox constructor below
    OuterPos[gid] = 0;
    Alphpapa[gid] = 0;

    group->Visible = (this->Tab && *this->Tab == group->Tab);

    if (group->Visible)
        group->Run();

    groups.push_back(std::move(group)); // keep ownership
}

TGUI::Groupbox::Groupbox() {}

TGUI::Groupbox::Groupbox(std::string Name, Vector2D Pos, Vector2D Size, int Index, int Tab)
{
    this->Name = Name;
    this->Pos = Pos;
    this->Size = Size;
    this->Tab = Tab;
    this->RelativePos = Pos;

    // Use a stable group id for indexing shared arrays (OuterPos/Alphpapa)
    this->GroupId = Index;

    // Reset internal stacking
    this->NextY = 0;
}

void TGUI::Groupbox::AddControl(std::unique_ptr<BaseControl> control)
{
    if (!Visible)
        return;

    // Controls use Menu::Index for their own indexing
    control->CurrentIndex = Menu::Index;

    // Absolute position relative to groupbox content area (below header)
    control->RelativePos = { 7, (float)headerHeight + NextY };
    control->Pos = this->Pos + control->RelativePos;
    NextY += control->GetHeight();

    // Keep if you use it elsewhere
    MaxElementsY[GroupId] += control->GetHeight();

    Menu::Index++;
    Controls.push_back(std::move(control));
}

void TGUI::Groupbox::Run()
{
    // Reset stacking offset when (re)running the group
    NextY = 0;
}

void Groupbox::Render()
{
    if (!Visible) return;

    // draw groupbox outline
    Draw->Rect(this->Pos - Vector2D(1, 1), this->Size + Vector2D(2, 2), Style::Outline);
    Draw->Rect(this->Pos - Vector2D(2, 2), this->Size + Vector2D(4, 4), Style::LightOutline);

    // title
    Draw->Text(this->Pos + Vector2D(this->Size.x / 2 -
        Draw->GetTextWidth(this->Name.c_str(), Draw->F.Segoe) / 2, 3),
        Draw->F.Segoe, this->Name, Style::Text, false);

    // clipping region (exclude header)
    RECT clip;
    clip.left = (LONG)Pos.x;
    clip.top = (LONG)(Pos.y + headerHeight);
    clip.right = (LONG)(Pos.x + Size.x);
    clip.bottom = (LONG)(Pos.y + Size.y);
    Draw->SetClipRect(clip);

    // Use the stable group id here
    const int gid = this->GroupId;
    const int yOffset = OuterPos[gid];

    for (auto& control : Controls)
    {
        Vector2D basePos = control->Pos;

        // Apply scroll offset only during render
        control->Pos.y = basePos.y + yOffset;

        bool insideClip =
            control->Pos.y >= clip.top &&
            (control->Pos.y + control->GetHeight()) <= clip.bottom;

        if (insideClip)
        {
            control->Run();
            control->Render();
        }
        else
        {
            // Still render (clipped), but skip input if you prefer
            control->Render();
        }

        // Restore absolute position
        control->Pos = basePos;
    }

    Draw->ResetClip();
}

void Groupbox::Render(float alpha)
{
    if (!Visible) return;

    auto ApplyAlpha = [alpha](Color c) {
        return Color((int)(c.a() * alpha), c.r(), c.g(), c.b());
        };

    // background outlines
    Draw->Rect(this->Pos - Vector2D(1, 1), this->Size + Vector2D(2, 2), ApplyAlpha(Style::Outline));
    Draw->Rect(this->Pos - Vector2D(2, 2), this->Size + Vector2D(4, 4), ApplyAlpha(Style::LightOutline));

    // title
    Draw->Text(this->Pos + Vector2D(this->Size.x / 2 -
        Draw->GetTextWidth(this->Name.c_str(), Draw->F.Segoe) / 2, 3),
        Draw->F.Segoe, this->Name, ApplyAlpha(Style::Text), false);

    // clipping region
    RECT clip;
    clip.left = (LONG)Pos.x;
    clip.top = (LONG)(Pos.y + headerHeight);
    clip.right = (LONG)(Pos.x + Size.x);
    clip.bottom = (LONG)(Pos.y + Size.y);
    Draw->SetClipRect(clip);

    const int gid = this->GroupId;
    const int yOffset = OuterPos[gid];

    for (auto& control : Controls)
    {
        // Absolute position = groupbox position + control's relative offset + scroll offset
        Vector2D drawPos = this->Pos + control->RelativePos;
        drawPos.y += yOffset;

        bool insideClip =
            drawPos.y >= clip.top &&
            (drawPos.y + control->GetHeight()) <= clip.bottom;

        // Temporarily set Pos for Run/Render
        Vector2D basePos = control->Pos;
        control->Pos = drawPos;

        if (insideClip)
        {
            control->Run();
            control->Render(alpha);
        }
        else
        {
            control->Render(alpha);
        }

        // Restore stored absolute position if you want to keep it stable
        control->Pos = basePos;
    }

    Draw->ResetClip();

    // Second pass: render dropdowns that are open
    for (auto& control : Controls)
        if (auto combo = dynamic_cast<Combobox*>(control.get()))
            if (combo->IsOpen)
                combo->RenderDropDown(alpha, this->Pos + control->RelativePos + Vector2D(0, OuterPos[gid]));
}

int Groupbox::computeContentHeight() const {
    int bottom = 0;
    for (auto& control : Controls) {
        int controlBottom = control->RelativePos.y + control->GetHeight();
        if (controlBottom > bottom)
            bottom = controlBottom;
    }
    return bottom;
}

void Groupbox::Scroll()
{
    const int gid = this->GroupId;

    bool hovered = Input::IsHovering(this->Pos, this->Size);
    int contentHeight = computeContentHeight();
    int visibleHeight = this->Size.y - headerHeight;
    int maxScroll = myMax(contentHeight - visibleHeight, 0);

    if (!Controls.empty())
        maxScroll -= Controls.back()->GetHeight();

    // mouse wheel scroll
    if (hovered && Input::MouseWheel != 0 && contentHeight > visibleHeight)
    {
        OuterPos[gid] += Input::MouseWheel * 10;
        Input::MouseWheel = 0;

        OuterPos[gid] = myMin((int)OuterPos[gid], 0);
        OuterPos[gid] = myMax((int)OuterPos[gid],  (-maxScroll - 5));
    }

    // fade alpha
    if (hovered)
        Alphpapa[gid] = myMin(Alphpapa[gid] + 12, 255);
    else
        Alphpapa[gid] = myMax(Alphpapa[gid] - 16, 0);

    // scrollbar
    if (this->Visible && contentHeight > visibleHeight)
    {
        float scrollRatio = (float)-OuterPos[gid] / (float)maxScroll;
        scrollRatio = myMin(myMax(scrollRatio, 0.0f), 1.0f);

        float barHeightRatio = (float)visibleHeight / (float)contentHeight;
        barHeightRatio = myMin(myMax(barHeightRatio, 0.1f), 1.0f);

        float barHeight = barHeightRatio * (float)visibleHeight;
        float scrollRange = (float)this->Size.y - barHeight - 6;
        float barY = this->Pos.y + 3 + scrollRatio * scrollRange;

        Draw->RectFilled(
            Vector2D(this->Pos.x + this->Size.x - 4, barY),
            Vector2D(2, barHeight),
            Color(Alphpapa[gid],
                Style::Accent.r(),
                Style::Accent.g(),
                Style::Accent.b())
        );
    }
}