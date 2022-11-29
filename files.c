/*
 *   Files.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   All stuff for files and documents, still some stuff from emu42
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "mops.h"
#include "kml.h"
#include "debugger.h"
#include "resource.h"

TCHAR  szHP8xDirectory[MAX_PATH];
TCHAR  szCurrentDirectory[MAX_PATH];
TCHAR  szCurrentKml[MAX_PATH];
TCHAR  szCurrentFilename[MAX_PATH];
TCHAR  szBufferFilename[MAX_PATH];

static TCHAR  szBackupKml[MAX_PATH];
static TCHAR  szBackupFilename[MAX_PATH];

// pointers for roms data
LPBYTE pbyRom = NULL;

UINT   nCurrentRomType = 16;					// Model
UINT   nCurrentClass = 0;						// Class -> derivate

// document signatures
static BYTE pbySignature[] = "HP98x6  Document\xFE";
static HANDLE hCurrentFile = NULL;

static SYSTEM BackupChipset;

BOOL   bBackup = FALSE;

//
// 7680 Kb = 8Mb - 512kb 
//
static DWORD dwRamSize[] = { 256, 512, 768, 1024, 2048, 4096, 6144, 7680};
static WORD wRamSizeNumbers = 8;
static TCHAR *szDiscType0[] = { _T("None"), _T("9121D"), _T("9895D"), _T("9122D"), _T("9134A")};
static WORD wDiscType0Numbers = 5;
static TCHAR *szDiscType2[] = { _T("None"), _T("9121D"), _T("9895D"), _T("9122D"), _T("9134A")};
static WORD wDiscType2Numbers = 5;
static TCHAR *szDiscType3[] = { _T("None"), _T("7908"), _T("7911"), _T("7912")};
static WORD wDiscType3Numbers = 4;

//################
//#
//#    Low level subroutines
//#
//################

//
//   Roms emulator crc
//
static WORD CrcRom(VOID)					// calculate fingerprint of ROMs
{
	WORD dwCrc = 0;
/*	int i;
	WORD dwCount;

	_ASSERT(Chipset.Rom);						// view on ROM
	// use checksum, because it's faster
	for (dwCount = 0; dwCount < 12*1024; dwCount += 1)
		dwCrc += ((WORD *) Chipset.Rom)[dwCount];

	for(i = 0; i < 256; i++)
		if (pbyRomPage[i] != NULL)
			for (dwCount = 0; dwCount < 4*1024; dwCount += 1)
				dwCrc += ((WORD *) pbyRomPage[i])[dwCount];
*/
	return (WORD) dwCrc;
}

//
// load rom from S records
//
static LPBYTE read1Byte(LPBYTE p, BYTE *d)
{	
	BYTE a,b;
	a = (*p++)-'0';
	if (a> 9)  a-= 7;
	b = (*p++)-'0';
	if (b> 9) b-= 7;
	*d = (a << 4) + b;
	return p;
}
static LPBYTE read2Bytes(LPBYTE p, DWORD *d)
{	
	BYTE a,b;
	p = read1Byte(p, &a);
	p = read1Byte(p, &b);
	*d = (a << 8) + b;
	return p;
}
static LPBYTE read3Bytes(LPBYTE p, DWORD *d)
{	
	BYTE a,b,c;
	p = read1Byte(p, &a);
	p = read1Byte(p, &b);
	p = read1Byte(p, &c);
	*d = (a << 16) + (b << 8) + c;
	return p;
}
static LPBYTE read4Bytes(LPBYTE p, DWORD *d)
{	
	BYTE a,b,c,e;
	p = read1Byte(p, &a);
	p = read1Byte(p, &b);
	p = read1Byte(p, &c);
	p = read1Byte(p, &e);
	*d = (a << 24) + (b << 16) + (c << 8) + e;
	return p;
}
static VOID LoadSrecs(LPBYTE pData, LPBYTE pRom)
{
	LPBYTE ptrRom = pRom;					
	LPBYTE ptrData = pData;					// data to read
	DWORD saddr;							// addr to load in rom
	BYTE len;								// len of current Srec
	BYTE rec;

	while (1) {
		ptrData++;							// skip S
		rec = *ptrData++;
		ptrData = read1Byte(ptrData, &len);
		len -= 1;							// correct len for crc
		saddr = 0x00000000;
		switch (rec) {															// n
			case '0':																// S0	
				len = 0x0000;															// skip it
				break;
			case '1':																// S1 : 2 byte address
				ptrData = read2Bytes(ptrData, &saddr);									// get addr
				len -= 2;																// correct len for address
				break;
			case '2':																// S2 : 3 bytes address
				ptrData = read3Bytes(ptrData, &saddr);									// get addr
				len -= 3;																// correct len for address
				break;
			case '3':																// S3 : 4 bytes address
				ptrData = read4Bytes(ptrData, &saddr);									// get addr
				len -= 4;																// correct len for address
				break;
			case '4':
			case '5':
				len = 0x00;															// skip it
				break;
			case '6':
			case '7':
			case '8':
			case '9':
				goto end;															// skip it
				break;
		}
		while (len != 0x00) {
			if (saddr <= 0x00003FFF) {
				ptrData = read1Byte(ptrData, (BYTE *) pRom + saddr); 
			}
			len--;
			saddr++;
		}
		while ((*ptrData++) != 0x0A) {			// search for end of line
		}
	}
	end:
	return;
}

