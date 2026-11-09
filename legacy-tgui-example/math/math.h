#pragma once
class Vector2D {
public:
	float x, y;

	Vector2D( ) { };

	Vector2D( float x, float y ) {
		this->x = x;
		this->y = y;
	};

	Vector2D operator - ( Vector2D other ) {
		return {
			x - other.x,
			y - other.y
		};
	}

	Vector2D operator + ( Vector2D other ) {
		return {
			x + other.x,
			y + other.y
		};
	}

	Vector2D operator * ( Vector2D other ) {
		return {
			x * other.x,
			y * other.y
		};
	}

	Vector2D operator / ( Vector2D other ) {
		return {
			x / other.x,
			y / other.y
		};
	}

	void operator += ( Vector2D other ) {
		x += other.x;
		y += other.y;
	}

	void operator -= ( Vector2D other ) {
		x -= other.x;
		y -= other.y;
	}

	void operator *= ( Vector2D other ) {
		x *= other.x;
		y *= other.y;
	}

	void operator /= ( Vector2D other ) {
		x /= other.x;
		y /= other.y;
	}
};