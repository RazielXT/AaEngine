#pragma once

#include <vector>
#include <windows.h>

#include "AaWindow.h"
#include "AaRenderSystem.h"
#include "AaFrameListener.h"
#include "AaModelResources.h"
#include "AaMaterialResources.h"
#include "AaShaderLibrary.h"

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
	ResourcesManager* mResources{};
 	AaMaterialResources* mMaterials;
 	AaModelResources* mModels;
 	AaShaderLibrary* mShaders;
	ShaderConstantBuffers* mShaderConstants;

	std::vector<AaFrameListener*> frameListeners;
};
