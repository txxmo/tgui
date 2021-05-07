#include "controls.hpp"

using namespace TGUI;

void TGUI::Groupbox::AddControl( Slider* Slid )
{
	if ( this->Visible )
	{
		Slid->CurrentIndex = Menu::Index;

		D3DVIEWPORT9 OldViewport = Draw->GetViewport( );
		Draw->SetViewport( { ( DWORD )this->Pos.x, ( DWORD )this->Pos.y + 25, ( DWORD )this->Size.x, ( DWORD )this->Size.y - 25, 0.f, 1.f } );
		
		Slid->Pos = Vector2D( this->ControlOffset[ this->CurrentIndex ].x + 2, this->ControlOffset[ this->CurrentIndex ].y );
		Slid->Run( );

		this->ControlOffset[ this->CurrentIndex ] += Vector2D( 0, 35 );
		this->MaxElementsY[ this->CurrentIndex ] += 35;
		Menu::Index++;

		Draw->SetViewport( OldViewport );
	}

	delete Slid;
}

TGUI::Slider::Slider( std::string Name, float* Value, float Min, float Max, bool Float )
{
	this->Name = Name;
	this->Value = Value;
	this->Min = Min;
	this->Max = Max;
	this->FloatS = Float;
}

static inline float SliderBehaviour( float v, float v_min, float v_max, float power, float linear_zero_pos )
{
	if ( v_min == v_max )
		return 0.0f;

	const bool is_non_linear = ( power < 1.0f - 0.00001f ) || ( power > 1.0f + 0.00001f );
	const float v_clamped = ( v_min < v_max ) ? std::clamp( v, v_min, v_max ) : std::clamp( v, v_max, v_min );
	if ( is_non_linear )
	{
		if ( v_clamped < 0.0f )
		{
			const float f = 1.0f - ( v_clamped - v_min ) / ( TMin( 0.0f, v_max ) - v_min );
			return ( 1.0f - powf( f, 1.0f / power ) ) * linear_zero_pos;
		}
		else
		{
			const float f = ( v_clamped - TMax( 0.0f, v_min ) ) / ( v_max - TMax( 0.0f, v_min ) );
			return linear_zero_pos + powf( f, 1.0f / power ) * ( 1.0f - linear_zero_pos );
		}
	}

	// Linear slider
	return ( v_clamped - v_min ) / ( v_max - v_min );
}

static float DoMove[ 264 ];
void TGUI::Slider::Run( )
{
	static int LastSelectedControlIndex = -1;
	static bool ShouldAffectValue = false;
	Vector2D Size = { 150, 12 };

	bool Hovering = Input::IsHovering( this->Pos, Vector2D( Size.x, Size.y ) );

	if ( Hovering && Input::KeyDown( VK_LBUTTON ) )
		LastSelectedControlIndex = Menu::Index;

	ShouldAffectValue = Input::KeyDown( VK_LBUTTON ) && Menu::Index == LastSelectedControlIndex;

	if ( !Input::KeyRelease( VK_LBUTTON ) )
		LastSelectedControlIndex = -1;

	if ( ShouldAffectValue )
	{
		*this->Value = this->Min + ( this->Max - this->Min ) * ( Input::MousePos.x - ( this->Pos.x ) ) / ( Size.x );

		if ( *this->Value > this->Max )
			*this->Value = this->Max;

		if ( *this->Value < this->Min )
			*this->Value = this->Min;
	}

	// auto GrabCur = SliderBehaviour( DoMove[ MenuInfo::ControlIndex ], this->Min, this->Max, 1.0f, 0.0f );

	if ( *this->Value > DoMove[ Menu::Index ] )
	{
		DoMove[ Menu::Index ] += ( *this->Value - DoMove[ Menu::Index ] / 6.09 );
		DoMove[ Menu::Index ] = std::clamp( DoMove[ Menu::Index ], 0.0f, *this->Value );
	}
	else if ( *this->Value < DoMove[ Menu::Index ] )
	{
		DoMove[ Menu::Index ] -= ( DoMove[ Menu::Index ] - *this->Value ) / 6.09;
	}

	char Buffer[ 1024 ];

	if ( this->FloatS )
		sprintf_s( Buffer, "%.2f", *this->Value );
	else
		sprintf_s( Buffer, "%d", ( int )*this->Value );

	auto Dynamic = ( ( *this->Value - this->Min ) / ( this->Max - this->Min ) * ( Size.x ) );

	Draw->Rect( this->Pos - Vector2D( 2, 2 ), Vector2D( Size.x + 3, Size.y + 4 ), Style::LightOutline );
	Draw->Rect( this->Pos - Vector2D( 1, 1 ), Vector2D( Size.x + 1, Size.y + 2 ), Style::Outline );
	Draw->RectFilled( this->Pos, Vector2D( Size.x, Size.y + 1 ), Style::MenuBG );

	/* value */
	Draw->RectFilled( this->Pos, Vector2D( Dynamic, Size.y + 1 ), Style::Accent );
	Draw->Gradient( this->Pos, Vector2D( Dynamic, Size.y + 1 ), Color( 0, 0, 0, 0 ), Color( 75, 5, 5, 5 ) );

	Draw->Text( this->Pos + Vector2D( 0, 15 ), Draw->F.Segoe, this->Name, Style::Text );
	Draw->Text( this->Pos + Vector2D( Size.x - Draw->GetTextWidth( Buffer, Draw->F.Segoe ) + 2, 15 ), Draw->F.Segoe, Buffer, Style::Text );
}