static LPBYTE  LoadRomSrec(LPCTSTR szRomDirectory, LPCTSTR szFilename)
{
	HANDLE  hRomFile = NULL;
	DWORD dwFileSize, dwRead;
	LPBYTE pRom = NULL;
	LPBYTE pData = NULL;

	SetCurrentDirectory(szHP8xDirectory);
	SetCurrentDirectory(szRomDirectory);
	hRomFile = CreateFile(szFilename,
						  GENERIC_READ,
						  FILE_SHARE_READ,
						  NULL,
						  OPEN_EXISTING,
						  FILE_FLAG_SEQUENTIAL_SCAN,
						  NULL);
	SetCurrentDirectory(szCurrentDirectory);
	if (hRomFile == INVALID_HANDLE_VALUE) {
		hRomFile = NULL;
		return NULL;
	}
	dwFileSize = GetFileSize(hRomFile, NULL);

	pData = HeapAlloc(hHeap, 0, dwFileSize);
	if (pData == NULL) {
		CloseHandle(hRomFile);
		hRomFile = NULL;
		return NULL;
	}
	// load file content
	ReadFile(hRomFile,pData,dwFileSize,&dwRead,NULL);
	CloseHandle(hRomFile);
	hRomFile = NULL;
	_ASSERT(dwFileSize == dwRead);

	pRom = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 16*1024);
	if (pRom == NULL) {
		return NULL;
	}

	LoadSrecs(pData, pRom);

	if (pData != NULL)  {
		HeapFree(hHeap, 0, pData);
		pData = NULL;
	}

	return pRom;
}

static LPBYTE  LoadRom(LPCTSTR szRomDirectory, LPCTSTR szFilename)
{
	HANDLE  hRomFile = NULL;
	DWORD dwFileSize, dwRead;
	LPBYTE pRom = NULL;

	SetCurrentDirectory(szHP8xDirectory);
	SetCurrentDirectory(szRomDirectory);
	hRomFile = CreateFile(szFilename,
						  GENERIC_READ,
						  FILE_SHARE_READ,
						  NULL,
						  OPEN_EXISTING,
						  FILE_FLAG_SEQUENTIAL_SCAN,
						  NULL);
	SetCurrentDirectory(szCurrentDirectory);
	if (hRomFile == INVALID_HANDLE_VALUE) {
		hRomFile = NULL;
		return NULL;
	}
	dwFileSize = GetFileSize(hRomFile, NULL);
	if (dwFileSize < 16*1024) { // file is too small.
		CloseHandle(hRomFile);
		hRomFile = NULL;
		return NULL;
	}

	pRom = HeapAlloc(hHeap, 0, dwFileSize);
	if (pRom == NULL) {
		CloseHandle(hRomFile);
		hRomFile = NULL;
		return NULL;
	}

	// load file content
	ReadFile(hRomFile,pRom,dwFileSize,&dwRead,NULL);
	CloseHandle(hRomFile);
	_ASSERT(dwFileSize == dwRead);
	return pRom;
}

