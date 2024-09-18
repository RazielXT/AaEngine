#pragma once

#include <windows.h>
#include <functional>

class ScreenListener
{
public:
	virtual void onScreenResize() = 0;
};

class AaWindow
{
public:

	AaWindow(HINSTANCE hInstance, uint32_t width, uint32_t height);
	~AaWindow();

	uint32_t getHeight() const;
	uint32_t getWidth() const;
	HWND getHwnd() const;

	std::vector<ScreenListener*> listeners;

private:

	uint32_t height;
	uint32_t width;
	HWND hwnd;

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};
