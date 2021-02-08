#include "license.txt"
#include "framework.h"
#include "shellapi.h"
#include "tdialog.h"
#include <stdint.h>
#include <string.h>
#include "RadioHandler.h"
#include <commctrl.h>
#include "ExtIO_sddc.h"
#include "config.h"
#include "uti.h"
#include "LC_ExtIO_Types.h"

#include <windowsx.h>

HWND hTabCtrlMain;
UINT nSel = 0; //selected tab

HBITMAP bitmap;

COLORREF clrBtnSel = RGB(24, 160, 244);
COLORREF clrBtnUnsel = RGB(0, 44, 107);
COLORREF clrBackground = RGB(158, 188, 188);
HBRUSH g_hbrBackground = CreateSolidBrush(clrBackground);
int  _xfp = 1;
int  _xfm = 1;
unsigned int cntime = 0;

extern RadioHandlerClass RadioHandler;
extern "C" int SetOverclock(uint32_t adcfreq);

void UpdateGain(HWND hControl, int current, const float* gains, int length)
{
	char ebuffer[128];

	EnableWindow(hControl, length > 0);

	if (length > 0)
	{
		if (current >= length)
			current = length - 1;
		if (current < 0)
			current = 0;

		if (gains[current] >= 0)
			sprintf(ebuffer, "%+d", (int)(gains[current] + 0.5));
		else
			sprintf(ebuffer, "%+d", (int)(gains[current] - 0.5));
	}
	else
	{
		sprintf(ebuffer, "NA");
	}

	SetWindowText(hControl, ebuffer);
}

bool Support128M()
{
	auto model = RadioHandler.getModel();

	return model == RX888 ||
		model == RX888r2 ||
		model == RX999;
}

bool SupportPGA()
{
	auto model = RadioHandler.getModel();

	return model == RX888r2 ||
		model == RX999;
}

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

	case WM_INITDIALOG:
	{
		//       RegisterHotKey( hWnd, IHK_CR, 0, 0x0D); // no sound on CR
		HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);


		hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDB_ICON1));
		if (hIcon)
		{
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}

#ifndef _DEBUG
		ShowWindow(GetDlgItem(hWnd, IDC_ADCSAMPLES), FALSE);
