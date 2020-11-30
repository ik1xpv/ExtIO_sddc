#include "license.txt"
#include "framework.h"
#include "shellapi.h"
#include "tdialog.h"
#include <stdint.h>
#include <string.h>
#include "RadioHandler.h"
#include "ExtIO_sddc.h"
#include "config.h"
#include "uti.h"

HWND hTabCtrlMain;
UINT nSel = 0; //selected tab

HBITMAP bitmap;

extern float	g_Bps;
extern float	g_SpsIF;
extern bool     LWMode;

COLORREF clrBtnSel = RGB(24, 160, 244);
COLORREF clrBtnUnsel = RGB(0, 44, 107);
COLORREF clrBackground = RGB(158, 188, 188);
HBRUSH g_hbrBackground = CreateSolidBrush(clrBackground);
float  _xfp = (float) 0.001;
float  _xfm = (float) 0.001;
unsigned int cntime = 0;

BOOL CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
		return FALSE;
	}
	break;

	case WM_INITDIALOG:
	{
		//       RegisterHotKey( hWnd, IHK_CR, 0, 0x0D); // no sound on CR
		HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);


		hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDB_ICON1));
		if (hIcon)
		{
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}

		SetFocus(GetDlgItem(hWnd, IDC_USBOUT));
		ShowWindow(GetDlgItem(hWnd, IDC_RESTART), SW_HIDE);

		sprintf(ebuffer, "%2d", (int) gdGainCorr_dB);
		SetWindowText(GetDlgItem(hWnd, IDC_GAINCORR), ebuffer);

		sprintf(ebuffer, "%3.3f", gdFreqCorr_ppm);
		SetWindowText(GetDlgItem(hWnd, IDC_FREQ), ebuffer);
		
		SetTimer(hWnd, 0, 100, NULL);

#ifndef TRACE
		ShowWindow(GetDlgItem(hWnd, IDC_TRACE),SW_HIDE);
