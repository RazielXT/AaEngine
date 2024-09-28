#include "AaApplication.h"
#include "MyListener.h"
#include "AaLogger.h"
#include <dxgidebug.h>
#include <dxgi1_4.h>

AaApplication::AaApplication(HINSTANCE hInstance)
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	frequency = (float)freq.QuadPart;
	QueryPerformanceCounter(&lastTime);

	int x = 1280;
	int y = 800;

	mWindow = new AaWindow(hInstance, x, y);
 	mRenderSystem = new AaRenderSystem(mWindow);
	mShaderConstants = new ShaderConstantBuffers(mRenderSystem);
	mShaders = new AaShaderResources(mRenderSystem);
	mMaterials = new AaMaterialResources(mRenderSystem);
	mModels = new AaModelResources(mRenderSystem);

	AaLogger::log("Application created");
}

AaApplication::~AaApplication()
{
	for (auto frameListener : frameListeners)
		delete frameListener;

	mRenderSystem->WaitForAllFrames();

 	delete mModels;
 	delete mMaterials;
 	delete mShaders;
	delete mShaderConstants;
 	delete mRenderSystem;
	delete mWindow;

#ifndef NDEBUG
	IDXGIDebug* dxgiDebug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
	{
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		dxgiDebug->Release();
	}
#endif
}

void AaApplication::start()
{
	frameListeners.push_back(new MyListener(mRenderSystem));

	runtime();
}

void AaApplication::runtime()
{
	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// Update time
			LARGE_INTEGER thisTime;
			QueryPerformanceCounter(&thisTime);
			float timeSinceLastFrame = (thisTime.QuadPart - lastTime.QuadPart) / frequency;
			lastTime = thisTime;

			for (auto f : frameListeners)
			{
				if (!f->frameStarted(timeSinceLastFrame))
					return;
			}
		}
	}
}
