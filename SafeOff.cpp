// SafeOff.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "SafeOff.h"
#include <Windows.h>
#include <objidl.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <gdiplus.h>

#pragma comment (lib, "gdiplus.lib")

using namespace Gdiplus;

#define MAX_LOADSTRING 100

std::vector<LPCWSTR> targetProcesses = {
	L"notepad++.exe",
	L"cmd.exe",
};

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

bool isDragging = false;
POINT dragStartPoint;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateRoundWindow(HWND hwnd);
void Draw3DBall(HDC hdc, int x, int y, int radius);
void SnapToEdge(HWND hWnd, RECT rc);
BOOL IsAnyTargetProcessRunning(const std::vector<LPCWSTR>& processNames);


// Check if target process is running
BOOL IsAnyTargetProcessRunning(const std::vector<LPCWSTR>& processNames) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(hSnapshot, &pe32)) {
		do {
			for (const auto& processName : processNames) {
				if (wcscmp(pe32.szExeFile, processName) == 0) {
					CloseHandle(hSnapshot);
					return TRUE;
				}
			}
		} while (Process32Next(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return FALSE;
}

// Shutdown function
void ShutdownSystem() {
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;

	// Get the current process token
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		return;
	}

	// Get the LUID for shutdown privilege
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	// Adjust token privileges
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

	// Shut down the system
	ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_SAFEOFF, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SAFEOFF));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	GdiplusShutdown(gdiplusToken);

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SAFEOFF));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowExW(
		WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		szWindowClass, szTitle, WS_POPUP,
		GetSystemMetrics(SM_CXSCREEN) - 150, 0, 150, 150,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	SetLayeredWindowAttributes(hWnd, 0, 127, LWA_ALPHA);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	CreateRoundWindow(hWnd);

	return TRUE;
}

void CreateRoundWindow(HWND hWnd) {
	HRGN hRgn = CreateEllipticRgn(0, 0, 150, 150);
	SetWindowRgn(hWnd, hRgn, TRUE);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_LBUTTONDBLCLK:
		if (IsAnyTargetProcessRunning(targetProcesses)) {
			int result = MessageBox(hWnd, L"Target process detected. Do you want to shut down?", L"Shutdown", MB_YESNO);
			if (result == IDYES) {
				ShutdownSystem();
			}
		}

		break;
	case WM_LBUTTONDOWN:
		isDragging = true;
		dragStartPoint.x = LOWORD(lParam);
		dragStartPoint.y = HIWORD(lParam);
		SetCapture(hWnd);

		break;
	case WM_MOUSEMOVE:
		if (isDragging) {
			POINT currentPoint;
			currentPoint.x = LOWORD(lParam);
			currentPoint.y = HIWORD(lParam);
			int dx = currentPoint.x - dragStartPoint.x;
			int dy = currentPoint.y - dragStartPoint.y;

			RECT rc;
			GetWindowRect(hWnd, &rc);
			SetWindowPos(hWnd, nullptr, rc.left + dx, rc.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

			std::wstring message = L"Window Position: (" + std::to_wstring(rc.left) + L", " + std::to_wstring(rc.top) + L")\n";
			message += L"Drag Start Point Position: (" + std::to_wstring(dragStartPoint.x) + L", " + std::to_wstring(dragStartPoint.y) + L")\n";
			message += L"Current Point Position: (" + std::to_wstring(currentPoint.x) + L", " + std::to_wstring(currentPoint.y) + L")\n";
			OutputDebugString(message.c_str());
		}

		break;
	case WM_LBUTTONUP:
		isDragging = false;
		RECT rc;
		GetWindowRect(hWnd, &rc);
		SnapToEdge(hWnd, rc);
		ReleaseCapture();
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		Draw3DBall(hdc, 0, 0, 75);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void Draw3DBall(HDC hdc, int x, int y, int radius)
{
	Graphics graphics(hdc);

	graphics.SetSmoothingMode(SmoothingModeAntiAlias);
	graphics.SetCompositingQuality(CompositingQualityHighQuality);
	graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

	GraphicsPath path;
	path.AddEllipse(x, y, radius * 2, radius * 2);

	PathGradientBrush gradientBrush(&path);

	Color centerColor(255, 255, 255, 255);
	gradientBrush.SetCenterColor(centerColor);

	Color surroundColor(255, 0, 128, 255);
	INT count = 1;
	Color colors[1] = { surroundColor };
	gradientBrush.SetSurroundColors(colors, &count);


	PointF centerPoint(0.3f, 0.3f);
	gradientBrush.SetCenterPoint(centerPoint);

	graphics.FillEllipse(&gradientBrush, x, y, radius * 2, radius * 2);

	SolidBrush highlightBrush(Color(180, 255, 255, 255));
	int highlightRadius = radius / 3;
	graphics.FillEllipse(&highlightBrush, x + radius / 3, y + radius / 3, highlightRadius, highlightRadius);
}

void SnapToEdge(HWND hWnd, RECT rc)
{
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	int windowWidth = rc.right - rc.left;
	int windowHeight = rc.bottom - rc.top;

	const int snapThreshold = 30;

	int x = rc.left, y = rc.top;

	if (rc.left < snapThreshold)
	{
		x = -windowWidth / 2;
	}
	else if (screenWidth - rc.right < snapThreshold)
	{
		x = screenWidth - (windowWidth / 2);
	}

	if (rc.top < snapThreshold)
	{
		y = -windowHeight / 2;
	}
	else if (screenHeight - rc.bottom < snapThreshold)
	{
		y = screenHeight - (windowHeight / 2);
	}

	SetWindowPos(hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

