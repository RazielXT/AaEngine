#pragma once

#include "AaApplication.h"
#include "FreeCamera.h"
#include "InputHandler.h"
#include "AaDebugWindow.h"

class MyListener : public AaFrameListener, public InputListener, public ScreenListener
{
public:

	MyListener(AaSceneManager* mSceneMgr);
	~MyListener();

	bool frameStarted(float timeSinceLastFrame) override;

	bool keyPressed(int key) override;
	bool keyReleased(int key) override;
	bool mousePressed(MouseButton button) override;
	bool mouseReleased(MouseButton button) override;
	bool mouseMoved(int x, int y) override;

	void onScreenResize() override;

private:

	FreeCamera* cameraMan;
	AaRenderSystem* mRS;
	AaSceneLights mLights;
	AaSceneManager* mSceneMgr;
	AaVoxelScene* voxelScene;
	AaShadowMapping* mShadowMapping;
	AaBloomPostProcess* pp;
	DebugWindow debugWindow;

	bool continue_rendering = true;
};
