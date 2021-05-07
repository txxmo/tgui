
#include "../tgui.hpp"
#include "controls.hpp"

using namespace TGUI;

TGUI::Window::Window( std::string Name, Vector2D Pos, Vector2D Size, int* Tab )
{
	this->Name = Name;
	this->Pos = Pos;
	this->Size = Size;
	this->Tab = Tab;

	this->HandlePos( );

	/* background and accent header */
	Draw->RectFilled( this->Pos, this->Size, Style::MenuBG );
	Draw->RectFilled( this->Pos + Vector2D( 0, 20 ), Vector2D( this->Size.x, 2 ), Style::Accent );
	Draw->RectFilled( this->Pos + Vector2D( 0, 21 ), Vector2D( this->Size.x, 1 ), Color( Style::Accent.r( ) - 15, Style::Accent.g( ) - 15, Style::Accent.b( ) - 15 ) );

	Draw->Rect( this->Pos, this->Size, Style::Outline );
	Draw->Rect( this->Pos + Vector2D( 1, 1 ), this->Size - Vector2D( 2, 2 ), Style::LightOutline );

	auto Wid = Draw->GetTextWidth( this->Name.c_str( ), Draw->F.Segoe );
	Draw->Text( this->Pos + Vector2D( this->Size.x / 2 - Wid / 2, 3 ), Draw->F.Segoe, this->Name, Style::Text );

	Draw->Text( this->Pos + Vector2D( this->Size.x - 28, this->Size.y - 16 ), Draw->F.Segoe, "v.2.5", Style::TextDim );
}

void TGUI::Window::End( )
{
	auto TabSize = Size.x / Tabs.size( ) - 1;

	for ( int i = 0; i < this->Tabs.size( ); i++ )
	{
		auto Tab = this->Tabs[ i ];

		RECT TabPos[ 4 ] = { Pos.x + ( i * TabSize ) + 2, Pos.y + 23, TabSize, 20 };

		bool Hovered = Input::IsHovering( { ( float )TabPos->left, ( float )TabPos->top }, { ( float )TabPos->right, ( float )TabPos->bottom } );

		Color TabColor = *this->Tab != i ? Hovered ? Color( 53, 53, 53 ) : Color( 43, 43, 43 ) : Color( 244, 244, 244 );

		if ( Hovered && Input::KeyPress( VK_LBUTTON ) )
		{
			*this->Tab = i;
		}

		Draw->Rect( Vector2D( TabPos->left, TabPos->top ), Vector2D( TabPos->right, TabPos->bottom ), Style::LightOutline );
		Draw->Rect( Vector2D( TabPos->left, TabPos->top ) + Vector2D( 1, 1 ), Vector2D( TabPos->right, TabPos->bottom ) - Vector2D( 2, 2 ), Style::Outline );
		Draw->RectFilled( Vector2D( TabPos->left, TabPos->top ) + Vector2D( 2, 2 ), Vector2D( TabPos->right, TabPos->bottom ) - Vector2D( 3, 3 ), Style::DarkBG );

		Draw->Text( Vector2D( TabPos->left + ( TabPos->right / 2 ), TabPos->top + 3 ), Draw->F.Segoe, Tab, *this->Tab == i ? Style::Text : Style::TextDim, true );
	}
}

void TGUI::Window::AddTab( std::string Name )
{
	this->Tabs.push_back( Name );
}

void TGUI::Window::HandlePos( )
{
	static Vector2D DragOffset;
	static bool ShouldDrag = false;
	bool Clicked = false;

	if ( GetAsyncKeyState( VK_LBUTTON ) )
		Clicked = true;

	if ( ShouldDrag && !Clicked )
	{
		ShouldDrag = false;
	}

	if ( ShouldDrag && Clicked )
	{
		Menu::WindowPos = Input::MousePos - DragOffset;
	}

	if ( Input::IsHovering( Pos, { Size.x, 20 } ) )
	{
		ShouldDrag = true;
		DragOffset = Input::MousePos - Menu::WindowPos;
	}
}

Vector2D TGUI::Window::GetPos( )
{
	return this->Pos;
}

void TGUI::Window::SetPos( Vector2D Pos )
{
	this->Pos = Pos;
}