//
// New document settings
//
static BOOL CALLBACK NewSettingsProc(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	HWND hList;
	UINT nIndex, i, j, k;
	TCHAR buffer[32];

	switch (message) {
		case WM_INITDIALOG:
			// init rom checkboxes
			CheckDlgButton(hDlg, IDC_SETROM10, (Chipset.RomVer == 10));
			CheckDlgButton(hDlg, IDC_SETROM20, (Chipset.RomVer == 20));
			CheckDlgButton(hDlg, IDC_SETROM30, (Chipset.RomVer == 30));
			CheckDlgButton(hDlg, IDC_SETROM30L, (Chipset.RomVer == 31));
			CheckDlgButton(hDlg, IDC_SETROM40, (Chipset.RomVer == 40));
			// init speed checkboxes
			CheckDlgButton(hDlg, IDC_NSETx1, (wRealSpeed == 1));
			CheckDlgButton(hDlg, IDC_NSETx2, (wRealSpeed == 2));
			CheckDlgButton(hDlg, IDC_NSETx3, (wRealSpeed == 3));
			CheckDlgButton(hDlg, IDC_NSETx4, (wRealSpeed == 4));
			CheckDlgButton(hDlg, IDC_NSETx8, (wRealSpeed == 5));
			CheckDlgButton(hDlg, IDC_NSETxMAX, (wRealSpeed == 0));
			CheckDlgButton(hDlg, IDC_DEBUGSTART, (nDbgState != DBG_OFF));

			CheckDlgButton(hDlg, IDC_HP98635, Chipset.Hp98635 ? 1 : 0);

			CheckDlgButton(hDlg, IDC_KEEPTIME, Chipset.keeptime ? 1 : 0);
			CheckDlgButton(hDlg, IDC_USKEYMAP, Chipset.Keyboard.keymap ? 1 : 0);

			CheckDlgButton(hDlg, IDC_SETSC071, Chipset.Hpib71x ? 1 : 0);

			// init ram list
			hList = GetDlgItem(hDlg,IDC_SETRAM);
			SendMessage(hList, CB_RESETCONTENT, 0, 0);
			for (i = 0, k = 0; i < wRamSizeNumbers; i++) {
				j = wsprintf(buffer, "%d Kb", dwRamSize[i]);
				buffer[j] = 0x00;
				nIndex = SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)buffer);
				SendMessage(hList, CB_SETITEMDATA, nIndex, i);
				if (Chipset.RamSize == _KB(dwRamSize[i]))
					k = i;
			}
			SendMessage(hList, CB_SETCURSEL, k, 0);
			// init disc list 70x
			hList = GetDlgItem(hDlg,IDC_SETDISC70);
			SendMessage(hList, CB_RESETCONTENT, 0, 0);
			for (i = 0; i < wDiscType0Numbers; i++) {
				nIndex = SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)szDiscType0[i]);
				SendMessage(hList, CB_SETITEMDATA, nIndex, i);
			}
			SendMessage(hList, CB_SETCURSEL, Chipset.Hpib70x, 0);
			// init disc list 72x
			hList = GetDlgItem(hDlg,IDC_SETDISC72);
			SendMessage(hList, CB_RESETCONTENT, 0, 0);
			for (i = 0; i < wDiscType2Numbers; i++) {
				nIndex = SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)szDiscType2[i]);
				SendMessage(hList, CB_SETITEMDATA, nIndex, i);
			}
			SendMessage(hList, CB_SETCURSEL, Chipset.Hpib72x, 0);
			// init disc list 73x
			hList = GetDlgItem(hDlg,IDC_SETDISC73);
			SendMessage(hList, CB_RESETCONTENT, 0, 0);
			for (i = 0; i < wDiscType3Numbers; i++) {
				nIndex = SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)szDiscType3[i]);
				SendMessage(hList, CB_SETITEMDATA, nIndex, i);
			}
			SendMessage(hList, CB_SETCURSEL, Chipset.Hpib73x, 0);
			// init disc list 74x
			hList = GetDlgItem(hDlg,IDC_SETDISC74);
			SendMessage(hList, CB_RESETCONTENT, 0, 0);
			for (i = 0; i < wDiscType3Numbers; i++) {
				nIndex = SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)szDiscType3[i]);
				SendMessage(hList, CB_SETITEMDATA, nIndex, i);
			}
			SendMessage(hList, CB_SETCURSEL, Chipset.Hpib74x, 0);

			return TRUE;
		case WM_COMMAND:
			if (wParam == IDOK) {
				if (szCurrentKml[0]) {	// really chose a kml file
					ResetDocument();

					hList = GetDlgItem(hDlg,IDC_SETRAM);
					nIndex = SendMessage(hList, CB_GETCURSEL, 0, 0);
					Chipset.RamSize = _KB(dwRamSize[nIndex]);			// RAM size 
					Chipset.RamStart = 0x01000000 - Chipset.RamSize;	// from ... to 0x00FFFFFF

					Chipset.Hpib71x = IsDlgButtonChecked(hDlg,IDC_SETSC071) ? 1 : 0;	// printer ?

					hList = GetDlgItem(hDlg,IDC_SETDISC70);				// disk 70x
					nIndex = SendMessage(hList, CB_GETCURSEL, 0, 0);
					Chipset.Hpib70x = (BYTE)(nIndex);					// type of disk
					if (Chipset.Hpib70x == 1) 
						Chipset.Hp9121_0.ctype = 0;
					else if (Chipset.Hpib70x == 2) 
						Chipset.Hp9121_0.ctype = 1;
					else if (Chipset.Hpib70x == 4) 
						Chipset.Hp9121_0.ctype = 2;
					hList = GetDlgItem(hDlg,IDC_SETDISC72);				// disk 72x
					nIndex = SendMessage(hList, CB_GETCURSEL, 0, 0);
					Chipset.Hpib72x = (BYTE)(nIndex);					// type of disk
					if (Chipset.Hpib72x == 1) 
						Chipset.Hp9121_1.ctype = 0;
					else if (Chipset.Hpib72x == 2) 
						Chipset.Hp9121_1.ctype = 1;
					else if (Chipset.Hpib72x == 4) 
						Chipset.Hp9121_1.ctype = 2;
					hList = GetDlgItem(hDlg,IDC_SETDISC73);				// disk 73x
					nIndex = SendMessage(hList, CB_GETCURSEL, 0, 0);
					Chipset.Hpib73x = (BYTE)(nIndex);					// type of disk
					hList = GetDlgItem(hDlg,IDC_SETDISC74);				// disk 73x
					nIndex = SendMessage(hList, CB_GETCURSEL, 0, 0);
					Chipset.Hpib74x = (BYTE)(nIndex);					// type of disk

					hpib_init();										// initialize it

					// get roms checkbox values

					Chipset.RomVer = IsDlgButtonChecked(hDlg,IDC_SETROM10) ? 10 :
							  IsDlgButtonChecked(hDlg,IDC_SETROM20) ? 20 : 
							  IsDlgButtonChecked(hDlg,IDC_SETROM30) ? 30 : 
							  IsDlgButtonChecked(hDlg,IDC_SETROM30L) ? 31 : 
							  IsDlgButtonChecked(hDlg,IDC_SETROM40) ? 40 :10;
					nDbgState = IsDlgButtonChecked(hDlg,IDC_DEBUGSTART) ? DBG_RUN : DBG_OFF;
					wRealSpeed = IsDlgButtonChecked(hDlg, IDC_NSETx1) ? 1 :
								 IsDlgButtonChecked(hDlg, IDC_NSETx2) ? 2 :
								 IsDlgButtonChecked(hDlg, IDC_NSETx3) ? 3 :
								 IsDlgButtonChecked(hDlg, IDC_NSETx4) ? 4 :
								 IsDlgButtonChecked(hDlg, IDC_NSETx8) ? 5 :
								 IsDlgButtonChecked(hDlg, IDC_NSETxMAX) ? 0 : 0;
					SetSpeed(wRealSpeed);			// set speed

					Chipset.keeptime = IsDlgButtonChecked(hDlg, IDC_KEEPTIME) ? 1 : 0;

					Chipset.Keyboard.keymap = IsDlgButtonChecked(hDlg, IDC_USKEYMAP) ? 1 : 0;

					Chipset.Hp98635 = IsDlgButtonChecked(hDlg, IDC_HP98635) ? 1 : 0;

					EndDialog(hDlg, wParam);
				}
			}
			if (wParam == IDCANCEL) {
				EndDialog(hDlg, wParam);
			}
			if (wParam == IDC_SETKML) {
				DisplayChooseKml(0);
			}
			break;
	} 
	return FALSE;
    UNREFERENCED_PARAMETER(lParam);
}

