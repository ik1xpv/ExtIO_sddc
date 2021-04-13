#ifndef _UTIH32_
#define _UTIH32_

BOOL MakeWindowTransparent(HWND hWnd, unsigned char factor);
void SendF4();
bool GetStateButton(HWND hwnd, int controlID);
void Command(HWND hwnd, int controlID, int command);

/*
#define FOREGROUND_BLUE              1      // Text color contains blue.
#define FOREGROUND_GREEN             2      //  Text color contains green.
#define FOREGROUND_RED               4      //  Text color contains red.
#define FOREGROUND_INTENSITY         8      //  Text color is intensified.
#define BACKGROUND_BLUE             10      //  Background color contains blue.
#define BACKGROUND_GREEN            20      //  Background color contains green.
#define BACKGROUND_RED              40      //  Background color contains red.
#define BACKGROUND_INTENSITY        80      //  Background color is intensifie
*/

enum console_color {
	TXT_CYAN = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	TXT_GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	TXT_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	TXT_RED = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
};

void SetConsoleColorTXT(console_color c);

#endif
