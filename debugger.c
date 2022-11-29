/*
 *	 Debugger.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   from Emu42
//
//   still some bugs on skip over, and on main dialog
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "resource.h"
#include "color.h"
#include "mops.h"
#include "debugger.h"

// #define PC (Chipset.Cpu.PC)

#define MAXCODELINES     17					// number of lines in code window
#define MAXMEMLINES      12					// number of lines in memory window
#define MAXMEMITEMS      16					// number of address items in a memory window line
#define MAXBREAKPOINTS  256					// max. number of breakpoints

// assert for register update
#define INSTRSIZE  256						// size of last instruction buffer

#define WM_UPDATE (WM_USER+0x1000)			// update debugger dialog box

#define	MEMWNDMAX (sizeof(nCol) / sizeof(nCol[0]))
#define RT_TOOLBAR MAKEINTRESOURCE(241)		// MFC toolbar resource type

typedef struct CToolBarData
{
	WORD wVersion;
	WORD wWidth;
	WORD wHeight;
	WORD wItemCount;
	WORD aItems[1];
} CToolBarData;

typedef struct								// type of breakpoint table
{
	BOOL	bEnable;						// breakpoint enabled
	UINT	nType;							// breakpoint type
	DWORD	dwAddr;							// breakpoint address
} BP_T;

static CONST int rUpt[] =
{
	IDC_D_A0, IDC_D_A1, IDC_D_A2, IDC_D_A3,
	IDC_D_A4, IDC_D_A5, IDC_D_A6,
	IDC_D_USP, IDC_D_SSP, 
	IDC_D_D0, IDC_D_D1, IDC_D_D2, IDC_D_D3,
	IDC_D_D4, IDC_D_D5, IDC_D_D6, IDC_D_D7,
	IDC_D_PC,
	IDC_D_SR,
	IDC_D_X, IDC_D_N, IDC_D_Z, IDC_D_V, IDC_D_C,
	IDC_D_I, IDC_D_T, IDC_D_S
};

#define UPTMAX (sizeof(rUpt) / sizeof(rUpt[0]))

static CONST int nCol[] =
{
	IDC_DEBUG_MEM_COL0, IDC_DEBUG_MEM_COL1, IDC_DEBUG_MEM_COL2, IDC_DEBUG_MEM_COL3,
	IDC_DEBUG_MEM_COL4, IDC_DEBUG_MEM_COL5, IDC_DEBUG_MEM_COL6, IDC_DEBUG_MEM_COL7,
	IDC_DEBUG_MEM_COL8, IDC_DEBUG_MEM_COL9, IDC_DEBUG_MEM_COL10, IDC_DEBUG_MEM_COL11,
	IDC_DEBUG_MEM_COL12, IDC_DEBUG_MEM_COL13, IDC_DEBUG_MEM_COL14, IDC_DEBUG_MEM_COL15
};

static CONST TCHAR cHex[] = { _T('0'),_T('1'), _T('2'), _T('3'),
							  _T('4'), _T('5'), _T('6'), _T('7'),
							  _T('8'), _T('9'), _T('A'), _T('B'),
							  _T('C'), _T('D'), _T('E'), _T('F') };

static INT    nDbgPosX = 20;					// position of debugger window
static INT    nDbgPosY = 20;

static WORD   wBreakpointCount = 0;			// number of breakpoints
static BP_T   sBreakpoint[MAXBREAKPOINTS];	// breakpoint table

 BOOL	  bBreakExceptions = FALSE;		// break on exception

static DWORD  dwAdrLine[MAXCODELINES];		// addresses of disassember lines in code window
static DWORD  dwAdrMem = 0;					// start address of memory window

static LPBYTE lbyMapData;					// data
static DWORD  dwMapDataSize;				// data size

static LONG   lCharWidth;					// width of a character (is a fix font)

static HMENU  hMenuCode, hMenuMem;			// handle of context menues
static HWND   hWndToolbar;					// toolbar handle

static SYSTEM  OldChipset;					// old chipset content
static BOOL    bRegUpdate[64];				// register update table

static HBITMAP hBmpCheckBox;				// checked and unchecked bitmap

static CONST LPCTSTR cExceptions[] = { _T("Reset SSP"), _T("Reset PC"), _T("Bus Error"), _T("Address Error"),
								   _T("Illegal Instruction"), _T("Zero Divide"), _T("CHK"), _T("TRAPV"),
								   _T("Privilege Violation"), _T("Trace"), _T("Line A"), _T("Line F"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Uninitialized"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Reserved"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Reserved"),
								   _T("Spurious Interrupt"), _T("Level 1 Autovector"), _T("Level 2 Autovector"), _T("Level 3 Autovector"),
								   _T("Level 4 Autovector"), _T("Level 5 Autovector"), _T("Level 6 Autovector"), _T("Level 7 Autovector"),
								   _T("Trap 0"), _T("Trap 1"), _T("Trap 2"), _T("Trap 3"),
								   _T("Trap 4"), _T("Trap 5"), _T("Trap 6"), _T("Trap 7"),
								   _T("Trap 8"), _T("Trap 9"), _T("Trap 10"), _T("Trap 11"),
								   _T("Trap 12"), _T("Trap 13"), _T("Trap 14"), _T("Trap 15"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Reserved"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Reserved"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Reserved"),
								   _T("Reserved"), _T("Reserved"), _T("Reserved"), _T("Reserved") };
//
// function prototypes
//
static BOOL OnMemFind(HWND hDlg);
static INT  OnNewValue(LPTSTR lpszValue);
static VOID OnEnterAddress(HWND hDlg, DWORD *dwValue);
static BOOL OnEditBreakpoint(HWND hDlg);

//################
//#
//#    Low level subroutines
//#
//################

//
// disable menu keys
//
static VOID DisableMenuKeys(HWND hDlg)
{
	HMENU hMenu = GetMenu(hDlg);

	EnableMenuItem(hMenu, ID_DEBUG_RUN, MF_GRAYED);
	EnableMenuItem(hMenu, ID_DEBUG_RUNCURSOR, MF_GRAYED);
	EnableMenuItem(hMenu, ID_DEBUG_STEP, MF_GRAYED);
	EnableMenuItem(hMenu, ID_DEBUG_STEPOVER, MF_GRAYED);
	EnableMenuItem(hMenu, ID_DEBUG_STEPOUT, MF_GRAYED);
	EnableMenuItem(hMenu, ID_INFO_IO, MF_GRAYED);

	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_RUN, MAKELONG((FALSE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_BREAK, MAKELONG((TRUE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_RUNCURSOR, MAKELONG((FALSE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_STEP, MAKELONG((FALSE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_STEPOVER, MAKELONG((FALSE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_STEPOUT, MAKELONG((FALSE), 0));
}

//
// set/reset breakpoint
//
static __inline VOID ToggleBreakpoint(DWORD dwAddr)
{
	INT i;

	for (i = 0; i < wBreakpointCount; ++i) {	// scan all breakpoints
		// code breakpoint found
		if (sBreakpoint[i].nType == BP_EXEC && sBreakpoint[i].dwAddr == dwAddr) {
			if (!sBreakpoint[i].bEnable) {			// breakpoint disabled
				sBreakpoint[i].bEnable = TRUE;
				return;
			}

			// purge breakpoint
			for (++i; i < wBreakpointCount; ++i)
				sBreakpoint[i-1] = sBreakpoint[i];
			--wBreakpointCount;
			return;
		}
	}

	// breakpoint not found
	if (wBreakpointCount >= MAXBREAKPOINTS)	{		// breakpoint buffer full 
		AbortMessage(_T("Reached maximum number of breakpoints !"));
		return;
	}

	sBreakpoint[wBreakpointCount].bEnable = TRUE;
	sBreakpoint[wBreakpointCount].nType   = BP_EXEC;
	sBreakpoint[wBreakpointCount].dwAddr  = dwAddr;
	++wBreakpointCount;
}

//
// convert byte register to string
//
static LPTSTR RegToStr(BYTE *pReg, WORD wByt)
{
	static TCHAR szBuffer[32];

	WORD i, j;

	for (i = 0, j = 0;i < wByt;++i) {
		szBuffer[j++] = cHex[pReg[i]>>4];
		szBuffer[j++] = cHex[pReg[i]&0xF];
	}
	szBuffer[j] = 0;

	return szBuffer;
}
static LPTSTR RegToStrBin(BYTE *pReg, WORD wByt)
{
	static TCHAR szBuffer[32];

	WORD i, j;

	for (i = 0, j = 0;i < wByt;++i) {
		szBuffer[j++] = cHex[(pReg[i]&0x80) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x40) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x20) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x10) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x08) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x04) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x02) ? 1 : 0];
		szBuffer[j++] = cHex[(pReg[i]&0x01) ? 1 : 0];
	}
	szBuffer[j] = 0;

	return szBuffer;
}

//
// convert string to byte register
//
static VOID StrToReg(BYTE *pReg, WORD wByt, LPTSTR lpszValue)
{
	int i, nValuelen, val;

	nValuelen = lstrlen(lpszValue)/2;
	for (i = 0; i < wByt; i++) {
		while (*lpszValue == _T(' '))			// no character in string
			lpszValue++;
		if (*lpszValue == 0)
			pReg[i] = 0;					// fill with zero
		else {
			// convert to number
			val = _totupper(*lpszValue) - _T('0');
			if (val > 9) val -= _T('A') - _T('9') - 1;
			++lpszValue;
			pReg[i] = _totupper(*lpszValue) - _T('0');
			if (pReg[i] > 9) pReg[i] -= _T('A') - _T('9') - 1;
			++lpszValue;
			pReg[i] += val*16;
		}
	}
}

//
// write code window
//
static VOID ViewCodeWnd(HWND hWnd, DWORD dwAddress)
{
	INT   i, j;
	TCHAR szAddress[128];

	SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
	SendMessage(hWnd, LB_RESETCONTENT, 0, 0);
	for (i = 0; i < MAXCODELINES; ++i) {
		dwAdrLine[i] = dwAddress;
		j = wsprintf(szAddress,
					 ((dwAddress & 0xFFFFFF) == (Chipset.Cpu.PC & 0xFFFFFF)) ? _T("%06X-> ") : _T("%06X   "), dwAddress & 0xFFFFFF);
//		dwAddress = disassemble(dwAddress, &szAddress[j], VIEW_SHORT);
		dwAddress = disassemble(dwAddress, &szAddress[j], VIEW_LONG);
		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM) szAddress);
	}
	SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
}

//
// write memory window
//
static VOID ViewMemWnd(HWND hDlg, DWORD dwAddress)
{
	#define TEXTOFF 32

	INT   i, j, k;
	TCHAR szBuffer[32], szItem[4];
	BYTE  cChar;
	WORD  data;

	szItem[2] = 0;							// end of string
	dwAdrMem = dwAddress;					// save start address of memory window

	// purge all list boxes
	SendDlgItemMessage(hDlg, IDC_DEBUG_MEM_ADDR, LB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hDlg, IDC_DEBUG_MEM_TEXT, LB_RESETCONTENT, 0, 0);
	for (j = 0; j < MEMWNDMAX; ++j)
		SendDlgItemMessage(hDlg, nCol[j], LB_RESETCONTENT, 0, 0);

	for (i = 0; i < MAXMEMLINES; ++i) {
		BYTE  byLineData[MAXMEMITEMS];

		// fetch mapping data line
			
		wsprintf(szBuffer, _T("%06X"), dwAddress & 0x00FFFFFF);

		SendDlgItemMessage(hDlg, IDC_DEBUG_MEM_ADDR, LB_ADDSTRING, 0, (LPARAM) szBuffer);

		for (k = 0, j = 0; j < MAXMEMITEMS; ++j)
		{
			if ((j & 0x01) == 0x00) {
				data = GetWORD(dwAddress);
				byLineData[j] = (data >> 8);
				byLineData[j+1] = data &0x00FF;
				dwAddress += 2;
			}
			// read from fetched data line
			szItem[0] = cHex[byLineData[j]>>4];
			szItem[1] = cHex[byLineData[j]&0x0F];
			// characters are saved in LBS, MSB order
			cChar = byLineData[j];

			SendDlgItemMessage(hDlg, nCol[j], LB_ADDSTRING, 0, (LPARAM) szItem);

			// text field
			szBuffer[j] = (isprint(cChar) != 0) ? cChar : _T('.');
		}
		szBuffer[j] = 0;					// end of text string
		SendDlgItemMessage(hDlg, IDC_DEBUG_MEM_TEXT, LB_ADDSTRING, 0, (LPARAM) szBuffer);
	}
	#undef TEXTOFF
}


//################
//#
//#    High level Window draw routines
//#
//################

//
// update code window with scrolling
//
static VOID UpdateCodeWnd(HWND hDlg)
{
	DWORD dwAddress;
	INT   i, j;

	HWND hWnd = GetDlgItem(hDlg, IDC_DEBUG_CODE);

	j = SendMessage(hWnd, LB_GETCOUNT, 0, 0);	// no. of items in table

	// seach for actual address in code area
	for (i = 0; i < j; ++i) {
		if (dwAdrLine[i] == Chipset.Cpu.PC)		// found new pc address line
			break;
	}

	// redraw code window
	dwAddress = dwAdrLine[0];				// redraw list box with modified pc
	if (i == j)	{							// address not found
		dwAddress = Chipset.Cpu.PC;						// begin with actual pc
		i = 0;								// set cursor on top
	}
	if (i > 14)	{							// cursor near bottom line
		dwAddress = dwAdrLine[i - 14];		// move that pc is in line 11
		i = 14;								// set cursor to actual pc
	}
	ViewCodeWnd(hWnd, dwAddress);			// init code area
	SendMessage(hWnd, LB_SETCURSEL, i, 0);		// set
}

//
// update register window
//
static VOID UpdateRegisterWnd(HWND hDlg)
{
	TCHAR szBuffer[64];

	bRegUpdate[ 0] = (Chipset.Cpu.A[0].l != OldChipset.Cpu.A[0].l);				// A0
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[0].l);
	SetDlgItemText(hDlg, IDC_D_A0, szBuffer);
	bRegUpdate[ 1] = (Chipset.Cpu.A[1].l != OldChipset.Cpu.A[1].l);				// A1
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[1].l);
	SetDlgItemText(hDlg, IDC_D_A1, szBuffer);
	bRegUpdate[ 2] = (Chipset.Cpu.A[2].l != OldChipset.Cpu.A[2].l);				// A2
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[2].l);
	SetDlgItemText(hDlg, IDC_D_A2, szBuffer);
	bRegUpdate[ 3] = (Chipset.Cpu.A[3].l != OldChipset.Cpu.A[3].l);				// A3
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[3].l);
	SetDlgItemText(hDlg, IDC_D_A3, szBuffer);
	bRegUpdate[ 4] = (Chipset.Cpu.A[4].l != OldChipset.Cpu.A[4].l);				// A4
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[4].l);
	SetDlgItemText(hDlg, IDC_D_A4, szBuffer);
	bRegUpdate[ 5] = (Chipset.Cpu.A[5].l != OldChipset.Cpu.A[5].l);				// A5
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[5].l);
	SetDlgItemText(hDlg, IDC_D_A5, szBuffer);
	bRegUpdate[ 6] = (Chipset.Cpu.A[6].l != OldChipset.Cpu.A[6].l);				// A6
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[6].l);
	SetDlgItemText(hDlg, IDC_D_A6, szBuffer);

	bRegUpdate[ 7] = (Chipset.Cpu.A[7].l != OldChipset.Cpu.A[7].l);				// USP
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[7].l);
	SetDlgItemText(hDlg, IDC_D_USP, szBuffer);

	bRegUpdate[ 8] = (Chipset.Cpu.A[8].l != OldChipset.Cpu.A[8].l);				// SSP
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.A[8].l);
	SetDlgItemText(hDlg, IDC_D_SSP, szBuffer);

	bRegUpdate[ 9] = (Chipset.Cpu.D[0].l != OldChipset.Cpu.D[0].l);				// D0
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[0].l);
	SetDlgItemText(hDlg, IDC_D_D0, szBuffer);
	bRegUpdate[10] = (Chipset.Cpu.D[1].l != OldChipset.Cpu.D[1].l);				// D1
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[1].l);
	SetDlgItemText(hDlg, IDC_D_D1, szBuffer);
	bRegUpdate[11] = (Chipset.Cpu.D[2].l != OldChipset.Cpu.D[2].l);				// D2
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[2].l);
	SetDlgItemText(hDlg, IDC_D_D2, szBuffer);
	bRegUpdate[12] = (Chipset.Cpu.D[3].l != OldChipset.Cpu.D[3].l);				// D3
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[3].l);
	SetDlgItemText(hDlg, IDC_D_D3, szBuffer);
	bRegUpdate[13] = (Chipset.Cpu.D[4].l != OldChipset.Cpu.D[4].l);				// D4
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[4].l);
	SetDlgItemText(hDlg, IDC_D_D4, szBuffer);
	bRegUpdate[14] = (Chipset.Cpu.D[5].l != OldChipset.Cpu.D[5].l);				// D5
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[5].l);
	SetDlgItemText(hDlg, IDC_D_D5, szBuffer);
	bRegUpdate[15] = (Chipset.Cpu.D[6].l != OldChipset.Cpu.D[6].l);				// D6
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[6].l);
	SetDlgItemText(hDlg, IDC_D_D6, szBuffer);
	bRegUpdate[16] = (Chipset.Cpu.D[7].l != OldChipset.Cpu.D[7].l);				// D7
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.D[7].l);
	SetDlgItemText(hDlg, IDC_D_D7, szBuffer);

	bRegUpdate[17] = (Chipset.Cpu.PC != OldChipset.Cpu.PC);						// PC
	wsprintf(szBuffer, _T("%08X"), Chipset.Cpu.PC);
	SetDlgItemText(hDlg, IDC_D_PC, szBuffer);

	bRegUpdate[18] = (Chipset.Cpu.SR.sr != OldChipset.Cpu.SR.sr);				// SR
	wsprintf(szBuffer, _T("%04X"), Chipset.Cpu.SR.sr);
	SetDlgItemText(hDlg, IDC_D_SR, szBuffer);	

	bRegUpdate[19] = (Chipset.Cpu.SR.X != OldChipset.Cpu.SR.X);					// X
	wsprintf(szBuffer, _T("X=%d"), Chipset.Cpu.SR.X);
	SetDlgItemText(hDlg, IDC_D_X, szBuffer);	
	bRegUpdate[20] = (Chipset.Cpu.SR.N != OldChipset.Cpu.SR.N);					// N
	wsprintf(szBuffer, _T("N=%d"), Chipset.Cpu.SR.N);
	SetDlgItemText(hDlg, IDC_D_N, szBuffer);	
	bRegUpdate[21] = (Chipset.Cpu.SR.Z != OldChipset.Cpu.SR.Z);					// Z
	wsprintf(szBuffer, _T("Z=%d"), Chipset.Cpu.SR.Z);
	SetDlgItemText(hDlg, IDC_D_Z, szBuffer);	
	bRegUpdate[22] = (Chipset.Cpu.SR.V != OldChipset.Cpu.SR.V);					// V
	wsprintf(szBuffer, _T("V=%d"), Chipset.Cpu.SR.V);
	SetDlgItemText(hDlg, IDC_D_V, szBuffer);	
	bRegUpdate[23] = (Chipset.Cpu.SR.C != OldChipset.Cpu.SR.C);					// C
	wsprintf(szBuffer, _T("C=%d"), Chipset.Cpu.SR.C);
	SetDlgItemText(hDlg, IDC_D_C, szBuffer);	
	bRegUpdate[24] = (Chipset.Cpu.SR.MASK != OldChipset.Cpu.SR.MASK);			// I
	wsprintf(szBuffer, _T("I=%X"), Chipset.Cpu.SR.MASK);
	SetDlgItemText(hDlg, IDC_D_I, szBuffer);	
	bRegUpdate[25] = (Chipset.Cpu.SR.T != OldChipset.Cpu.SR.T);					// T
	wsprintf(szBuffer, _T("T=%d"), Chipset.Cpu.SR.T);
	SetDlgItemText(hDlg, IDC_D_T, szBuffer);	
	bRegUpdate[26] = (Chipset.Cpu.SR.S != OldChipset.Cpu.SR.S);					// S
	wsprintf(szBuffer, _T("S=%d"), Chipset.Cpu.SR.S);
	SetDlgItemText(hDlg, IDC_D_S, szBuffer);	

	// bRegUpdate[19] = (Chipset.cycles != OldChipset.cycles);						// Cycles
	wsprintf(szBuffer, _T("%9d"), Chipset.cycles);
	SetDlgItemText(hDlg, IDC_D_CYCLES, szBuffer);
	// bRegUpdate[20] = (Chipset.dcycles != OldChipset.dcycles);					// Dcycles
	wsprintf(szBuffer, _T("%3d"), Chipset.dcycles);
	SetDlgItemText(hDlg, IDC_D_DCYCLES, szBuffer);
	
	switch (Chipset.Cpu.State) {
		case HALT: 
			wsprintf(szBuffer, _T("HALT"));
			break;
		case NORMAL:
			wsprintf(szBuffer, _T("NORMAL"));
			break;
		case EXCEPTION:
			if (Chipset.Cpu.lastVector < 64) 
				wsprintf(szBuffer, _T("%s (%d)"), cExceptions[Chipset.Cpu.lastVector], Chipset.Cpu.lastVector);
			else
				wsprintf(szBuffer, _T("User Interrupt %d"), Chipset.Cpu.lastVector);
			break;
	}
	SetDlgItemText(hDlg, IDC_D_STATE, szBuffer);
}

//
// update memory window
//
static VOID UpdateMemoryWnd(HWND hDlg)
{
	ViewMemWnd(hDlg, dwAdrMem);
}

//
// update complete debugger dialog
//
static VOID OnUpdate(HWND hDlg)
{
	nDbgState = DBG_STEPINTO;				// state "step into"
	dwDbgStopPC = -1;						// disable "cursor stop address"

	// enable debug buttons
	EnableMenuItem(GetMenu(hDlg), ID_DEBUG_RUN, MF_ENABLED);
	EnableMenuItem(GetMenu(hDlg), ID_DEBUG_RUNCURSOR, MF_ENABLED);
	EnableMenuItem(GetMenu(hDlg), ID_DEBUG_STEP, MF_ENABLED);
	EnableMenuItem(GetMenu(hDlg), ID_DEBUG_STEPOVER, MF_ENABLED);
	EnableMenuItem(GetMenu(hDlg), ID_DEBUG_STEPOUT, MF_ENABLED);
//	EnableMenuItem(GetMenu(hDlg), ID_INFO_LASTINSTRUCTIONS, MF_ENABLED);
//	EnableMenuItem(GetMenu(hDlg), ID_INFO_WRITEONLYREG, MF_ENABLED);

	// enable toolbar buttons
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_RUN, MAKELONG((TRUE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_BREAK, MAKELONG((FALSE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_RUNCURSOR, MAKELONG((TRUE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_STEP, MAKELONG((TRUE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_STEPOVER, MAKELONG((TRUE), 0));
	SendMessage(hWndToolbar, TB_ENABLEBUTTON, ID_DEBUG_STEPOUT, MAKELONG((TRUE), 0));

	// update windows
	UpdateCodeWnd(hDlg);					// update code window
	UpdateRegisterWnd(hDlg);				// update registers window
	UpdateMemoryWnd(hDlg);					// update memory window
	ShowWindow(hDlg, SW_RESTORE);			// pop up if minimized
	SetFocus(hDlg);							// set focus to debugger
}


//################
//#
//#    Virtual key handler
//#
//################

//
// toggle breakpoint key handler (F2)
//
static BOOL OnKeyF2(HWND hDlg)
{
	HWND hWnd;
	RECT rc;
	LONG i;

	hWnd = GetDlgItem(hDlg, IDC_DEBUG_CODE);
	i = SendMessage(hWnd, LB_GETCURSEL, 0, 0);	// get selected item
	ToggleBreakpoint(0x00FFFFFF & dwAdrLine[i]);			// toggle breakpoint at address
	// update region of toggled item
	SendMessage(hWnd, LB_GETITEMRECT, i, (LPARAM)&rc);
	InvalidateRect(hWnd, &rc, TRUE);
	return -1;								// call windows default handler
}

//
// run key handler (F5)
//
static BOOL OnKeyF5(HWND hDlg)
{
	HWND  hWnd;
	INT   i, nPos;
	TCHAR szBuf[128];

	if (nDbgState != DBG_RUN) {				// emulation stopped
		DisableMenuKeys(hDlg);				// disable menu keys

		hWnd = GetDlgItem(hDlg, IDC_DEBUG_CODE);
		nPos = SendMessage(hWnd, LB_GETCURSEL, 0, 0);

		// clear "->" in code window
		for (i = 0; i < MAXCODELINES; ++i) {
			SendMessage(hWnd, LB_GETTEXT, i, (LPARAM) szBuf);
			if (szBuf[7] != _T(' ')) {				// PC in window
				szBuf[7] = szBuf[8] = _T(' ');
				SendMessage(hWnd, LB_DELETESTRING, i, 0);
				SendMessage(hWnd, LB_INSERTSTRING, i, (LPARAM)(LPTSTR)szBuf);
				break;
			}
		}
		SendMessage(hWnd, LB_SETCURSEL, nPos, 0);

		nDbgState = DBG_RUN;				// state "run"
		OldChipset = Chipset;				// save chipset values
		SetEvent(hEventDebug);				// run emulation
	}
	return -1;								// call windows default handler
    UNREFERENCED_PARAMETER(hDlg);
}

//
// step cursor key handler (F6)
//
static BOOL OnKeyF6(HWND hDlg)
{
	if (nDbgState != DBG_RUN) {				// emulation stopped
		// get address of selected item
		INT nPos = SendDlgItemMessage(hDlg, IDC_DEBUG_CODE, LB_GETCURSEL, 0, 0);
		dwDbgStopPC = dwAdrLine[nPos];

		OnKeyF5(hDlg);						// run emulation
	}
	return -1;								// call windows default handler
}

//
// step into key handler (F7)
//
static BOOL OnKeyF7(HWND hDlg)
{
	if (nDbgState != DBG_RUN) {				// emulation stopped
		nDbgState = DBG_STEPINTO;				// state "step into"
		OldChipset = Chipset;					// save chipset values
		SetEvent(hEventDebug);					// run emulation
	}
	return -1;								// call windows default handler
    UNREFERENCED_PARAMETER(hDlg);
}

//
// step over key handler (F8)
//
static BOOL OnKeyF8(HWND hDlg)
{
	return -1;								// call windows default handler
    UNREFERENCED_PARAMETER(hDlg);
}

//
// step out key handler (F9)
//
static BOOL OnKeyF9(HWND hDlg)
{
	if (nDbgState != DBG_RUN) {				// emulation stopped
		DisableMenuKeys(hDlg);				// disable menu keys
		nDbgState = DBG_STEPOUT;			// state "step out"
		OldChipset = Chipset;				// save chipset values
		SetEvent(hEventDebug);				// run emulation
	}
	return -1;								// call windows default handler
    UNREFERENCED_PARAMETER(hDlg);
}

//
// break key handler (F11)
//
static BOOL OnKeyF11(HWND hDlg)
{
	nDbgState = DBG_STEPINTO;				// state "step into"
	return -1;								// call windows default handler
    UNREFERENCED_PARAMETER(hDlg);
}

//
// view of given address in disassembler window
//
static BOOL OnCodeGoAdr(HWND hDlg)
{
	DWORD dwAddress = -1;					// no address given

	OnEnterAddress(hDlg, &dwAddress);
	if (dwAddress != -1) {
		HWND hWnd = GetDlgItem(hDlg, IDC_DEBUG_CODE);
		ViewCodeWnd(hWnd, (DWORD)(dwAddress & 0xFFFFFF));
		SendMessage(hWnd, LB_SETCURSEL, 0, 0);
	}
	return -1;								// call windows default handler
}

//
// view pc in disassembler window
//
static BOOL OnCodeGoPC(HWND hDlg)
{
	UpdateCodeWnd(hDlg);
	return 0;
}

//
// set pc to selection
//
static BOOL OnCodeSetPcToSelection(HWND hDlg)
{
	return OnCodeGoPC(hDlg);
}

//
// view from address in memory window
//
static BOOL OnMemGoDx(HWND hDlg, DWORD dwAddress)
{
	HWND hWnd = GetDlgItem(hDlg, IDC_DEBUG_MEM_COL0);

	ViewMemWnd(hDlg, dwAddress);
	SendMessage(hWnd, LB_SETCURSEL, 0, 0);
	SetFocus(hWnd);
	return -1;								// call windows default handler
}

//
// view of given address in memory window
//
static BOOL OnMemGoAdr(HWND hDlg)
{
	DWORD dwAddress = -1;					// no address given

	OnEnterAddress(hDlg, &dwAddress);
	if (dwAddress != -1)					// not Cancel key
		//while (dwAddress >= (0x8000 + Chipset.RamSize))
		//	dwAddress -= 0x8000 + Chipset.RamSize;
		OnMemGoDx(hDlg, dwAddress);
	return -1;								// call windows default handler
}

//
// clear all breakpoints
//
static BOOL OnClearAll(HWND hDlg)
{
	wBreakpointCount = 0;
	// redraw code window
	InvalidateRect(GetDlgItem(hDlg, IDC_DEBUG_CODE), NULL, TRUE);
	return 0;
}

//
// toggle breakpoints on Exceptions
//
static BOOL OnExceptionsBreak(HWND hDlg)
{
	bBreakExceptions = !bBreakExceptions;
	CheckMenuItem(GetMenu(hDlg), ID_BREAKPOINTS_EXCEP, bBreakExceptions ? MF_CHECKED : MF_UNCHECKED);	return 0;
}
//
// toggle menu selection
//
static BOOL OnToggleMenuItem(HWND hDlg, UINT uIDCheckItem, BOOL *bCheck)
{
	*bCheck = !*bCheck;					// toggle flag
	CheckMenuItem(GetMenu(hDlg), uIDCheckItem, *bCheck ? MF_CHECKED : MF_UNCHECKED);
	return 0;
}

//
// new register setting, not done
//
static BOOL	OnLButtonUp(HWND hDlg, LPARAM lParam)
{
	TCHAR szBuffer[32];
	POINT pt;
	HWND  hWnd;
	INT   nId;

	if (nDbgState != DBG_STEPINTO)			// not in single step mode
		return TRUE;

	POINTSTOPOINT(pt, MAKEPOINTS(lParam));

	// handle of selected window
	hWnd = ChildWindowFromPointEx(hDlg, pt, CWP_SKIPDISABLED);
	nId  = GetDlgCtrlID(hWnd);				// control ID of window

	SendMessage(hWnd, WM_GETTEXT, ARRAYSIZEOF(szBuffer), (LPARAM)szBuffer);
	switch (nId) {
		case IDC_D_D0: // R00 R01
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 0);
			break;
		case IDC_D_D1: // R02 R03
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 2);
			break;
		case IDC_D_D2: // R04 R05
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 4);
			break;
		case IDC_D_D3: // R06 R07
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 6);
			break;
		case IDC_D_D4: // R10 R11
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 8);
			break;
		case IDC_D_D5: // R12 R13
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 10);
			break;
		case IDC_D_D6: // R14 R15
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 12);
			break;
		case IDC_D_D7: // R16 R17
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 14);
			break;
		case IDC_D_A0: // R20 R21
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 16);
			break;
		case IDC_D_A1: // R22 R23
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 18);
			break;
		case IDC_D_A2: // R24 R25
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 20);
			break;
		case IDC_D_A3: // R26 R27
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 22);
			break;
		case IDC_D_A4: // R30 R31
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 24);
			break;
		case IDC_D_A5: // R32 R33
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 26);
			break;
		case IDC_D_A6: // R34 R35
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 28);
			break;
		case IDC_D_USP: // R36 R37
			OnNewValue(&szBuffer[0]);
			// _stscanf(&szBuffer[0], _T("%4hX"), Chipset.R + 30);
			break;
		case IDC_D_SSP: // R40 R47
			OnNewValue(&szBuffer[0]);
			// StrToReg(Chipset.R+32, 8, &szBuffer[0]);
			break;
		case IDC_D_PC: // R50 R57
			OnNewValue(&szBuffer[0]);
			// StrToReg(Chipset.R+40, 8, &szBuffer[0]);
			break;

		case IDC_D_X:		// DCM
			// Chipset.DCM = !Chipset.DCM;
			break;
		case IDC_D_N:		// CY
			// Chipset.CY = !Chipset.CY;
			break;
		case IDC_D_Z:		// OVF
			// Chipset.OVF = !Chipset.OVF;
			break;
		case IDC_D_V:		// LSB
			// Chipset.LSB = !Chipset.LSB;
			break;
		case IDC_D_C:		// MSB
			// Chipset.MSB = !Chipset.MSB;
			break;
		case IDC_D_T:		// Z
			// Chipset.Z = !Chipset.Z;
			break;
		case IDC_D_S:		// LDZ
			// Chipset.LDZ = !Chipset.LDZ;
			break;
		case IDC_D_I:		// RDZ
			// Chipset.RDZ = !Chipset.RDZ;
			break;
	}
	UpdateRegisterWnd(hDlg);				// update register
	return TRUE;
}

//
// double click in list box area
//
static BOOL OnDblClick(HWND hWnd, WORD wId)
{
	TCHAR szBuffer[8];
	BYTE  byData;
	INT   i;
	DWORD dwAddress;

	for (i = 0; i < MEMWNDMAX; ++i)			// scan all Id's
		if (nCol[i] == wId)					// found ID
			break;

	// not IDC_DEBUG_MEM window or module mode -> default handler
	if (i == MEMWNDMAX)
		return FALSE;

	// calculate address of byte
	dwAddress = i;
	i = SendMessage(hWnd, LB_GETCARETINDEX, 0, 0);
	dwAddress += MAXMEMITEMS * i + dwAdrMem;

	// enter new value
	SendMessage(hWnd, LB_GETTEXT, i, (LPARAM) szBuffer);
	OnNewValue(szBuffer);
	sscanf_s(szBuffer, _T("%2X"), &byData);

	UpdateMemoryWnd(GetParent(hWnd));		// update memory window
	SendMessage(hWnd, LB_SETCURSEL, i, 0);
	return FALSE;
}

//
// request for context menu
//
static VOID OnContextMenu(HWND hDlg, LPARAM lParam, WPARAM wParam)
{
	POINT pt;
	INT   nId;

	POINTSTOPOINT(pt, MAKEPOINTS(lParam));	// mouse position
	nId  = GetDlgCtrlID((HWND) wParam);		// control ID of window

	switch(nId) {
		case IDC_DEBUG_CODE: // handle code window
			TrackPopupMenu(hMenuCode, 0, pt.x, pt.y, 0, hDlg, NULL);
			break;
		case IDC_DEBUG_MEM_COL0:
		case IDC_DEBUG_MEM_COL1:
		case IDC_DEBUG_MEM_COL2:
		case IDC_DEBUG_MEM_COL3:
		case IDC_DEBUG_MEM_COL4:
		case IDC_DEBUG_MEM_COL5:
		case IDC_DEBUG_MEM_COL6:
		case IDC_DEBUG_MEM_COL7:
		case IDC_DEBUG_MEM_COL8:
		case IDC_DEBUG_MEM_COL9:
		case IDC_DEBUG_MEM_COL10:
		case IDC_DEBUG_MEM_COL11:
		case IDC_DEBUG_MEM_COL12:
		case IDC_DEBUG_MEM_COL13:
		case IDC_DEBUG_MEM_COL14:
		case IDC_DEBUG_MEM_COL15: // handle memory window
			TrackPopupMenu(hMenuMem, 0, pt.x, pt.y, 0, hDlg, NULL);
			break;
	}
}

//################
//#
//#    Dialog handler
//#
//################

//
// handle right/left keys in memory window
//
static __inline BOOL OnKeyRightLeft(HWND hWnd, WPARAM wParam)
{
	HWND hWndNew;
	WORD wX, wY;
	INT  nId;

	nId  = GetDlgCtrlID(hWnd);				// control ID of window

	for (wX = 0; wX < MEMWNDMAX; ++wX)		// scan all Id's
		if (nCol[wX] == nId)				// found ID
			break;

	if (wX == MEMWNDMAX) return -1;			// not IDC_DEBUG_MEM window, default handler

	// delete old focus
	wY = HIWORD(wParam);
	SendMessage(hWnd, LB_SETCURSEL, -1, 0);

	// new position
	wX = (LOWORD(wParam) == VK_RIGHT) ? (wX + 1) : (wX + MEMWNDMAX - 1);
	wX %= MEMWNDMAX;

	// set new focus
	hWndNew = GetDlgItem(GetParent(hWnd), nCol[wX]);
	SendMessage(hWndNew, LB_SETCURSEL, wY, 0);
	SetFocus(hWndNew);
	return -2;
}

//
// handle (page) up/down keys in memory window
//
static __inline BOOL OnKeyUpDown(HWND hWnd, WPARAM wParam)
{
	INT  wX, wY;
	INT  nId;

	nId  = GetDlgCtrlID(hWnd);				// control ID of window

	for (wX = 0; wX < MEMWNDMAX; ++wX)		// scan all Id's
		if (nCol[wX] == nId)				// found ID
			break;

	if (wX == MEMWNDMAX) return -1;			// not IDC_DEBUG_MEM window, default handler

	wY = HIWORD(wParam);					// get old focus

	switch(LOWORD(wParam)) {
		case VK_NEXT:
			dwAdrMem = dwAdrMem + MAXMEMITEMS * MAXMEMLINES;
			UpdateMemoryWnd(GetParent(hWnd));
			SendMessage(hWnd, LB_SETCURSEL, wY, 0);
			return -2;

		case VK_PRIOR:
			dwAdrMem = dwAdrMem - MAXMEMITEMS * MAXMEMLINES;
			UpdateMemoryWnd(GetParent(hWnd));
			SendMessage(hWnd, LB_SETCURSEL, wY, 0);
			return -2;

		case VK_DOWN:
			if (wY+1 >= MAXMEMLINES) {
				dwAdrMem = dwAdrMem + MAXMEMITEMS;
				UpdateMemoryWnd(GetParent(hWnd));
				SendMessage(hWnd, LB_SETCURSEL, wY, 0);
				return -2;
			}
			break;

		case VK_UP:
			if (wY == 0) {
				dwAdrMem = dwAdrMem - MAXMEMITEMS;
				UpdateMemoryWnd(GetParent(hWnd));
				SendMessage(hWnd, LB_SETCURSEL, wY, 0);
				return -2;
			}
			break;
	}
	return -1;
}

//
// handle keys in code window
//
static __inline BOOL OnKeyCodeWnd(HWND hDlg, WPARAM wParam)
{
	HWND hWnd  = GetDlgItem(hDlg, IDC_DEBUG_CODE);
	WORD wKey  = LOWORD(wParam);
	WORD wItem = HIWORD(wParam);

	// down key on last line
	if ((wKey == VK_DOWN || wKey == VK_NEXT) && wItem == MAXCODELINES - 1) {
		ViewCodeWnd(hWnd, dwAdrLine[1]);
		SendMessage(hWnd, LB_SETCURSEL, wItem, 0);
	}
	// up key on first line
	if ((wKey == VK_UP || wKey == VK_PRIOR) && wItem == 0) {
		if (dwAdrLine[0] > 0) ViewCodeWnd(hWnd, (DWORD)(dwAdrLine[0]-2));
	}

	if (wKey == _T('G')) return OnCodeGoAdr(GetParent(hWnd)); // goto new address

	return -1;
}

//
// handle drawing in code window
//
static __inline BOOL OnDrawCodeWnd(LPDRAWITEMSTRUCT lpdis)
{
	TCHAR    szBuf[128];
	COLORREF crBkColor;
	COLORREF crTextColor;
	BOOL     bBrk, bPC;

	if (lpdis->itemID == -1)				// no item in list box
		return TRUE;

	// get item text
	SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LONG)(LPTSTR)szBuf);

	// check for codebreakpoint
	bBrk = CheckBreakpoint(dwAdrLine[lpdis->itemID], 1, BP_EXEC);
	bPC = szBuf[6] != _T(' ');				// check if line of program counter

	crTextColor = COLOR_WHITE;				// standard text color

	if (lpdis->itemState & ODS_SELECTED) {	// cursor line
		if (bPC) {							// PC line
			crBkColor = bBrk ? COLOR_DKGRAY : COLOR_TEAL;
		} else	{							// normal line
			crBkColor = bBrk ? COLOR_PURPLE : COLOR_NAVY;
		}
	} else {									// not cursor line
		if (bPC) {							// PC line
			crBkColor = bBrk ? COLOR_OLIVE : COLOR_GREEN;
		} else {								// normal line
			if (bBrk) {
				crBkColor   = COLOR_MAROON;
			} else {
				crBkColor   = COLOR_WHITE;
				crTextColor = COLOR_BLACK;
			}
		}
	}

	// write Text
	crBkColor   = SetBkColor(lpdis->hDC, crBkColor);
	crTextColor = SetTextColor(lpdis->hDC, crTextColor);

	ExtTextOut(lpdis->hDC, (int)(lpdis->rcItem.left)+2, (int)(lpdis->rcItem.top),
	           ETO_OPAQUE, (LPRECT)&lpdis->rcItem, szBuf, lstrlen(szBuf), NULL);

	SetBkColor(lpdis->hDC, crBkColor);
	SetTextColor(lpdis->hDC, crTextColor);

	if (lpdis->itemState & ODS_FOCUS)		// redraw focus
		DrawFocusRect(lpdis->hDC, &lpdis->rcItem);

	return TRUE;							// focus handled here
}

//
// detect changed register
//
static __inline BOOL OnCtlColorStatic(HWND hWnd)
{
	BOOL bError = FALSE;					// not changed
	int i;

	int nId = GetDlgCtrlID(hWnd);
	
	for (i=0; i < UPTMAX; i++)
		if (rUpt[i] == nId) {
			bError = bRegUpdate[i];
			break;
		}
	return bError;
}


//################
//#
//#    Public functions
//#
//################

//
// check for breakpoints
//
BOOL CheckBreakpoint(DWORD dwAddr, DWORD dwRange, UINT nType)
{
	INT i;

	dwAddr &= 0x00FFFFFF;					// 24 bits mask
	for (i = 0; i < wBreakpointCount; ++i) {	// scan all breakpoints
		// check address range and type
		if (   sBreakpoint[i].bEnable
			&& sBreakpoint[i].dwAddr >= dwAddr && sBreakpoint[i].dwAddr < (DWORD)(dwAddr + dwRange)
			&& (sBreakpoint[i].nType & nType) != 0)
			return TRUE;
	}
	return FALSE;
}

//
// notify debugger that emulation stopped
//
VOID NotifyDebugger(BOOL bType)				// update registers
{
	_ASSERT(hDlgDebug);						// debug dialog box open
	PostMessage(hDlgDebug, WM_UPDATE, 0, 0);
}

//
// disable debugger
//
VOID DisableDebugger(VOID)
{
	if (hDlgDebug)							// debugger running
		DestroyWindow(hDlgDebug);			// then close debugger to renter emulation
}


//################
//#
//#    Debugger Message loop
//#
//################

//
// ID_TOOL_DEBUG
//
static __inline HWND CreateToolbar(HWND hWnd)
{
	HRSRC        hRes;
	HGLOBAL      hGlobal;
	CToolBarData *pData;
    TBBUTTON     *ptbb;
	INT          i, j;

	HWND hWndToolbar = NULL;				// toolbar window

	InitCommonControls();					// ensure that common control DLL is loaded

	if ((hRes = FindResource(hApp, MAKEINTRESOURCE(IDR_DEBUG_TOOLBAR), RT_TOOLBAR)) == NULL)
		goto quit;

	if ((hGlobal = LoadResource(hApp, hRes)) == NULL)
		goto quit;

	if ((pData = (CToolBarData*) LockResource(hGlobal)) == NULL)
		goto unlock;

	_ASSERT(pData->wVersion == 1);			// toolbar resource version

	// alloc memory for TBBUTTON stucture
    if (!(ptbb = (PTBBUTTON) HeapAlloc(hHeap, 0, pData->wItemCount*sizeof(TBBUTTON))))
		goto unlock;

	// fill TBBUTTON stucture with resource data
	for (i = j = 0; i < pData->wItemCount; ++i) {
		if (pData->aItems[i]) {
			ptbb[i].iBitmap = j++;
			ptbb[i].fsStyle = TBSTYLE_BUTTON;
		} else {
			ptbb[i].iBitmap = 5;			// separator width
			ptbb[i].fsStyle = TBSTYLE_SEP;
		}
		ptbb[i].idCommand = pData->aItems[i];
		ptbb[i].fsState = TBSTATE_ENABLED;
		ptbb[i].dwData  = 0;
		ptbb[i].iString = j;
    }

	hWndToolbar = CreateToolbarEx(hWnd, WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS,
								  IDR_DEBUG_TOOLBAR, j, hApp, IDR_DEBUG_TOOLBAR, ptbb, pData->wItemCount,
								  pData->wWidth, pData->wHeight, pData->wWidth, pData->wHeight,
								  sizeof(TBBUTTON));

	HeapFree(hHeap, 0, ptbb);

unlock:
	FreeResource(hGlobal);
quit:
	return hWndToolbar;
}

static BOOL CALLBACK Debugger(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	static HMENU hMenuMainCode, hMenuMainMem, hMenuMainStack;

	WINDOWPLACEMENT wndpl;
 	TEXTMETRIC tm;
	HDC   hDC;
	HFONT hFont;
	INT   i;

	switch (message) {
		case WM_INITDIALOG:
			SetWindowLocation(hDlg, nDbgPosX, nDbgPosY);
			SetClassLong(hDlg, GCL_HICON, (LONG) LoadIcon(hApp, MAKEINTRESOURCE(IDI_HP98X6)));
			hWndToolbar = CreateToolbar(hDlg);	// add toolbar
//			CheckMenuItem(GetMenu(hDlg), ID_INTR_STEPOVERINT, bDbgSkipInt ? MF_CHECKED : MF_UNCHECKED);
			hDlgDebug = hDlg;					// handle for debugger dialog
			hEventDebug = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (hEventDebug == NULL)
			{
				AbortMessage(_T("Event creation failed !"));
				return TRUE;
			}
			hMenuMainCode = LoadMenu(hApp, MAKEINTRESOURCE(IDR_DEBUG_CODE));
			_ASSERT(hMenuMainCode);
			hMenuCode = GetSubMenu(hMenuMainCode, 0);
			_ASSERT(hMenuCode);
			hMenuMainMem = LoadMenu(hApp, MAKEINTRESOURCE(IDR_DEBUG_MEM));
			_ASSERT(hMenuMainMem);
			hMenuMem = GetSubMenu(hMenuMainMem, 0);
			_ASSERT(hMenuMem);

			// font settings
			SendDlgItemMessage(hDlg, IDC_STATIC_CODE,      WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hDlg, IDC_STATIC_REGISTERS, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hDlg, IDC_STATIC_MEMORY,    WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hDlg, IDC_STATIC_MISC,      WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));

			// init last instruction circular buffer
			pdwInstrArray = HeapAlloc(hHeap, 0, INSTRSIZE*sizeof(*pdwInstrArray));
			wInstrSize = INSTRSIZE;				// size of last instruction array
			wInstrWp = wInstrRp = 0;			// write/read pointer

			dwDbgStopPC = -1;					// no stop address for goto cursor

			nDbgState = DBG_STEPINTO;			// state "step into"

			UpdateWindowStatus();				// disable application menu items
			OldChipset = Chipset;				// save chipset values
			return TRUE;

		case WM_DESTROY:
			if (Chipset.keeptime)
				SetHPTime();					// update time & date
			nDbgState = DBG_OFF;				// debugger inactive
			bInterrupt = TRUE;					// exit opcode loop
			SetEvent(hEventDebug);
			if (pdwInstrArray) {				// free last instruction circular buffer
				HeapFree(hHeap, 0, pdwInstrArray);
				pdwInstrArray = NULL;
			}
			CloseHandle(hEventDebug);
			wndpl.length = sizeof(wndpl);		// save debugger window position
			GetWindowPlacement(hDlg, &wndpl);
			nDbgPosX = wndpl.rcNormalPosition.left;
			nDbgPosY = wndpl.rcNormalPosition.top;
			DestroyMenu(hMenuMainCode);
			DestroyMenu(hMenuMainMem);
 			_ASSERT(hWnd);
			UpdateWindowStatus();				// enable application menu items
			hDlgDebug = NULL;					// debugger windows closed
			break;

		case WM_CLOSE:
			DestroyWindow(hDlg);
			break;

		case WM_UPDATE:
			OnUpdate(hDlg);
			return TRUE;

		case WM_COMMAND:
			switch (HIWORD(wParam)) {
				case LBN_DBLCLK:
					return OnDblClick((HWND) lParam, LOWORD(wParam));

				case LBN_SETFOCUS:
					i = SendMessage((HWND) lParam, LB_GETCARETINDEX, 0, 0);
					SendMessage((HWND) lParam, LB_SETCURSEL, i, 0);
					return TRUE;

				case LBN_KILLFOCUS:
					SendMessage((HWND) lParam, LB_SETCURSEL, -1, 0);
					return TRUE;
			}

			switch (LOWORD(wParam)) {
				case ID_BREAKPOINTS_SETBREAK:     return OnKeyF2(hDlg);
				case ID_DEBUG_RUN:                return OnKeyF5(hDlg);
				case ID_DEBUG_RUNCURSOR:          return OnKeyF6(hDlg);
				case ID_DEBUG_STEP:               return OnKeyF7(hDlg);
				case ID_DEBUG_STEPOVER:           return OnKeyF8(hDlg);
				case ID_DEBUG_STEPOUT:            return OnKeyF9(hDlg);
				case ID_DEBUG_BREAK:              return OnKeyF11(hDlg);
				case ID_DEBUG_CODE_GOADR:         return OnCodeGoAdr(hDlg);
				case ID_DEBUG_CODE_GOPC:          return OnCodeGoPC(hDlg);
				case ID_DEBUG_CODE_SETPCTOSELECT: return OnCodeSetPcToSelection(hDlg);
				case ID_BREAKPOINTS_CODEEDIT:     return OnEditBreakpoint(hDlg);
				case ID_BREAKPOINTS_CLEARALL:     return OnClearAll(hDlg);
				case ID_BREAKPOINTS_EXCEP:		  return OnExceptionsBreak(hDlg);
//				case ID_INFO_LASTINSTRUCTIONS:    return OnInfoIntr(hDlg);
//				case ID_INTR_STEPOVERINT:         return OnToggleMenuItem(hDlg, LOWORD(wParam), &bDbgSkipInt);
				case ID_DEBUG_MEM_GOADR:          return OnMemGoAdr(hDlg);
				case ID_DEBUG_MEM_GOPC:           return OnMemGoDx(hDlg, Chipset.Cpu.PC);
				case ID_DEBUG_MEM_GOUSP:          return OnMemGoDx(hDlg, Chipset.Cpu.A[7].l);
				case ID_DEBUG_MEM_GOSSP:          return OnMemGoDx(hDlg, Chipset.Cpu.A[8].l);
				case ID_DEBUG_MEM_GOA0:           return OnMemGoDx(hDlg, Chipset.Cpu.A[0].l);
				case ID_DEBUG_MEM_GOA1:           return OnMemGoDx(hDlg, Chipset.Cpu.A[1].l);
				case ID_DEBUG_MEM_GOA2:           return OnMemGoDx(hDlg, Chipset.Cpu.A[2].l);
				case ID_DEBUG_MEM_GOA3:           return OnMemGoDx(hDlg, Chipset.Cpu.A[3].l);
				case ID_DEBUG_MEM_GOA4:           return OnMemGoDx(hDlg, Chipset.Cpu.A[4].l);
				case ID_DEBUG_MEM_GOA5:           return OnMemGoDx(hDlg, Chipset.Cpu.A[5].l);
				case ID_DEBUG_MEM_GOA6:           return OnMemGoDx(hDlg, Chipset.Cpu.A[6].l);
				case ID_DEBUG_MEM_FIND:           return OnMemFind(hDlg);
				case ID_DEBUG_CANCEL:             DestroyWindow(hDlg); return TRUE;
			}
			break;

		case WM_VKEYTOITEM:
			switch (LOWORD(wParam))	{				// always valid
				case VK_F2:  return OnKeyF2(hDlg);	// toggle breakpoint
				case VK_F5:  return OnKeyF5(hDlg);	// key run
				case VK_F6:  return OnKeyF6(hDlg);  // key step cursor
				case VK_F7:  return OnKeyF7(hDlg);	// key step into
				case VK_F8:  return OnKeyF8(hDlg);	// key step over
				case VK_F9:  return OnKeyF9(hDlg);  // key step out
				case VK_F11: return OnKeyF11(hDlg);	// key break
			}

			switch(GetDlgCtrlID((HWND) lParam))	{	// calling window
				// handle code window
				case IDC_DEBUG_CODE:
					return OnKeyCodeWnd(hDlg, wParam);

				// handle memory window
				case IDC_DEBUG_MEM_COL0:
				case IDC_DEBUG_MEM_COL1:
				case IDC_DEBUG_MEM_COL2:
				case IDC_DEBUG_MEM_COL3:
				case IDC_DEBUG_MEM_COL4:
				case IDC_DEBUG_MEM_COL5:
				case IDC_DEBUG_MEM_COL6:
				case IDC_DEBUG_MEM_COL7:
				case IDC_DEBUG_MEM_COL8:
				case IDC_DEBUG_MEM_COL9:
				case IDC_DEBUG_MEM_COL10:
				case IDC_DEBUG_MEM_COL11:
				case IDC_DEBUG_MEM_COL12:
				case IDC_DEBUG_MEM_COL13:
				case IDC_DEBUG_MEM_COL14:
				case IDC_DEBUG_MEM_COL15:
					switch (LOWORD(wParam)) {
						case _T('G'):  return OnMemGoAdr(GetParent((HWND) lParam));
						case _T('F'):  return OnMemFind(GetParent((HWND) lParam));
						case VK_RIGHT:
						case VK_LEFT:  return OnKeyRightLeft((HWND) lParam, wParam);
						case VK_NEXT:
						case VK_PRIOR:
						case VK_DOWN:
						case VK_UP:    return OnKeyUpDown((HWND) lParam, wParam);
					}
					break;
			}
			return -1;							// default action

		case WM_LBUTTONUP:
			return OnLButtonUp(hDlg, lParam);
			break;

		case WM_CONTEXTMENU:
			OnContextMenu(hDlg, lParam, wParam);
			break;

		case WM_CTLCOLORSTATIC:					// register color highlighting
			// highlight text?
			if (OnCtlColorStatic((HWND) lParam)) {
				SetTextColor((HDC) wParam, COLOR_RED);
				SetBkColor((HDC) wParam, GetSysColor(CTLCOLOR_DLG));
				return (BOOL) GetStockObject(NULL_BRUSH); // transparent brush
			}
			break;

		case WM_NOTIFY:
			// tooltip for toolbar
			if(((LPNMHDR) lParam)->code == TTN_GETDISPINFO) {
				((LPTOOLTIPTEXT) lParam)->hinst = hApp;
				((LPTOOLTIPTEXT) lParam)->lpszText = MAKEINTRESOURCE(((LPTOOLTIPTEXT) lParam)->hdr.idFrom);
				break;
			}
			break;

		case WM_DRAWITEM:
			if (wParam == IDC_DEBUG_CODE) return OnDrawCodeWnd((LPDRAWITEMSTRUCT) lParam);
			break;

		case WM_MEASUREITEM:
			hDC = GetDC(hDlg);

			// GetTextMetrics from "Courier New 8" font
			hFont = CreateFont(-MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
							   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, _T("Courier New"));

			hFont = SelectObject(hDC, hFont);
			GetTextMetrics(hDC, &tm);
			hFont = SelectObject(hDC, hFont);
			DeleteObject(hFont);

			((LPMEASUREITEMSTRUCT) lParam)->itemHeight = tm.tmHeight;
			lCharWidth = tm.tmAveCharWidth;

			ReleaseDC(hDlg, hDC);
			return TRUE;
	}
	return FALSE;
}

LRESULT OnToolDebug()				// debugger dialogbox call
{
	if ((hDlgDebug = CreateDialog(hApp, MAKEINTRESOURCE(IDD_DEBUG), GetParent(hWnd),
	    (DLGPROC)Debugger)) == NULL)
		AbortMessage(_T("Debugger Dialog Box Creation Error !"));
	return 0;
}


//################
//#
//#    Find dialog box
//#
//################

static __inline BOOL OnFindOK(HWND hDlg, BOOL bASCII, DWORD *pdwAddrLast)
{
	return TRUE;
}

//
// enter find dialog
//
static BOOL CALLBACK Find(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	return FALSE;
	UNREFERENCED_PARAMETER(lParam);
}

static BOOL OnMemFind(HWND hDlg)
{
	return -1;								// call windows default handler
}

//################
//#
//#    New Value dialog box
//#
//################

//
// enter new value dialog
//
static BOOL CALLBACK NewValue(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	static LPTSTR lpszBuffer;				// handle of buffer
	static int    nBufferlen;				// length of buffer

	HWND  hWnd;
	TCHAR szBuffer[64];
	LONG  i;

	switch (message) {
		case WM_INITDIALOG:
			lpszBuffer = (LPTSTR) lParam;
			// length with zero string terminator
			nBufferlen = lstrlen(lpszBuffer)+1;
			_ASSERT(ARRAYSIZEOF(szBuffer) >= nBufferlen);
			SetDlgItemText(hDlg, IDC_NEWVALUE, lpszBuffer);
			return TRUE;
		case WM_COMMAND:
			wParam = LOWORD(wParam);
			switch(wParam) {
				case IDOK:
					hWnd = GetDlgItem(hDlg, IDC_NEWVALUE);
					SendMessage(hWnd, WM_GETTEXT, (WPARAM)nBufferlen, (LPARAM)szBuffer);
					// test if valid hex address
					for (i = 0; i < (LONG) lstrlen(szBuffer); ++i)
					{
						if (_istxdigit(szBuffer[i]) == 0)
						{
							SendMessage(hWnd, EM_SETSEL, 0, -1);
							SetFocus(hWnd);			// focus to edit control
							return FALSE;
						}
					}
					lstrcpy(lpszBuffer, szBuffer);	// copy valid value
					// no break
				case IDCANCEL:
					EndDialog(hDlg, wParam);
					return TRUE;
			}
	}
	return FALSE;
}

static INT OnNewValue(LPTSTR lpszValue)
{
	INT nResult;

	if ((nResult = DialogBoxParam(hApp,
								  MAKEINTRESOURCE(IDD_NEWVALUE),
								  hDlgDebug,
								  (DLGPROC)NewValue,
								  (LPARAM)lpszValue)
								 ) == -1)
		AbortMessage(_T("Input Dialog Box Creation Error !"));
	return nResult;
}


//################
//#
//#    Goto Address dialog box
//#
//################

//
// enter goto address dialog
//
static BOOL CALLBACK EnterAddr(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	static DWORD *dwAddress;

	HWND  hWnd;
	TCHAR szBuffer[16];
	LONG  i;

	switch (message) {
		case WM_INITDIALOG:
			dwAddress = (DWORD *) lParam;
			return TRUE;
		case WM_COMMAND:
			hWnd = GetDlgItem(hDlg, IDC_ENTERADR);
			switch(wParam) {
				case IDOK:
					SendMessage(hWnd, WM_GETTEXT, 8, (LPARAM)szBuffer);
					// test if valid hex address
					for (i = 0; i < (LONG) lstrlen(szBuffer); ++i)
					{
						if (_istxdigit(szBuffer[i]) == 0)
						{
							SendMessage(hWnd, EM_SETSEL, 0, -1);
							SetFocus(hWnd);			// focus to edit control
							return FALSE;
						}
					}
					if (*szBuffer) sscanf_s(szBuffer, _T("%6X"), dwAddress);
					// no break
				case IDCANCEL:
					EndDialog(hDlg, wParam);
					return TRUE;
			}
	}
	return FALSE;
    UNREFERENCED_PARAMETER(wParam);
}

static VOID OnEnterAddress(HWND hDlg, DWORD *dwValue)
{
	if (DialogBoxParam(hApp, MAKEINTRESOURCE(IDD_ENTERADR), hDlg, (DLGPROC)EnterAddr, (LPARAM)dwValue) == -1)
		AbortMessage(_T("Address Dialog Box Creation Error !"));
}


//################
//#
//#    Breakpoint dialog box
//#
//################

//
// enter breakpoint dialog
//
static BOOL CALLBACK EnterBreakpoint(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	static BP_T *sBp;

	HWND  hWnd;
	TCHAR szBuffer[16];
	LONG  i;

	switch (message) {
		case WM_INITDIALOG:
			sBp = (BP_T *) lParam;
			sBp->bEnable = TRUE;
			sBp->nType   = BP_EXEC;
			SendDlgItemMessage(hDlg, IDC_BPCODE, BM_SETCHECK, 1, 0);
			return TRUE;
		case WM_COMMAND:
			hWnd = GetDlgItem(hDlg, IDC_ENTERADR);
			switch(wParam) {
				case IDC_BPCODE:   sBp->nType = BP_EXEC;   return TRUE;
				case IDC_BPACCESS: sBp->nType = BP_ACCESS; return TRUE;
				case IDC_BPREAD:   sBp->nType = BP_READ;   return TRUE;
				case IDC_BPWRITE:  sBp->nType = BP_WRITE;  return TRUE;
				case IDOK:
					SendMessage(hWnd, WM_GETTEXT, 16, (LPARAM)szBuffer);
					// test if valid hex address
					for (i = 0; i < (LONG) lstrlen(szBuffer); ++i)
					{
						if (_istxdigit(szBuffer[i]) == 0)
						{
							SendMessage(hWnd, EM_SETSEL, 0, -1);
							SetFocus(hWnd);
							return FALSE;
						}
					}
					if (*szBuffer) sscanf_s(szBuffer, _T("%6X"), &sBp->dwAddr);
					// no break
				case IDCANCEL:
					EndDialog(hDlg, wParam);
					return TRUE;
			}
	}
	return FALSE;
    UNREFERENCED_PARAMETER(wParam);
}

static VOID OnEnterBreakpoint(HWND hDlg, BP_T *sValue)
{
	if (DialogBoxParam(hApp, MAKEINTRESOURCE(IDD_ENTERBREAK), hDlg, (DLGPROC)EnterBreakpoint, (LPARAM)sValue) == -1)
		AbortMessage(_T("Breakpoint Dialog Box Creation Error !"));
}


//################
//#
//#    Edit breakpoint dialog box
//#
//################

//
// toggle enable/disable breakpoint
//
static BOOL OnToggleCheck(HWND hWnd)
{
	RECT rc;
	INT  i, nItem;

	if ((nItem = SendMessage(hWnd, LB_GETCURSEL, 0, 0)) == LB_ERR)
		return FALSE;

	// get breakpoint number
	i = SendMessage(hWnd, LB_GETITEMDATA, nItem, 0);

	sBreakpoint[i].bEnable = !sBreakpoint[i].bEnable;
	// update region of toggled item
	SendMessage(hWnd, LB_GETITEMRECT, nItem, (LPARAM)&rc);
	InvalidateRect(hWnd, &rc, TRUE);
	return TRUE;
}

//
// handle drawing in breakpoint window
//
static __inline BOOL OnDrawBreakWnd(LPDRAWITEMSTRUCT lpdis)
{
	TCHAR    szBuf[64];
	COLORREF crBkColor, crTextColor;
	HDC      hdcMem;
	HBITMAP  hBmpOld;
	INT      i;

	if (lpdis->itemID == -1)				// no item in list box
		return TRUE;

	if (lpdis->itemState & ODS_SELECTED) {	// cursor line
		crBkColor   = COLOR_NAVY;
		crTextColor = COLOR_WHITE;
	} else {
		crBkColor   = COLOR_WHITE;
		crTextColor = COLOR_BLACK;
	}

	// write Text
	crBkColor   = SetBkColor(lpdis->hDC, crBkColor);
	crTextColor = SetTextColor(lpdis->hDC, crTextColor);

	SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LONG)(LPTSTR)szBuf);
	ExtTextOut(lpdis->hDC, (int)(lpdis->rcItem.left)+17, (int)(lpdis->rcItem.top),
	           ETO_OPAQUE, (LPRECT)&lpdis->rcItem, szBuf, lstrlen(szBuf), NULL);

	SetBkColor(lpdis->hDC, crBkColor);
	SetTextColor(lpdis->hDC, crTextColor);

	// draw checkbox
	i = SendMessage(lpdis->hwndItem, LB_GETITEMDATA, lpdis->itemID, 0);
	hdcMem = CreateCompatibleDC(lpdis->hDC);
	_ASSERT(hBmpCheckBox);
	hBmpOld = SelectObject(hdcMem, hBmpCheckBox);

	BitBlt(lpdis->hDC, lpdis->rcItem.left+2, lpdis->rcItem.top+2,
		   11, lpdis->rcItem.bottom - lpdis->rcItem.top,
		   hdcMem, sBreakpoint[i].bEnable ? 0 : 10, 0, SRCCOPY);

	SelectObject(hdcMem, hBmpOld);
	DeleteDC(hdcMem);

	if (lpdis->itemState & ODS_FOCUS)		// redraw focus
		DrawFocusRect(lpdis->hDC, &lpdis->rcItem);

	return TRUE;							// focus handled here
}

//
// draw breakpoint type
//
static VOID DrawBreakpoint(HWND hWnd, INT i)
{
	LPTSTR szText;
	TCHAR  szBuffer[32];
	INT    nItem;

	switch(sBreakpoint[i].nType) {
		case BP_EXEC:   // code breakpoint
			szText = _T("Code");
			break;
		case BP_READ:   // read memory breakpoint
			szText = _T("Memory Read");
			break;
		case BP_WRITE:  // write memory breakpoint
			szText = _T("Memory Write");
			break;
		case BP_ACCESS: // memory breakpoint
			szText = _T("Memory Access");
			break;
		default:        // unknown breakpoint type
			szText = _T("unknown");
			_ASSERT(0);
	}
	wsprintf(szBuffer, _T("%06X (%s)"), sBreakpoint[i].dwAddr, szText);
	nItem = SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM) szBuffer);
	SendMessage(hWnd, LB_SETITEMDATA, nItem, i);
}

//
// enter edit breakpoint dialog
//
static BOOL CALLBACK EditBreakpoint(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	TEXTMETRIC tm;

	HWND   hWnd;
	HDC    hDC;
	HFONT  hFont;
	BP_T   sBp;
	INT    i, nItem;

	switch (message) {
		case WM_INITDIALOG:
			// font settings
			SendDlgItemMessage(hDlg, IDC_STATIC_BREAKPOINT, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hDlg, IDC_BREAKEDIT_ADD,     WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hDlg, IDC_BREAKEDIT_DELETE,  WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hDlg, IDCANCEL,              WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));

			hBmpCheckBox = LoadBitmap(hApp, MAKEINTRESOURCE(IDB_CHECKBOX));
			_ASSERT(hBmpCheckBox);

			hWnd = GetDlgItem(hDlg, IDC_BREAKEDIT_WND);
			SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
			SendMessage(hWnd, LB_RESETCONTENT, 0, 0);
			for (i = 0; i < wBreakpointCount; ++i)
				DrawBreakpoint(hWnd, i);
			SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
			return TRUE;

		case WM_DESTROY:
			DeleteObject(hBmpCheckBox);
			return TRUE;

		case WM_COMMAND:
			hWnd = GetDlgItem(hDlg, IDC_BREAKEDIT_WND);
			switch (HIWORD(wParam)) {
				case LBN_DBLCLK:
					if (LOWORD(wParam) == IDC_BREAKEDIT_WND) return OnToggleCheck(hWnd);
			}

			switch(wParam) {
				case IDC_BREAKEDIT_ADD:
					sBp.dwAddr = -1;				// no breakpoint given
					OnEnterBreakpoint(hDlg, &sBp);
					if (sBp.dwAddr != -1) {
						for (i = 0; i < wBreakpointCount; ++i) {
							if (sBreakpoint[i].dwAddr == sBp.dwAddr) {
								// tried to add used code breakpoint
								if (sBreakpoint[i].bEnable && (sBreakpoint[i].nType & sBp.nType & (BP_EXEC)) != 0)
									return FALSE;

								// only modify memory breakpoints
								if ((sBreakpoint[i].bEnable == FALSE
										&& (sBreakpoint[i].nType & sBp.nType & (BP_EXEC)) != 0)
									  || ((sBreakpoint[i].nType & BP_ACCESS) && (sBp.nType & BP_ACCESS))) {
									// replace breakpoint type
									sBreakpoint[i].bEnable = TRUE;
									sBreakpoint[i].nType   = sBp.nType;

									// redaw breakpoint list
									SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
									SendMessage(hWnd, LB_RESETCONTENT, 0, 0);
									for (i = 0; i < wBreakpointCount; ++i)
										DrawBreakpoint(hWnd, i);
									SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
									return FALSE;
								}
							}
						}

						// check for breakpoint buffer full
						if (wBreakpointCount >= MAXBREAKPOINTS) {
							AbortMessage(_T("Reached maximum number of breakpoints !"));
							return FALSE;
						}

						sBreakpoint[wBreakpointCount].bEnable = sBp.bEnable;
						sBreakpoint[wBreakpointCount].nType   = sBp.nType;
						sBreakpoint[wBreakpointCount].dwAddr  = sBp.dwAddr;

						DrawBreakpoint(hWnd, wBreakpointCount);

						++wBreakpointCount;
					}
					return TRUE;
				case IDC_BREAKEDIT_DELETE:
					// scan all breakpoints from top
					for (nItem = wBreakpointCount-1; nItem >= 0; --nItem) {
						// item selected
						if (SendMessage(hWnd, LB_GETSEL, nItem, 0) > 0) {
							INT j;

							// get breakpoint index
							i = SendMessage(hWnd, LB_GETITEMDATA, nItem, 0);
							SendMessage(hWnd, LB_DELETESTRING, nItem, 0);
							--wBreakpointCount;

							// update remaining list box references
							for (j = 0; j < wBreakpointCount; ++j) {
								INT k = SendMessage(hWnd, LB_GETITEMDATA, j, 0);
								if (k > i) SendMessage(hWnd, LB_SETITEMDATA, j, k - 1);
							} 

							// remove breakpoint from breakpoint table
							for (++i; i <= wBreakpointCount; ++i)
								sBreakpoint[i-1] = sBreakpoint[i];
						}
					}
					return TRUE;

				case IDCANCEL:
					EndDialog(hDlg, wParam);
					return TRUE;
			}

		case WM_VKEYTOITEM:
			if(LOWORD(wParam) == VK_SPACE) {
				OnToggleCheck(GetDlgItem(hDlg, IDC_BREAKEDIT_WND));
				return -2;
			}
			return -1;							// default action

		case WM_DRAWITEM:
			if (wParam == IDC_BREAKEDIT_WND) return OnDrawBreakWnd((LPDRAWITEMSTRUCT) lParam);
			break;

		case WM_MEASUREITEM:
			hDC = GetDC(hDlg);

			// GetTextMetrics from "Courier New 8" font
			hFont = CreateFont(-MulDiv(8, GetDeviceCaps(hDC,  LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
							   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, _T("Courier New"));

			hFont = SelectObject(hDC, hFont);
			GetTextMetrics(hDC, &tm);
			hFont = SelectObject(hDC, hFont);
			DeleteObject(hFont);

			((LPMEASUREITEMSTRUCT) lParam)->itemHeight = tm.tmHeight;

			ReleaseDC(hDlg, hDC);
			return TRUE;
	}
	return FALSE;
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
}

static BOOL OnEditBreakpoint(HWND hDlg)
{
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_BREAKEDIT), hDlg, (DLGPROC)EditBreakpoint) == -1)
		AbortMessage(_T("Edit Breakpoint Dialog Box Creation Error !"));

	// update code window
	InvalidateRect(GetDlgItem(hDlg, IDC_DEBUG_CODE), NULL, TRUE);
	return -1;
}

//################
//#
//#    File operations
//#
//################

//
// load breakpoint list
//
VOID LoadBreakpointList(HANDLE hFile)		// NULL = clear breakpoint list
{
	DWORD lBytesRead = 0;

	// read number of breakpoints
	if (hFile) ReadFile(hFile, &wBreakpointCount, sizeof(wBreakpointCount), &lBytesRead, NULL);
	if(lBytesRead) {							// breakpoints found
		WORD wBreakpointSize;

		// read size of one breakpoint
		ReadFile(hFile, &wBreakpointSize, sizeof(wBreakpointSize), &lBytesRead, NULL);
		if (lBytesRead == sizeof(wBreakpointSize) && wBreakpointSize == sizeof(sBreakpoint[0]))
		{
			// read breakpoints
			ReadFile(hFile, sBreakpoint, wBreakpointCount * sizeof(sBreakpoint[0]), &lBytesRead, NULL);
			_ASSERT(lBytesRead == wBreakpointCount * sizeof(sBreakpoint[0]));
		} else {								// changed breakpoint structure
			wBreakpointCount = 0;			// clear breakpoint list
		}
	} else {
		wBreakpointCount = 0;				// clear breakpoint list
	}
}

//
// save breakpoint list
//
VOID SaveBreakpointList(HANDLE hFile)
{
	if (wBreakpointCount) {					// defined breakpoints
		DWORD lBytesWritten;

		WORD wBreakpointSize = sizeof(sBreakpoint[0]);

		_ASSERT(hFile);						// valid file pointer?

		// write number of breakpoints
		WriteFile(hFile, &wBreakpointCount, sizeof(wBreakpointCount), &lBytesWritten, NULL);
		_ASSERT(lBytesWritten == sizeof(wBreakpointCount));

		// write size of one breakpoint
		WriteFile(hFile, &wBreakpointSize, sizeof(wBreakpointSize), &lBytesWritten, NULL);
		_ASSERT(lBytesWritten == sizeof(wBreakpointSize));

		// write breakpoints
		WriteFile(hFile, sBreakpoint, wBreakpointCount * sizeof(sBreakpoint[0]), &lBytesWritten, NULL);
		_ASSERT(lBytesWritten == wBreakpointCount * sizeof(sBreakpoint[0]));
	}
}
