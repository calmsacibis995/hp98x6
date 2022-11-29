/*
 *   HP98x6.c aka Chipmunk system
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
// Main file for HP98x6 emulator
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "resource.h"
#include "kml.h"
#include "debugger.h"

#define VERSION   "0.01"

//#define DEBUG_KEYBOARDW
#if defined DEBUG_KEYBOARDW
	TCHAR buffer[256];
	int k;
#endif


#if defined DEBUG_ONDISK
HANDLE hDebug;
#endif

HANDLE hHeap;								// private heap

#ifdef _DEBUG
LPTSTR szNoTitle = _T("HP98x6 ")_T(VERSION)_T(" Debug");
#else
LPTSTR szNoTitle = _T("HP98x6 ")_T(VERSION);
#endif
// LPTSTR szAppName = _T("HP98x6");				// application name for DDE server
// LPTSTR szTopic   = _T("Nothing");			// topic for DDE server
LPTSTR szTitle   = NULL;

static const LPCTSTR szLicence =
	_T("This program is free software; you can redistribute it and/or modify\r\n")
	_T("it under the terms of the GNU General Public License as published by\r\n")
	_T("the Free Software Foundation; either version 2 of the License, or\r\n")
	_T("(at your option) any later version.\r\n")
	_T("\r\n")
	_T("This program is distributed in the hope that it will be useful,\r\n")
	_T("but WITHOUT ANY WARRANTY; without even the implied warranty of\r\n")
	_T("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\r\n")
	_T("See the GNU General Public License for more details.\r\n")
	_T("\r\n")
	_T("You should have received a copy of the GNU General Public License\r\n")
	_T("along with this program; if not, write to the Free Software Foundation,\r\n")
    _T("Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA");

static LPCTSTR          *ppArgv;					// command line arguments
static INT              nArgc;						// no. of command line arguments
static DWORD            lThreadId;
static LARGE_INTEGER    lFreq;						// high performance counter frequency

HANDLE           hThread;
HANDLE           hEventShutdn;				// event handle to stop cpu thread

HINSTANCE        hApp = NULL;
HWND             hWnd = NULL;
HWND             hDlgDebug = NULL;			// handle for debugger dialog
HWND             hDlgFind = NULL;			// handle for debugger find dialog
HDC              hWindowDC = NULL;
HPALETTE         hPalette = NULL;
HPALETTE         hOldPalette = NULL;		// old palette of hWindowDC
BOOL             bAutoSave = FALSE;
BOOL             bAutoSaveOnExit = FALSE;
BOOL             bAlwaysDisplayLog = TRUE;

BOOL			bDebugOn = FALSE;

static int		 nDumpNr = 0;


//################
//#
//#    Window Status
//#
//################

VOID SetWindowTitle(LPTSTR szString)
{
	if (szTitle) HeapFree(hHeap, 0, szTitle);

	_ASSERT(hWnd != NULL);
	if (szString) {
		szTitle = DuplicateString(szString);
		SetWindowText(hWnd, szTitle);
	} else {
		szTitle = NULL;
		SetWindowText(hWnd, szNoTitle);
	}
}

VOID UpdateWindowStatus(VOID)
{
	if (hWnd) {								// window open
		BOOL bRun         = nState == SM_RUN;
		UINT uRun         = bRun                   ? MF_ENABLED : MF_GRAYED;
		HMENU hMenu = GetMenu(hWnd);		// get menu handle

		EnableMenuItem(hMenu, ID_FILE_NEW, MF_ENABLED);
		EnableMenuItem(hMenu, ID_FILE_OPEN, MF_ENABLED);
		EnableMenuItem(hMenu, ID_FILE_SAVE, (bRun && szCurrentFilename[0]) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(hMenu, ID_FILE_SAVEAS, uRun);
		EnableMenuItem(hMenu, ID_FILE_CLOSE, uRun);
	
		// internal drive
		switch (Chipset.type) {
			default:
				EnableMenuItem(hMenu, ID_INT0_LOAD, (Chipset.Hp9130.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT0_SAVE, (Chipset.Hp9130.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT0_EJECT, (Chipset.Hp9130.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_LOAD, (Chipset.Hp9130.disk[1] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_SAVE, (Chipset.Hp9130.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_EJECT, (Chipset.Hp9130.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
			case 37:
			case 16:
				EnableMenuItem(hMenu, ID_INT1_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_SAVE, MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_EJECT, MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_SAVE, MF_GRAYED);
				EnableMenuItem(hMenu, ID_INT1_EJECT, MF_GRAYED);
				break;
		}

		// HPIB 700 drives
		switch (Chipset.Hpib70x) {
			default: 
				EnableMenuItem(hMenu, ID_D700_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D700_SAVE, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D700_EJECT, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_SAVE, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_EJECT, MF_GRAYED);
				break;
			case 1:
			case 2:
			case 4:
				EnableMenuItem(hMenu, ID_D700_LOAD, (Chipset.Hp9121_0.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D700_SAVE, (Chipset.Hp9121_0.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D700_EJECT, (Chipset.Hp9121_0.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_LOAD, (Chipset.Hp9121_0.disk[1] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_SAVE, (Chipset.Hp9121_0.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_EJECT, (Chipset.Hp9121_0.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
			case 3:
				EnableMenuItem(hMenu, ID_D700_LOAD, (Chipset.Hp9122_0.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D700_SAVE, (Chipset.Hp9122_0.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D700_EJECT, (Chipset.Hp9122_0.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_LOAD, (Chipset.Hp9122_0.disk[1] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_SAVE, (Chipset.Hp9122_0.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D701_EJECT, (Chipset.Hp9122_0.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
		}
		// HPIB 702 drives
		switch (Chipset.Hpib72x) {
			default: 
				EnableMenuItem(hMenu, ID_D720_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D720_SAVE, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D720_EJECT, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_SAVE, MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_EJECT, MF_GRAYED);
				break;
			case 1:
			case 2:
			case 4:
				EnableMenuItem(hMenu, ID_D720_LOAD, (Chipset.Hp9121_1.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D720_SAVE, (Chipset.Hp9121_1.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D720_EJECT, (Chipset.Hp9121_1.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_LOAD, (Chipset.Hp9121_1.disk[1] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_SAVE, (Chipset.Hp9121_1.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_EJECT, (Chipset.Hp9121_1.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
			case 3:
				EnableMenuItem(hMenu, ID_D720_LOAD, (Chipset.Hp9122_1.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D720_SAVE, (Chipset.Hp9122_1.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D720_EJECT, (Chipset.Hp9122_1.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_LOAD, (Chipset.Hp9122_1.disk[1] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_SAVE, (Chipset.Hp9122_1.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_D721_EJECT, (Chipset.Hp9122_1.disk[1] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
		}

		// HPIB 703 drives
		switch (Chipset.Hpib73x) {
			default: 
				EnableMenuItem(hMenu, ID_H730_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_H730_EJECT, MF_GRAYED);
				break;
			case 1:
				EnableMenuItem(hMenu, ID_H730_LOAD, (Chipset.Hp7908_0.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_H730_EJECT, (Chipset.Hp7908_0.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
		}
		// HPIB 704 drives
		switch (Chipset.Hpib74x) {
			default: 
				EnableMenuItem(hMenu, ID_H740_LOAD, MF_GRAYED);
				EnableMenuItem(hMenu, ID_H740_EJECT, MF_GRAYED);
				break;
			case 1:
				EnableMenuItem(hMenu, ID_H740_LOAD, (Chipset.Hp7908_1.disk[0] == NULL) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(hMenu, ID_H740_EJECT, (Chipset.Hp7908_1.disk[0] != NULL) ? MF_ENABLED : MF_GRAYED);
				break;
		}

		EnableMenuItem(hMenu, ID_PASTE_STRING, 0);
		EnableMenuItem(hMenu, ID_COPY_STRING, 0);
		EnableMenuItem(hMenu, ID_COPY_SCREEN, 0);
		EnableMenuItem(hMenu, ID_VIEW_RESET, 0);
		EnableMenuItem(hMenu, ID_BACKUP_SAVE, 0);
		EnableMenuItem(hMenu, ID_BACKUP_RESTORE, 0);
		EnableMenuItem(hMenu, ID_BACKUP_DELETE, 0);
		EnableMenuItem(hMenu, ID_TOOL_DISASM, uRun);
		EnableMenuItem(hMenu, ID_TOOL_DEBUG, (bRun && nDbgState == DBG_OFF) ? MF_ENABLED : MF_GRAYED);
	}
}

//
// to copy text from windows (used only by the disassm part)
//
static VOID CopyItemsToClipboard(HWND hWnd)		// save selected Listbox Items to Clipboard
{
	LONG  i;
	LPINT lpnCount;

	// get number of selections
	if ((i = SendMessage(hWnd,LB_GETSELCOUNT,0,0)) == 0)
		return;								// no items selected

	if ((lpnCount = HeapAlloc(hHeap,0,i * sizeof(INT))) != NULL) {
		HANDLE hClipObj;
		LONG j,lMem = 0;

		// get indexes of selected items
		i = SendMessage(hWnd,LB_GETSELITEMS,i,(LPARAM) lpnCount);
		for (j = 0;j < i;++j) {				// scan all selected items
			// calculate total amount of needed memory
			lMem += SendMessage(hWnd,LB_GETTEXTLEN,lpnCount[j],0) + 2;
		}
		// allocate clipboard data
		if ((hClipObj = GlobalAlloc(GMEM_MOVEABLE,lMem + 1)) != NULL) {
			LPBYTE lpData;

			if ((lpData = GlobalLock(hClipObj))) {
				for (j = 0;j < i;++j) {		// scan all selected items
					#if defined _UNICODE
					{
						INT nLength = SendMessage(hWnd,LB_GETTEXTLEN,lpnCount[j],0) + 1;

						LPTSTR szTmp = HeapAlloc(hHeap,0,nLength * sizeof(szTmp[0]));
						if (szTmp != NULL)
						{
							SendMessage(hWnd,LB_GETTEXT,lpnCount[j],(LPARAM) szTmp);
							WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK,
												szTmp, nLength,
												lpData, nLength, NULL, NULL);
							HeapFree(hHeap,0,szTmp);
							lpData += nLength - 1;
						}
					}
					#else
					{
						lpData += SendMessage(hWnd,LB_GETTEXT,lpnCount[j],(LPARAM) lpData);
					}
					#endif
					*lpData++ = '\r';
					*lpData++ = '\n';
				}
				*lpData = 0;				// set end of string
				GlobalUnlock(hClipObj);		// unlock memory
			}

			if (OpenClipboard(hWnd)) {
				if (EmptyClipboard())
					SetClipboardData(CF_TEXT,hClipObj);
				else
					GlobalFree(hClipObj);
				CloseClipboard();
			} else {							// clipboard open failed
				GlobalFree(hClipObj);
			}
		}
		HeapFree(hHeap,0,lpnCount);			// free item table
	}
}

//################
//#
//#    Settings
//#
//################

//
// New 'system' settings
//
static BOOL CALLBACK SettingsProc(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	switch (message) {
		case WM_INITDIALOG:
			// init speed checkbox
			CheckDlgButton(hDlg, IDC_DATE, Chipset.keeptime);
			// CheckDlgButton(hDlg, IDC_SOUND, bSound);
			CheckDlgButton(hDlg, IDC_AUTOSAVE, bAutoSave);
			CheckDlgButton(hDlg, IDC_AUTOSAVEONEXIT, bAutoSaveOnExit);
			CheckDlgButton(hDlg, IDC_ALWAYSDISPLOG, bAlwaysDisplayLog);
			CheckDlgButton(hDlg, IDC_SETx1, (wRealSpeed == 1));
			CheckDlgButton(hDlg, IDC_SETx2, (wRealSpeed == 2));
			CheckDlgButton(hDlg, IDC_SETx3, (wRealSpeed == 3));
			CheckDlgButton(hDlg, IDC_SETx4, (wRealSpeed == 4));
			CheckDlgButton(hDlg, IDC_SETx8, (wRealSpeed == 5));
			CheckDlgButton(hDlg, IDC_SETxMAX, (wRealSpeed == 0));
			return TRUE;
		case WM_COMMAND:
			if (wParam == IDOK) {
				// get speed checkbox value
				Chipset.keeptime = IsDlgButtonChecked(hDlg, IDC_DATE);
				// bSound = IsDlgButtonChecked(hDlg, IDC_SOUND);
				wRealSpeed = IsDlgButtonChecked(hDlg, IDC_SETx1) ? 1 :
							 IsDlgButtonChecked(hDlg, IDC_SETx2) ? 2 :
							 IsDlgButtonChecked(hDlg, IDC_SETx3) ? 3 :
							 IsDlgButtonChecked(hDlg, IDC_SETx4) ? 4 :
							 IsDlgButtonChecked(hDlg, IDC_SETx8) ? 5 :
							 IsDlgButtonChecked(hDlg, IDC_SETxMAX) ? 0 : 0;
				bAutoSave = IsDlgButtonChecked(hDlg, IDC_AUTOSAVE);
				bAutoSaveOnExit = IsDlgButtonChecked(hDlg, IDC_AUTOSAVEONEXIT);
				bAlwaysDisplayLog = IsDlgButtonChecked(hDlg, IDC_ALWAYSDISPLOG);
				SetSpeed(wRealSpeed);			// set speed
	/*			CloseWaves();					// set sound
				if (bSound)	
					bSound = InitWaves(); */
				EndDialog(hDlg, wParam);
			}
			if (wParam == IDCANCEL)
			{
				EndDialog(hDlg, wParam);
			}
			break;
		} 
	return FALSE;
    UNREFERENCED_PARAMETER(lParam);
}