//################
//#
//#    Public functions
//#
//################

//
//  Window Position Tools
//
VOID SetWindowLocation(HWND hWnd,INT nPosX,INT nPosY)
{
	WINDOWPLACEMENT wndpl;
	RECT *pRc = &wndpl.rcNormalPosition;

	wndpl.length = sizeof(wndpl);
	GetWindowPlacement(hWnd,&wndpl);
	pRc->right = pRc->right - pRc->left + nPosX;
	pRc->bottom = pRc->bottom - pRc->top + nPosY;
	pRc->left = nPosX;
	pRc->top = nPosY;
	SetWindowPlacement(hWnd,&wndpl);
}

//
//   Roms
//

BOOL PatchRom(LPCTSTR szFilename)
{
	return TRUE;
}

//
// get the right BootRom
//
BOOL MapRom(LPCTSTR szRomDirectory)
{
	if (pbyRom != NULL) {
		return FALSE;
	}

	switch (Chipset.RomVer) {
		case 10:
			Chipset.Rom = pbyRom = LoadRomSrec(szRomDirectory,"boot rom 9826.S68");
			Chipset.RomSize = 16*1024;
			break;
		case 20:
			Chipset.Rom = pbyRom = LoadRom(szRomDirectory,"rom20.bin");
			Chipset.RomSize = 16*1024;
			break;
		default:
		case 30:
			Chipset.Rom = pbyRom = LoadRom(szRomDirectory,"rom30.bin");
			Chipset.RomSize = 64*1024;
			break;
		case 31:
			Chipset.Rom = pbyRom = LoadRom(szRomDirectory,"hp425-prom.bin");
			Chipset.RomSize = 256*1024;
			break;
		case 40:
			Chipset.Rom = pbyRom = LoadRom(szRomDirectory,"rom40.bin");
			Chipset.RomSize = 64*1024;
			break;
	}

//	Chipset.Rom = pbyRom = LoadRom(szRomDirectory,"9836_sys.bin");
//	Chipset.RomSize = 16*1024;

//	Chipset.Rom = pbyRom = LoadRom(szRomDirectory,"hp425-prom.bin");
//	Chipset.RomSize = 256*1024;

//	Chipset.Rom = pbyRom = LoadRomSrec(szRomDirectory,"boot rom 9826.S68");
//	Chipset.RomSize = 16*1024;

	return TRUE;
}

//
// remove BootRom from bus
//
VOID UnmapRom(VOID)
{
	if (pbyRom==NULL) 
		return;
	HeapFree(hHeap, 0, pbyRom);
	pbyRom = NULL;
}

//
//   Documents
//

VOID ResetDocument(VOID)
{
	hpib_stop_bus();

	if (szCurrentKml[0]) {
		KillKML();
	}
	if (hCurrentFile) {
		CloseHandle(hCurrentFile);
		hCurrentFile = NULL;
	}
	// szCurrentKml[0] = 0;					// preserve the current KML
	szCurrentFilename[0]=0;
	if (Chipset.Ram)  {
		HeapFree(hHeap, 0, Chipset.Ram);
		Chipset.Ram = NULL;
	}
	ZeroMemory(&Chipset,sizeof(Chipset));

	UpdateWindowStatus();
}

