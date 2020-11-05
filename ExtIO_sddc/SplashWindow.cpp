#include "SplashWindow.h"
#include <time.h>
#include <process.h>

const LPCSTR SW_CLASSNAME = "SplashWindowClass";

LRESULT CALLBACK SplashWindow_WndProc(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam);
HRGN CreateRgnFromBitmap(HBITMAP hBmp, COLORREF color, HWND hwnd);

SplashWindow::SplashWindow()
{
	clearMembers();
}


SplashWindow::~SplashWindow()
{
	destroySplashWindow();
}


void SplashWindow::showWindow()
{
	if (!isValidWindow())
	{
		return;
	}

	MSG msg;
	BOOL gotMSG;


	ZeroMemory(&msg, sizeof(MSG));




	//Display the window
	UpdateWindow(mHWND);
	ShowWindow(mHWND, SW_SHOW);
	int waitpaint = 500;
	while (waitpaint--)
	{
		gotMSG = PeekMessage(&msg, 0L, 0L, 0L, PM_REMOVE);
		//safely get the value of mThreadFinsshed

		//if the thread is finished, exit the loop
		//thread tuns for at least mMinDisplayTime milliseconds

		if (gotMSG)
		{
			//Don't try this at home!
			//I am capturing the WM_PAINT message before it
			//is sent to the WndProc. I am sure this is not recommended
			//by MS, but I see no adverse effects.
			if (msg.message == WM_PAINT)
			{
				//Blit the bitmap to the screen
				PAINTSTRUCT paint;

				HDC dc = BeginPaint(mHWND, &paint);
				HDC memDC = CreateCompatibleDC(dc);

				SelectObject(memDC, mBitmap);

				BitBlt(dc, 0, 0, mBitmapWidth, mBitmapHeight, memDC, 0, 0, SRCCOPY);

				DeleteObject(dc);
				waitpaint = 100;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

		}
		Sleep(1);
	}
	return;

}





bool SplashWindow::createSplashWindow(HINSTANCE hinst, LPCSTR bitmapFileName, const int r, const int g, const int b)
{
	mBitmap = (HBITMAP)LoadImage(hinst,
		bitmapFileName,
		IMAGE_BITMAP,
		0,
		0,
		LR_LOADFROMFILE);
	if (mBitmap == 0L)
	{
		return false;
	}

	return createWindowHelper(r, g, b);
}

bool SplashWindow::createSplashWindow(HINSTANCE hinst, DWORD bitmapResourceID, const int r, const int g, const int b)
{
	mBitmap = (HBITMAP)LoadImage(hinst,
		MAKEINTRESOURCE(bitmapResourceID),
		IMAGE_BITMAP,
		0,
		0,
		LR_DEFAULTCOLOR);
	if (mBitmap == 0L)
	{
		return false;
	}

	return createWindowHelper(r, g, b);
}

bool SplashWindow::isValidWindow() const
{
	return mHWND != 0L;
}

//
// some idea and code from http://freesourcecode.net/cprojects/91599/Win32-splash-screen-in-c#.WmMV3KjiaUk
//
bool SplashWindow::createWindowHelper(const int r, const int g, const int b)
{
	//check if the bitmap is loaded.
	if (!mBitmap)
	{
		return false;
	}

	BITMAP bm;
	int x, y;


	//create the window...
	WNDCLASS wc;

	wc.lpfnWndProc = SplashWindow_WndProc;
	wc.lpszClassName = SW_CLASSNAME;
	wc.hInstance = GetModuleHandle(0L);

	wc.style = 0;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.hIcon = NULL;

	if (!RegisterClass(&wc))
	{
		DeleteObject(mBitmap);
		clearMembers();
		return false;
	}

	mHWND = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
		wc.lpszClassName,
		"",
		WS_POPUP,
		0,
		0,
		0,
		0,
		HWND_DESKTOP,
		0L,
		wc.hInstance,
		0L);

	if (!isValidWindow())
	{
		destroySplashWindow();
		return false;
	}

	//get the info on our bitmap
	GetObject(mBitmap, sizeof(bm), &bm);

	//get the dimensions
	mBitmapWidth = bm.bmWidth;
	mBitmapHeight = bm.bmHeight;

	//center the splash window on the screen
	x = (GetSystemMetrics(SM_CXFULLSCREEN) - mBitmapWidth) / 2;
	y = (GetSystemMetrics(SM_CYFULLSCREEN) - mBitmapHeight) / 2;

	MoveWindow(mHWND, x, y, mBitmapWidth, mBitmapHeight, FALSE);

	//if r, g, b are valid values, "cut out" all pixels equal to r,g,b
	if (r != -1 && g != -1 && b != -1)
	{
		HRGN region = CreateRgnFromBitmap(mBitmap, RGB(r, g, b), mHWND);

		if (FAILED(SetWindowRgn(mHWND, region, TRUE)))
		{
			destroySplashWindow();
			return false;
		}
	}

	return true;
}