//################
//#
//#    Save Helper
//#
//################

static UINT SaveChanges(BOOL bAuto)
{
	UINT uReply;

	if (pbyRom == NULL) return IDNO;

	if (bAuto)
		uReply = IDYES;
	else
		uReply = YesNoCancelMessage(_T("Do you want to save changes ?"));

	if (uReply != IDYES) return uReply;

	if (szCurrentFilename[0]==0) { // Save As...
		uReply = GetSaveAsFilename();
		if (uReply != IDOK) return uReply;
		if (!SaveDocumentAs(szBufferFilename)) return IDCANCEL;
		WriteLastDocument(szCurrentFilename);
		return IDYES;
	}

	SaveDocument();
	return IDYES;
}

//################
//#
//#    Message Handlers
//#
//################

static LRESULT OnCreate(HWND hWindow)
{
	hWnd = hWindow;
	hWindowDC = GetDC(hWnd);
	DragAcceptFiles(hWnd,FALSE);				// no support for dropped files
	return 0;
}

static LRESULT OnDestroy(HWND hWindow)
{
	DragAcceptFiles(hWnd,FALSE);			// no WM_DROPFILES message any more
	SwitchToState(SM_RETURN);				// exit emulation thread
	SetWindowTitle(NULL);					// free memory of title
	if (hWindowDC) SelectPalette(hWindowDC, hOldPalette, FALSE);
	ReleaseDC(hWnd, hWindowDC);
	hWindowDC = NULL;						// hWindowDC isn't valid any more
	hWnd = NULL;
	PostQuitMessage(0);					// exit message loop
	return 0;
	UNREFERENCED_PARAMETER(hWindow);
}

