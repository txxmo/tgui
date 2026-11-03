#pragma once
#pragma once

class Color
{
private:
	int	A, R, G, B;

public:
	inline Color( void )
	{

	}
	inline Color( int a, int r, int g, int b )
	{
		A = a;
		R = r;
		G = g;
		B = b;
	}
	inline Color( int r, int g, int b )
	{
		A = 255;
		R = r;
		G = g;
		B = b;
	}
	inline int a( void ) const
	{
		return A;
	}
	inline int r( void ) const
	{
		return R;
	}
	inline int g( void ) const
	{
		return G;
	}
	inline int b( void ) const
	{
		return B;
	}
};