void SplashWindow::clearMembers()
{
	mHWND = 0L;
	mBitmap = 0L;
	mBitmapWidth = mBitmapHeight = 0;


}

void SplashWindow::destroySplashWindow()
{
	DestroyWindow(mHWND);
	DeleteObject(mBitmap);
	clearMembers();
	UnregisterClass(SW_CLASSNAME, GetModuleHandle(0L));
}

HRGN CreateRgnFromBitmap(HBITMAP hBmp, COLORREF color, HWND hwnd)
{
	// this code is written by Davide Pizzolato
	// see www.codeproject.com/Articles/1014/CxSkinButton

	if (!hBmp) return NULL;

	BITMAP bm;
	GetObject(hBmp, sizeof(BITMAP), &bm);	// get bitmap attributes

	HDC windowHDC = GetDC(hwnd);
	HDC hdc = CreateCompatibleDC(windowHDC);	//Creates a memory device context for the bitmap
	SelectObject(hdc, hBmp);			//selects the bitmap in the device context

	const DWORD RDHDR = sizeof(RGNDATAHEADER);
	const DWORD MAXBUF = 40;		// size of one block in RECTs
	// (i.e. MAXBUF*sizeof(RECT) in bytes)
	LPRECT	pRects;
	DWORD	cBlocks = 0;			// number of allocated blocks

	INT		i, j;					// current position in mask image
	INT		first = 0;				// left position of current scan line
	// where mask was found
	bool	wasfirst = false;		// set when if mask was found in current scan line
	bool	ismask;					// set when current color is mask color

	// allocate memory for region data
	RGNDATAHEADER* pRgnData = (RGNDATAHEADER*)new BYTE[RDHDR + ++cBlocks * MAXBUF * sizeof(RECT)];
	memset(pRgnData, 0, RDHDR + cBlocks * MAXBUF * sizeof(RECT));
	// fill it by default
	pRgnData->dwSize = RDHDR;
	pRgnData->iType = RDH_RECTANGLES;
	pRgnData->nCount = 0;
	for (i = 0; i < bm.bmHeight; i++)
		for (j = 0; j < bm.bmWidth; j++) {
			// get color
			ismask = (GetPixel(hdc, j, bm.bmHeight - i - 1) != color);
			// place part of scan line as RECT region if transparent color found after mask color or
			// mask color found at the end of mask image
			if (wasfirst && ((ismask && (j == (bm.bmWidth - 1))) || (ismask ^ (j < bm.bmWidth)))) {
				// get offset to RECT array if RGNDATA buffer
				pRects = (LPRECT)((LPBYTE)pRgnData + RDHDR);
				// save current RECT
				SetRect(&pRects[pRgnData->nCount++], first, bm.bmHeight - i - 1, j + (j == (bm.bmWidth - 1)), bm.bmHeight - i);
				// if buffer full reallocate it
				if (pRgnData->nCount >= cBlocks * MAXBUF) {
					LPBYTE pRgnDataNew = new BYTE[RDHDR + ++cBlocks * MAXBUF * sizeof(RECT)];
					memcpy(pRgnDataNew, pRgnData, RDHDR + (cBlocks - 1) * MAXBUF * sizeof(RECT));
					delete[] pRgnData;
					pRgnData = (RGNDATAHEADER*)pRgnDataNew;
				}
				wasfirst = false;
			}
			else if (!wasfirst && ismask) {		// set wasfirst when mask is found
				first = j;
				wasfirst = true;
			}
		}

	DeleteDC(hdc);	//release the bitmap
	DeleteDC(windowHDC);
	// create region
	/*  Under WinNT the ExtCreateRegion returns NULL (by Fable@aramszu.net) */
	//	HRGN hRgn = ExtCreateRegion( NULL, RDHDR + pRgnData->nCount * sizeof(RECT), (LPRGNDATA)pRgnData );
	/* ExtCreateRegion replacement { */
	HRGN hRgn = CreateRectRgn(0, 0, 0, 0);

	if (hRgn == NULL)
	{
		return hRgn;
	}

	pRects = (LPRECT)((LPBYTE)pRgnData + RDHDR);
	for (i = 0; i < (int)pRgnData->nCount; i++)
	{
		HRGN hr = CreateRectRgn(pRects[i].left, pRects[i].top, pRects[i].right, pRects[i].bottom);
		if (FAILED(CombineRgn(hRgn, hRgn, hr, RGN_OR)))
		{
			DeleteObject(hr);
			return hRgn;
		}

		if (hr)
		{
			DeleteObject(hr);
		}
	}

	/* } ExtCreateRegion replacement */

	delete[] pRgnData;
	return hRgn;
}


LRESULT CALLBACK SplashWindow_WndProc(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam)
{
	return DefWindowProc(hwnd, message, wParam, lParam);
}
