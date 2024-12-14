#include "AaApplication.h"
#include "MyListener.h"
#include "AaLogger.h"
#include <dxgidebug.h>
#include <dxgi1_4.h>

AaApplication::AaApplication(HINSTANCE hInstance)
{
	int x = 1280;
	int y = 800;

	mWindow = new AaWindow(hInstance, x, y);
 	mRenderSystem = new RenderSystem(mWindow);
	mResources = new DescriptorManager(mRenderSystem->device);
	mResources->init(1000);
	mShaderConstants = new ShaderConstantBuffers(mRenderSystem);
	mShaders = new AaShaderLibrary(mRenderSystem);
	mMaterials = new AaMaterialResources(mRenderSystem);
	mModels = new AaModelResources(mRenderSystem);
	mRenderSystem->init();
	mResources->initializeSamplers(mRenderSystem->upscale.getMipLodBias());

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
	delete mResources;
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
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	auto frequency = (float)freq.QuadPart;

	LARGE_INTEGER lastTime{};
	QueryPerformanceCounter(&lastTime);

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

			if (timeSinceLastFrame > 1.f)
				timeSinceLastFrame = 1.f;

			for (auto f : frameListeners)
			{
				if (!f->frameStarted(timeSinceLastFrame))
					return;
			}
		}
	}
}
