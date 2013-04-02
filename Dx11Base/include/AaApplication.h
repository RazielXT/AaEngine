#ifndef _AA_APPLICATION_
#define _AA_APPLICATION_


#include <vector>

#include "AaLogger.h"
#include "AaRenderSystem.h"
#include "AaWindow.h"
#include "AaFrameListener.h"
#include "AaSceneManager.h"
#include "AaPhysicsManager.h"
#include "AaVoxelScene.h"
#include "AaBloomPostProcess.h"
#include "AaShadowMapping.h"
#include "AaGuiSystem.h"
#include <windows.h>

class AaApplication
{

public:
	
	AaApplication(HINSTANCE hInstance, int cmdShow);
	AaApplication(HINSTANCE hInstance, int cmdShow, LPWSTR cmdLine);
	~AaApplication();
	void addFrameListener(AaFrameListener* listener);
	bool start();
	bool initialize();

	AaWindow* mWindow;
	AaRenderSystem* mRenderSystem;
	AaSceneManager* mSceneMgr;
	AaPhysicsManager* mPhysicsMgr;
	AaGuiSystem* mGuiMgr;

protected:

	void runtime();

private:

	bool started;
	HINSTANCE hInstance;
	int cmdShow;
	LARGE_INTEGER lastTime;
	float frequency;
	std::vector<AaFrameListener*> frameListeners;
};


#endif