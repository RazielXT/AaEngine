#ifndef _AA_SHADINGMGR_
#define _AA_SHADINGMGR_

#include <vector>
#include "AaRenderSystem.h"
#include "AaCamera.h"

enum LightType
{
	LightType_Point, LightType_Directional, LightType_Spotlight
};

struct Light
{
	LightType type;
	XMFLOAT3 direction;
	XMFLOAT3 color;
	XMFLOAT3 position;
	float attenuation;
	float range;
};

class AaShadingManager
{
public:
	AaShadingManager(AaRenderSystem* mRS);
	~AaShadingManager();

	void updatePerFrameConstants(float timeSinceLastFrame, AaCamera* cam, AaCamera* sun);
	void updatePerFrameConstants(AaCamera* cam);

	XMFLOAT3 ambientColor;
	std::vector<Light*> lights;
	Light* directionalLight;

private:
	
	AaRenderSystem* mRenderSystem;
};

#endif