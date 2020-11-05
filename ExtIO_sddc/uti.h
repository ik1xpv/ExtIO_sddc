#ifndef _UTIH32_
#define _UTIH32_

BOOL MakeWindowTransparent(HWND hWnd, unsigned char factor);
void SendF4();
bool GetStateButton(HWND hwnd, int controlID);
void Command(HWND hwnd, int controlID, int command);

#endif // _UTIH_
