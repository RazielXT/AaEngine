#pragma once

#include <vector>
#include <windows.h>

#include "AaWindow.h"
#include "AaRenderSystem.h"
#include "AaFrameListener.h"
#include "AaModelResources.h"
#include "AaMaterialResources.h"
#include "AaShaderResources.h"

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
 	AaMaterialResources* mMaterials;
 	AaModelResources* mModels;
 	AaShaderResources* mShaders;
	ShaderConstantBuffers* mShaderConstants;

	LARGE_INTEGER lastTime{};
	float frequency{};
	std::vector<AaFrameListener*> frameListeners;
};
