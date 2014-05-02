
#include <memory>
#include <iostream>

#include "AaApplication.h"
#include "MyListener.h"

using namespace std;

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE prevInstance,LPWSTR cmdLine, int cmdShow )
{
	AaApplication app(hInstance,cmdShow);
	app.initialize();

	MyListener* mListener =new MyListener(app.mSceneMgr,app.mPhysicsMgr);
	app.addFrameListener(mListener);

	app.start();

	return 0;
}

