#ifndef _AA_MLISTENER_
#define _AA_MLISTENER_

#include "AaApplication.h"
#include "FreeCamera.h"
#include <OIS.h>

class MyListener : public AaFrameListener, public OIS::KeyListener, public OIS::MouseListener
{

public:

	MyListener(AaSceneManager* mSceneMgr, AaPhysicsManager* mPhysicsMgr);
	~MyListener();

	void setupInputSystem();

	bool frameStarted(float timeSinceLastFrame);

	//input overrides
	bool keyPressed( const OIS::KeyEvent &arg );
	bool keyReleased( const OIS::KeyEvent &arg );
	bool mouseMoved( const OIS::MouseEvent &arg );
	bool mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
	bool mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
	

private: 

	FreeCamera* cameraMan;
	AaRenderSystem* mRS;
	AaSceneManager* mSceneMgr;
	AaPhysicsManager* mPhysicsMgr;
	OIS::InputManager *mInputManager;	//Our Input System
	OIS::Keyboard *mKeyboard;				//Keyboard Device
	OIS::Mouse	 *mMouse;				//Mouse Device
	bool continue_rendering;
};

#endif