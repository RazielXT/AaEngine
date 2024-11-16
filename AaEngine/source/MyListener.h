#pragma once

#include "AaApplication.h"
#include "FreeCamera.h"
#include "InputHandler.h"
#include "AaRenderSystem.h"
#include "SceneManager.h"
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
 	SceneManager* sceneMgr;
	InstancingManager instancing;
	GrassAreaGenerator* grass;
	AaShadowMap* shadowMap;
	imgui::DebugWindow debugWindow;
	aa::SceneLights lights;

	bool continueRendering = true;

	FrameParameters params;
	FrameCompositor* compositor;

	void loadScene(const char* scene);
};
