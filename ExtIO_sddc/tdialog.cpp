#include "license.txt"

#include "tdialog.h"
#include <commctrl.h>
#include "bbrf103.h"

HWND hTabCtrlMain;
UINT nSel = 0; //selected tab

#define NTABS 4
char tabname[NTABS][20]= {" &Status  "," &BBRF103  "," &Test  "," &About  "};
void UpdateTab(HWND hWnd, UINT tabidx);

HBITMAP bitmap;

COLORREF clrBtnSel = RGB(24, 160, 244);
COLORREF clrBtnUnsel = RGB(0, 44, 107);
COLORREF clrBackground = RGB(15, 15, 15);
COLORREF clrTextL = RGB (128,128,128);
COLORREF clrTextW = RGB (255,255,255);
HBRUSH g_hbrBackground = CreateSolidBrush(clrBackground);
HBRUSH g_hbrBtnBackg = CreateSolidBrush(clrBtnUnsel);
HBRUSH g_hbrBtnSelBkg = CreateSolidBrush(clrBtnSel);


BOOL CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int i;
    HICON hIcon = NULL;
    char ebuffer[32];

    switch (uMsg)
    {

        case WM_DESTROY:
            DeleteObject(bitmap);
//            UnregisterHotKey(hWnd,IHK_CR);
            return TRUE;

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            return TRUE;

        case WM_PAINT:
            {
                BITMAP bm;
                PAINTSTRUCT ps;
                RECT rect;
                int dx,dy;
                HWND hwnd = GetDlgItem(hWnd, IDC_STATIC42); // item to draw
                GetWindowRect(hwnd,&rect);
                dx = rect.right-rect.left;
                dy = rect.bottom-rect.top;
                HDC hdc = BeginPaint(hwnd, &ps);
                HDC hdcMem = CreateCompatibleDC(hdc);
                HGDIOBJ hbmOld = SelectObject(hdcMem, bitmap);
                GetObject(bitmap, sizeof(bm), &bm);
                StretchBlt(hdc, 0, 0, dx, dy, hdcMem,0,0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                SelectObject(hdcMem, hbmOld);
                DeleteDC(hdcMem);
                EndPaint(hwnd, &ps);
            }
            break;

        case WM_INITDIALOG:
            {
         //       RegisterHotKey( hWnd, IHK_CR, 0, 0x0D); // no sound on CR
                HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
                bitmap = (HBITMAP)::LoadImage(hInstance, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, 0);

                hIcon = LoadIcon(hInstance, MAKEINTRESOURCE( IDB_ICON1 ) );
                if (hIcon)
                {
                    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                }

                TCITEM tcBtn;
                hTabCtrlMain=GetDlgItem(hWnd,IDT_TAB_CTRL_MAIN);
                memset(&tcBtn,0x0,sizeof(TCITEM));
                tcBtn.mask = TCIF_TEXT;
                for(i= 0; i < NTABS; i++)
                {
                    tcBtn.pszText = tabname[i];
                    SendMessage(hTabCtrlMain, TCM_INSERTITEM, i, (LPARAM)&tcBtn);
                }
                sprintf(ebuffer,"%d",Xfreq);
                SetWindowText(GetDlgItem(hWnd, IDC_EDIT33),ebuffer);
                for(i = 0; i < 5; i++)
                {
                    SendMessage(GetDlgItem(hWnd, IDC_CBMODE30),
                                CB_ADDSTRING,
                                0,
                                reinterpret_cast<LPARAM>((LPCTSTR)signal_mode[i]));
                }
                SendMessage(GetDlgItem(hWnd, IDC_CBMODE30), WM_SETTEXT, 0, (LPARAM)signal_mode[0]);

            //    ShowWindow(GetDlgItem(hWnd, IDC_TRACE_PAGE1),SW_HIDE);

                UpdateTab(hWnd, nSel); // Update tab area
            }
            return TRUE;

        case WM_CTLCOLORDLG:
             return (LONG)g_hbrBackground;

        case WM_CTLCOLORBTN: //In order to make those edges invisble when we use RoundRect(),
            {                //we make the color of our button's background match window's background
                return (LRESULT)GetSysColorBrush(COLOR_WINDOW+1);
            }
        break;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSTATIC:
            {
                HDC hDc = (HDC) wParam;
                SetTextColor(hDc, RGB(255, 255, 255));
                SetBkMode (hDc, TRANSPARENT);
                return (LONG)g_hbrBackground;
            }

        case WM_NOTIFY:
            {
                UINT uNotify=((LPNMHDR)lParam)->code;
                switch(uNotify){
                    case TCN_SELCHANGE:
                        nSel=SendMessage(hTabCtrlMain, TCM_GETCURSEL, 0, 0);
                        UpdateTab(hWnd, nSel);
                    case NM_CLICK:          // Fall through to the next case.
                    case NM_RETURN:
                        {
                            if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd,IDC_SYSLINK41))
                                {
                                    ShellExecute(NULL, "open", URL1, NULL, NULL, SW_SHOW);
                                }
                            if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd,IDC_SYSLINK43))
                                {
                                    ShellExecute(NULL, "open", URLBBRF103, NULL, NULL, SW_SHOW);
                                }
                            if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd,IDC_SYSLINK42))
                                {
                                    ShellExecute(NULL, "open", URL_HDSR, NULL, NULL, SW_SHOW);
                                }
                        }
                        break;

                }
            }
            return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_CBMODE30: // If the combo box sent the message,
                        switch(HIWORD(wParam)) // Find out what message it was
                        {
                        case CBN_DROPDOWN: // the list is about to display, stop
                            if (  pfnCallback ) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Stop);
                            break;
                        case CBN_SELCHANGE:  // the selection is done, start
                            inject_tone = (Inject_Signal)SendMessage(GetDlgItem(hWnd, IDC_CBMODE30), CB_GETCURSEL, 0, 0);
                            if (  pfnCallback ) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Start);
                            UpdateTab(hWnd, nSel); // Update tab area
                            break;
                        }
                        return TRUE;
                case IDC_EDIT33:
                     switch(HIWORD(wParam)) // Find out what message it was
                         {
                         case EN_UPDATE:    // process EDITTEXT change
                            GetWindowText( GetDlgItem(hWnd, IDC_EDIT33),ebuffer,16);
                            int f = atoi(ebuffer);
                            if (f > 32000)
                            {
                                f = 32000;
                                sprintf(ebuffer,"%d",f);
                                SetWindowText(GetDlgItem(hWnd, IDC_EDIT33),ebuffer);
                            }
                            Xfreq = f;
                            return TRUE;
                         }
                    break;
                case IDC_RADIO21:
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                             BBRF103.UptDither(!BBRF103.GetDither());
                        break;
                    }
                    break;
                case IDC_RADIO22:
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            BBRF103.UptRand(!BBRF103.GetRand());
                        break;
                    }
                    break;
                case IDC_TRACE_PAGE1: // trace
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            BBRF103.UptTrace(!BBRF103.GetTrace());
                        break;
                    }
                    break;
                case IDC_RADIO23:  // VLFMODE
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            BBRF103.UpdatemodeRF(VLFMODE);
                            InvalidateRect (hWnd, NULL, TRUE);
                            UpdateWindow (hWnd);
                            if (  pfnCallback ) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
                        break;
                    }
                    break;
                case IDC_RADIO24:
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            BBRF103.UpdatemodeRF(HFMODE);
                            InvalidateRect (hWnd, NULL, TRUE);
                            UpdateWindow (hWnd);
                            if (  pfnCallback )
                                EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
                        break;
                    }
                    break;
                case IDC_RADIO25:
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            BBRF103.UpdatemodeRF(VHFMODE);
                            InvalidateRect (hWnd, NULL, TRUE);
                            UpdateWindow (hWnd);
                            if (  pfnCallback ) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
                        break;
                    }
                    break;
            }
            break;
        case WM_DRAWITEM:
            {
                    WORD wID = (WORD)wParam;
                    const DRAWITEMSTRUCT& dis = *(DRAWITEMSTRUCT*)lParam;
                    bool ceck = false;
                    bool btn2redraw = false;
                    switch (wID)
                    {
                        case IDC_RADIO21:
                            ceck = BBRF103.GetDither();
                            btn2redraw = true;
                            break;
                        case IDC_RADIO22:
                            ceck = BBRF103.GetRand();
                            btn2redraw = true;
                            break;
                        case IDC_TRACE_PAGE1:
                            ceck = BBRF103.GetTrace();
                            btn2redraw = true;
                            break;
                        case IDC_RADIO23:
                            if (BBRF103.GetmodeRF() == VLFMODE) ceck = true;
                            btn2redraw = true;
                            break;
                        case IDC_RADIO24:
                            if (BBRF103.GetmodeRF() == HFMODE) ceck = true;
                            btn2redraw = true;
                            break;
                        case IDC_RADIO25:
                            if(BBRF103.GetmodeRF() == VHFMODE)ceck = true;
                            btn2redraw = true;
                            break;
                    }
                    if(btn2redraw)
                    {
                            RECT rect = dis.rcItem;
                            COLORREF bbkg;
                            char buffer [16];
                            int len=0;
                            if(ceck){
                                bbkg = clrBtnSel;
                                FillRect(dis.hDC,&dis.rcItem, g_hbrBtnSelBkg);
                                SetTextColor(dis.hDC,clrTextW);
                            }
                            else{
                                bbkg = clrBtnUnsel;
                                FillRect(dis.hDC,&dis.rcItem, g_hbrBtnBackg);
                                SetTextColor(dis.hDC,clrTextL);
                            }
                            SetBkColor(dis.hDC,bbkg);         // set text background to match button’s background.
                            len = GetWindowText(dis.hwndItem,buffer, 16 );
                            DrawText(dis.hDC,buffer,len, &rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE ) ;

                            return TRUE;

                    }

            }
            break;
