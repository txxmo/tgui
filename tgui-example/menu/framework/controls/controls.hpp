#pragma once
#include "../tgui.hpp"
#include <memory>
#include <vector>
#include <string>

#define MAXCONTROLS 512

// Manual min/max helpers
template<typename T>
inline T myMin(const T& a, const T& b) {
    return (a < b) ? a : b;
}

template<typename T>
inline T myMax(const T& a, const T& b) {
    return (a > b) ? a : b;
}

namespace TGUI
{
    class Groupbox;

    // ---------------- Base Control ----------------
    class BaseControl
    {
    public:
        virtual ~BaseControl() = default;

        virtual void Run() = 0;
        virtual void Render() = 0;
       
        // NEW: render with alpha multiplier
        virtual void Render(float alpha)
        {
            // Default implementation just calls Render()
            // Derived controls should override if they support fading
            Render();
        }

        // Common properties
        std::string Name;
        Vector2D Pos;
        Vector2D RelativePos;
        int CurrentIndex = 0;
        int Flags = 0;

        // Default spacing
        virtual int GetHeight() const { return 25; }
    };

    // ---------------- Control Priority ----------------
    struct Focus
    {
        static BaseControl* ActiveControl;
        static bool JustClosed;
    };

    // ---------------- Window ----------------
    class Window
    {
    public:
        Window() {}
        Window(std::string Name, Vector2D Pos, Vector2D Size, int* Tab = nullptr);

        void AddTab(std::string Name);
        void AddGroup(std::unique_ptr<Groupbox> group);

        void End();
        void HandlePos();

        void SetPos(Vector2D Pos);
        Vector2D GetPos();

        std::string Name;
        Vector2D Pos, Size;
        int* Tab = nullptr;
        std::vector<std::string> Tabs;

        // Animation state
        int CurrentTab = 0;
        int TargetTab = 0;
        float TabAnimProgress = 1.0f; // 0 → 1
        bool Animating = false;

        int Flags = 0;

        std::vector<std::unique_ptr<Groupbox>> groups;
    };

    // ---------------- Groupbox ----------------
    class Groupbox : public Window
    {
    public:
        Groupbox();
        Groupbox(std::string Name, Vector2D Pos, Vector2D Size, int Index, int Tab = 0);

        void Run();
        void Render();                  // legacy render (no alpha)
        void Render(float alpha);       // NEW: render with fade support
        void Scroll();
        void RecalcLayout();

        // Add any control polymorphically
        void AddControl(std::unique_ptr<BaseControl> control);

        std::string Name;
        Vector2D Pos, Size;
        bool Visible = true;
        int Tab = 0;
        int GroupId;
        Vector2D RelativePos;

        Vector2D ControlOffset[MAXCONTROLS];
        int MaxElementsY[MAXCONTROLS];
        int CurrentIndex = 0;
        int NextY = 0;

        // Groupbox members to add
        int ScrollOffset = 0;            // current scroll (negative when scrolled down)
        int ContentHeight = 0;           // total height of all controls
        int NextLayoutY = 0;             // running layout cursor used when adding controls
        std::vector<int> ControlLayoutY; // parallel array to Controls storing each control's Y (relative to content)
        const int HeaderArea = 27;       // top offset used for controls (title + padding)
        const int LeftPadding = 7;       // left padding for controls inside group

        int computeContentHeight() const;

        // All controls stored here
        std::vector<std::unique_ptr<BaseControl>> Controls;
    };

    // ---------------- Checkbox ----------------
    class Checkbox : public BaseControl
    {
    public:
        Checkbox() {}
        Checkbox(std::string Name, bool* Value);

        void Run() override;
        void Render() override;
        void Render(float alpha) override;

        void SetState(bool onoff);
        bool GetState();

        bool* Value = nullptr;

        int GetHeight() const override { return 25; }
    };

    // ---------------- Slider ----------------
    class Slider : public BaseControl
    {
    public:
        Slider() {}
        Slider(std::string Name, float* Value, float Min = 0, float Max = 100, bool FloatS = false);

        void Run() override;
        void Render() override;
        void Render(float alpha) override;

        float* Value = nullptr;
        float Min = 0.f, Max = 100.f;
        bool FloatS = false;

        int GetHeight() const override { return 35; }
    };

    // ---------------- Combobox ----------------
    class Combobox : public BaseControl
    {
    public:
        Combobox() {}
        Combobox(std::string Name, int* Value, std::vector<std::string> Items);

        void Run() override;
        void Render() override;
        void Render(float alpha) override;
        void RenderDropDown(float tapAlpha, Vector2D drawPos);

        int* Value = nullptr;
        std::vector<std::string> Items;
        bool IsOpen;

        int GetHeight() const override { return 40; }
    };

    // ---------------- Button ----------------
    class Button : public BaseControl
    {
    public:
        using ButtonFn = void(*)(void);

        Button() {}
        Button(std::string Name, ButtonFn Func);

        void Run() override;
        void Render() override;
        void Render(float alpha) override;

        ButtonFn Func = nullptr;

        int GetHeight() const override { return 30; }
    };

    // ---------------- Keybind ----------------
    class Keybind : public BaseControl
    {
    public:
        Keybind() {}
        Keybind(std::string Name, int* Key);

        void Run() override;
        void Render() override;
        void Render(float alpha) override;

        int* Key = nullptr;

        int GetHeight() const override { return 25; }
    };
}
