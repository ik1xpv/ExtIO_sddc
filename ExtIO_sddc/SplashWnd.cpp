/*! \file SplashWnd.cpp
\brief Класс, реализующий splash-окно в отдельном потоке. Без MFC.

Created 2006-06-08 by Kirill V. Lyadvinsky \n
Modified:
2007-05-25 Additional synchronization added. Fixed working with the timer. \ n
2008-06-20 Added text output. Support for multiple monitors. Support for loadable resources (DLL) \ n

* The contents of this file are subject to the terms of the Common Development and    \n
* Distribution License ("CDDL")(collectively, the "License"). You may not use this    \n
* file except in compliance with the License. You can obtain a copy of the CDDL at    \n
* http://www.opensource.org/licenses/cddl1.php.                                       \n

*/

#include <Windows.h>
#include <CommCtrl.h>
#include <GdiPlus.h>
#include <string>
#include <process.h>

#include "SplashWnd.h"
//#include "MemStream.h"

CSplashWnd::CSplashWnd( HWND hParent )
{
	m_hThread = NULL;

	m_hSplashWnd = NULL;
	m_ThreadId = 0;
	m_hEvent = NULL;
	m_hParentWnd = hParent;
}

CSplashWnd::~CSplashWnd()
{
	Hide();
	if (mBitmap) delete mBitmap;
}



// void CSplashWnd::SetImage(HMODULE hModule, UINT nResourceID/*, DWORD dwLangLCID*/ )
// {
// 	if (m_pImage != NULL) return;
//
// 	HRSRC hImage = FindResource( hModule, MAKEINTRESOURCE(nResourceID), L"IMAGE" );
// 	IMalloc* pMalloc = NULL;
// 	if (FAILED(CoGetMalloc(1, 	&pMalloc)))
// 	{
// 		if (pMalloc) pMalloc->Release();
// 		return;
// 	}
//
// 	IMemStream* pImageStream = CreateStreamOnMalloc(pMalloc, 1,
// 		static_cast<LPBYTE>(LockResource(LoadResource(hModule, hImage))),
// 		SizeofResource(hModule, hImage), TRUE);
// 	pMalloc->Release();
//
// 	if (pImageStream)
// 	{
// 		m_pImage = new Gdiplus::Image(pImageStream);
// 		pImageStream->Release();
// 	}
// }

bool CSplashWnd::createSplashWindow (DWORD bitmapResourceID, const int r, const int g, const int b)
{
	mBitmap = (HBITMAP)LoadImage (GetModuleHandle (0L),
								MAKEINTRESOURCE (bitmapResourceID),
								IMAGE_BITMAP,
								0,
								0,
								LR_DEFAULTCOLOR);
	if (mBitmap == 0L)
	{
		return false;
	}

	return createWindowHelper (r, g, b);
}

bool CSplashWnd::createWindowHelper (const int r, const int g, const int b)
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
	wc.hInstance = GetModuleHandle (0L);

	wc.style = 0;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)GetStockObject (WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.hIcon = NULL;

	if (!RegisterClass(&wc))
	{
		DeleteObject (mBitmap);
		clearMembers ();
		return false;
	}

	mHWND = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
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

	if (!isValidWindow ())
	{
		destroySplashWindow ();
		return false;
	}

	//get the info on our bitmap
	GetObject(mBitmap, sizeof(bm), &bm);

	//get the dimensions
	mBitmapWidth = bm.bmWidth;
	mBitmapHeight = bm.bmHeight;

	//center the splash window on the screen
	x =(GetSystemMetrics(SM_CXFULLSCREEN)-mBitmapWidth)/2;
	y =(GetSystemMetrics(SM_CYFULLSCREEN)-mBitmapHeight)/2;

	MoveWindow (mHWND, x, y, mBitmapWidth, mBitmapHeight, FALSE);

	//if r, g, b are valid values, "cut out" all pixels equal to r,g,b
	if (r != -1 && g != -1 && b != -1)
	{
		HRGN region = CreateRgnFromBitmap (mBitmap, RGB(r,g,b), mHWND);

		if (FAILED (SetWindowRgn (mHWND, region, TRUE)))
		{
			destroySplashWindow ();
			return false;
		}
	}

	return true;
}