#endif
		char lbuffer[64];
		sprintf(lbuffer, "%d", DEFAULT_ADC_FREQ);
		SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer);
		sprintf(lbuffer, "%3.2f", 0.0);
		SetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer);

		SetTimer(hWnd, 0, 200, NULL);
	}
	return TRUE;

	case WM_TIMER:
	{
		char lbuffer[64];
		if (cntime-- <= 0)
		{
			cntime = 5;
			sprintf(lbuffer, "%3.1fMsps",  RadioHandler.getSampleRate() / 1000000.0f);
			SetWindowText(GetDlgItem(hWnd, IDC_STATIC13), lbuffer);
			sprintf(lbuffer, "%3.1fMsps", RadioHandler.getBps());
			SetWindowText(GetDlgItem(hWnd, IDC_STATIC14), lbuffer);
			sprintf(lbuffer, "%3.1fMsps", RadioHandler.getSpsIF());
			SetWindowText(GetDlgItem(hWnd, IDC_STATIC16), lbuffer);
		}

		if (!Support128M()) {
			EnableWindow(GetDlgItem(hWnd, IDC_OVERCLOCK), FALSE);
		}

		if (!SupportPGA()) {
			EnableWindow(GetDlgItem(hWnd, IDC_PGA), FALSE);
		}

		if (GetStateButton(hWnd, IDC_IFGAINP))
		{
			Command(hWnd, IDC_IFGAINP, BN_CLICKED);
		}

		if (GetStateButton(hWnd, IDC_IFGAINM))
		{
			Command(hWnd, IDC_IFGAINM, BN_CLICKED);
		}

		if (GetStateButton(hWnd, IDC_RFGAINP))
		{
			Command(hWnd, IDC_RFGAINP, BN_CLICKED);
		}

		if (GetStateButton(hWnd, IDC_RFGAINM))
		{
			Command(hWnd, IDC_RFGAINM, BN_CLICKED);
		}

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

	case WM_USER + 1:
	{
		switch(wParam)
		{
			case extHw_READY:
				if (!bSupportDynamicSRate) {
					EnableWindow(GetDlgItem(hWnd, IDC_BANDWIDTH), TRUE);
					if (Support128M()) {
						EnableWindow(GetDlgItem(hWnd, IDC_OVERCLOCK), TRUE);
					}
				}
				break;
			case extHw_RUNNING:
				if (!bSupportDynamicSRate) {
					EnableWindow(GetDlgItem(hWnd, IDC_BANDWIDTH), FALSE);
					if (Support128M()) {
						EnableWindow(GetDlgItem(hWnd, IDC_OVERCLOCK), FALSE);
					}
				}
				break;

			case extHw_Changed_MGC:
			case extHw_Changed_ATT:
			case extHw_Changed_RF_IF:
				const float *gains;
				int length;
				length = RadioHandler.GetRFAttSteps(&gains);
				UpdateGain(GetDlgItem(hWnd, IDC_RFGAIN), GetActualAttIdx(), gains, length);

				length = RadioHandler.GetIFGainSteps(&gains);
				UpdateGain(GetDlgItem(hWnd, IDC_IFGAIN), ExtIoGetActualMgcIdx(), gains, length);
				break;

			case extHw_Changed_SRATES:
				double rate;
				ComboBox_ResetContent(GetDlgItem(hWnd, IDC_BANDWIDTH));
				for(int i=0; ; i++) {
					if (ExtIoGetSrates(i, &rate) == -1)
						break;
					sprintf(ebuffer, "%.0fM", rate/1000000);
					ComboBox_InsertString(GetDlgItem(hWnd, IDC_BANDWIDTH), i, ebuffer);
				}
				// fallthrough
			case extHw_Changed_SampleRate:
				int index = ExtIoGetActualSrateIdx();
				ExtIoGetSrates(index, &rate);
				sprintf(ebuffer, "%.0fM", rate/1000000);
				ComboBox_SelectItemData(GetDlgItem(hWnd, IDC_BANDWIDTH), -1, ebuffer);
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
				break;
		}
		break;
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
		case IDC_PGA:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				RadioHandler.UptPga(!RadioHandler.GetPga());
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
		case IDC_RFGAINP:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				const float *gains;
				int length;
				int index = GetActualAttIdx();
				length = RadioHandler.GetRFAttSteps(&gains);

				index += 1;

				if (index >= 0 && index < length)
					SetAttenuator(index);

				UpdateGain(GetDlgItem(hWnd, IDC_RFGAIN), GetActualAttIdx(), gains, length);
				break;
			}
			break;
		case IDC_RFGAINM:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				const float *gains;
				int length;
				int index = GetActualAttIdx();
				length = RadioHandler.GetRFAttSteps(&gains);

				index -= 1;
				if (index >= 0 && index < length)
					SetAttenuator(index);

				UpdateGain(GetDlgItem(hWnd, IDC_RFGAIN), GetActualAttIdx(), gains, length);
				break;
			}
			break;

		case IDC_IFGAINP:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				const float *gains;
				int length;
				int index = ExtIoGetActualMgcIdx();
				length = RadioHandler.GetIFGainSteps(&gains);

				index += 1;
				if (index >= 0 && index < length)
					ExtIoSetMGC(index);

				UpdateGain(GetDlgItem(hWnd, IDC_IFGAIN), ExtIoGetActualMgcIdx(), gains, length);
				break;
			}
			break;
		case IDC_IFGAINM:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				const float *gains;
				int length;
				int index = ExtIoGetActualMgcIdx();
				length = RadioHandler.GetIFGainSteps(&gains);

				index -= 1;
				if (index >= 0 && index < length)
					ExtIoSetMGC(index);

				UpdateGain(GetDlgItem(hWnd, IDC_IFGAIN), ExtIoGetActualMgcIdx(), gains, length);
				break;
			}
			break;
		case IDC_BANDWIDTH:
			switch(HIWORD(wParam))
			{
				case CBN_SELCHANGE:
				int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_BANDWIDTH));
				ExtIoSetSrate(index);
				break;
			}
		case IDC_OVERCLOCK: // ADC in stream screenshot
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if (Button_GetCheck(GetDlgItem(hWnd, IDC_OVERCLOCK)) == BST_CHECKED)
				{
					SetOverclock(DEFAULT_ADC_FREQ * 2);
				}
				else
				{
					SetOverclock(DEFAULT_ADC_FREQ);
				}
				break;
			}
			break;

		case IDC_ADCSAMPLES: // ADC in stream screenshot
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				saveADCsamplesflag = true;
				break;
			}
			break;

		case IDC_FREQUPDATE:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				char lbuffer[64];
				uint32_t adcfreq= 0;
				GetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer, sizeof(lbuffer));
				sscanf(lbuffer, "%d", &adcfreq);
				if ((adcfreq > (DEFAULT_ADC_FREQ * 1.1)) || (adcfreq < (DEFAULT_ADC_FREQ * 0.9)))
				{
					adcfreq = DEFAULT_ADC_FREQ;
				}
				sprintf(lbuffer, "%d", adcfreq);
				SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer);
				SetOverclock(adcfreq);
				break;
			}
			break;

		case IDC_CORRUPDATE:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				char lbuffer[64];
				float adjppm;
				float maxppm = 200.0;
				GetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer, sizeof(lbuffer));
				sscanf(lbuffer, "%f", &adjppm);
				if (adjppm > maxppm) adjppm =maxppm;
				if (adjppm < -maxppm) adjppm = -maxppm;
				sprintf(lbuffer, "%3.2f", adjppm);
				SetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer);
				//SetOverclock(adcfreq);
				break;
			}
			break;
		}
		break;
	break;



	}
	return FALSE;

}
