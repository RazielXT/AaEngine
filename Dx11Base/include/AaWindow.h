#ifndef _AA_WINDOW_
#define _AA_WINDOW_

#include <windows.h>

class AaWindow
{

public:

	AaWindow(HINSTANCE hInstance, int cmdShow, int width = 640, int heigth = 480);
	~AaWindow();

	const int getHeight();
	const int getWidth();
	const HWND getHwnd();

private:

	int heigth;
	int width;
	HWND hwnd;
};

#endif