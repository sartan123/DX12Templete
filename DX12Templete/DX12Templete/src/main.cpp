#include <windows.h>
#include <tchar.h>
#include "DXRenderer.h"
#include "DX12Renderer.h"

#define WINDOW_WIDTH  720
#define WINDOW_HEIGHT 480
LPCTSTR szAppName = "DirectX12 Templete";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND SetupWindow(HINSTANCE hInstance);
WNDCLASSEX CreateWindowClass(HINSTANCE hInstance);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPTSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wndClass;
	HWND hwnd;
	MSG msg;
	
	wndClass = CreateWindowClass(hInstance);
	if (!RegisterClassEx(&wndClass))
	{
		return -1;
	}

	hwnd = SetupWindow(hInstance);
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	//DXRenderer* renderer = new DXRenderer(hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);
	DX12Renderer* renderer = new DX12Renderer(hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);

	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			renderer->Render();
		}
	}
	return (INT)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return(DefWindowProc(hwnd, msg, wParam, lParam));
	}
	return (0L);
}

HWND SetupWindow(HINSTANCE hInstance)
{	
	HWND  hwnd;
	DWORD dwExStyle = 0;
	DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~(WS_SIZEBOX | WS_MAXIMIZEBOX);
	RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	int cx = CW_USEDEFAULT, cy = CW_USEDEFAULT;
	hwnd = CreateWindowEx( dwExStyle, szAppName, szAppName, dwStyle,
		cx, cy, rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, hInstance, nullptr);

	return hwnd;
}

WNDCLASSEX CreateWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = szAppName;
	wndClass.hIconSm = LoadIcon(NULL, IDI_ASTERISK);

	return wndClass;
}
