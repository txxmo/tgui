#include "controls.hpp"

using namespace TGUI;

int CurAlpha[ 264 ];
int TextColor[ 264 ];
int GradColor[ 264 ];
bool DoOnce[ 264 ] = { false };
void TGUI::Groupbox::AddControl( Checkbox* Check )
{
	if ( this->Visible )
	{
		Check->CurrentIndex = Menu::Index;

		D3DVIEWPORT9 OldViewport = Draw->GetViewport( );
		Draw->SetViewport( { ( DWORD )this->Pos.x, ( DWORD )this->Pos.y + 25, ( DWORD )this->Size.x, ( DWORD )this->Size.y - 25, 0.f, 1.f } );

		Check->Pos = Vector2D( this->ControlOffset[ this->CurrentIndex ].x, this->ControlOffset[ this->CurrentIndex ].y );

		Check->Run( );

		Check->Render( );

		this->ControlOffset[ this->CurrentIndex ] += Vector2D( 0, 25 );
		this->MaxElementsY[ this->CurrentIndex ] += 25;
		Menu::Index++;

		Draw->SetViewport( OldViewport );
	}

	delete Check;
}

TGUI::Checkbox::Checkbox( std::string Name, bool* Value )
{
	this->Name = Name;
	this->Value = Value;

	this->Flags = CLICKABLE | ISACTIVE;
}

void TGUI::Checkbox::Run( )
{
	auto Width = Draw->GetTextWidth( this->Name.c_str( ), Draw->F.Segoe );

	bool Hovering = Input::IsHovering( this->Pos, Vector2D( 15 + Width, 15 ) );

	if ( Hovering && Input::KeyPress( VK_LBUTTON ) )
		*this->Value = !*this->Value;

	if ( !*this->Value )
	{
		if ( CurAlpha[ this->CurrentIndex ] > 0 )
			CurAlpha[ this->CurrentIndex ] -= 16.09;

		CurAlpha[ this->CurrentIndex ] = std::clamp( CurAlpha[ this->CurrentIndex ], 0, 255 );

		if ( TextColor[ this->CurrentIndex ] > 125 )
			TextColor[ this->CurrentIndex ] -= 16.09;

		TextColor[ this->CurrentIndex ] = std::clamp( TextColor[ this->CurrentIndex ], 125, 255 );

		if ( GradColor[ this->CurrentIndex ] > 0 )
			GradColor[ this->CurrentIndex ] -= 16.09;

		GradColor[ this->CurrentIndex ] = std::clamp( GradColor[ this->CurrentIndex ], 0, 75 );
	}
	else
	{
		if ( CurAlpha[ this->CurrentIndex ] < 255 )
			CurAlpha[ this->CurrentIndex ] += 12.09;

		CurAlpha[ this->CurrentIndex ] = std::clamp( CurAlpha[ this->CurrentIndex ], 0, 255 );

		if ( TextColor[ this->CurrentIndex ] < 255 )
			TextColor[ this->CurrentIndex ] += 12.09;

		TextColor[ this->CurrentIndex ] = std::clamp( TextColor[ this->CurrentIndex ], 125, 255 );

		if ( GradColor[ this->CurrentIndex ] < 75 )
			GradColor[ this->CurrentIndex ] += 12.09;

		GradColor[ this->CurrentIndex ] = std::clamp( GradColor[ this->CurrentIndex ], 0, 75 );
	}
}

void TGUI::Checkbox::Render( )
{
	Draw->RectFilled( this->Pos, Vector2D( 15, 15 ), Style::MenuBG );

	Draw->RectFilled( this->Pos + Vector2D( 2, 2 ), Vector2D( 12, 12 ), Color( CurAlpha[ this->CurrentIndex ], Style::Accent.r( ), Style::Accent.g( ), Style::Accent.b( ) ) );

	Draw->Gradient( this->Pos + Vector2D( 2, 2 ), Vector2D( 12, 12 ), Color( 0, 0, 0, 0 ), Color( GradColor[ this->CurrentIndex ], 5, 5, 5 ) );

	Draw->Rect( this->Pos, Vector2D( 15, 15 ), Style::LightOutline );
	Draw->Rect( this->Pos + Vector2D( 1, 1 ), Vector2D( 13, 13 ), Style::Outline );

	Draw->Text( this->Pos + Vector2D( 20, 1 ), Draw->F.Segoe, this->Name, Color( TextColor[ this->CurrentIndex ], Style::Text.r( ), Style::Text.g( ), Style::Text.b( ) ) );
}