void CSplashWnd::Show()
{
	if (m_hThread == NULL)
	{
		m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, SplashThreadProc, static_cast<LPVOID>(this), 0, &m_ThreadId);
		if (WaitForSingleObject(m_hEvent, 5000) == WAIT_TIMEOUT)
		{
			OutputDebugString("Error starting SplashThread\n");
		}
	}
	else
	{
		PostThreadMessage( m_ThreadId, WM_ACTIVATE, WA_CLICKACTIVE, 0 );
	}
}

void CSplashWnd::Hide()
{
	if (m_hThread)
	{
		PostThreadMessage(m_ThreadId, WM_QUIT, 0, 0);
		if ( WaitForSingleObject(m_hThread, 9000) == WAIT_TIMEOUT )
		{
			::TerminateThread( m_hThread, 2222 );
		}
		CloseHandle(m_hThread);
		CloseHandle(m_hEvent);
	}
	m_hThread = NULL;
}

unsigned int __stdcall CSplashWnd::SplashThreadProc( void* lpParameter )
{
	CSplashWnd* pThis = static_cast<CSplashWnd*>(lpParameter);
	if (pThis->mBitmap == 0L) return 0;

	// Register your unique class name
	WNDCLASS wndcls = {0};

	wndcls.style = CS_HREDRAW | CS_VREDRAW;
	wndcls.lpfnWndProc = SplashWndProc;
	wndcls.hInstance = GetModuleHandle(NULL);
	wndcls.hCursor = LoadCursor(NULL, IDC_APPSTARTING);
	wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndcls.lpszClassName = "SplashWnd";
//	wndcls.hIcon = LoadIcon(wndcls.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

	if (!RegisterClass(&wndcls))
	{
		if (GetLastError() != 0x00000582) // already registered)
		{
			OutputDebugString("Unable to register class SplashWnd\n");
			return 0;
		}
	}


	ShowWindow(pThis->m_hSplashWnd, SW_SHOWNOACTIVATE);

	MSG msg;
	BOOL bRet;
	LONG timerCount = 0;

	PeekMessage(&msg, NULL, 0, 0, 0); // invoke creating message queue
	SetEvent(pThis->m_hEvent);

	while ((bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
	{
		if (msg.message == WM_QUIT) break;
		if (msg.message == PBM_SETPOS)
		{
			KillTimer(NULL, pThis->m_TimerId);
			SendMessage(pThis->m_hSplashWnd, PBM_SETPOS, msg.wParam, msg.lParam);
			continue;
		}
		if (msg.message == PBM_SETSTEP)
		{
			SendMessage(pThis->m_hSplashWnd, PBM_SETPOS, LOWORD(msg.wParam), 0); // initiate progress bar creation
			timerCount = static_cast<LONG>(msg.lParam);
			pThis->m_TimerId = SetTimer(NULL, 0, 1000, NULL);
			continue;
		}
		if (msg.message == WM_TIMER && msg.wParam == pThis->m_TimerId)
		{
			timerCount--;
			if (timerCount <= 0) {
				timerCount =0;
				KillTimer(NULL, pThis->m_TimerId);
        Sleep(0);
			}
			continue;
		}


		if (bRet == -1)
		{
			// handle the error and possibly exit
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	DestroyWindow(pThis->m_hSplashWnd);

	return 0;
}

LRESULT CALLBACK CSplashWnd::SplashWndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	CSplashWnd* pInstance = reinterpret_cast<CSplashWnd*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
	if (pInstance == NULL)
	{
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

  switch (uMsg)
	{
		case WM_PAINT:
		{
		    /*
			if (pInstance->m_pImage)
			{
				Gdiplus::Graphics gdip(hwnd);
				gdip.DrawImage(pInstance->m_pImage, 0, 0, pInstance->m_pImage->GetWidth(), pInstance->m_pImage->GetHeight());

				if ( pInstance->m_ProgressMsg.size() > 0 )
				{
					Gdiplus::Font msgFont( L"Tahoma", 8, Gdiplus::UnitPixel );
					Gdiplus::SolidBrush msgBrush( static_cast<DWORD>(Gdiplus::Color::Black) );
					gdip.DrawString( pInstance->m_ProgressMsg.c_str(), -1, &msgFont, Gdiplus::PointF(2.0f, pInstance->m_pImage->GetHeight()-34.0f), &msgBrush );
				}
			}
			*/
			ValidateRect(hwnd, NULL);
			return 0;
		} break;

	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CSplashWnd::SetWindowName( const char* windowName )
{
	m_WindowName = (char*) windowName;
}


