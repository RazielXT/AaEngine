#include "InputHandler.h"
#include <windows.h>
#include <vector>
#include "FileLogger.h"
#include "hidusage.h"

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
			listener.mouseMoved(i.wParam, i.lParam);
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
		case WM_MBUTTONDOWN:
			listener.mousePressed(MouseButton::Middle);
			break;
		case WM_MBUTTONUP:
			listener.mouseReleased(MouseButton::Middle);
			break;
		case WM_MOUSEWHEEL:
			listener.mouseWheel(GET_WHEEL_DELTA_WPARAM(i.wParam) / float(WHEEL_DELTA));
			break;
		}
	}

	inputs.clear();
}

void InputHandler::clearInput()
{
	inputs.clear();
}

void InputHandler::registerWindow(HWND hwnd)
{
	RAWINPUTDEVICE rid{};
	rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid.usUsage = HID_USAGE_GENERIC_MOUSE;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = hwnd;

	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		FileLogger::log("Failed to register raw input device");
	}
}

bool InputHandler::handleMessage(uint32_t message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
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
			const auto& mouse = raw.data.mouse;
			int moveX{};
			int moveY{};

			if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
			{
				bool virtualDesktop = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP);
				auto width = GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
				auto height = GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

				int currentX = int(width * mouse.lLastX / 65535.f);
				int currentY = int(height * mouse.lLastY / 65535.f);

				static auto lastX = currentX;
				static auto lastY = currentY;

				moveX = currentX - lastX;
				moveY = currentY - lastY;

				lastX = currentX;
				lastY = currentY;
			}
			else
			{
				moveX = mouse.lLastX;
				moveY = mouse.lLastY;
			}

			if (!inputs.empty() && inputs.back().message == WM_INPUT)
			{
				auto& last = inputs.back();
				last.wParam += moveX;
				last.lParam += moveY;
			}
			else
				inputs.push_back({ WM_INPUT, moveX, moveY });
		}
		break;
	}
	default:
		return false;
	}

	if (inputs.size() > 20)
		inputs.erase(inputs.begin());

	return true;
}