BOOL NewDocument(VOID)
{
	hpib_stop_bus();
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_NEWSETTINGS), hWnd, (DLGPROC)NewSettingsProc) == -1)
		AbortMessage(_T("New Emulation Dialog Creation Error !"));
	if (!InitKML(szCurrentKml,FALSE)) goto restore;

	Chipset.type = nCurrentRomType;

	// allocate memory
	if (Chipset.Ram == NULL) {
		Chipset.Ram = (LPBYTE)HeapAlloc(hHeap, 0, Chipset.RamSize);
		_ASSERT(Chipset.Ram != NULL);
	}

	if (Chipset.Rom == NULL) {
		MapRom(szCurrentDirectory);
		_ASSERT(Chipset.Rom != NULL);
	}

	SystemReset();
	LoadBreakpointList(NULL);				// clear debugger breakpoint list
	SaveBackup();
	return TRUE;
 restore:
	RestoreBackup();
	ResetBackup();
	SetWindowLocation(hWnd,Chipset.nPosX,Chipset.nPosY);

	return FALSE;
}

BOOL OpenDocument(LPCTSTR szFilename)
{
	HANDLE  hFile = INVALID_HANDLE_VALUE;
	DWORD   lBytesRead,lSizeofChipset;
	BYTE    pbyFileSignature[18];
	UINT    ctBytesCompared;
	UINT    nLength;

	SaveBackup();
	ResetDocument();

	// Open file
	if (lstrcmpi(szBackupFilename, szFilename)==0) {
		if (YesNoMessage(_T("Do you want to reload this document ?")) == IDNO)
			goto restore;
	}
	hFile = CreateFile(szFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		AbortMessage(_T("This file is missing or already loaded in another instance of Hp98x6."));
		goto restore;
	}

	// Read and Compare signature
	ReadFile(hFile, pbyFileSignature, sizeof(pbyFileSignature), &lBytesRead, NULL);
	for (ctBytesCompared=0; ctBytesCompared<sizeof(pbyFileSignature); ctBytesCompared++) {
		if (pbyFileSignature[ctBytesCompared]!=pbySignature[ctBytesCompared]) {
			AbortMessage(_T("This file is not a valid Hp98x6 document."));
			goto restore;
		}
	}

	switch (pbyFileSignature[16]) {
	case 0xFE: // Hp98x6 1.0 format
		ReadFile(hFile,&nLength,sizeof(nLength),&lBytesRead,NULL);
		#if defined _UNICODE
		{
			LPSTR szTmp = HeapAlloc(hHeap, 0, nLength);
			if (szTmp == NULL)
			{
				AbortMessage(_T("Memory Allocation Failure."));
				goto restore;
			}
			ReadFile(hFile, szTmp, nLength, &lBytesRead, NULL);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szTmp, lBytesRead,
								szCurrentKml, ARRAYSIZEOF(szCurrentKml));
			HeapFree(hHeap, 0, szTmp);
		}
		#else
		{
			ReadFile(hFile, szCurrentKml, nLength, &lBytesRead, NULL);
		}
		#endif
		if (nLength != lBytesRead) goto read_err;
		szCurrentKml[nLength] = 0;
		break;
	default:
		AbortMessage(_T("This file is for an unknown version of Hp98x6."));
		goto restore;
	}

	// read chipset size inside file
	ReadFile(hFile, &lSizeofChipset, sizeof(lSizeofChipset), &lBytesRead, NULL);
	if (lBytesRead != sizeof(lSizeofChipset)) goto read_err;
	if (lSizeofChipset <= sizeof(Chipset)) {	// actual or older chipset version
		// read chipset content
		ZeroMemory(&Chipset,sizeof(Chipset)); // init chipset
		ReadFile(hFile, &Chipset, lSizeofChipset, &lBytesRead, NULL);
	} else {									// newer chipset version
		// read my used chipset content
		_ASSERT(0);
		ReadFile(hFile, &Chipset, sizeof(Chipset), &lBytesRead, NULL);

		// skip rest of chipset
		SetFilePointer(hFile, lSizeofChipset-sizeof(Chipset), NULL, FILE_CURRENT);
		lSizeofChipset = sizeof(Chipset);
	}
	if (lBytesRead != lSizeofChipset) goto read_err;

	// clean pointers

	hAlpha1DC = NULL;
	hAlpha2DC = NULL;
	hGraphDC = NULL;
	hFontDC = NULL;

	SetWindowLocation(hWnd,Chipset.nPosX,Chipset.nPosY);

	if (szCurrentKml == NULL) {
		if (!DisplayChooseKml(Chipset.type))
			goto restore;
	}
	while (TRUE) {
		BOOL bOK;

		bOK = InitKML(szCurrentKml,FALSE);

		bOK = bOK && (nCurrentRomType == Chipset.type);
		if (bOK) break;

		KillKML();
		if (!DisplayChooseKml(Chipset.type))
			goto restore;
	}

	Chipset.Ram = (LPBYTE)HeapAlloc(hHeap, 0, Chipset.RamSize);
	if (Chipset.Ram == NULL) {
		AbortMessage(_T("Memory Allocation Failure."));
		goto restore;
	}

	ReadFile(hFile, Chipset.Ram, Chipset.RamSize, &lBytesRead, NULL);
	if (lBytesRead != Chipset.RamSize) goto read_err;

	// clean pointers

	Chipset.Hp9130.disk[0] = NULL;
	Chipset.Hp9130.disk[1] = NULL;
	Chipset.Hp9121_0.disk[0] = NULL;
	Chipset.Hp9121_0.disk[1] = NULL;
	Chipset.Hp9121_1.disk[0] = NULL;
	Chipset.Hp9121_1.disk[1] = NULL;
	Chipset.Hp9122_0.disk[0] = NULL;
	Chipset.Hp9122_0.disk[1] = NULL;
	Chipset.Hp9122_1.disk[0] = NULL;
	Chipset.Hp9122_1.disk[1] = NULL;

	Chipset.Hp7908_0.hdisk[0] = NULL;
	Chipset.Hp7908_0.hdisk[1] = NULL;
	Chipset.Hp7908_1.hdisk[0] = NULL;
	Chipset.Hp7908_1.hdisk[1] = NULL;

	Chipset.Hp2225.hfile = NULL;

	hp9130_reset();												// reload Disk if needed
	hp9121_reset(&Chipset.Hp9121_0);							// reload Disk if needed
	hp9121_reset(&Chipset.Hp9121_1);							// reload Disk if needed
	hp9122_reset(&Chipset.Hp9122_0);							// reload Disk if needed
	hp9122_reset(&Chipset.Hp9122_1);							// reload Disk if needed
	hp7908_reset(&Chipset.Hp7908_0);							// reload Disk if needed
	hp7908_reset(&Chipset.Hp7908_1);							// reload Disk if needed

	hp2225_reset(&Chipset.Hp2225);								// reset printer

	hpib_init();

	Init_Keyboard();

	LoadBreakpointList(hFile);				// load debugger breakpoint list

