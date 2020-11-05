#ifndef _TABDIALOGH_
#define _TABDIALOGH_

#include "license.txt" 
#include "framework.h"
#include <commctrl.h>
#include "resource.h"
#include "config.h"
#include "LC_ExtIO_Types.h"
#include <stdio.h>


extern int Xfreq;
extern pfnExtIOCallback	pfnCallback;

BOOL CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateDialogTitle(HWND hWnd, char dll_version[], char hardware_type[]);

#endif // _TABDIALOGH_
