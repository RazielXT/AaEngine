#include "TargetWindow.h"
#include <windows.h>
#include <iostream>
#include <string>

static TargetWindow* instance = nullptr;

LRESULT CALLBACK TargetWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (instance)
		return instance->WndProcHandler(hwnd, message, wParam, lParam);

	return false;
}

LRESULT TargetWindow::WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (eventHandler(hwnd, message, wParam, lParam))
		return true;

	static const UINT_PTR resizeTimerId = 1;
	static const UINT timerDelay = 100;

	switch (message)
	{
	case WM_PAINT: {
		PAINTSTRUCT paintStruct;
		HDC hDC = BeginPaint(hwnd, &paintStruct);
		EndPaint(hwnd, &paintStruct);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE: {
			if (!LOWORD(lParam))
				break;

			if (LOWORD(lParam) == width && HIWORD(lParam) == height)
				break;

			width = LOWORD(lParam);
			height = HIWORD(lParam);

			bool isFullscreen = (GetWindowLong(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) == 0;

			if (fullscreen != isFullscreen || isFullscreen)
			{
				fullscreen = isFullscreen;

				for (auto l : listeners)
					l->onScreenResize(width, height);
			}
			else
				SetTimer(hwnd, resizeTimerId, timerDelay, nullptr);
		break;
	}
	case WM_TIMER: {
		if (wParam == resizeTimerId)
		{
			for (auto l : listeners)
				l->onScreenResize(width, height);

			KillTimer(hwnd, resizeTimerId);
		}
		break;
	}
	default:
		return defWindowProc(hwnd, message, wParam, lParam);
	}

	return false;
}

TargetWindow::TargetWindow(const char* name, HINSTANCE hInstance, uint32_t width, uint32_t height, bool fullscreen)
{
	instance = this;
	this->width = width;
	this->height = height;
	this->fullscreen = fullscreen;

	WNDCLASSEX wndClass{};
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.hInstance = hInstance;
	wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndClass.lpszMenuName = nullptr;
	wndClass.lpszClassName = "AaEngineWindow";
	if (!RegisterClassEx(&wndClass))
		return;

	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD showWindow = SW_SHOWNORMAL;

	if (fullscreen)
	{
		style = WS_POPUP;
		showWindow = SW_SHOWMAXIMIZED;
	}

	RECT rc = { 0, 0, (int)width, (int)height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	int posX = CW_USEDEFAULT;
	int posY = CW_USEDEFAULT;

	int windowWidth = rc.right - rc.left;
	int windowHeight = rc.bottom - rc.top;

	if (!fullscreen)
	{
		// Get screen dimensions
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		// Calculate position to center the window
		posX = (screenWidth - windowWidth) / 2;
		posY = (screenHeight - windowHeight) / 2;
	}

	hwnd = CreateWindowA("AaEngineWindow", name,
		style, posX, posY, windowWidth, windowHeight,
		nullptr, nullptr, hInstance, nullptr);

	if (!hwnd)
		return;

	ShowWindow(hwnd, showWindow);
}

TargetWindow::~TargetWindow()
{
	instance = nullptr;
}

uint32_t TargetViewport::getHeight() const
{
	return height;
}

uint32_t TargetViewport::getWidth() const
{
	return width;
}

HWND TargetViewport::getHwnd() const
{
	return hwnd;
}
