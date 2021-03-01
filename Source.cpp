#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <windowsx.h>
#include <math.h>

#define DEFAULT_DPI 96
#define SCALEX(X) MulDiv(X, uDpiX, DEFAULT_DPI)
#define SCALEY(Y) MulDiv(Y, uDpiY, DEFAULT_DPI)
#define POINT2PIXEL(PT) MulDiv(PT, uDpiY, 72)

TCHAR szClassName[] = TEXT("Window");

BOOL GetScaling(HWND hWnd, UINT* pnX, UINT* pnY)
{
	BOOL bSetScaling = FALSE;
	const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (hMonitor)
	{
		HMODULE hShcore = LoadLibrary(TEXT("SHCORE"));
		if (hShcore)
		{
			typedef HRESULT __stdcall GetDpiForMonitor(HMONITOR, int, UINT*, UINT*);
			GetDpiForMonitor* fnGetDpiForMonitor = reinterpret_cast<GetDpiForMonitor*>(GetProcAddress(hShcore, "GetDpiForMonitor"));
			if (fnGetDpiForMonitor)
			{
				UINT uDpiX, uDpiY;
				if (SUCCEEDED(fnGetDpiForMonitor(hMonitor, 0, &uDpiX, &uDpiY)) && uDpiX > 0 && uDpiY > 0)
				{
					*pnX = uDpiX;
					*pnY = uDpiY;
					bSetScaling = TRUE;
				}
			}
			FreeLibrary(hShcore);
		}
	}
	if (!bSetScaling)
	{
		HDC hdc = GetDC(NULL);
		if (hdc)
		{
			*pnX = GetDeviceCaps(hdc, LOGPIXELSX);
			*pnY = GetDeviceCaps(hdc, LOGPIXELSY);
			ReleaseDC(NULL, hdc);
			bSetScaling = TRUE;
		}
	}
	if (!bSetScaling)
	{
		*pnX = DEFAULT_DPI;
		*pnY = DEFAULT_DPI;
		bSetScaling = TRUE;
	}
	return bSetScaling;
}

struct line {
	POINT start;
	POINT end;
	bool select;
	void draw(HDC hdc) {
		HPEN hPen = CreatePen(PS_SOLID, select ? 4 : 1, select ? RGB(255, 0, 0) : RGB(0, 0, 0));
		HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
		MoveToEx(hdc, start.x, start.y, 0);
		LineTo(hdc, end.x, end.y);
		SelectObject(hdc, hOldPen);
		DeleteObject(hPen);
	}
	// 参考 https://teratail.com/questions/155180
	bool HitTest(const POINT& p, double d)
	{
		double x0 = p.x, y0 = p.y;
		double x1 = start.x, y1 = start.y;
		double x2 = end.x, y2 = end.y;
		double a = x2 - x1;
		double b = y2 - y1;
		double a2 = a * a;
		double b2 = b * b;
		double r2 = a2 + b2;
		double tt = -(a * (x1 - x0) + b * (y1 - y0));
		if (tt < 0)
			return sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) <= d;
		else if (tt > r2)
			return sqrt((x2 - x0) * (x2 - x0) + (y2 - y0) * (y2 - y0)) <= d;
		double f1 = a * (y1 - y0) - b * (x1 - x0);
		return sqrt((f1 * f1) / r2) <= d;
	}
};


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hButton;
	static HFONT hFont;
	static UINT uDpiX = DEFAULT_DPI, uDpiY = DEFAULT_DPI;
	static line l[16];
	static int nPoint;
	static bool select;
	switch (msg)
	{
	case WM_CREATE:
		hButton = CreateWindow(TEXT("BUTTON"), TEXT("クリア"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDOK, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hWnd, WM_DPICHANGED, 0, 0);
		SendMessage(hWnd, WM_COMMAND, IDOK, 0);
		break;
	case WM_SIZE:
		MoveWindow(hButton, POINT2PIXEL(10), POINT2PIXEL(10), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			for (int i = 0; i < _countof(l); i++)
			{
				l[i].start.x = rand() % rect.right;
				l[i].start.y = rand() % rect.bottom;
				l[i].end.x = rand() % rect.right;
				l[i].end.y = rand() % rect.bottom;
				l[i].select = false;
			}
			InvalidateRect(hWnd, 0, TRUE);
		}
		break;
	case WM_LBUTTONDOWN:
		{
			POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			for (int i = 0; i < _countof(l); i++)
			{
				l[i].select = false;
			}
			for (int i = 0; i < _countof(l); i++)
			{
				if (l[i].HitTest(point, POINT2PIXEL(10))) {
					l[i].select = true;
					break;
 				}
			}
			InvalidateRect(hWnd, 0, TRUE);
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			for (int i = 0; i < _countof(l); i++)
			{
				l[i].draw(hdc);
			}
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_NCCREATE:
	{
		const HMODULE hModUser32 = GetModuleHandle(TEXT("user32.dll"));
		if (hModUser32)
		{
			typedef BOOL(WINAPI* fnTypeEnableNCScaling)(HWND);
			const fnTypeEnableNCScaling fnEnableNCScaling = (fnTypeEnableNCScaling)GetProcAddress(hModUser32, "EnableNonClientDpiScaling");
			if (fnEnableNCScaling)
			{
				fnEnableNCScaling(hWnd);
			}
		}
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
	case WM_DPICHANGED:
		GetScaling(hWnd, &uDpiX, &uDpiY);
		DeleteObject(hFont);
		hFont = CreateFontW(-POINT2PIXEL(10), 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 0, 0, L"MS Shell Dlg");
		SendMessage(hButton, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_DESTROY:
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("Window"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
