#pragma once

#include "App/TargetWindow.h"

class ImguiPanelViewport : public TargetViewport
{
public:

	void set(HWND hw, UINT w, UINT h)
	{
		hwnd = hw;
		height = h;
		width = w;
	}
};
