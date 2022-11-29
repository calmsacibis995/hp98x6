/*
 *   HP98x6.h
 * 
 *	 General definitions of functions
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

#include "98x6.h"
#include "opcodes.h"

#define SM_RUN			0											// states of cpu emulation thread
#define SM_INVALID		1
#define SM_RETURN		2

#define VIEW_SHORT		FALSE										// view of disassembler output
#define VIEW_LONG		TRUE

#define	ARRAYSIZEOF(a)	(sizeof(a) / sizeof(a[0]))
#define	_KB(a)			((a)*1024)

#define HARDWARE		"Chipmunk"									// emulator hardware

// HP98x6.c
extern HANDLE           hHeap;										// private heap to allocate mem from
extern HPALETTE			hPalette;									// current window display palette
extern HPALETTE			hOldPalette;								// old one to restore when leaving
extern HANDLE			hEventShutdn;
extern LPTSTR			szTitle;									// title for window
extern HINSTANCE		hApp;
extern HWND				hWnd;
extern HWND				hDlgDebug;
extern HWND             hDlgFind;
extern HDC				hWindowDC;
extern BOOL				bAutoSave;									// in registry
extern BOOL				bAutoSaveOnExit;							// in registry
extern BOOL				bAlwaysDisplayLog;							// in registry
		
extern HANDLE			hThread;									// system thread
extern VOID				SetWindowTitle(LPTSTR szString);
extern VOID				UpdateWindowStatus(VOID);
extern VOID				ButtonEvent(BOOL state, UINT nId);
extern LRESULT			OnToolDebug(VOID);

extern BOOL				bDebugOn;

// Debugger.c
extern BOOL				bBreakExceptions;							// should we break on exceptions ? (was used for debugging)

// Settings.c
extern VOID				ReadSettings(VOID);							// read registry settings
extern VOID				WriteSettings(VOID);						// write registry settings
extern VOID				ReadLastDocument(LPTSTR szFileName, DWORD nSize);
extern VOID				WriteLastDocument(LPCTSTR szFilename);

// Display.c
extern UINT				nBackgroundX;
extern UINT				nBackgroundY;
extern UINT				nBackgroundW;
extern UINT				nBackgroundH;
extern UINT				nLcdX;
extern UINT				nLcdY;

extern HDC				hFontDC;									// handle of alpha font
extern HDC				hMainDC;									// handle of main bitmap for border display
extern HDC				hAlpha1DC;									// handle of display alpha 1
extern HDC				hAlpha2DC;									// handle of display alpha 2 for blink
extern HDC				hGraphDC;									// handle of display graph
extern HDC				hScreenDC;

extern LPBYTE			pbyGraph;

extern VOID				UpdateClut(VOID);
extern VOID				SetLcdColor(UINT nId, UINT nRed, UINT nGreen, UINT nBlue);
extern VOID				SetGraphColor(BYTE col);
extern VOID				CreateLcdBitmap(VOID);
extern VOID				DestroyLcdBitmap(VOID);
extern BOOL				CreateMainBitmap(LPCTSTR szFilename);
extern VOID				DestroyMainBitmap(VOID);
extern VOID				UpdateAnnunciators(BOOL bForce);
extern VOID				ResizeWindow(VOID);
extern VOID				Refresh_Display(BOOL bforce);				// refresh altered part of display at vsync
extern VOID				UpdateMainDisplay(BOOL bforce);				// refresh altered part of display at vsync
extern VOID				Reload_Graph(VOID);							// reload graph bitmap from graph mem					
extern VOID				UpdateClut(VOID);

// Display16.c
extern BYTE				Write_Display16(BYTE *a, WORD d, BYTE s);	// for alpha part
extern BYTE				Read_Display16(BYTE *a, WORD d, BYTE s);
extern BYTE				Write_Graph16(BYTE *a, WORD d, BYTE s);		// for graph part
extern BYTE				Read_Graph16(BYTE *a, WORD d, BYTE s);
extern VOID				Refresh_Display16(BOOL bforce);				// refresh altered part of display at vsync
extern VOID				UpdateMainDisplay16(BOOL force);
extern VOID				do_display_timers16(VOID);
extern VOID				Reload_Graph16(VOID);								

// Display36c.c
extern BYTE				Write_Display36c(BYTE *a, WORD d, BYTE s);	// for alpha part
extern BYTE				Read_Display36c(BYTE *a, WORD d, BYTE s);
extern BYTE				Write_Graph36c(BYTE *a, DWORD d, BYTE s);	// for graph part
extern BYTE				Read_Graph36c(BYTE *a, DWORD d, BYTE s);
extern VOID				Refresh_Display36c(BOOL bforce);			// refresh altered part of display at vsync
extern VOID				UpdateMainDisplay36c(BOOL force);
extern VOID				do_display_timers36c(VOID);
extern VOID				Reload_Graph36c(VOID);								

// Display37.c
extern BYTE				Write_Display37(BYTE *a, WORD d, BYTE s);	// for crt part
extern BYTE				Read_Display37(BYTE *a, WORD d, BYTE s);
extern BYTE				Write_Graph37(BYTE *a, DWORD d, BYTE s);	// for graph part
extern BYTE				Read_Graph37(BYTE *a, DWORD d, BYTE s);
extern VOID				Refresh_Display37(BOOL bforce);				// refresh altered part of display at vsync
extern VOID				SaveMainDisplay37(VOID);					// save bitmap into graph mem
extern VOID				UpdateMainDisplay37(BOOL bforce);			// refresh altered part of display at vsync
extern VOID				do_display_timers37(VOID);
extern VOID				Reload_Graph37(VOID);								
extern VOID				do_display37(VOID);							// for line mover

// 98x6.c
extern BOOL				bInterrupt;
extern UINT				nState;
extern UINT				nNextState;
extern WORD				wRealSpeed;
//extern BOOL				bSound;									// no sound actually done
extern DWORD			dwMaxTimers;
extern WORD				wCyclesTimer;
extern SYSTEM			Chipset;
extern DWORD			dwCapricornCycles;
extern HANDLE			hEventDebug;
extern BOOL				bDbgAutoStateCtrl;
extern INT				nDbgState;
extern DWORD			dwDbgStopPC;
extern WORD				dwDbgRstkp;
extern WORD				dwDbgRstk;
extern DWORD			*pdwInstrArray;
extern WORD				wInstrSize;
extern WORD				wInstrWp;
extern WORD				wInstrRp;
extern WORD				byLastOp;
extern DWORD			dwVsyncRef;
extern VOID				SuspendDebugger(VOID);
extern VOID				ResumeDebugger(VOID);
extern VOID				SetSpeed(WORD wAdjust);
extern UINT				SwitchToState(UINT nNewState);
extern UINT				WorkerThread(LPVOID pParam);
extern VOID				SetHPTime(VOID);

// Fetch.c
extern VOID				EvalOpcode(BYTE newI210);
extern VOID				initOP(VOID);

// Files.c
extern TCHAR			szHP8xDirectory[MAX_PATH];
extern TCHAR			szCurrentDirectory[MAX_PATH];
extern TCHAR			szCurrentKml[MAX_PATH];
extern TCHAR			szCurrentFilename[MAX_PATH];
extern TCHAR			szBufferFilename[MAX_PATH];
extern LPBYTE			pbyRom;
extern LPBYTE			pbyRomPage[256];
extern UINT				nCurrentRomType;
extern UINT				nCurrentClass;
extern BOOL				bBackup;
extern VOID				SetWindowLocation(HWND hWnd,INT nPosX,INT nPosY);
extern BOOL				PatchRom(LPCTSTR szFilename);
extern BOOL				MapRom(LPCTSTR szRomDirectory);
extern VOID				UnmapRom(VOID);
extern VOID				ResetDocument(VOID);
extern BOOL				NewDocument(VOID);
extern BOOL				OpenDocument(LPCTSTR szFilename);
extern BOOL				SaveDocument(VOID);
extern BOOL				SaveDocumentAs(LPCTSTR szFilename);
extern BOOL				SaveBackup(VOID);
extern BOOL				RestoreBackup(VOID);
extern BOOL				ResetBackup(VOID);
extern BOOL				GetOpenFilename(VOID);
extern BOOL				GetSaveAsFilename(VOID);
extern BOOL				GetLoadLifFilename(VOID);
extern BOOL				GetSaveLifFilename(VOID);
extern BOOL				GetSaveDiskFilename(LPCTSTR lpstrName);
extern BOOL				LoadLif(LPCTSTR szFilename);
extern BOOL				SaveLif(LPCTSTR szFilename);
extern BOOL				LoadDisk(LPCTSTR szFilename);
extern BOOL				SaveDisk(LPCTSTR szFilename);
extern HBITMAP			LoadBitmapFile(LPCTSTR szFilename);

// Hp-ib.c
extern BYTE				Write_HPIB(BYTE *a, WORD d, BYTE s);		// write in HPIB I/O space
extern BYTE				Read_HPIB(BYTE *a, WORD d, BYTE s);			// read in HPIB I/O space
extern VOID				DoHPIB(VOID);								// fsm for HPIB controller
extern VOID				hpib_names(VOID);							// update names of HIB instruments
extern BOOL				hpib_init_bus(VOID);						// to initialize peripherals on bus (call xxxx_reset()) 
extern BOOL				hpib_stop_bus(VOID);						// to stop peripherals on bus (call xxxx_stop()) 
extern VOID				hpib_init(VOID);							// to initialize peripherals functions
extern BOOL				h_push(BYTE b, BYTE st);					// called by instruments to reply back on bus

//Hp-9121.c	
extern VOID				DoHp9121(HP9121 *ctrl);						// fsm for HP9121 disk unit
extern BOOL				hp9121_eject(HP9121 *ctrl, BYTE unit);
extern BOOL				hp9121_save(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL				hp9121_load(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL				hp9121_widle(HP9121 *ctrl);
extern VOID				hp9121_reset(VOID *controler);
extern VOID				hp9121_stop(VOID *controler);
extern BOOL				hp9121_push_c(VOID *controler, BYTE c);
extern BOOL				hp9121_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-9122.c
extern VOID				DoHp9122(HPSS80 *ctrl);						// fsm for HP9122 disk unit
extern BOOL				hp9122_eject(HPSS80 *ctrl, BYTE unit);
extern BOOL				hp9122_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL				hp9122_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL				hp9122_widle(HPSS80 *ctrl);
extern VOID				hp9122_reset(VOID *controler);
extern VOID				hp9122_stop(VOID *controler);
extern BOOL				hp9122_push_c(VOID *controler, BYTE c);
extern BOOL				hp9122_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-7908.c
extern VOID				DoHp7908(HPSS80 *ctrl);						// fsm for HP7908 hard disk unit
extern BOOL				hp7908_eject(HPSS80 *ctrl, BYTE unit);
extern BOOL				hp7908_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL				hp7908_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL				hp7908_widle(HPSS80 *ctrl);
extern VOID				hp7908_reset(VOID *controler);
extern VOID				hp7908_stop(VOID *controler);
extern BOOL				hp7908_push_c(VOID *controler, BYTE c);
extern BOOL				hp7908_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-9130.c
extern VOID				DoHp9130(VOID);								// fsm for internal HP9130 like unit
extern BOOL				hp9130_eject(BYTE unit);
extern BOOL				hp9130_save(BYTE unit, LPCTSTR szFilename);
extern BOOL				hp9130_load(BYTE unit, LPCTSTR szFilename);
extern BOOL				hp9130_widle(VOID);
extern VOID				hp9130_reset(VOID);
extern VOID				hp9130_stop(VOID);
extern BYTE				Write_9130(BYTE *a, WORD d, BYTE s);
extern BYTE				Read_9130(BYTE *a, WORD d, BYTE s);

//Hp-2225.c
extern VOID				DoHp2225(HP2225 *ctrl);						// fsm for HP2225 like printer
extern BOOL				hp2225_eject(HP2225 *ctrl);
extern BOOL				hp2225_save(HP2225 *ctrl);
extern BOOL				hp2225_widle(HP2225 *ctrl);
extern VOID				hp2225_reset(VOID *controler);
extern VOID				hp2225_stop(HP2225 *ctrl);
extern BOOL				hp2225_push_c(VOID *controler, BYTE c);
extern BOOL				hp2225_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-98635.c
extern BYTE				Write_98635(BYTE *a, WORD d, BYTE s);		// write in HP98635 I/O space
extern BYTE				Read_98635(BYTE *a, WORD d, BYTE s);		// read in HP98635 I/O space

//Hp-98626.c
extern BYTE				Write_98626(BYTE *a, WORD d, BYTE s);		// write in HP98626 I/O space
extern BYTE				Read_98626(BYTE *a, WORD d, BYTE s);		// read in HP98626 I/O space

// Mops.c
extern VOID				SystemReset(VOID);							// reset system as power on
extern WORD				GetWORD(DWORD d);							// read a word in RAM/ROM for external purpose
extern BYTE				ReadMEM(BYTE *a, DWORD d, BYTE s);			// read on system bus 
extern BYTE				WriteMEM(BYTE *a, DWORD d, BYTE s);			// write on system bus

// opcodes.c
extern VOID				decode_op(WORD op, OP *ope);				// decode 'op' mc68000 opcode into *ope struct

// Keyboard.c
extern VOID				KnobRotate(SWORD knob);						// mouse wheel is knob
extern VOID				KeyboardEventDown(BYTE nId);
extern VOID				KeyboardEventUp(BYTE nId);
//extern BOOL				WriteToKeyboard(LPBYTE lpData, DWORD dwSize);
extern BYTE				Write_Keyboard(BYTE *a, WORD d, BYTE s);
extern BYTE				Read_Keyboard(BYTE *a, WORD d, BYTE s);
extern VOID				Do_Keyboard_Timers(DWORD cycles);			// do keyboard timers
extern VOID				Do_Keyboard(VOID);							// do keyboard stuff
extern VOID				Reset_Keyboard(VOID);						// at cold boot
extern VOID				Init_Keyboard(VOID);						// at re-load doc

// Disasm.c
extern DWORD			disassemble (DWORD addr, LPTSTR out, BOOL view);

// Message Boxes
static __inline int		InfoMessage(LPCTSTR szMessage)  {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_OK|MB_ICONINFORMATION|MB_SETFOREGROUND);}
static __inline int		AbortMessage(LPCTSTR szMessage) {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);}
static __inline int		YesNoMessage(LPCTSTR szMessage) {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_YESNO|MB_ICONEXCLAMATION|MB_SETFOREGROUND);}
static __inline int		YesNoCancelMessage(LPCTSTR szMessage) {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_YESNOCANCEL|MB_ICONEXCLAMATION|MB_SETFOREGROUND);}

// Missing Win32 API calls
static __inline LPTSTR DuplicateString(LPCTSTR szString)
{
	UINT   uLength = lstrlen(szString) + 1;
	LPTSTR szDup   = HeapAlloc(hHeap, 0, uLength*sizeof(szDup[0]));
	lstrcpy(szDup,szString);
	return szDup;
}