static LRESULT OnPaint(HWND hWindow)
{
	PAINTSTRUCT Paint;
	HDC hPaintDC;

	hPaintDC = BeginPaint(hWindow, &Paint);
	if (hMainDC != NULL) {
		RECT rcMainPaint = Paint.rcPaint;
		rcMainPaint.left   += nBackgroundX;	// coordinates in source bitmap
		rcMainPaint.top    += nBackgroundY;
		rcMainPaint.right  += nBackgroundX;
		rcMainPaint.bottom += nBackgroundY;

		// redraw background bitmap
		BitBlt(hPaintDC, Paint.rcPaint.left, Paint.rcPaint.top,
			   Paint.rcPaint.right-Paint.rcPaint.left, Paint.rcPaint.bottom-Paint.rcPaint.top,
			   hMainDC, rcMainPaint.left, rcMainPaint.top, SRCCOPY);
		GdiFlush(); 
		// redraw main display area
		Refresh_Display(TRUE);
		hpib_names();
		UpdateAnnunciators(TRUE); 
		RefreshButtons(&rcMainPaint);
	} 
	EndPaint(hWindow, &Paint);
	return 0;
}

//
// ID_FILE_NEW
//
static LRESULT OnFileNew(VOID)
{
	SaveBackup();
	if (pbyRom) {
		SwitchToState(SM_INVALID);
		if (IDCANCEL == SaveChanges(bAutoSave))
			goto cancel;
	}
	if (NewDocument()) SetWindowTitle(_T("Untitled"));
	UpdateWindowStatus();
cancel:
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_FILE_OPEN
//
static LRESULT OnFileOpen(VOID)
{
	if (pbyRom) {
		SwitchToState(SM_INVALID);
		if (IDCANCEL == SaveChanges(bAutoSave))
			goto cancel;
	}
	if (GetOpenFilename()) {
		OpenDocument(szBufferFilename);
	}
cancel:
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_FILE_SAVE
//
static LRESULT OnFileSave(VOID)
{
	if (pbyRom == NULL) return 0;
	SwitchToState(SM_INVALID);
	SaveChanges(TRUE);
	SwitchToState(SM_RUN);
	return 0;
}

//
// ID_FILE_SAVEAS
//
static LRESULT OnFileSaveAs(VOID)
{
	UINT uReply;

	if (pbyRom == NULL) return 0;
	SwitchToState(SM_INVALID);

	uReply = GetSaveAsFilename();
	if (uReply != IDOK) {
		SwitchToState(SM_RUN);
		return 0;
	}
	if (!SaveDocumentAs(szBufferFilename)) {
		SwitchToState(SM_RUN);
		return 0;
	}
	WriteLastDocument(szCurrentFilename);

	SwitchToState(SM_RUN);
	return 0;}

//
// ID_FILE_CLOSE
//
static LRESULT OnFileClose(VOID)
{
	if (pbyRom == NULL) return 0;
	SwitchToState(SM_INVALID);
	if (SaveChanges(bAutoSave)!=IDCANCEL) {
		DisableDebugger();
		KillKML();
		ResetDocument();
		SetWindowTitle(NULL);
	} else {
		SwitchToState(SM_RUN);
	}
	return 0;
}

//
// ID_FILE_EXIT
//
// WM_SYS_CLOSE
//
static LRESULT OnFileExit(VOID)
{
	SwitchToState(SM_INVALID);				// hold emulation thread
	if (SaveChanges(bAutoSaveOnExit) == IDCANCEL)
	{
		SwitchToState(SM_RUN);				// on cancel restart emulation thread
		return 0;
	} 
	DestroyWindow(hWnd); 
	return 0;
}

//
// ID_COPY_STRING
//
static LRESULT OnCopyString(VOID)			// copy data from screen (ALPHA)
{
	return 0;
}

//
// ID_PASTE_STRING
//
static LRESULT OnPasteString(VOID)			// paste data to keyboard
{
	return 0;
}

//
// ID_COPY_SCREEN
//
static LRESULT OnCopyScreen(VOID)
{
	return 0;
}

//
// ID_VIEW_RESET
//
static LRESULT OnViewReset(VOID)
{
	if (nState != SM_RUN) return 0;
	if (YesNoMessage(_T("Are you sure you want to press the Reset Button ?"))==IDYES) {
		SwitchToState(SM_INVALID);
		SystemReset();							// Chipset setting after power cycle
		SwitchToState(SM_RUN);
	}
	return 0;
}

//
// ID_VIEW_SETTINGS
//
static LRESULT OnViewSettings(VOID)
{
	ReadSettings();		// read from registry

	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_SETTINGS), hWnd, (DLGPROC)SettingsProc) == -1)
		AbortMessage(_T("Settings Dialog Creation Error !"));

	WriteSettings();	// update to registry
	return 0;
}

