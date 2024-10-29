#include "InputHandler.h"
#include <windows.h>
#include <vector>

struct InputInfo { uint32_t message; int wParam; int lParam; };
std::vector<InputInfo> inputs;

void InputHandler::consumeInput(InputListener& listener)
{
	for (auto& i : inputs)
	{
		switch (i.message)
		{
		case WM_KEYDOWN:
			listener.keyPressed(i.wParam);
			break;
		case WM_KEYUP:
			listener.keyReleased(i.wParam);
			break;
		case WM_INPUT:
			listener.mouseMoved(i.lParam, i.wParam);
			break;
		case WM_LBUTTONDOWN:
			listener.mousePressed(MouseButton::Left);
			break;
		case WM_LBUTTONUP:
			listener.mouseReleased(MouseButton::Left);
			break;
		case WM_RBUTTONDOWN:
			listener.mousePressed(MouseButton::Right);
			break;
		case WM_RBUTTONUP:
			listener.mouseReleased(MouseButton::Right);
			break;
		}
	}

	inputs.clear();
}

bool InputHandler::handleMessage(uint32_t message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
		inputs.push_back({ message, (int)wParam, (int)lParam });
		break;
	case WM_INPUT:
	{
		RAWINPUT raw{};
		UINT rawSize = sizeof(raw);
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER)) == rawSize && raw.header.dwType == RIM_TYPEMOUSE)
		{
			inputs.push_back({ WM_INPUT, raw.data.mouse.lLastY, raw.data.mouse.lLastX });
		}
		break;
	}
	default:
		return false;
	}

	if (inputs.size() > 50)
		inputs.erase(inputs.begin());

	return true;
}
