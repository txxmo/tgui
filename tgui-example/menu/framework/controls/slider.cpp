#include "controls.hpp"

using namespace TGUI;

TGUI::Slider::Slider( std::string Name, float* Value, float Min, float Max, bool Float )
{
	this->Name = Name;
	this->Value = Value;
	this->Min = Min;
	this->Max = Max;
	this->FloatS = Float;
}

// Smoothly interpolate 'current' toward 'target'.
// smoothing > 1.0f : higher = slower, lower = snappier
inline float SmoothSlider(float current, float target, float smoothing = 6.0f)
{
	return current + (target - current) / smoothing;
}

// Springy easing toward target.
// current: displayed value
// target: actual value
// velocity: persistent per-control velocity (store in array like DoVel[Menu::Index])
// deltaTime: frame time in seconds (e.g. 1/60.0f)
// stiffness: spring strength (higher = snappier)
// damping: resistance (higher = less oscillation)
inline float SpringSlider(float current, float target, float& velocity,
	float deltaTime,
	float stiffness = 180.0f,
	float damping = 15.0f)
{
	float x = current - target;
	float accel = -stiffness * x - damping * velocity;
	velocity += accel * deltaTime;
	return current + velocity * deltaTime;
}

static float DoMove[ 264 ];
static float DoVel[264];
void TGUI::Slider::Run()
{
    if (Focus::ActiveControl && Focus::ActiveControl != this)
        return;

    static int LastSelectedControlIndex = -1;
    static bool ShouldAffectValue = false;
    Vector2D Size = { 150, 12 };

    bool Hovering = Input::IsHovering(this->Pos, Vector2D(Size.x, Size.y));

    if (Hovering && Input::KeyDown(VK_LBUTTON))
        LastSelectedControlIndex = this->CurrentIndex;

    ShouldAffectValue = Input::KeyDown(VK_LBUTTON) && this->CurrentIndex == LastSelectedControlIndex;

    if (!Input::KeyRelease(VK_LBUTTON))
        LastSelectedControlIndex = -1;

    if (ShouldAffectValue)
    {
        *this->Value = this->Min + (this->Max - this->Min) *
            (Input::MousePos.x - this->Pos.x) / Size.x;

        *this->Value = std::clamp(*this->Value, this->Min, this->Max);
    }

    // fuck ass spring animation
    DoMove[this->CurrentIndex] = SpringSlider(
        DoMove[this->CurrentIndex],
        *this->Value,
        DoVel[this->CurrentIndex],
        1.0f / 60.0f // assuming ~60 FPS
    );

    // keep animation inside bounds
    DoMove[this->CurrentIndex] = std::clamp(
        DoMove[this->CurrentIndex],
        this->Min,
        this->Max
    );
}

void TGUI::Slider::Render()
{

}

void TGUI::Slider::Render(float tabAlpha)
{
    Vector2D Size = { 150, 12 };

    // Helper to apply tab alpha to any color
    auto ApplyAlpha = [tabAlpha](Color c) {
        return Color((int)(c.a() * tabAlpha), c.r(), c.g(), c.b());
        };

    // Use DoMove for drawing (already updated in Run)
    auto Dynamic = ((DoMove[this->CurrentIndex] - this->Min) / (this->Max - this->Min)) * Size.x;

    char Buffer[1024];
    if (this->FloatS)
        sprintf_s(Buffer, "%.2f", *this->Value);
    else
        sprintf_s(Buffer, "%d", (int)*this->Value);

    // Outer outlines
    Draw->Rect(this->Pos - Vector2D(2, 2),
        Vector2D(Size.x + 3, Size.y + 4),
        ApplyAlpha(Style::LightOutline));
    Draw->Rect(this->Pos - Vector2D(1, 1),
        Vector2D(Size.x + 1, Size.y + 2),
        ApplyAlpha(Style::Outline));

    // Background
    Draw->RectFilled(this->Pos,
        Vector2D(Size.x, Size.y + 1),
        ApplyAlpha(Style::MenuBG));

    // Value fill
    Draw->RectFilled(this->Pos,
        Vector2D(Dynamic, Size.y + 1),
        ApplyAlpha(Style::Accent));

    // Gradient overlay
    Draw->Gradient(this->Pos,
        Vector2D(Dynamic, Size.y + 1),
        Color(0, 0, 0, 0),
        Color((int)(75 * tabAlpha), 5, 5, 5));

    // Label
    Draw->Text(this->Pos + Vector2D(0, 15),
        Draw->F.Segoe,
        this->Name,
        ApplyAlpha(Style::Text));

    // Value text
    Draw->Text(this->Pos + Vector2D(Size.x - Draw->GetTextWidth(Buffer, Draw->F.Segoe) + 2, 15),
        Draw->F.Segoe,
        Buffer,
        ApplyAlpha(Style::Text));
}