// backup functions removed (can be added back if needed)

//
// ID_BACKUP_SAVE
//
static LRESULT OnBackupSave(VOID)
{
	return 0;
}

//
// ID_BACKUP_RESTORE
//
static LRESULT OnBackupRestore(VOID)
{
	return 0;
}

//
// ID_BACKUP_DELETE
//
static LRESULT OnBackupDelete(VOID)
{
	return 0;
}


//
// ID_9130_LOAD
//
static LRESULT On9130Load(BYTE byUnit)		// load an image to a disc unit
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to load a Disk."));
		goto cancel;
	}
	if (hp9130_widle())	{					// wait for idle hp9130 state
		InfoMessage(_T("The HP9130 is busy."));
		goto cancel;
	} 
	if (!GetLoadLifFilename())
		goto cancel;
	hp9130_load(byUnit, szBufferFilename);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9130_SAVE
//
static LRESULT On9130Save(BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to save a Disk."));
		goto cancel;
	}
	if (hp9130_widle()) {			// wait for idle hp9130 state
		InfoMessage(_T("The HP9130 is busy."));
		goto cancel;
	}
	_ASSERT(Chipset.Hp9130.name[byUnit][0] != 0x00);
	if (!GetSaveDiskFilename(Chipset.Hp9130.name[byUnit]))
		goto cancel;
	hp9130_save(byUnit, szBufferFilename);
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9130_EJECT
//
static LRESULT On9130Eject(BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to eject Disk."));
		goto cancel;
	}
	if (hp9130_widle())	{			// wait for idle hp9130 state
		InfoMessage(_T("The HP9130 is busy."));
		goto cancel;
	}
	hp9130_eject(byUnit);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return (0);
}

//
// ID_9122_LOAD
//
static LRESULT On9122Load(HPSS80 *ctrl, BYTE byUnit)		// load an image to a disc unit
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to load a Disk."));
		goto cancel;
	}
	if (hp9122_widle(ctrl)) {						// wait for idle hp9122 state
		InfoMessage(_T("The HP9122 is busy."));
		goto cancel;
	} 
	if (!GetLoadLifFilename())
		goto cancel;
	hp9122_load(ctrl, byUnit, szBufferFilename);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9122_SAVE
//
static LRESULT On9122Save(HPSS80 *ctrl, BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to save a Disk."));
		goto cancel;
	}
	if (hp9122_widle(ctrl))	{			// wait for idle hp9122 state
		InfoMessage(_T("The HP9122 is busy."));
		goto cancel;
	}
	_ASSERT(ctrl->name[byUnit][0] != 0x00);
	if (!GetSaveDiskFilename(ctrl->name[byUnit]))
		goto cancel;
	hp9122_save(ctrl, byUnit, szBufferFilename);
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9122_EJECT
//
static LRESULT On9122Eject(HPSS80 *ctrl, BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to eject a Disk."));
		goto cancel;
	}
	if (hp9122_widle(ctrl))	{			// wait for idle hp9122 state
		InfoMessage(_T("The HP9122 is busy."));
		goto cancel;
	}
	hp9122_eject(ctrl, byUnit);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return (0);
}

//
// ID_9121_LOAD
//
static LRESULT On9121Load(HP9121 *ctrl, BYTE byUnit)		// load an image to a disc unit
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to load a Disk."));
		goto cancel;
	}
	if (hp9121_widle(ctrl))	{					// wait for idle hp9121 state
		InfoMessage(_T("The HP9121 is busy."));
		goto cancel;
	} 
	if (!GetLoadLifFilename())
		goto cancel;
	hp9121_load(ctrl, byUnit, szBufferFilename);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9121_SAVE
//
static LRESULT On9121Save(HP9121 *ctrl, BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to save a Disk."));
		goto cancel;
	}
	if (hp9121_widle(ctrl))	{			// wait for idle hp9122 state
		InfoMessage(_T("The HP9121 is busy."));
		goto cancel;
	}
	_ASSERT(ctrl->name[byUnit][0] != 0x00);
	if (!GetSaveDiskFilename(ctrl->name[byUnit]))
		goto cancel;
	hp9121_save(ctrl, byUnit, szBufferFilename);
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9121_EJECT
//
static LRESULT On9121Eject(HP9121 *ctrl, BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to eject a Disk."));
		goto cancel;
	}
	if (hp9121_widle(ctrl))	{			// wait for idle hp9121 state
		InfoMessage(_T("The HP9121 is busy."));
		goto cancel;
	}
	hp9121_eject(ctrl, byUnit);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return (0);
}

//
// ID_7908_LOAD
//
static LRESULT On7908Load(HPSS80 *ctrl, BYTE byUnit)		// load an image to a disc unit
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to load a Disk."));
		goto cancel;
	}
	if (hp7908_widle(ctrl))	{					// wait for idle hp7908 state
		InfoMessage(_T("The HP7908 is busy."));
		goto cancel;
	} 
	if (!GetLoadLifFilename())
		goto cancel;
	hp7908_load(ctrl, byUnit, szBufferFilename);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_9122_SAVE
//
static LRESULT On7908Save(HPSS80 *ctrl, BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to save a Disk."));
		goto cancel;
	}
	if (hp7908_widle(ctrl))	{			// wait for idle hp7908 state
		InfoMessage(_T("The HP7908 is busy."));
		goto cancel;
	}
	_ASSERT(ctrl->name[byUnit][0] != 0x00);
	if (!GetSaveDiskFilename(ctrl->name[byUnit]))
		goto cancel;
	hp7908_save(ctrl, byUnit, szBufferFilename);
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return 0;
}

