#pragma once
#include "../tgui.hpp"

/* visual studio didnt like my separated files */
/* have to make all my shit in one file... */

#define MAXCONTROLS 512

namespace TGUI
{
	class Groupbox;

	class Window
	{
	public:
		Window( ) { }
		Window( std::string Name, Vector2D Pos, Vector2D Size, int* Tab = 0 );

	public:
		void AddTab( std::string Name );
		void AddGroup( Groupbox* Group );

	public:
		void End( );
		void HandlePos( );

	public:
		void SetPos( Vector2D Pos );
		Vector2D GetPos( );

	public:
		std::string Name;
		Vector2D Pos, Size;
		int* Tab;
		std::vector<std::string> Tabs;

	public:
		int Flags;
	};

	class Checkbox;
	class Slider;
	class Combobox;
	class Button;
	class Keybind;

	class Groupbox : public TGUI::Window
	{
	public:
		Groupbox( );
		Groupbox( std::string Name, Vector2D Pos, Vector2D Size, int Index, int Tab = 0 );

		void Run( );
		void Render( );
		void Scroll( );

	public:
		void AddControl( Checkbox* Check );
		void AddControl( Slider* Slid );
		void AddControl( Combobox* Combo );
		void AddControl( Button* Butt ); // LOL
		void AddControl( Keybind* Bind );

	public:
		std::string Name;
		Vector2D Pos, Size;
		bool Visible;
		int Tab;

	public:
		Vector2D ControlOffset[ MAXCONTROLS ];
		int MaxElementsY[ MAXCONTROLS ]; // we use this for scrolling logic
		int CurrentIndex;

	};

	class Checkbox : public TGUI::Groupbox
	{
	public:
		Checkbox( ) { }
		Checkbox( std::string Name, bool* Value );

		void Run( );
		void Render( );

	public:
		void SetState( bool onoff );
		bool GetState( );

	public:
		std::string Name;
		bool* Value;

	public:
		int Flags;
		int HasFlag( int Flags );
	};

	class Slider : public TGUI::Groupbox
	{
	public:
		Slider( ) { }
		Slider( std::string Name, float* Value, float Min = 0, float Max = 100, bool FloatS = false );

		void Run( );
		void Render( );

	public:
		std::string Name;
		float* Value;
		float Min, Max;
		bool FloatS;
	};

	class Combobox : public Groupbox
	{
	public:
		Combobox( );
		Combobox( std::string Name, int* Value, std::vector<std::string> Items );
		void Run( );
		void Render( );

	public:
		std::string Name;
		int* Value;
		std::vector<std::string> Items;
	};

	class Button : public TGUI::Groupbox
	{
	public:
		typedef void( *ButtonFn )( void );

		Button( );
		Button( std::string Name, ButtonFn Func );
		void Run( );

	public:
		std::string Name;
		ButtonFn Func;

	};

	class Keybind : public TGUI::Groupbox
	{
	public:
		Keybind( );
		Keybind( std::string Name, int* Key );

		void Run( );

	public:
		int* Key;
		std::string Name;
	};
}