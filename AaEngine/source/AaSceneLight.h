#pragma once

#include "Math.h"

struct AaSceneLight
{
	Vector3 ambientColor{};

	struct Light
	{
		Vector3 direction;
		Vector3 color;
	};
	Light directionalLight;
};
