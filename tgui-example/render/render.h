#pragma once
#include <array>
#include <mutex>
#include <shared_mutex>

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "../math/math.h"
#include "color.h"

struct Vertice
{
	float X, Y, Z, W;
	int C;
};

class CRender
{
public:
	CRender( IDirect3DDevice9* Device )
	{
		this->Device = Device;
		this->VertexShader = VertexShader;

		Device->GetViewport( &ScreenSize );
	}

	CRender( ) { }

	void InitDevice( );
	void Shutdown( );

public:
	void Rect( Vector2D Pos, Vector2D Size, Color C );
	void RectFilled( Vector2D Pos, Vector2D Size, Color C );
	void Line( Vector2D StartPos, Vector2D EndPos, Color C );
	void Gradient( Vector2D Pos, Vector2D Size, Color C1, Color C2, bool Horz = false );

	void Text( Vector2D Pos, LPD3DXFONT Font, std::string Text, Color C, bool Center = false, int Shadow = 0, Color SC = Color( ) );
	int GetTextWidth( const char* Text, LPD3DXFONT Font );

public:
	struct
	{
		LPD3DXFONT Segoe;
		LPD3DXFONT SegoeBold;
	} F;

public:
	void SetViewport( D3DVIEWPORT9 ViewP )
	{
		Device->SetViewport( &ViewP );
	}

	D3DVIEWPORT9 GetViewport( )
	{
		D3DVIEWPORT9 ViewP;
		Device->GetViewport( &ViewP );
		return ViewP;
	}

public:
	IDirect3DDevice9* Device;
	D3DVIEWPORT9 ScreenSize;
	IDirect3DVertexShader9* VertexShader;
};

extern CRender* Draw;