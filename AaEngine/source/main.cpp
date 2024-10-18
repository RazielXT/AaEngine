#include "AaApplication.h"
#include <shellscalingapi.h>

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow )
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	AaApplication app(hInstance);
	app.start();

	return 0;
}