/*
        case WM_HOTKEY:
            int idHotKey = (int) wParam;  // used to disable sound
            return TRUE;
*/
        }
    return FALSE;
}


void UpdateTab(HWND hWnd, UINT tabidx)
{
UINT i,j;
    if (tabidx >= NTABS) tabidx = NTABS-1;
    for (i =0; i < NTABS; i++)
    {
        if (i != tabidx)
        {
          switch (i)
          {
              case 0:
                for (j =IDC_PAGE0_INIT; j <=IDC_PAGE0_END; j++) ShowWindow(GetDlgItem(hWnd,j), SW_HIDE);
                break;
              case 1:
                for (j =IDC_PAGE1_INIT; j <=IDC_PAGE1_END; j++) ShowWindow(GetDlgItem(hWnd,j),SW_HIDE);
                break;
              case 2:
                for (j =IDC_PAGE2_INIT; j <=IDC_PAGE2_END; j++) ShowWindow(GetDlgItem(hWnd,j),SW_HIDE);
                break;
              case 3:
                for (j =IDC_PAGE3_INIT; j <=IDC_PAGE3_END; j++) ShowWindow(GetDlgItem(hWnd,j),SW_HIDE);
                break;
          }
        }
        else
        {
          switch (i)
            {
              case 0:
                for (j =IDC_PAGE0_INIT; j <=IDC_PAGE0_END; j++) ShowWindow(GetDlgItem(hWnd,j), SW_SHOW);
                break;
              case 1:
                for (j =IDC_PAGE1_INIT; j <=IDC_PAGE1_END; j++) ShowWindow(GetDlgItem(hWnd,j),SW_SHOW);
                break;
              case 2:
                for (j =IDC_PAGE2_INIT; j <=IDC_PAGE2_END; j++) ShowWindow(GetDlgItem(hWnd,j),SW_SHOW);
                switch(inject_tone)
                {
                    case ToneRF:
                        ShowWindow(GetDlgItem(hWnd,IDC_STATIC32),SW_SHOW);
                        SendMessage(GetDlgItem(hWnd,IDC_STATIC32), WM_SETTEXT, 0, (LPARAM) ("Frequency ~kHz"));
                        ShowWindow(GetDlgItem(hWnd,IDC_EDIT33  ),SW_SHOW);
                        break;
                    case SweepIF:
                        SendMessage(GetDlgItem(hWnd,IDC_STATIC32), WM_SETTEXT, 0, (LPARAM) ("Sweep speed"));
                        ShowWindow(GetDlgItem(hWnd,IDC_STATIC32),SW_SHOW);
                        ShowWindow(GetDlgItem(hWnd,IDC_EDIT33  ),SW_SHOW);
                        break;
                    default:
                        ShowWindow(GetDlgItem(hWnd,IDC_STATIC32),SW_HIDE);
                        ShowWindow(GetDlgItem(hWnd,IDC_EDIT33  ),SW_HIDE);
                        break;
                }
                break;
              case 3:
                for (j =IDC_PAGE3_INIT; j <=IDC_PAGE3_END; j++) ShowWindow(GetDlgItem(hWnd,j),SW_SHOW);
 //               SendMessage(GetDlgItem(hWnd, IDC_STATIC42), STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM) bitmap);
                break;
            }
        }
        #ifdef NDEBUG
         ShowWindow(GetDlgItem(hWnd, IDC_TRACE_PAGE1),SW_HIDE);    // hide trace button
        #endif
    }
}

