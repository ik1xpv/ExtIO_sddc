#ifndef _TABDIALOGH_
#define _TABDIALOGH_

#include "license.txt" // Oscar Steila ik1xpv
#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "config.h"
#include "LC_ExtIO_Types.h"
#include <stdio.h>

extern Inject_Signal inject_tone;
extern int Xfreq;
extern pfnExtIOCallback	pfnCallback;

BOOL CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // _TABDIALOGH_
