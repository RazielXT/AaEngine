#pragma once

#include <cstdint>
#include <windows.h>

enum class MouseButton { Left, Right, Middle };

class InputListener
{
public:

	virtual bool keyPressed(int key) = 0;
	virtual bool keyReleased(int key) = 0;

	virtual bool mousePressed(MouseButton button) = 0;
	virtual bool mouseReleased(MouseButton button) = 0;
	virtual bool mouseMoved(int x, int y) = 0;
	virtual bool mouseWheel(float change) = 0;
};

namespace InputHandler
{
	void registerWindow(HWND hwnd);
	bool handleMessage(uint32_t, WPARAM, LPARAM);

	void consumeInput(InputListener&);
	void clearInput();
};
