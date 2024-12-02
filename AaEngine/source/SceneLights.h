#pragma once

#include "AaMath.h"

namespace aa
{
	struct SceneLights
	{
	public:

		Vector3 ambientColor{};

		struct Light
		{
			Vector3 direction = { 0,0,1 };
			Vector3 color = { 1,1,1 };
		};
		Light directionalLight;
	};
}