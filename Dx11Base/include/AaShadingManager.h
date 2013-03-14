#ifndef _AA_SHADINGMGR_
#define _AA_SHADINGMGR_

#include <vector>
#include "AaRenderSystem.h"
#include "AaCamera.h"

enum LightType
{
	LightType_Point, LightType_Directional
};

struct Light
{
	LightType type;
	XMFLOAT3 direction;
	XMFLOAT3 color;
	XMFLOAT3 position;
};

class AaShadingManager
{
public:
	AaShadingManager(AaRenderSystem* mRS);
	~AaShadingManager();

	void updatePerFrameConstants(float timeSinceLastFrame, AaCamera* cam);
	void updatePerFrameConstants(AaCamera* cam);

	XMFLOAT3 ambientColor;
	std::vector<Light*> lights;
	Light* directionalLight;

private:
	
	AaRenderSystem* mRenderSystem;
};

#endif