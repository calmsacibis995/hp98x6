/*
 *	 Debugger.h
 *
 *   Copyright 2010-11 Olivier De Smet
 */

//
//   Taken from emu42 without too much changes
//

// breakpoint type definitions
#define BP_EXEC		0x01					// code breakpoint
#define BP_READ		0x02					// read memory breakpoint (not done)
#define BP_WRITE	0x04					// write memory breakpoint (not done)
#define BP_BASIC	0x08					// BASIC breakpoint (not done)
#define BP_ACCESS	(BP_READ|BP_WRITE)		// read/write memory breakpoint (not done)

// debugger state definitions
#define DBG_SUSPEND		-1
#define DBG_OFF			0
#define DBG_RUN			1
#define DBG_STEPINTO	2
#define DBG_STEPOVER    3
#define DBG_STEPOUT     4

// debugger.c
extern BOOL    CheckBreakpoint(DWORD dwAddr, DWORD wRange, UINT nType);
extern VOID    NotifyDebugger(BOOL bType);
extern VOID    DisableDebugger(VOID);
extern LRESULT OnToolDebug(VOID);
extern VOID    LoadBreakpointList(HANDLE hFile);
extern VOID    SaveBreakpointList(HANDLE hFile);