//
// ID_7908_EJECT
//
static LRESULT On7908Eject(HPSS80 *ctrl, BYTE byUnit)
{
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (nState != SM_RUN) {
		InfoMessage(_T("The emulator must be running to eject a Disk."));
		goto cancel;
	}
	if (hp9122_widle(ctrl))	{			// wait for idle hp7908 state
		InfoMessage(_T("The HP7908 is busy."));
		goto cancel;
	}
	hp7908_eject(ctrl, byUnit);
	UpdateWindowStatus();
cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	return (0);
}

//
// dispatch for HPIB 700 load
//
static LRESULT OnD70xLoad(BYTE byUnit)
{
	switch (Chipset.Hpib70x) {
		default:
			break;
		case 1:
		case 2:
		case 4:
			return On9121Load(&Chipset.Hp9121_0, byUnit);
			break;
		case 3:
			return On9122Load(&Chipset.Hp9122_0, byUnit);
	}
	return 0;
}

//
// dispatch for HPIB 700 save
//
static LRESULT OnD70xSave(BYTE byUnit)
{
	switch (Chipset.Hpib70x) {
		default:
			break;
		case 1:
		case 2:
		case 4:
			return On9121Save(&Chipset.Hp9121_0, byUnit);
			break;
		case 3:
			return On9122Save(&Chipset.Hp9122_0, byUnit);
	}
	return 0;
}

//
// dispatch for HPIB 700 eject
//
static LRESULT OnD70xEject(BYTE byUnit)
{
	switch (Chipset.Hpib70x) {
		default:
			break;
		case 1:
		case 2:
		case 4:
			return On9121Eject(&Chipset.Hp9121_0, byUnit);
			break;
		case 3:
			return On9122Eject(&Chipset.Hp9122_0, byUnit);
	}
	return 0;
}

//
// dispatch for HPIB 702 load
//
static LRESULT OnD72xLoad(BYTE byUnit)
{
	switch (Chipset.Hpib72x) {
		default:
			break;
		case 1:
		case 2:
		case 4:
			return On9121Load(&Chipset.Hp9121_1, byUnit);
			break;
		case 3:
			return On9122Load(&Chipset.Hp9122_1, byUnit);
	}
	return 0;
}

//
// dispatch for HPIB 702 save
//
static LRESULT OnD72xSave(BYTE byUnit)
{
	switch (Chipset.Hpib72x) {
		default:
			break;
		case 1:
		case 2:
		case 4:
			return On9121Save(&Chipset.Hp9121_1, byUnit);
			break;
		case 3:
			return On9122Save(&Chipset.Hp9122_1, byUnit);
	}
	return 0;
}

//
// dispatch for HPIB 702 eject
//
static LRESULT OnD72xEject(BYTE byUnit)
{
	switch (Chipset.Hpib72x) {
		default:
			break;
		case 1:
		case 2:
		case 4:
			return On9121Eject(&Chipset.Hp9121_1, byUnit);
			break;
		case 3:
			return On9122Eject(&Chipset.Hp9122_1, byUnit);
	}
	return 0;
}

//
// dispatch for HPIB 703 load
//
static LRESULT OnH73xLoad(BYTE byUnit)
{
	switch (Chipset.Hpib73x) {
		default:
			break;
		case 1:
			return On7908Load(&Chipset.Hp7908_0, byUnit);
			break;
	}
	return 0;
}

//
// dispatch for HPIB 703 eject
//
static LRESULT OnH73xEject(BYTE byUnit)
{
	switch (Chipset.Hpib73x) {
		default:
			break;
		case 1:
			return On7908Eject(&Chipset.Hp7908_0, byUnit);
			break;
	}
	return 0;
}

//
// dispatch for HPIB 704 load
//
static LRESULT OnH74xLoad(BYTE byUnit)
{
	switch (Chipset.Hpib74x) {
		default:
			break;
		case 1:
			return On7908Load(&Chipset.Hp7908_1, byUnit);
			break;
	}
	return 0;
}

//
// dispatch for HPIB 704 eject
//
static LRESULT OnH74xEject(BYTE byUnit)
{
	switch (Chipset.Hpib74x) {
		default:
			break;
		case 1:
			return On7908Eject(&Chipset.Hp7908_1, byUnit);
			break;
	}
	return 0;
}

static BOOL CALLBACK About(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hDlg,IDC_VERSION,szNoTitle);
		SetDlgItemText(hDlg,IDC_LICENSE,szLicence);
		return TRUE;
	case WM_COMMAND:
		wParam = LOWORD(wParam);
		if ((wParam==IDOK)||(wParam==IDCANCEL))
		{
			EndDialog(hDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
    UNREFERENCED_PARAMETER(lParam);
}

static BOOL CALLBACK Disasm(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	static DWORD dwAddress;
	LONG  i;
	TCHAR *cpStop, szAddress[256] = _T("0");

	switch (message)
	{
	case WM_INITDIALOG:
		// set fonts & cursor
		SendDlgItemMessage(hDlg,IDC_ADDRESS,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_ADR,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_NEXT,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_COPY,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDCANCEL,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SetDlgItemText(hDlg,IDC_DISASM_ADR,szAddress);
		dwAddress = (DWORD)(_tcstoul(szAddress,&cpStop,32));	// only 32 bits of address
		return TRUE;
	case WM_COMMAND:
		switch(wParam)
		{
		case IDOK:
			SendDlgItemMessage(hDlg, IDC_DISASM_ADR, EM_SETSEL, 0, -1);
			GetDlgItemText(hDlg, IDC_DISASM_ADR, szAddress, ARRAYSIZEOF(szAddress));
			// test if valid hex address
			for (i = 0; i < (LONG) lstrlen(szAddress); ++i)
			{
				if (_istxdigit(szAddress[i]) == FALSE)
					return FALSE;
			}
			dwAddress = (DWORD)(_tcstoul(szAddress,&cpStop,32));
			// no break
		case IDC_DISASM_NEXT:
			i = wsprintf(szAddress,_T("%06X   "), dwAddress);
			dwAddress = disassemble(dwAddress, &szAddress[i], VIEW_LONG);
			i = SendDlgItemMessage(hDlg, IDC_DISASM_WIN, LB_ADDSTRING, 0, (LPARAM) szAddress);
			SendDlgItemMessage(hDlg, IDC_DISASM_WIN, LB_SELITEMRANGE, FALSE, MAKELPARAM(0,i));
			SendDlgItemMessage(hDlg, IDC_DISASM_WIN, LB_SETSEL, TRUE, i);
			SendDlgItemMessage(hDlg, IDC_DISASM_WIN, LB_SETTOPINDEX, i, 0);
			return TRUE;
		case IDC_DISASM_COPY:
			// copy selected items to clipboard
			CopyItemsToClipboard(GetDlgItem(hDlg, IDC_DISASM_WIN));
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, wParam);
			return TRUE;
		}
		break;
	} 
	return FALSE;
    UNREFERENCED_PARAMETER(lParam);
}

//
// ID_TOOL_DISASM
//
static LRESULT OnToolDisasm(VOID)			// disasm dialogbox call
{
//	if (pbyRom) SwitchToState(SM_SLEEP);
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_DISASM), hWnd, (DLGPROC)Disasm) == -1)
		AbortMessage(_T("Disassembler Dialog Box Creation Error !"));
