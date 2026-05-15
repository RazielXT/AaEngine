#pragma once

#include "RenderCore/UpscaleTypes.h"

enum class InteractionMode
{
	Editor,
	Motorbike,
	WalkingFirstPerson,
	WalkingThirdPerson
};

struct DebugState
{
	bool reloadShaders = false;

	int DlssMode = (int)UpscaleMode::Off;
	int FsrMode = (int)UpscaleMode::Off;

	bool limitFrameRate = false;

	bool wireframe = false;
	bool wireframeChange = false;
	std::optional<int> wireframePhysicsChange;

	InteractionMode interactionMode = InteractionMode::Editor;
	bool interactionModeChanged = false;
	bool showPlayerBody = false;
};
