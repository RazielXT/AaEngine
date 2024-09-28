#include "AaApplication.h"
#include "d3d12sdklayers.h"
#include <shellscalingapi.h>
#include "AaLogger.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		AaLogger::logError("Failed to initialize COM");
		return 0;
	}

#ifndef NDEBUG
	ID3D12Debug* debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif

	AaApplication app(hInstance);
	app.start();

	return 0;
}