//	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_TOOL_DUMPRAM
//
static LRESULT OnToolDumpRam(VOID)			// disasm dialogbox call
{
/*	HANDLE  hDumpFile = NULL;
	DWORD dwWritten;
	TCHAR  szDumpFilename[MAX_PATH];
	int i;

	i = wsprintf(szDumpFilename,_T("dump_ram_%04d.bin"),nDumpNr);
	szDumpFilename[i++] = 0x00;
	nDumpNr++;

	hDumpFile = CreateFile(szDumpFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	if (hDumpFile == INVALID_HANDLE_VALUE)		// error, couldn't create a new file
	{
		InfoMessage(_T("Can't create the file."));
		return 0;
	}

	if (Chipset.Ram) WriteFile(hDumpFile, Chipset.Ram,  Chipset.RamSize,  &dwWritten, NULL);
	CloseHandle(hDumpFile);
*/
	return 0;
}

//
// ID_ABOUT
//
static LRESULT OnAbout(VOID)
{
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_ABOUT), hWnd, (DLGPROC)About) == -1)
		AbortMessage(_T("About Dialog Box Creation Error !"));
	return 0;
}

//
// Mouse wheel handler
//
static LRESULT OnMouseWheel(UINT nFlags, WORD x, WORD y)
{
	#ifdef DEBUG_KEYBOARDW
		k = wsprintf(buffer,_T("      : Wheel : wP %08X lP %04X%04X\n"), nFlags, x, y);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif
	KnobRotate(4 * GET_WHEEL_DELTA_WPARAM(nFlags)/WHEEL_DELTA); // *4 to speed up things
	return 0;
}

// 
// Keyboard handler
//
static LRESULT OnLButtonDown(UINT nFlags, WORD x, WORD y)
{
 	if (nState == SM_RUN) MouseButtonDownAt(nFlags, x,y); 
	return 0;
}

static LRESULT OnLButtonUp(UINT nFlags, WORD x, WORD y)
{
 	if (nState == SM_RUN) MouseButtonUpAt(nFlags, x,y); 
	return 0;
}

static LRESULT OnKeyDown(int wParam, DWORD lParam)			// for special key down
{
	BYTE send;
	int vk = MapVirtualKey((lParam >> 16) & 0xFF, 3);

	#ifdef DEBUG_KEYBOARDW
		k = wsprintf(buffer,_T("      : Down : vk %04X wP %08X lP %08X\n"), vk, wParam, lParam);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif

	if ((nState == SM_RUN) && ((lParam & 0x40000000) == 0)) {		// first time down (for autorepeat)
		if (((wParam > 0x5F) && (wParam < 0x70)) || (wParam == 0x13))
			vk = wParam;
		else 
			vk = MapVirtualKey((lParam >> 16) & 0xFF, 3);
		send = TRUE;
		switch ((BYTE) vk) {
			case 0xA0:
			case 0xA1:
				Chipset.Keyboard.shift = 1;		// pressed
				send = FALSE;
				break;
			case 0xA2:					// left ctrl
			case 0xA3:					// right ctrl
				send = FALSE;
				if (!(lParam & 0x200000000))	{
					Chipset.Keyboard.ctrl = 1;
				}
				break;
			case 0xA4:					// alt
				send = FALSE;
				if (lParam & 0x01000000) {
					Chipset.Keyboard.altgr = 1;
					Chipset.Keyboard.ctrl = 0;
				} else {
					Chipset.Keyboard.alt = 1;
				}
				break;
		}
		if (send) {
			KeyboardEventDown((BYTE) vk); // down
		} 
	}

	return 0;
}

static LRESULT OnKeyUp(int wParam, DWORD lParam)			// for all key up
{
	BYTE send;
	int vk; // = MapVirtualKey((lParam >> 16) & 0xFF, 3);

	#ifdef DEBUG_KEYBOARDW
		k = wsprintf(buffer,_T("      : Up : vk %04X wP %08X lP %08X\n"), vk, wParam, lParam);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif

	if (nState == SM_RUN) {	
		if (((wParam > 0x5F) && (wParam < 0x70)) || (wParam == 0x13))
			vk = wParam;
		else 
			vk = MapVirtualKey((lParam >> 16) & 0xFF, 3);
		send = TRUE;
		switch ((BYTE) vk) {
			case 0xA0:
			case 0xA1:
				send = FALSE;
				Chipset.Keyboard.shift = 0;					// released
				break;
			case 0xA2:					// left ctrl
			case 0xA3:					// right ctrl
				send = FALSE;	
				if (!(lParam & 0x200000000)) {
					Chipset.Keyboard.ctrl = 0;
				}
				break;
			case 0xA4:					// alt
				send = FALSE;	
				if (lParam & 0x01000000) {
					Chipset.Keyboard.altgr = 0;
					Chipset.Keyboard.ctrl = 0;
				} else {
					Chipset.Keyboard.alt = 0;
				}
			break;
		}
		if (send) {
			KeyboardEventUp((BYTE) vk); //, kbshift, kctrl, kbaltgr);		// up
		} 
	}

	return 0;
}

