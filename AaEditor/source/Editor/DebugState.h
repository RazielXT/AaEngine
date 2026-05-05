#pragma once

#include "RenderCore/UpscaleTypes.h"

struct DebugState
{
	bool reloadShaders = false;

	int DlssMode = (int)UpscaleMode::Off;
	int FsrMode = (int)UpscaleMode::Off;

	bool limitFrameRate = false;

	bool wireframe = false;
	bool wireframeChange = false;
	std::optional<int> wireframePhysicsChange;
};
