#include "AaWindow.h"
#include <windows.h>
#include "InputHandler.h"
#include "hidusage.h"
#include <iostream>

void RegisterRawInput(HWND hwnd)
{
	RAWINPUTDEVICE rid{};
	rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid.usUsage = HID_USAGE_GENERIC_MOUSE;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = hwnd;

	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		std::cout << "Failed to register raw input device" << std::endl;
	}
}

// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern bool ImguiUsesInput();

AaWindow* instance = nullptr;

LRESULT CALLBACK AaWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);
	if (ImguiUsesInput())
 		return true;

	static UINT_PTR resizeTimerId = 1;
	static const UINT timerDelay = 100;

	PAINTSTRUCT paintStruct;
	HDC hDC;

	switch (message)
	{
	case WM_PAINT:
		hDC = BeginPaint(hwnd, &paintStruct);
		EndPaint(hwnd, &paintStruct);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE: {
		if (instance)
		{
			if (!LOWORD(lParam))
				break;

			if (LOWORD(lParam) == instance->width && HIWORD(lParam) == instance->height)
				break;

			instance->width = LOWORD(lParam);
			instance->height = HIWORD(lParam);

			bool isFullscreen = (GetWindowLong(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) == 0;

			if (instance->fullscreen != isFullscreen || isFullscreen)
			{
				instance->fullscreen = isFullscreen;

				for (auto l : instance->listeners)
					l->onScreenResize();
			}
			else
				SetTimer(hwnd, resizeTimerId, timerDelay, nullptr);
		}
		break;
	}
	case WM_TIMER: {
		if (wParam == resizeTimerId)
		{
			for (auto l : instance->listeners)
				l->onScreenResize();

			KillTimer(hwnd, resizeTimerId);
		}
		break;
	}
	default:
		return InputHandler::handleMessage(message, wParam, lParam) ? true : DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

AaWindow::AaWindow(HINSTANCE hInstance, uint32_t width, uint32_t height)
{
	this->width = width;
	this->height = height;
	fullscreen = false;

	WNDCLASSEX wndClass = { 0 };
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

	RECT rc = { 0, 0, (int)width, (int)height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	hwnd = CreateWindowA("AaEngineWindow", "AaEngine",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.
		left,
		rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	if (!hwnd)
		return;

	RegisterRawInput(hwnd);
	ShowWindow(hwnd, true);
	instance = this;
}

AaWindow::~AaWindow()
{
	instance = nullptr;
}

HWND AaWindow::getHwnd() const
{
	return hwnd;
}

uint32_t AaWindow::getWidth() const
{
	return width;
}

uint32_t AaWindow::getHeight() const
{
	return height;
}
