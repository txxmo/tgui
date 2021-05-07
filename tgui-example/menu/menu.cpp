#include "framework/tgui.hpp"


using namespace TGUI;

TGUI::GUI* TGui;

int SelectedTab;
float SliderEx, fSliderEx;
int iComboEx, iKeybind, iSlider;
bool BooleanEx;

void GUI::Create( )
{
	if ( !Menu::IsOpen )
		return;

	Input::HandleKeys( );

	Menu::Index = 0;

	/* Create our example window */
	auto ExampleWindow = new Window( "Test Window", Menu::WindowPos, { 400, 300 }, &SelectedTab );
	{
		/* Create some tabs */
		ExampleWindow->AddTab( "Tab 1" );
		ExampleWindow->AddTab( "Tab 2" );
		ExampleWindow->AddTab( "Tab 3" );

		auto ExampleGroup = new Groupbox( "Example Group", { 4, 30 }, { 186, 230 }, 0, 0 );
		{
			/* Add our groupbox to the window */
			ExampleWindow->AddGroup( ExampleGroup );

			ExampleGroup->AddControl( new Checkbox( "Checkbox", &BooleanEx ) );
			
			ExampleGroup->AddControl( new Keybind( "Keybind", &iKeybind ) );
			
			ExampleGroup->AddControl( new Slider( "Slider float", &SliderEx, 0, 100, true ) );
			ExampleGroup->AddControl( new Slider( "Slider int", &fSliderEx, 0, 100, false ) );

			ExampleGroup->AddControl( new Combobox( "Combobox", &iComboEx, { "Item 1", "Item 2", "Item 3" } ) );

			ExampleGroup->Scroll( );
		}

		auto ExampleGroup2 = new Groupbox( "Example Group 2", { 200, 30 }, { 186, 230 }, 1, 0 );
		{
			ExampleWindow->AddGroup( ExampleGroup2 );

			ExampleGroup2->Scroll( );
		}

		ExampleWindow->End( );
	}

	delete ExampleWindow;
}