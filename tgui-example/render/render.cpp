#include "render.h"

void CRender::InitDevice( )
{
	Device->SetVertexShader( nullptr );
	Device->SetPixelShader( nullptr );
	Device->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
	Device->SetRenderState( D3DRS_LIGHTING, false );
	Device->SetRenderState( D3DRS_FOGENABLE, false );
	Device->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	Device->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	/* setup states */
	Device->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
	Device->SetRenderState( D3DRS_SCISSORTESTENABLE, true );
	Device->SetRenderState( D3DRS_ZWRITEENABLE, false );
	Device->SetRenderState( D3DRS_STENCILENABLE, false );

	/* setup states for fonts */
	Device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, true );
	Device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, true );

	/* setup states for render system */
	Device->SetRenderState( D3DRS_ALPHABLENDENABLE, true );
	Device->SetRenderState( D3DRS_ALPHATESTENABLE, false );
	Device->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, true );
	Device->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	Device->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_INVDESTALPHA );
	Device->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	Device->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_ONE );

	/* setup states for color */
	Device->SetRenderState( D3DRS_SRGBWRITEENABLE, false );
	Device->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );

	/* setup states for textures */
	Device->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	Device->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	Device->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	Device->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	Device->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	Device->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

	/* init samples */
	Device->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	Device->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
}

void CRender::Shutdown( )
{
	if ( Device != NULL )
		Device->Release( );
}

void CRender::Rect( Vector2D A, Vector2D B, Color C )
{
	/* Set width / height */
	B += A;

	Vertice Verts[ 5 ] =
	{
		{ float( A.x ), float( A.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( B.x ), float( A.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( B.x ), float( B.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( A.x ), float( B.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( A.x ), float( A.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) }
	};

	Device->SetTexture( 0, nullptr );
	Device->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, &Verts, 20 );
}

void CRender::RectFilled( Vector2D A, Vector2D B, Color C )
{
	/* Set width / height */
	B += A;

	Vertice Verts[ 4 ] =
	{
		{ float( A.x ), float( A.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( B.x ), float( A.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( A.x ), float( B.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ float( B.x ), float( B.y ), 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
	};

	Device->SetTexture( 0, nullptr );
	Device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &Verts, 20 );
}

void CRender::Line( Vector2D StartPos, Vector2D EndPos, Color C )
{
	Vertice Verts[ 2 ] =
	{
		{ StartPos.x, StartPos.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
		{ EndPos.x, EndPos.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) },
	};

	Device->SetTexture( 0, nullptr );
	Device->DrawPrimitiveUP( D3DPT_LINELIST, 1, &Verts, 20 );
}

void CRender::Gradient( Vector2D A, Vector2D B, Color C1, Color C2, bool Horz )
{
	B += A;

	Vertice Verts[ 4 ];

	if ( !Horz )
	{
		Verts[ 0 ] = { A.x, A.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C1.r( ), C1.g( ), C1.b( ), C1.a( ) ) };
		Verts[ 1 ] = { B.x, A.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C1.r( ), C1.g( ), C1.b( ), C1.a( ) ) };
		Verts[ 2 ] = { A.x, B.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C2.r( ), C2.g( ), C2.b( ), C2.a( ) ) };
		Verts[ 3 ] = { B.x, B.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C2.r( ), C2.g( ), C2.b( ), C2.a( ) ) };
	}

	if ( Horz )
	{
		Verts[ 0 ] = { A.x, A.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C1.r( ), C1.g( ), C1.b( ), C1.a( ) ) };
		Verts[ 1 ] = { B.x, A.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C2.r( ), C2.g( ), C2.b( ), C2.a( ) ) };
		Verts[ 2 ] = { A.x, B.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C1.r( ), C1.g( ), C1.b( ), C1.a( ) ) };
		Verts[ 3 ] = { B.x, B.y, 0.01f, 0.01f, D3DCOLOR_RGBA( C2.r( ), C2.g( ), C2.b( ), C2.a( ) ) };
	}

	Device->SetTexture( 0, nullptr );
	Device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &Verts, 20 );
}

int CRender::GetTextWidth( const char* Text, LPD3DXFONT Font )
{
	RECT Size = { 0, 0, 0, 0 };

	if ( Font )
	{
		Font->DrawText( NULL, Text, strlen( Text ), &Size, DT_CALCRECT, D3DCOLOR_XRGB( 0, 0, 0 ) );
	}

	return Size.right - Size.left;
}

void CRender::Text( Vector2D Pos, LPD3DXFONT Font, std::string Text, Color C, bool Center, int Shadow, Color SC )
{
	if ( Center )
		Pos.x -= ( GetTextWidth( Text.c_str( ), Font ) / 2 );

	RECT Area = { Pos.x, Pos.y, 1920, Area.top + 200 };

	if ( Shadow == 1 )
	{
		RECT Area2 = { Pos.x + 1, Pos.y, 1920, Area.top + 200 };
		RECT Area3 = { Pos.x, Pos.y + 1, 1920, Area.top + 200 };
		Font->DrawTextA( NULL, Text.c_str( ), -1, &Area2, 0, D3DCOLOR_RGBA( SC.r( ), SC.g( ), SC.b( ), SC.a( ) ) );
		Font->DrawTextA( NULL, Text.c_str( ), -1, &Area3, 0, D3DCOLOR_RGBA( SC.r( ), SC.g( ), SC.b( ), SC.a( ) ) );
	}
	else if ( Shadow == 2 )
	{
		RECT Area2 = { Pos.x + 1, Pos.y, 1920, Area.top + 200 };
		RECT Area3 = { Pos.x - 1, Pos.y + 1, 1920, Area.top + 200 };
		RECT Area4 = { Pos.x, Pos.y - 1, 1920, Area.top + 200 };
		RECT Area5 = { Pos.x, Pos.y - 1, 1920, Area.top + 200 };
		Font->DrawTextA( NULL, Text.c_str( ), -1, &Area2, 0, D3DCOLOR_RGBA( SC.r( ), SC.g( ), SC.b( ), SC.a( ) ) );
		Font->DrawTextA( NULL, Text.c_str( ), -1, &Area3, 0, D3DCOLOR_RGBA( SC.r( ), SC.g( ), SC.b( ), SC.a( ) ) );
		Font->DrawTextA( NULL, Text.c_str( ), -1, &Area4, 0, D3DCOLOR_RGBA( SC.r( ), SC.g( ), SC.b( ), SC.a( ) ) );
		Font->DrawTextA( NULL, Text.c_str( ), -1, &Area5, 0, D3DCOLOR_RGBA( SC.r( ), SC.g( ), SC.b( ), SC.a( ) ) );
	}

	Font->DrawTextA( NULL, Text.c_str( ), -1, &Area, 0, D3DCOLOR_RGBA( C.r( ), C.g( ), C.b( ), C.a( ) ) );
}