//	if (Chipset.RomCrc != CrcRom())	{		// ROM changed
//		SystemReset();
//	}

	lstrcpy(szCurrentFilename, szFilename);
	_ASSERT(hCurrentFile == NULL);
	hCurrentFile = hFile;
	SetWindowTitle(szCurrentFilename);
	UpdateWindowStatus();

	Reload_Graph();
	UpdateMainDisplay(TRUE);	// refresh screen alpha, graph & cmap
	hpib_names();
	UpdateAnnunciators(TRUE);
	return TRUE;

 read_err:
	AbortMessage(_T("This file must be truncated, and cannot be loaded."));
 restore:
	if (INVALID_HANDLE_VALUE != hFile)		// close if valid handle
		CloseHandle(hFile);
	RestoreBackup();
	ResetBackup(); 
	return FALSE;
}

BOOL SaveDocument(VOID)
{
	DWORD           lBytesWritten;
	DWORD           lSizeofChipset;
	UINT            nLength;
	WINDOWPLACEMENT wndpl;

	if (hCurrentFile == NULL) return FALSE;

	_ASSERT(hWnd);							// window open
	wndpl.length = sizeof(wndpl);			// update saved window position
	GetWindowPlacement(hWnd, &wndpl);
	Chipset.nPosX = (SWORD) wndpl.rcNormalPosition.left;
	Chipset.nPosY = (SWORD) wndpl.rcNormalPosition.top;

	if (Chipset.type == 37)
		SaveMainDisplay37();		// copy bitmap into grap mem

	SetFilePointer(hCurrentFile,0,NULL,FILE_BEGIN);

	if (!WriteFile(hCurrentFile, pbySignature, sizeof(pbySignature), &lBytesWritten, NULL)) {
		AbortMessage(_T("Could not write into file !"));
		return FALSE;
	}

	//Chipset.RomCrc = CrcRom();				// save fingerprint of ROM

	nLength = lstrlen(szCurrentKml);
	WriteFile(hCurrentFile, &nLength, sizeof(nLength), &lBytesWritten, NULL);
	#if defined _UNICODE
	{
		LPSTR szTmp = HeapAlloc(hHeap, 0, nLength);
		if (szTmp != NULL)
		{
			WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK,
								szCurrentKml, nLength,
								szTmp, nLength, NULL, NULL);
			WriteFile(hCurrentFile, szTmp, nLength, &lBytesWritten, NULL);
			HeapFree(hHeap, 0, szTmp);
		}
	}
	#else
	{
		WriteFile(hCurrentFile, szCurrentKml, nLength, &lBytesWritten, NULL);
	}
	#endif
	lSizeofChipset = sizeof(Chipset);
	WriteFile(hCurrentFile, &lSizeofChipset, sizeof(lSizeofChipset), &lBytesWritten, NULL);
	WriteFile(hCurrentFile, &Chipset, lSizeofChipset, &lBytesWritten, NULL);
	if (Chipset.Ram) WriteFile(hCurrentFile, Chipset.Ram,  Chipset.RamSize,  &lBytesWritten, NULL);
	// if (Chipset.Video) WriteFile(hCurrentFile, Chipset.Video,  Chipset.VideoSize,  &lBytesWritten, NULL);
	SaveBreakpointList(hCurrentFile);		// save debugger breakpoint list
	SetEndOfFile(hCurrentFile);				// cut the rest 
	return TRUE;
}

