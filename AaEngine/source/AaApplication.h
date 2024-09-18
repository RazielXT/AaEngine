#pragma once

#include <vector>

#include "AaRenderSystem.h"
#include "AaWindow.h"
#include "AaFrameListener.h"
#include "AaSceneManager.h"
#include "AaVoxelScene.h"
#include "AaBloomPostProcess.h"
#include "AaShadowMapping.h"
#include "AaShaderManager.h"
#include <windows.h>

class AaApplication
{
public:
	
	AaApplication(HINSTANCE hInstance);
	~AaApplication();

	void start();

private:

	void runtime();

	AaWindow* mWindow{};
	AaRenderSystem* mRenderSystem{};
	AaSceneManager* mSceneMgr{};
	AaMaterialResources* mMaterials;
	AaModelResources* mModels;
	AaShaderManager* mShaders;

	LARGE_INTEGER lastTime{};
	float frequency{};
	std::vector<AaFrameListener*> frameListeners;
};
