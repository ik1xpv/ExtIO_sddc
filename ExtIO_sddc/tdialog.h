#ifndef _TABDIALOGH_
#define _TABDIALOGH_

#include "license.txt" 
#include "framework.h"
#include "resource.h"
#include "config.h"
#include <stdio.h>

extern bool bSupportDynamicSRate;
extern void UpdatePPM(HWND hWnd);

BOOL CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

INT_PTR CALLBACK DlgSelectDevice(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct DevContext 
{
	unsigned char numdev;
	char dev[MAXNDEV][MAXDEVSTRLEN];
};
#endif // _TABDIALOGH_