BOOL SaveDocumentAs(LPCTSTR szFilename)
{
	HANDLE hFile;

	if (hCurrentFile) {						// already file in use
		CloseHandle(hCurrentFile);			// close it, even it's same, so data always will be saved
		hCurrentFile = NULL;
	}
	hFile = CreateFile(szFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {	// error, couldn't create a new file
		AbortMessage(_T("This file must be currently used by another instance of Hp8x."));
		return FALSE;
	}
	lstrcpy(szCurrentFilename, szFilename);	// save new file name
	hCurrentFile = hFile;					// and the corresponding handle
	SetWindowTitle(szCurrentFilename);		// update window title line
	UpdateWindowStatus();					// and draw it 
	return SaveDocument();					// save current content
}

//
//   Backups, removed ...
//

BOOL SaveBackup(VOID)
{
	return TRUE;
}

BOOL RestoreBackup(VOID)
{
	return TRUE;
}

BOOL ResetBackup(VOID)
{
	return TRUE;
}

//
//   Open File Common Dialog Boxes
//

static VOID InitializeOFN(LPOPENFILENAME ofn)
{
	ZeroMemory((LPVOID)ofn, sizeof(OPENFILENAME));
	ofn->lStructSize = sizeof(OPENFILENAME);
	ofn->hwndOwner = hWnd;
	ofn->Flags = OFN_EXPLORER|OFN_HIDEREADONLY;
}

BOOL GetOpenFilename(VOID)
{
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter =
		_T("Hp98x6 Document (*.E98x6)\0*.E98x6\0")
		_T("\0\0");
	ofn.lpstrDefExt = _T("E98x6");			// HP98x6
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = HeapAlloc(hHeap, 0, 512);
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = 512;
	ofn.Flags |= OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&ofn) == FALSE) {
		HeapFree(hHeap, 0, ofn.lpstrFile);
		return FALSE;
	}
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	HeapFree(hHeap, 0, ofn.lpstrFile); 
	return TRUE;
}

BOOL GetSaveAsFilename(VOID)
{
 	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter =
		_T("Hp98x6 Document (*.E98x6)\0*.E98x6\0")
		_T("\0\0");
	ofn.lpstrDefExt = _T("E98x6");			// HP98x6
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = HeapAlloc(hHeap, 0, 512);
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = 512;
	ofn.Flags |= OFN_CREATEPROMPT|OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&ofn) == FALSE) {
		HeapFree(hHeap, 0, ofn.lpstrFile);
		return FALSE;
	}
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	HeapFree(hHeap, 0, ofn.lpstrFile);
	return TRUE;
}

BOOL GetLoadLifFilename(VOID)
{
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter = _T("All Files (*.*)\0*.*\0") _T("\0\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = HeapAlloc(hHeap, 0, 512);
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = 512;
	ofn.Flags |= OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&ofn) == FALSE) {
		HeapFree(hHeap, 0, ofn.lpstrFile);
		return FALSE;
	}
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	HeapFree(hHeap, 0, ofn.lpstrFile); 
	return TRUE;
}

BOOL GetSaveLifFilename(VOID)
{
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter = _T("All Files (*.*)\0*.*\0") _T("\0\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = HeapAlloc(hHeap, 0, 512);
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = 512;
	ofn.Flags |= OFN_CREATEPROMPT|OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&ofn) == FALSE) {
		HeapFree(hHeap, 0, ofn.lpstrFile);
		return FALSE;
	}
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	HeapFree(hHeap, 0, ofn.lpstrFile); 
	return TRUE;
}

BOOL GetSaveDiskFilename(LPCTSTR lpstrName)
{
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter = _T("All Files (*.*)\0*.*\0") _T("\0\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = HeapAlloc(hHeap, 0, 512);
	lstrcpy(ofn.lpstrFile, lpstrName);
	ofn.nMaxFile = 512;
	ofn.Flags |= OFN_CREATEPROMPT|OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&ofn) == FALSE) {
		HeapFree(hHeap, 0, ofn.lpstrFile);
		return FALSE;
	}
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	HeapFree(hHeap, 0, ofn.lpstrFile); 
	return TRUE;
}
//################
//#
//#    Load Bitmap
//#
//################

static __inline UINT DibNumColors (LPBITMAPINFOHEADER lpbi)
{
	if (lpbi->biClrUsed != 0) return (UINT)lpbi->biClrUsed;

	// a 24 bitcount DIB has no color table
	return (lpbi->biBitCount <= 8) ? (1 << lpbi->biBitCount) : 0;
}

