#pragma once
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <WinUser.h>
#include <thread>
#include <chrono>
#include <string>
#include <functional>
#include <vector>
#include <array>
#include <iterator>
#include <TlHelp32.h>
#include <tchar.h>
#include <dinput.h>
#include <CommCtrl.h>
#include <random>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <mutex>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <cmath>
#include <thread>

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "render/color.h"
#include "render/render.h"
#include "math/math.h"

namespace Window
{
	void Create( );

	bool CreateDeviceD3D( HWND hWnd );

	void CleanupDeviceD3D( );

	void ResetDevice( );

	LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

	void Do( );
}