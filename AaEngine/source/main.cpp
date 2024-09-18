#include "AaApplication.h"

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow )
{
// 	// Allocate a console for this application
// 	AllocConsole();
// 
// 	// Redirect standard input/output to the console
// 	FILE* fp;
// 	freopen_s(&fp, "CONOUT$", "w", stdout);
// 	freopen_s(&fp, "CONOUT$", "w", stderr);
// 	freopen_s(&fp, "CONIN$", "r", stdin);

	AaApplication app(hInstance);
	app.start();

	return 0;
}

