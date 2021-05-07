#pragma once

#include "../../main.h"

#include "controls/controls.hpp"

template<typename T> static inline T TMin( T lhs, T rhs ) { return lhs < rhs ? lhs : rhs; }
template<typename T> static inline T TMax( T lhs, T rhs ) { return lhs >= rhs ? lhs : rhs; }

namespace TGUI
{
	class GUI
	{
	public:
		void Create( );
	};

	namespace Input
	{
		inline static bool KeyState[ 256 ];
		inline static bool PrevKeyState[ 256 ];

		bool KeyPress( const int Key );
		bool KeyDown( const int Key );
		bool KeyRelease( const int Key );

		inline Vector2D MousePos = { 0, 0 };
		inline float MouseWheel = 0;

		bool IsHovering( Vector2D Pos, Vector2D Area );

		void HandleMouse( UINT Msg, WPARAM WParam, LPARAM LParam );
		void HandleKeys( );
	}

	namespace Menu
	{
		inline int Index;
		inline bool IsOpen = true;
		inline Vector2D WindowPos = { 0, 0 };

	}

	namespace Style
	{
		extern Color MenuBG;
		extern Color WindowHeader;
		extern Color Outline;
		extern Color LightOutline;
		extern Color DarkBG;

		extern Color Accent;

		extern Color Text;
		extern Color TextDim;
	}

	inline enum ControlFlags
	{
		CLICKABLE = 1, // check if control is clickable
		INACTIVE = 2, // check if control is inactive // used for control prio
		ISACTIVE = 3, // check if control is active
		ISCOMBO = 4, // check if control is a combobox
		HASPRIO = 5, // check if control has priority
		COMBOACTIVE = 6 // check if a combo is active

	} Flags;
}

extern TGUI::GUI* TGui;

const char* const KeyNames[ ] = {
	( "..." ), ( "M1" ), ( "M2" ), ( "BRK" ), ( "M3" ), ( "M4" ), ( "M5" ),
	( "..." ), ( "BCK" ), ( "TAB" ), ( "..." ), ( "..." ), ( "..." ), ( "ENT" ), ( "..." ), ( "..." ), ( "SFT" ),
	( "CTL" ), ( "ALT" ), ( "PUS" ), ( "CAP" ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "ESC" ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "SPC" ), ( "PGU" ), ( "PGD" ), ( "END" ), ( "HOM" ), ( "LFT" ),
	( "UP" ), ( "RGT" ), ( "DWN" ), ( "..." ), ( "PRN" ), ( "..." ), ( "PRNS" ), ( "INS" ), ( "DEL" ), ( "..." ), ( "0" ), ( "1" ),
	( "2" ), ( "3" ), ( "4" ), ( "5" ), ( "6" ), ( "7" ), ( "8" ), ( "9" ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "A" ), ( "B" ), ( "C" ), ( "D" ), ( "E" ), ( "F" ), ( "G" ), ( "H" ), ( "I" ), ( "J" ), ( "K" ), ( "L" ), ( "M" ), ( "N" ), ( "O" ), ( "P" ), ( "Q" ), ( "R" ), ( "S" ), ( "T" ), ( "U" ),
	( "V" ), ( "W" ), ( "X" ), ( "Y" ), ( "Z" ), ( "WIN" ), ( "WIN" ), ( "..." ), ( "..." ), ( "..." ), ( "NU0" ), ( "NU1" ),
	( "NU2" ), ( "NU3" ), ( "NU4" ), ( "NU5" ), ( "NU6" ), ( "NU7" ), ( "NU8" ), ( "NU9" ), ( "*" ), ( "+" ), ( "" ), ( "..." ), ( "." ), ( "/" ), ( "F1" ), ( "F2" ), ( "F3" ),
	( "F4" ), ( "F5" ), ( "F6" ), ( "F7" ), ( "F8" ), ( "F9" ), ( "F10" ), ( "F11" ), ( "F12" ), ( "F13" ), ( "F14" ), ( "F15" ), ( "F16" ), ( "F17" ), ( "F18" ), ( "F19" ), ( "F20" ),
	( "F21" ), ( "F22" ), ( "F23" ), ( "F24" ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "NLK" ), ( "SCL" ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "SFT" ), ( "SFT" ), ( "CTL" ),
	( "CTL" ), ( "MEN" ), ( "MEN" ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "NRK" ), ( "PRK" ), ( "STP" ), ( "PLY" ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( ";" ), ( "+" ), ( ")," ), ( "..." ), ( "." ), ( "/?" ), ( "~" ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "{" ), ( "\\|" ), ( "}" ), ( "'\"" ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ), ( "..." ),
	( "..." ), ( "..." )
};