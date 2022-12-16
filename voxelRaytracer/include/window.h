#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

class Window
{
public:
	static void init(int aWidth, int aHeight, const char* aTitle);

	static void confineCursor() noexcept;
	static void freeCursor() noexcept;
	static void showCursor() noexcept;
	static void hideCursor() noexcept;

	static int getWidth();
	static int getHeight();

	static HWND getHwnd();

	static void processMessages();
	static bool getShouldClose();
};