//
// kml buttons handler
//
VOID ButtonEvent(BOOL state, UINT nId)
{
	if (state = FALSE)
		return;
	switch (nId)
	{
		case 1:													// RESET	
			OnViewReset();
			break;
		case 2:													// Internal floppy
		case 3:
			if ((Chipset.type != 16) && (Chipset.type != 37)) {
				if (Chipset.Hp9130.name[nId - 2][0] != 0x00)
					On9130Eject((BYTE)(nId - 2));
				else
					On9130Load((BYTE)(nId - 2));
			}
			break;
		case 4:													// External floppy
		case 5:
			switch (Chipset.Hpib70x) {
				case 1:
				case 2:
				case 4:
					if (Chipset.Hp9121_0.name[nId - 4][0] != 0x00)
						On9121Eject(&Chipset.Hp9121_0, (BYTE)(nId - 4));
					else
						On9121Load(&Chipset.Hp9121_0, (BYTE)(nId - 4));
					break;
				case 3:
					if (Chipset.Hp9122_0.name[nId - 4][0] != 0x00)
						On9122Eject(&Chipset.Hp9122_0, (BYTE)(nId - 4));
					else
						On9122Load(&Chipset.Hp9122_0, (BYTE)(nId - 4));
					break;
			}
			break;
		case 6:													// External floppy
		case 7:
			switch (Chipset.Hpib72x) {
				case 1:
				case 2:
				case 4:
					if (Chipset.Hp9121_1.name[nId - 6][0] != 0x00)
						On9121Eject(&Chipset.Hp9121_1, (BYTE)(nId - 6));
					else
						On9121Load(&Chipset.Hp9121_1, (BYTE)(nId - 6));
					break;
				case 3:
					if (Chipset.Hp9122_1.name[nId - 6][0] != 0x00)
						On9122Eject(&Chipset.Hp9122_1, (BYTE)(nId - 6));
					else
						On9122Load(&Chipset.Hp9122_1, (BYTE)(nId - 6));
					break;
			}
			break;
		case 8:													// External hard disk
			switch (Chipset.Hpib73x) {
				case 1:
					if (Chipset.Hp7908_0.name[nId - 8][0] != 0x00)
						On7908Eject(&Chipset.Hp7908_0, (BYTE)(nId - 8));
					else
						On7908Load(&Chipset.Hp7908_0, (BYTE)(nId - 8));
					break;
			}
			break;
		case 9:													// External hard disk
			switch (Chipset.Hpib74x) {
				case 1:
					if (Chipset.Hp7908_1.name[nId - 9][0] != 0x00)
						On7908Eject(&Chipset.Hp7908_1, (BYTE)(nId - 9));
					else
						On7908Load(&Chipset.Hp7908_1, (BYTE)(nId - 9));
					break;
			}
			break;
		case 10:
		case 11:
			break;
	} 
}

//
//	Debug on handler (used generally in #define #endif to activate debug infos at will)
//
static BOOL OnToolDebugOn(HWND hDlg)
{
	bDebugOn = !bDebugOn;
	CheckMenuItem(GetMenu(hDlg), ID_TOOL_DEBUGON, bDebugOn ? MF_CHECKED : MF_UNCHECKED);	
	return 0;
}

//
// Main window handler
//
LRESULT CALLBACK MainWndProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:          return OnCreate(hWindow);
	case WM_DESTROY:         return OnDestroy(hWindow);
	case WM_PAINT:           return OnPaint(hWindow);
	case WM_ACTIVATE:
		if (LOWORD(wParam)==WA_INACTIVE) break;
	case WM_QUERYNEWPALETTE:
		if (hPalette)
		{
			SelectPalette(hWindowDC, hPalette, FALSE);
			if (RealizePalette(hWindowDC))
			{
				InvalidateRect(hWindow, NULL, TRUE);
				return TRUE;
			}
		}
		return FALSE;
	case WM_PALETTECHANGED:
		if ((HWND)wParam == hWindow) break;
		if (hPalette)
		{
			SelectPalette(hWindowDC, hPalette, FALSE);
			if (RealizePalette(hWindowDC))
			{
				// UpdateColors(hWindowDC);
				InvalidateRect (hWnd, (LPRECT) (NULL), 1);
			}
		}
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_NEW:		return OnFileNew();
		case ID_FILE_OPEN:		return OnFileOpen();
		case ID_FILE_SAVE:		return OnFileSave();
		case ID_FILE_SAVEAS:	return OnFileSaveAs();
		case ID_FILE_CLOSE:		return OnFileClose();
		case ID_FILE_EXIT:		return OnFileExit();
//		case ID_COPY_STRING:	return OnCopyString();
//		case ID_PASTE_STRING:	return OnPasteString();
//		case ID_COPY_SCREEN:	return OnCopyScreen();
		case ID_VIEW_RESET:		return OnViewReset();
		case ID_VIEW_SETTINGS:	return OnViewSettings();
//		case ID_BACKUP_SAVE:	return OnBackupSave();
//		case ID_BACKUP_RESTORE:	return OnBackupRestore();
//		case ID_BACKUP_DELETE:	return OnBackupDelete();
//		case ID_LIF_LOAD:		return OnLifLoad();
//		case ID_LIF_SAVE:		return OnLifSave();
//		case ID_DISK_LOAD:		return OnDiskLoad();
//		case ID_DISK_SAVE:		return OnDiskSave();

		case ID_INT0_LOAD:		return On9130Load(0);
		case ID_INT0_SAVE:		return On9130Save(0);
		case ID_INT0_EJECT:		return On9130Eject(0);
		case ID_INT1_LOAD:		return On9130Load(1);
		case ID_INT1_SAVE:		return On9130Save(1);
		case ID_INT1_EJECT:		return On9130Eject(1);
		case ID_D700_LOAD:		return OnD70xLoad(0);
		case ID_D700_SAVE:		return OnD70xSave(0);
		case ID_D700_EJECT:		return OnD70xEject(0);
		case ID_D701_LOAD:		return OnD70xLoad(1);
		case ID_D701_SAVE:		return OnD70xSave(1);
		case ID_D701_EJECT:		return OnD70xEject(1);

		case ID_D720_LOAD:		return OnD72xLoad(0);
		case ID_D720_SAVE:		return OnD72xSave(0);
		case ID_D720_EJECT:		return OnD72xEject(0);
		case ID_D721_LOAD:		return OnD72xLoad(1);
		case ID_D721_SAVE:		return OnD72xSave(1);
		case ID_D721_EJECT:		return OnD72xEject(1);

		case ID_H730_LOAD:		return OnH73xLoad(0);
		case ID_H730_EJECT:		return OnH73xEject(0);
		case ID_H740_LOAD:		return OnH74xLoad(0);
		case ID_H740_EJECT:		return OnH74xEject(0);

		case ID_TOOL_DISASM:	return OnToolDisasm();
		case ID_TOOL_DEBUG:		return OnToolDebug();
		case ID_TOOL_DEBUGON:	return OnToolDebugOn(hWindow);
