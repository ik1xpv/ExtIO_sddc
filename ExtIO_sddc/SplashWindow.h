#ifndef SPLASHWINDOW_H
#define SPLASHWINDOW_H

#include "framework.h"

//
// some idea and code from http://freesourcecode.net/cprojects/91599/Win32-splash-screen-in-c#.WmMV3KjiaUk
// downgrade and modification by ik1xpv 2018

class SplashWindow
{
public:
	SplashWindow ();
	~SplashWindow ();
	//Shows the splash screen forever till destroySplashWindow ()
    void showWindow ();
	bool createSplashWindow (HINSTANCE hinst, LPCSTR bitmapFileName, const int r, const int g, const int b);
	bool createSplashWindow (HINSTANCE hinst, DWORD bitmapResourceID, const int r, const int g, const int b);
	void destroySplashWindow ();

private:
 	bool isValidWindow () const;
	bool createWindowHelper (const int r, const int g, const int b);
	//sets all data members to 0.
	void clearMembers ();
	//window handle
	HWND mHWND;
	//Splash Window image information
	HBITMAP  mBitmap;
	int mBitmapWidth;
	int mBitmapHeight;

};


#endif
