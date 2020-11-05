/**************************************************************
 * MakeWindowTrasparent(window, factor)
 *
 * A function that will try to make a window transparent
 * (layered) under versions of Windows that support that kind
 * of thing. Gracefully fails on versions of Windows that
 * don't.
 *
 * Returns FALSE if the operation fails.
 */
#define WS_EX_LAYERED           0x00080000

#define LWA_COLORKEY            0x00000001
#define LWA_ALPHA               0x00000002
#include <windows.h>

typedef DWORD(WINAPI *PSLWA)(HWND, DWORD, BYTE, DWORD);

static PSLWA pSetLayeredWindowAttributes = NULL;
static BOOL initialized = FALSE;


BOOL MakeWindowTransparent(HWND hWnd, unsigned char factor)
{
	/* First, see if we can get the API call we need. If we've tried
	 * once, we don't need to try again. */
	if (!initialized)
	{
		HMODULE hDLL = LoadLibrary("user32");

		pSetLayeredWindowAttributes =
			(PSLWA)GetProcAddress(hDLL, "SetLayeredWindowAttributes");

		initialized = TRUE;
	}

	if (pSetLayeredWindowAttributes == NULL)
		return FALSE;

	/* Windows need to be layered to be made transparent. This is done
	 * by modifying the extended style bits to contain WS_EX_LAYARED. */
	SetLastError(0);

	SetWindowLong(hWnd,
		GWL_EXSTYLE,
		GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

	if (GetLastError())
		return FALSE;

	/* Now, we need to set the 'layered window attributes'. This
	 * is where the alpha values get set. */
	return pSetLayeredWindowAttributes(hWnd,
		RGB(255, 255, 255),
		factor,
		LWA_COLORKEY | LWA_ALPHA);
}

void SendF4()
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;

	// Press the "F4" key
	ip.ki.wVk = VK_F4;
	ip.ki.dwFlags = 0; // 0 for key press
	SendInput(1, &ip, sizeof(INPUT));

	Sleep(10);

	// Release the "F4" key
	ip.ki.wVk = VK_F4;
	ip.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &ip, sizeof(INPUT));
}

bool GetStateButton(HWND hWnd, int controlID)
{
	return  (((int)(DWORD)SendMessage(GetDlgItem(hWnd, controlID), BM_GETSTATE, 0, 0) & 4 ) != 0);
}


void Command(HWND hwnd, int controlID, int command)
{
	SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(controlID, command), (LPARAM)0);
}