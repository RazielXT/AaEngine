#pragma once

#include <windows.h>
#include <vector>
#include <functional>

class ViewportListener
{
public:
	virtual void onViewportResize(UINT, UINT) = 0;
	virtual void onScreenResize(UINT, UINT) = 0;
};

class TargetViewport
{
public:

	uint32_t getHeight() const;
	uint32_t getWidth() const;

	HWND getHwnd() const;

	std::vector<ViewportListener*> listeners;

protected:

	uint32_t height{};
	uint32_t width{};
	HWND hwnd{};
};

class TargetWindow : public TargetViewport
{
public:

	TargetWindow(const char* name, HINSTANCE hInstance, uint32_t width, uint32_t height, bool fullscreen);
	~TargetWindow();

	using EventHandler = std::function<bool(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)>;
	EventHandler eventHandler = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> bool
		{
			return false;
		};

private:

	bool fullscreen{};

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	WNDPROC defWindowProc = DefWindowProc;
};