//		case ID_TOOL_DUMPRAM:	return OnToolDumpRam();
		case ID_ABOUT:			return OnAbout();
		}
		break;
	case WM_SYSCOMMAND:
		switch (LOWORD(wParam))
		{
		case SC_CLOSE: return OnFileExit();
		}
		break;
	case WM_MOUSEWHEEL:	 return OnMouseWheel(wParam, LOWORD(lParam), HIWORD(lParam));
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN: return OnLButtonDown(wParam, LOWORD(lParam), HIWORD(lParam));
	case WM_LBUTTONUP:   return OnLButtonUp(wParam, LOWORD(lParam), HIWORD(lParam));
	case WM_KEYUP:       return OnKeyUp((int)wParam, lParam);
	case WM_KEYDOWN:     return OnKeyDown((int)wParam, lParam);
	case WM_SYSKEYUP:    return OnKeyUp((int)wParam, lParam);
	case WM_SYSKEYDOWN:  return OnKeyDown((int)wParam, lParam);
	}
	return DefWindowProc(hWindow, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	WNDCLASS wc;
	RECT rectWindow;
	DWORD dwAffMask;

		// clean pointers

	hAlpha1DC = NULL;
	hAlpha2DC = NULL;
	hGraphDC = NULL;
	hFontDC = NULL;
	hScreenDC = NULL;

	hApp = hInst;
	#if defined _UNICODE
	{
		ppArgv = CommandLineToArgvW(GetCommandLine(),&nArgc);
	}
	#else
	{
		nArgc = __argc;						// no. of command line arguments
		ppArgv = (LPCTSTR*) __argv;			// command line arguments
	}
	#endif

	if(!QueryPerformanceFrequency(&lFreq)) {	// init high resolution counter
		AbortMessage(
			_T("No high resolution timer available.\n")
			_T("This application will now terminate."));
		return FALSE;
	}

	// 192Mo unlimited heap 
	hHeap = HeapCreate(HEAP_NO_SERIALIZE, 192*1024*1024, 0);
	if (hHeap == NULL) {					// error, couldn't create a new heap
		AbortMessage(_T("Can't allocate heap."));
		return FALSE;
	}

	// debug file creation
	#if defined DEBUG_ONDISK
		hDebug = CreateFile(_T("debug.txt"), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
		if (hDebug == INVALID_HANDLE_VALUE)		// error, couldn't create a new file
		{
			AbortMessage(_T("Can't create 'debug.txt' file."));
			return FALSE;
		}
	#endif

	wc.style = CS_BYTEALIGNCLIENT;
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_HP98X6));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;

	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);

	wc.lpszClassName = _T("CHP98x6");

	if (!RegisterClass(&wc)) {
		AbortMessage(
			_T("CHP98x6 class registration failed.\n")
			_T("This application will now terminate."));
		return FALSE;
	}

	// Create window
	rectWindow.left   = 0;
	rectWindow.top    = 0;
	rectWindow.right  = 256;
	rectWindow.bottom = 0;
	AdjustWindowRect(&rectWindow, WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED, TRUE);

	hWnd = CreateWindow(_T("CHP98x6"), _T("HP98x6"),
		WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rectWindow.right  - rectWindow.left,
		rectWindow.bottom - rectWindow.top,
		NULL, NULL, hApp, NULL
		);

	if (hWnd == NULL) {
		AbortMessage(_T("Window creation failed."));
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);

	GetCurrentDirectory(ARRAYSIZEOF(szCurrentDirectory), szCurrentDirectory);

	ReadSettings();

	// create auto event handle
	hEventShutdn = CreateEvent(NULL,FALSE,FALSE,NULL);
	if (hEventShutdn == NULL) {
		AbortMessage(_T("Event creation failed."));
		DestroyWindow(hWnd);
		return FALSE;
	}

	nState     = SM_RUN;					// init state must be <> nNextState
	nNextState = SM_INVALID;				// go into invalid state
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WorkerThread, NULL, CREATE_SUSPENDED, &lThreadId);
	if (hThread == NULL) {
		CloseHandle(hEventShutdn);			// close event handle
		AbortMessage(_T("Thread creation failed."));
		DestroyWindow(hWnd);
		return FALSE;
	}
	// on multiprocessor machines for QueryPerformanceCounter()
	dwAffMask = SetThreadAffinityMask(hThread,1);
	_ASSERT(dwAffMask != 0);
	ResumeThread(hThread);					// start thread
	while (nState!=nNextState) Sleep(0);	// wait for thread initialized

	_ASSERT(hWnd != NULL);
	_ASSERT(hWindowDC != NULL);

	if (nArgc >= 2)	{						// use decoded parameter line
		TCHAR szTemp[MAX_PATH+8] = _T("Loading ");
		lstrcat(szTemp, ppArgv[1]);
		SetWindowTitle(szTemp);
		if (OpenDocument(ppArgv[1]))
			goto start;
	}

	ReadLastDocument(szBufferFilename, ARRAYSIZEOF(szBufferFilename));
	if (szBufferFilename[0]) {
		TCHAR szTemp[MAX_PATH+8] = _T("Loading ");
		lstrcat(szTemp, szBufferFilename);
		SetWindowTitle(szTemp);
		if (OpenDocument(szBufferFilename))
			goto start;
	}

	SetWindowTitle(_T("New Document"));
	if (NewDocument()) {
		SetWindowTitle(_T("Untitled"));
		goto start;
	}

	ResetDocument();

start:

	if (nDbgState > DBG_OFF) OnToolDebug();

	ShowWindow(hWnd, nCmdShow);

	if (pbyRom) SwitchToState(SM_RUN);

	// main windows event dispatch loop
	while (GetMessage(&msg, NULL, 0, 0)) {
		if(   (hDlgDebug == NULL || !IsDialogMessage(hDlgDebug, &msg))
		   && (hDlgFind  == NULL || !IsDialogMessage(hDlgFind, &msg))) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WriteSettings();						// save emulation settings

	CloseHandle(hThread);					// close thread handle
	CloseHandle(hEventShutdn);				// close event handle
	_ASSERT(nState == SM_RETURN);			// emulation thread down?
	ResetDocument();
	ResetBackup();
	_ASSERT(pKml == NULL);					// KML script closed
	_ASSERT(szTitle == NULL);				// freed allocated memory
	_ASSERT(hPalette == NULL);				// freed resource memory

//	CloseWaves();							// free sound memory

#if defined DEBUG_ONDISK
	CloseHandle(hDebug);
#endif
	
	HeapDestroy(hHeap);						// free heap

	return msg.wParam;
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(hPrevInst);
}
