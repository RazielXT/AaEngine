#pragma once

#include "Math.h"

enum LightType
{
	LightType_Unspecified,
	LightType_Point,
	LightType_Directional,
	LightType_Spotlight
};

struct Light
{
	LightType type;
	Vector3 direction;
	Vector3 color;
	Vector3 position;
	float attenuation;
	float range;
};

class AaSceneLights
{
public:

	AaSceneLights();
	~AaSceneLights();

	Vector3 ambientColor{};
	std::vector<Light*> lights;
	Light directionalLight;
};
