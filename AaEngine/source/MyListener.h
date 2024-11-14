#pragma once

#include "AaApplication.h"
#include "FreeCamera.h"
#include "InputHandler.h"
#include "AaRenderSystem.h"
#include "AaSceneManager.h"
#include "AaModelResources.h"
#include "FrameCompositor.h"
#include "ShadowMap.h"
#include "DebugWindow.h"


class MyListener : public AaFrameListener, public InputListener, public ScreenListener
{
public:

	MyListener(AaRenderSystem* render);
	~MyListener();

	bool frameStarted(float timeSinceLastFrame) override;

	bool keyPressed(int key) override;
	bool keyReleased(int key) override;
	bool mousePressed(MouseButton button) override;
	bool mouseReleased(MouseButton button) override;
	bool mouseMoved(int x, int y) override;
	bool mouseWheel(float change) override;

	void onScreenResize() override;

private:

 	FreeCamera* cameraMan;
 	AaRenderSystem* renderSystem;
 	AaSceneManager* sceneMgr;
	AaShadowMap* shadowMap;
	imgui::DebugWindow debugWindow;
	aa::SceneLights lights;

	float elapsedTime = 0;
	bool continue_rendering = true;

	FrameParameters gpuParams;
	FrameCompositor* compositor;

	void initializeGpuResources();

	void updateFrameParams(float tslf);

	void loadScene(const char* scene);
};
