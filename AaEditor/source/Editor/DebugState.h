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

	enum LimitFrameRate : int
	{
		None,
		Light,
		Heavy
	}
	limitFrameRate{};

	bool wireframe = false;
	bool wireframeChange = false;
	std::optional<int> wireframePhysicsChange;

	InteractionMode interactionMode = InteractionMode::Editor;
	bool interactionModeChanged = false;
	bool showPlayerBody = false;
};
