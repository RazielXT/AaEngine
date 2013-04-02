#include "AaWindow.h"
#include <windows.h>

LRESULT CALLBACK WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT paintStruct;
	HDC hDC;

	switch( message )
	{
		case WM_PAINT:
			hDC = BeginPaint( hwnd, &paintStruct );
			EndPaint( hwnd, &paintStruct );
			break;
		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;
		default:
			return DefWindowProc( hwnd, message, wParam, lParam );
	}

	return 0;
}


AaWindow::AaWindow(HINSTANCE hInstance, int cmdShow, int width, int heigth)
{
	this->width = width;
	this->heigth = heigth;

	WNDCLASSEX wndClass = { 0 };
	wndClass.cbSize = sizeof( WNDCLASSEX ) ;
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.hInstance = hInstance;
	wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndClass.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = "DX11BookWindowClass";
	if( !RegisterClassEx( &wndClass ) )
	return;

	RECT rc = { 0, 0, width, heigth };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	hwnd = CreateWindowA( "DX11BookWindowClass", "Dx11 base engine",
	WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.
	left,
	rc.bottom - rc.top, NULL, NULL, hInstance, NULL );
	if( !hwnd )
	return;

	ShowWindow( hwnd, cmdShow );

}

AaWindow::~AaWindow()
{
}

const HWND AaWindow::getHwnd()
{
	return hwnd;
}

const int AaWindow::getWidth()
{
	return width;
}

const int AaWindow::getHeight()
{
	return heigth;
}
