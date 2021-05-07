#include "controls.hpp"
using namespace TGUI;

static short int OuterPos[ MAXCONTROLS ];
int Alphpapa[ MAXCONTROLS ];

void TGUI::Window::AddGroup( Groupbox* Group )
{
	Group->Visible = *this->Tab == Group->Tab;

	if ( Group->Visible )
	{
		Group->Pos += this->Pos + Vector2D( 5, 21 );
		Group->Run( );
		Group->Render( );

		Menu::Index++;
	}
}

TGUI::Groupbox::Groupbox( )
{

}

TGUI::Groupbox::Groupbox( std::string Name, Vector2D Pos, Vector2D Size, int Index, int Tab )
{
	this->Name = Name;
	this->Pos = Pos;
	this->Size = Size;
	this->CurrentIndex = Index;
	this->Tab = Tab;
}

void TGUI::Groupbox::Run( )
{
	this->ControlOffset[ this->CurrentIndex ] = { this->Pos.x + 7, this->Pos.y + 27 + OuterPos[ this->CurrentIndex ] };
}

void TGUI::Groupbox::Render( )
{
	Draw->Rect( this->Pos - Vector2D( 1, 1 ), this->Size + Vector2D( 2, 2 ), Style::Outline );
	Draw->Rect( this->Pos - Vector2D( 2, 2 ), this->Size + Vector2D( 4, 4 ), Style::LightOutline );

	Draw->Text( this->Pos + Vector2D( this->Size.x / 2 - Draw->GetTextWidth( this->Name.c_str( ), Draw->F.Segoe ) / 2, 3 ), Draw->F.Segoe, this->Name, Style::Text, false );
}

void TGUI::Groupbox::Scroll( )
{
	bool Hovered = Input::IsHovering( this->Pos, this->Size );

	if ( Hovered )
	{
		if ( Input::MouseWheel != 0 )
		{
			if ( this->MaxElementsY[ this->CurrentIndex ] > ( this->Size.y - 8 ) )
			{
				OuterPos[ this->CurrentIndex ] += Input::MouseWheel * 10;

				Input::MouseWheel = 0;

				if ( OuterPos[ this->CurrentIndex ] > 0 )
					OuterPos[ this->CurrentIndex ] = 0;

				if ( OuterPos[ this->CurrentIndex ] < ( this->Size.y - 24 ) - this->MaxElementsY[ this->CurrentIndex ] )
					OuterPos[ this->CurrentIndex ] = ( this->Size.y - 24 ) - this->MaxElementsY[ this->CurrentIndex ];
			}
		}

		if ( Alphpapa[ this->CurrentIndex ] < 255 )
			Alphpapa[ this->CurrentIndex ] += 12.09;

		Alphpapa[ this->CurrentIndex ] = std::clamp( Alphpapa[ this->CurrentIndex ], 0, 255 );
	}
	else
	{
		if ( Alphpapa[ this->CurrentIndex ] > 0 )
			Alphpapa[ this->CurrentIndex ] -= 16.09;

		Alphpapa[ this->CurrentIndex ] = std::clamp( Alphpapa[ this->CurrentIndex ], 0, 255 );
	}

	if ( this->Visible && this->MaxElementsY[ this->CurrentIndex ] > ( this->Size.y - 24 ) )
	{
		int MaxScrollValue = this->MaxElementsY[ this->CurrentIndex ] - ( this->Size.y - 24 );

		float ScrollPos = ( ( float )OuterPos[ this->CurrentIndex ] / ( float )this->MaxElementsY[ this->CurrentIndex ] ) * ( this->Size.y - 20.f ) * ( -1 );

		auto ScrollPosMax = MaxScrollValue / ( float )this->MaxElementsY[ this->CurrentIndex ] * ( this->Size.y + 20.f );

		static int Old;

		if ( Old != MaxScrollValue )
		{
			Old = MaxScrollValue;

			if ( OuterPos[ this->CurrentIndex ] > 0 )
				OuterPos[ this->CurrentIndex ] = 0;

			if ( OuterPos[ this->CurrentIndex ] < ( this->Size.y - 8 ) - this->MaxElementsY[ this->CurrentIndex ] )
				OuterPos[ this->CurrentIndex ] = ( this->Size.y - 8 ) - this->MaxElementsY[ this->CurrentIndex ];
		}

		Draw->RectFilled( Vector2D( this->Pos.x + this->Size.x - 4, this->Pos.y + ScrollPos + 3 ), Vector2D( 2, this->Size.y - ScrollPosMax - 1 ), Color( Alphpapa[ this->CurrentIndex ], Style::Accent.r( ), Style::Accent.g( ), Style::Accent.b( ) ) );
	}
}