static HPALETTE CreateBIPalette(LPBITMAPINFOHEADER lpbi)
{
	LOGPALETTE* pPal;
	HPALETTE    hpal = NULL;
	UINT        nNumColors;
	BYTE        red;
	BYTE        green;
	BYTE        blue;
	UINT        i;
	RGBQUAD*    pRgb;

	if (!lpbi)
		return NULL;

	if (lpbi->biSize != sizeof(BITMAPINFOHEADER))
		return NULL;

	// Get a pointer to the color table and the number of colors in it
	pRgb = (RGBQUAD FAR *)((LPBYTE)lpbi + (WORD)lpbi->biSize);
	nNumColors = DibNumColors(lpbi);

	if (nNumColors) {
		// Allocate for the logical palette structure
		pPal = (LOGPALETTE*)HeapAlloc(hHeap, 0, sizeof(LOGPALETTE) + nNumColors * sizeof(PALETTEENTRY));
		if (!pPal)
			return NULL;

		pPal->palNumEntries = nNumColors;
		pPal->palVersion    = 0x300;

		// Fill in the palette entries from the DIB color table and
		// create a logical color palette.
		for (i = 0; i < nNumColors; i++) {
			pPal->palPalEntry[i].peRed   = pRgb[i].rgbRed;
			pPal->palPalEntry[i].peGreen = pRgb[i].rgbGreen;
			pPal->palPalEntry[i].peBlue  = pRgb[i].rgbBlue;
			pPal->palPalEntry[i].peFlags = (BYTE)0;
		}
		hpal = CreatePalette(pPal);
		HeapFree(hHeap, 0, (HANDLE)pPal);
	} else {
		if (lpbi->biBitCount == 24) {
			// A 24 bitcount DIB has no color table entries so, set the
			// number of to the maximum value (256).
			nNumColors = 256;
			pPal = (LOGPALETTE*)HeapAlloc(hHeap, 0, sizeof(LOGPALETTE) + nNumColors * sizeof(PALETTEENTRY));
			if (!pPal)
				return NULL;

			pPal->palNumEntries = nNumColors;
			pPal->palVersion    = 0x300;

			red = green = blue = 0;

			// Generate 256 (= 8*8*4) RGB combinations to fill the palette
			// entries.
			for (i = 0; i < pPal->palNumEntries; i++) {
				pPal->palPalEntry[i].peRed   = red;
				pPal->palPalEntry[i].peGreen = green;
				pPal->palPalEntry[i].peBlue  = blue;
				pPal->palPalEntry[i].peFlags = (BYTE)0;

				if (!(red += 32))
					if (!(green += 32))
					blue += 64;
			}
			hpal = CreatePalette(pPal);
			HeapFree(hHeap, 0, (HANDLE)pPal);
		}
	}
	return hpal;
}

HBITMAP LoadBitmapFile(LPCTSTR szFilename)
{
	HANDLE  hFile;
	HANDLE  hMap;
	LPBYTE  pbyFile;
	HBITMAP hBitmap;
	LPBITMAPFILEHEADER pBmfh;
	LPBITMAPINFO pBmi;

	SetCurrentDirectory(szHP8xDirectory);
	hFile = CreateFile(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	SetCurrentDirectory(szCurrentDirectory);
	// opened with GENERIC_READ -> PAGE_READONLY
	hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hMap == NULL) {
		CloseHandle(hFile);
		return NULL;
	}
	// opened with GENERIC_READ -> PAGE_READONLY -> FILE_MAP_READ
	pbyFile = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	if (pbyFile == NULL) {
		CloseHandle(hMap);
		CloseHandle(hFile);
		return NULL;
	}

	hBitmap = NULL;
	pBmfh = (LPBITMAPFILEHEADER)pbyFile;
	if (pBmfh->bfType != 0x4D42) goto quit; // "BM"
	pBmi = (LPBITMAPINFO)(pbyFile+sizeof(BITMAPFILEHEADER));

	if (hPalette == NULL) {
		_ASSERT(hPalette == NULL);				// resource free
		hPalette = CreateBIPalette(&pBmi->bmiHeader);
		// save old palette
		hOldPalette = SelectPalette(hWindowDC, hPalette, FALSE);
		RealizePalette(hWindowDC);
	}
	if (FALSE&&(pBmi->bmiHeader.biBitCount <= 8)) {
		UINT i;
		LPDWORD pdwTable = ((LPDWORD)&(pBmi->bmiColors[0]));
		if (pBmi->bmiHeader.biClrUsed)
			for (i=0; i<(UINT)pBmi->bmiHeader.biClrUsed; i++) pdwTable[i]=i;
		else
			for (i=0; i<256; i++) pdwTable[i]=i;
		hBitmap = CreateDIBitmap(
			hWindowDC,
			&pBmi->bmiHeader,
			CBM_INIT,
			pbyFile+pBmfh->bfOffBits,
			pBmi, DIB_PAL_COLORS);
    } else {
		hBitmap = CreateDIBitmap(
			hWindowDC,
			&pBmi->bmiHeader,
			CBM_INIT,
			pbyFile+pBmfh->bfOffBits,
			pBmi, DIB_RGB_COLORS);
	}
	_ASSERT(hBitmap != NULL);

quit:
	UnmapViewOfFile(pbyFile);
	CloseHandle(hMap);
	CloseHandle(hFile);
	return hBitmap;
}
