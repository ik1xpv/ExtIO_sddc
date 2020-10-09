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

enum ButtonView { BUTTON_OFF = 0x0, BUTTON_ACTIVE = 0x1, BUTTON_INFO = 0x2 };
enum ButtonCeck { CECK_OFF = 0x0, CECK_ON = 0x1 };

#endif // _TABDIALOGH_