#endif
	}
	return TRUE;

	
	case WM_TIMER:
	{
		char lbuffer[64];
		if (cntime-- <= 0)
		{
			cntime = 5;
			sprintf(lbuffer, "%2.9f Msps",  adcfixedfreq / 1000000);
			SetWindowText(GetDlgItem(hWnd, IDC_STATIC13), lbuffer);
			sprintf(lbuffer, "%6.3f Msps measured", g_Bps * adcfixedfreq / ((double)ADC_FREQ*2.0) );
			SetWindowText(GetDlgItem(hWnd, IDC_STATIC14), lbuffer);
			sprintf(lbuffer, "%6.3f Msps measured", g_SpsIF);
			SetWindowText(GetDlgItem(hWnd, IDC_STATIC16), lbuffer);
		}
		if (GetStateButton(hWnd, IDC_FREQP))
		{
			Command(hWnd, IDC_FREQP, BN_CLICKED);
			if (_xfp < 10.0) _xfp *= (float) 1.2;
		}
		else
			_xfp = (float) 0.001;

		if (GetStateButton(hWnd, IDC_FREQM))
		{
			Command(hWnd, IDC_FREQM, BN_CLICKED);
			if (_xfm < 10.0) _xfm *= (float) 1.2;
		}
		else
			_xfm = (float) 0.001;
		break;
	}

	case WM_CTLCOLORDLG:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC:
	{
		HDC hDc = (HDC)wParam;
		SetBkMode(hDc, TRANSPARENT);
		return (LONG)g_hbrBackground;
	}

	case WM_NOTIFY:
	{
		UINT uNotify = ((LPNMHDR)lParam)->code;
		switch (uNotify) {
		case NM_CLICK:          // Fall through to the next case.
		case NM_RETURN:
		{
			if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd, IDC_SYSLINK41))
			{
				ShellExecute(NULL, "open", URL1, NULL, NULL, SW_SHOW);
			}
			if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd, IDC_SYSLINK42))
			{
				ShellExecute(NULL, "open", URL_HDSR, NULL, NULL, SW_SHOW);
			}
		}
		break;
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_DITHER:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UptDither(!RadioHandler.GetDither());
				break;
			}
			break;
		case IDC_RAND:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UptRand(!RadioHandler.GetRand());
				break;
			}
			break;
		case IDC_BIAS_HF:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UpdBiasT_HF(!RadioHandler.GetBiasT_HF());
				break;
			}
			break;
		case IDC_BIAS_VHF:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UpdBiasT_VHF(!RadioHandler.GetBiasT_VHF());
				break;
			}
			break;
		case IDC_FINE_LO:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UpdFine_LO(!RadioHandler.GetFine_LO());
				break;
			}
			break;
		case IDC_GAINP:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				GetWindowText(GetDlgItem(hWnd, IDC_GAINCORR), ebuffer, 16);
				int x = 0;
				if (sscanf(ebuffer, "%d", &x) > 0)
				{
					x++;
					if (x > 20) x = 20;
					if (x < -20) x = -20;
				}
				sprintf(ebuffer, "%2d", x);
				SetWindowText(GetDlgItem(hWnd, IDC_GAINCORR), ebuffer);
				gdGainCorr_dB = (double)x;
				break;
			}
			break;
		case IDC_GAINM:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				GetWindowText(GetDlgItem(hWnd, IDC_GAINCORR), ebuffer, 16);
				int x = 0;
				if (sscanf(ebuffer, "%d", &x) > 0)
				{
					x--;
					if (x > 20) x = 20;
					if (x < -20) x = -20;
				}
				sprintf(ebuffer, "%2d", x);
				SetWindowText(GetDlgItem(hWnd, IDC_GAINCORR), ebuffer);
				gdGainCorr_dB = (double)x;
				break;
			}
			break;

		case IDC_FREQP:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				GetWindowText(GetDlgItem(hWnd, IDC_FREQ), ebuffer, 32);
				double x = 0;
				if (sscanf(ebuffer, "%lf", &x) > 0)
				{
					x += _xfp;
					if (x > 200.0) x = 200.0;
					if (x < -200.0) x = -200.0;
					sprintf( ebuffer, "%3.3f", x);
					SetWindowText(GetDlgItem(hWnd, IDC_FREQ), ebuffer);
					gdFreqCorr_ppm = x;
				}
	
				ShowWindow(GetDlgItem(hWnd, IDC_RESTART), SW_SHOW); // activate Restart
	
				break;
			}
			break;

		case IDC_FREQM:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				GetWindowText(GetDlgItem(hWnd, IDC_FREQ), ebuffer, 32);
				double x = 0;
				if (sscanf(ebuffer, "%lf", &x) > 0)
				{
					x -= _xfm;
					if (x > 200.0) x = 200.0;
					if (x < -200.0) x = -200.0;
					sprintf(ebuffer, "%3.3f", x);
					SetWindowText(GetDlgItem(hWnd, IDC_FREQ), ebuffer);
					gdFreqCorr_ppm = x;
				}
				
				ShowWindow(GetDlgItem(hWnd, IDC_RESTART), SW_SHOW); // activate Restart

				break;
			}
			break;

		case IDC_RESTART:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (radio == HF103)
					{
						ShowWindow(hWnd, SW_HIDE);
						EXTIO_STATUS_CHANGE(pfnCallback, extHw_Stop);
						EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);     // this is ok!!
						// EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES); // ? this does not work ?
						// patch to exit
						RadioHandler.Stop();
						ShellExecute(NULL, ("open"), ("StartHDSDR.CMD"), NULL, NULL, SW_SHOWNORMAL); // run restart cmd
						SendF4();
					}
				else
					{
						RadioHandler.Stop();
						RadioHandler.Start(ExtIoGetActualSrateIdx());
						EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_TUNE);  // to updt demodulators
					}
				break;
			}
			break;
#ifdef TRACE
		case IDC_TRACE: // trace
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UptTrace(!RadioHandler.GetTrace());
				break;
			}
			break;
#endif
		case IDC_ADCSAMPLES: // ADC in stream screenshot
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				saveADCsamplesflag = true;
				break;
			}
			break;

		}
		break;
	break;



	}
	return FALSE;

}


void UpdateDialogTitle(HWND hWnd, char dll_version[], char hardware_type[])
{
	char str[80];
	strcpy(str, dll_version);
	strcat(str, " | ");
	strcat(str, hardware_type);
	SetWindowTextA(hWnd, str);
}