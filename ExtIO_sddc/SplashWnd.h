/*! \file SplashWnd.h
\brief class which implements the splash-window in a separate thread. Without MFC.

Created 2006-06-08 by Kirill V. Lyadvinsky \n

* The contents of this file are subject to the terms of the Common Development and    \n
* Distribution License ("CDDL")(collectively, the "License"). You may not use this    \n
* file except in compliance with the License. You can obtain a copy of the CDDL at    \n
* http://www.opensource.org/licenses/cddl1.php.                                       \n

*/
#ifndef __SPLASHWND_H_
#define __SPLASHWND_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//! A class that implements the splash window in a separate thread.
class CSplashWnd
{
private:
	CSplashWnd(const CSplashWnd&) {};
	CSplashWnd& operator=(const CSplashWnd&) {};
	bool createWindowHelper (const int r, const int g, const int b);
protected:
	HANDLE							m_hThread;
	unsigned int				m_ThreadId;
	HANDLE							m_hEvent;

//	Gdiplus::Image*			m_pImage;			//!< Output image
	HBITMAP                 mBitmap;          //!< Output image
	HWND			 		m_hSplashWnd;		//!< Window. It is created in a separate thread.
	char *                	m_WindowName;		//!< The title of the window. It is displayed in the task bar. If empty, then there will not be a button in the task bar
	HWND					m_hProgressWnd;		//!< Progress bar. Displays at the bottom of the window. For more information, #SetProgress.
	HWND					m_hParentWnd;
	char *	        		m_ProgressMsg;		//!< Progress Message Cache.
    UINT_PTR                m_TimerId;          //!< Identifier of the created timer.

public:
	CSplashWnd( HWND hParent = NULL );
	~CSplashWnd();								//!< Causes #Hide.
    bool createSplashWindow (DWORD bitmapResourceID, const int r, const int g, const int b);

	void SetWindowName(const char * windowName);		//!< Specify the title of the window. For more information, #m_WindowName.
	void Show();								//!< Show the window. This function is called after all Set*
	void Hide();								//!< Closes the window and stops the flow.


	HWND GetWindowHwnd() const								//!< Returns the handle of the window
	{
		return m_hSplashWnd;
	};

protected:
	static LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static unsigned int __stdcall SplashThreadProc(void* lpParameter);
};

#endif//__SPLASHWND_H_
