#include "framework/tgui.hpp"

using namespace TGUI;

TGUI::GUI* TGui;

int SelectedTab;
float SliderEx; 
float fSliderEx;
int iComboEx, iKeybind, iSlider;
bool BooleanEx, BooleanEx2;

void GUI::Create()
{
    if (!Menu::IsOpen)
        return;

    TGUI::Input::HandleKeys();
    Menu::Index = 0;

    static auto ExampleWindow = std::make_unique<TGUI::Window>("Test Window", Menu::WindowPos, Vector2D{ 400,300 }, &SelectedTab);
    ExampleWindow->HandlePos();
    static bool inited = false;
    if (!inited)
    {
        inited = true;
        ExampleWindow->AddTab("Tab 1");
        ExampleWindow->AddTab("Tab 2");
        ExampleWindow->AddTab("Tab 3");

        auto ExampleGroup = std::make_unique<TGUI::Groupbox>("Example Group", Vector2D{ 4,30 }, Vector2D{ 186,230 }, 0, 0);
        ExampleGroup->AddControl(std::make_unique<TGUI::Checkbox>("Checkbox", &BooleanEx));
        ExampleGroup->AddControl(std::make_unique<TGUI::Checkbox>("Checkbox 2", &BooleanEx2));
        ExampleGroup->AddControl(std::make_unique<TGUI::Keybind>("Keybind", &iKeybind));
        ExampleGroup->AddControl(std::make_unique<TGUI::Combobox>("Combobox", &iComboEx, std::vector<std::string>{"Item 1", "Item 2", "Item 3"}));
        ExampleGroup->AddControl(std::make_unique<TGUI::Slider>("Slider float", &SliderEx, 0, 100, true));
        ExampleGroup->AddControl(std::make_unique<TGUI::Slider>("Slider int", &fSliderEx, 0, 100, false));
        ExampleWindow->AddGroup(std::move(ExampleGroup));

        auto ExampleGroup2 = std::make_unique<TGUI::Groupbox>("Example Group 2 (SCROLLING)", Vector2D{ 200,30 }, Vector2D{ 186,230 }, 1, 0);
        for (int i = 0; i < 30; ++i)
            ExampleGroup2->AddControl(std::make_unique<TGUI::Checkbox>("Checkbox " + std::to_string(i + 1), &BooleanEx));
        ExampleWindow->AddGroup(std::move(ExampleGroup2));


        auto ExampleGroup3 = std::make_unique<TGUI::Groupbox>("Example Group 3", Vector2D{ 4,30 }, Vector2D{ 186,230 }, 2, 1);
        ExampleGroup3->AddControl(std::make_unique<TGUI::Checkbox>("Checkbox", &BooleanEx));
        ExampleWindow->AddGroup(std::move(ExampleGroup3));

        auto ExampleGroup4 = std::make_unique<TGUI::Groupbox>("Example Group 4", Vector2D{ 200,30 }, Vector2D{ 186,230 }, 3, 1);
        ExampleWindow->AddGroup(std::move(ExampleGroup4));

        // initial scroll/setup calls if needed
        for (auto& g : ExampleWindow->groups) g->Scroll();
    }  

    // per-frame: just call End
    ExampleWindow->End();
}
