#include "AaApplication.h"


AaApplication::AaApplication(HINSTANCE hInstance, int cmdShow)
{
	this->hInstance=hInstance;
	this->cmdShow=cmdShow;
	started=false;

	
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	frequency=freq.QuadPart;

	QueryPerformanceCounter(&lastTime);

	AaLogger::getLogger()->writeMessage("Application created");

}

AaApplication::AaApplication(HINSTANCE hInstance, int cmdShow, LPWSTR cmdLine)
{
	this->hInstance=hInstance;
	this->cmdShow=cmdShow;
	started=false;
}

AaApplication::~AaApplication()
{
	for(int i=0;i<frameListeners.size();i++)
		delete frameListeners.at(i);
	frameListeners.clear();

	delete mSceneMgr;
	delete mRenderSystem;
	delete mWindow;
	delete mGuiMgr;
	delete AaLogger::getLogger();
}

void AaApplication::addFrameListener(AaFrameListener* listener)
{
	frameListeners.push_back(listener);
}

bool AaApplication::initialize()
{
	if(started) 
		return 1;

	started=true;

	mWindow=new AaWindow(hInstance,cmdShow,512,384);
	mRenderSystem=new AaRenderSystem(mWindow);
	mGuiMgr=new AaGuiSystem();
	mGuiMgr->init(mRenderSystem->getDevice(),mRenderSystem->getContext(),mWindow->getWidth(),mWindow->getHeight());
	mPhysicsMgr=new AaPhysicsManager();
	mSceneMgr=new AaSceneManager(mRenderSystem,mGuiMgr);
	

	return 0;
}

bool AaApplication::start()
{

	runtime();

	return true;

}

void AaApplication::runtime()
{
	// Demo Initialize
	MSG msg = { 0 };
	while( msg.message != WM_QUIT )
	{
		if( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
		{
			// Update time
			LARGE_INTEGER thisTime;
			QueryPerformanceCounter(&thisTime);
			float timeSinceLastFrame=(thisTime.QuadPart-lastTime.QuadPart)/frequency;
			lastTime=thisTime;

			char fContinue=0;
			char fSum=frameListeners.size();

			//frame started
			for(int i=0;i<fSum;i++)
				fContinue += frameListeners.at(i)->frameStarted(timeSinceLastFrame);
			
			if(fContinue<fSum)
				break;
		}
	}

}