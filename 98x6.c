/*
 *   98x6.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   taken long ago from Emu42
//
//	 WorkerThread as main emulation loop, timers, debugger stuff 
//
//   Peripherals are are synchronous with 68000 (after each 68000 opcode)
//	 
//	 Can be changed, but be careful for HPIB ones.
//	 Timing are very delicate with the use of 9914A and the code used.
//	 The code addressing the TI9914A is different from BOOTROM, BASIC and PASCAL.
//	 It was very delicate to have something working for all three.
//
//	 Be carefull, the debugger is not finished (many lines commented)
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "mops.h"
#include "debugger.h"

static INT   nDbgOldState = DBG_OFF;		// old state of debugger for suspend/resume
static BOOL  bCpuSlow = TRUE;				// enable/disable real speed

static DWORD dwOldCyc;						// cpu cycles at last event
static DWORD dwSpeedRef;					// timer value at last event
static DWORD dwTickRef;						// 1ms sample timer ticks

static DWORD dwHpFreq;						// high performance counter for timers
static DWORD dwHpCount500;					// counter value for 500Hz timer
static DWORD dwHpCount1000;					// counter value for 1000Hz timer
static DWORD dwHp500;						// for 500Hz timer
static DWORD dwHp1000;						// for 1000Hz timer
static BOOL  bDbgSkipInt = FALSE;			// execute interrupt handler

BOOL    bInterrupt = FALSE;					// no demand for interrupt
UINT    nState = SM_INVALID;				// actual system state
UINT    nNextState = SM_RUN;				// next initial system state
WORD    wRealSpeed = 1;						// real speed emulation x1
//BOOL    bSound = FALSE;						// sound emulation activated ?

SYSTEM Chipset;

// variables for debugger engine
HANDLE  hEventDebug;						// event handle to stop cpu thread
BOOL    bDbgAutoStateCtrl = TRUE;			// debugger suspend control by SwitchToState()
INT     nDbgState = DBG_OFF;				// state of debugger at start
DWORD   dwDbgStopPC = -1;					// stop address for goto cursor
WORD    dwDbgRstkp;							// stack recursion level of step over end
WORD    dwDbgRstk;							// possible return address
DWORD   *pdwInstrArray = NULL;				// last instruction array
WORD    wInstrSize;							// size of last instruction array
WORD    wInstrWp;							// write pointer of instruction array
WORD    wInstrRp;							// read pointer of instruction array
WORD    byLastOp;							// last opcode on debug

DWORD   dwMaxTimers = 0;					// Max number of call to DoTimers before 2 ms elapsed 
DWORD   dwCyclesTimer = 16000;				// Number of MC68000 cycles before calling DoTimers at 500Hz for 8 MHz at full speed (will change)
DWORD   dwBaseRef = 16000;					// Number of MC68000 cycles before calling DoTimers at 500Hz for 8 MHz for real speed
WORD	wDivRef = 1;						// divisor to get equivalent 8MHz cycles

DWORD   dwVsyncRef = 133333;				// Number of 8 MHz MC68000 cycles for vsync at 60Hz 


//################
//#
//#    Low level subroutines
//#
//################

//
// save last instruction in circular instruction buffer
//
static __inline VOID SaveInstrAddr(DWORD dwAddr)
{
	if (pdwInstrArray) {						// circular buffer allocated
		pdwInstrArray[wInstrWp] = dwAddr;
		wInstrWp = (wInstrWp + 1) % wInstrSize;
		if (wInstrWp == wInstrRp)
			wInstrRp = (wInstrRp + 1) % wInstrSize;
	}
}

//
// crude changes for debbuger part
//
static __inline VOID Debugger(VOID)			// debugger part
{
	LARGE_INTEGER lDummyInt;				// sample timer ticks
	BOOL bStopEmulation;

//	SaveInstrAddr(PC);						// save pc in last instruction buffer

	// check for code breakpoints
	bStopEmulation = CheckBreakpoint(Chipset.Cpu.PC, 1, BP_EXEC);

	// check for Exceptions
	bStopEmulation |= (Chipset.Cpu.State == EXCEPTION) & bBreakExceptions;
	bStopEmulation |= (Chipset.Cpu.State == HALT);

	// check for step cursor
	bStopEmulation |= (dwDbgStopPC == Chipset.Cpu.PC);

	// step over interrupt execution
//	if (bDbgSkipInt && !bStopEmulation && !Chipset.inte)
//		return;

	// check for step into
	bStopEmulation |= (nDbgState == DBG_STEPINTO);

	// check for step over
//	bStopEmulation |= (nDbgState == DBG_STEPOVER) && (dwDbgRstkp == (Chipset.R[6]|(Chipset.R[7]<<8)));

	// check for step out, something was popped from return stack (not good)
/*	if (nDbgState == DBG_STEPOUT && dwDbgRstkp == (Chipset.R[6]|(Chipset.R[7]<<8)))
	{
		_ASSERT(bStopEmulation == FALSE);
		if ((bStopEmulation = (PC == dwDbgRstk)) == FALSE)
		{
//			dwDbgRstkp = (Chipset.R[6]|Chipset.R[7]<<8) - 2;	// save stack data
//			dwDbgRstk = Nread_nio2(dwDbgRstkp);
		}
	}*/

	if (bStopEmulation)						// stop condition
	{
		// redraw debugger window and stop
		NotifyDebugger(TRUE);
		Refresh_Display(FALSE);						// refresh display
		WaitForSingleObject(hEventDebug,INFINITE);

		// init slow down part
//		dwOldCyc = (DWORD) (Chipset.cycles & 0xFFFFFFFF);
		QueryPerformanceCounter(&lDummyInt);
		dwSpeedRef = lDummyInt.LowPart;
	}
}

//
// Adjust all timers: with capricorn cycles for real speed
//   with High Performance counter otherwise.
//   dtime is the time elapsed in capricorn cycles
//
static DWORD dwNbCalls = 0;

static BOOL __forceinline __fastcall DoTimers(DWORD dtime)
{
	if ( !bCpuSlow && (nDbgState == DBG_OFF)) {	// compute elapsed time with High Performance counter 
		LARGE_INTEGER lAct;

		dwNbCalls++;
		QueryPerformanceCounter(&lAct);
		if ((lAct.LowPart - dwHpCount500) < dwHp500) {		// two ms elapsed ?
			dtime = 0;											// no
			if (dwNbCalls > dwMaxTimers)						// something ticks
				dwMaxTimers = dwNbCalls;
			if (dwNbCalls > 1) {								// number of calls 
				//if (dwCyclesTimer < 0x7FFFFFFF)
					dwCyclesTimer++;									// too much, raise  
			} else {
				//if (dwCyclesTimer > 16000)								
					dwCyclesTimer--;									// not enough, lower
			}
			dwVsyncRef = (dwCyclesTimer * 25);
			dwVsyncRef /= 3;
		} else {											// yes
			dtime = 2457 * 2 * (lAct.LowPart - dwHpCount500);		// convert to 2.4576 MHz
			dtime /= dwHp500;										// number of cycles (2.4576 MHz)
			dwHpCount500 = lAct.LowPart;							// ref for next call
			dwNbCalls = 0;											
		}
	} else {												// correct from 2ms base freq
		dtime = 1000 * dtime;
		dtime /= wDivRef;
		dtime /= 3255;											// 2.4576 MHz base
	}

	if (dtime > 0) {	// in 2.4576 MHz cycles
		Do_Keyboard_Timers(dtime);
		return TRUE;
	}
	return FALSE;
}

//
// adjust emulation speed
//
static __forceinline VOID AdjustSpeed(VOID)		
{
	DWORD dwTicks;

	// cycles elapsed for next check
	if ((Chipset.cycles - dwOldCyc) >=  dwBaseRef) {		// enough cycles ?
		LARGE_INTEGER lAct;

		QueryPerformanceCounter(&lAct);							// update
		while((lAct.LowPart - dwSpeedRef) <= dwHp500) {				// 2 ms elasped ?
			if ((lAct.LowPart - dwSpeedRef) <= dwTickRef) Sleep(1);				// less than 1 ms
			else Sleep(0);														// more
			QueryPerformanceCounter(&lAct);										// update
		}
		dwTicks = lAct.LowPart - dwSpeedRef;
		// workaround for QueryPerformanceCounter() in Win2k,
		// if last command sequence took over 50ms -> synchronize
		if(dwTicks > 50 * dwTickRef) {	// time for last commands > 50ms (223 at 4474 Hz)
			dwOldCyc = Chipset.cycles;				// new synchronizing
			QueryPerformanceCounter(&lAct);			// get timer ticks
			dwSpeedRef = lAct.LowPart;				// save reference time
		} else {
			dwOldCyc += dwBaseRef;					// adjust cycles reference
			dwSpeedRef += dwHp500;				// adjust reference time
		}
	} 
}

//################
//#
//#    Public functions
//#
//################

VOID SuspendDebugger(VOID)
{
	// auto control enabled, emulation halted by debugger
	if (bDbgAutoStateCtrl && nDbgState > DBG_OFF) {
		nDbgOldState = nDbgState;			// save old state
		nDbgState = DBG_SUSPEND;			// suspend state
		SetEvent(hEventDebug);				// exit debugger
	}
}

VOID ResumeDebugger(VOID)
{
	// auto control enabled, debugger is suspended
	if (bDbgAutoStateCtrl && nDbgState == DBG_SUSPEND)
		nDbgState = nDbgOldState;			// set to old debugger state
}

//
// Set HP time and date
// try to find if basic or pascal ...
// 
VOID SetHPTime(VOID)
{
	WORD	   system = 0;
	WORD	   data;
	BYTE	   chr;
	DWORD	   sysname = 0xFFFDC2;
	DWORD	   centisec, dw;	
	SYSTEMTIME ts;

	while (1) {
		data = GetWORD(sysname);
		chr = (BYTE)(data >> 8);
		if (chr == 'B') system = 1;
		if (chr == 'P') system = 2;
		if (chr == 0x00) break;
		sysname++;
		if (system != 0) break;
		if (sysname == 0xFFFDCC) break;
		chr = (BYTE)(data & 0xFF);
		if (chr == 'B') system = 1;
		if (chr == 'P') system = 2;
		if (chr == 0x00) break;
		sysname++;
		if (system != 0) break;
		if (sysname == 0xFFFDCC) break;
	}
	if (system != 0) {
		GetLocalTime(&ts);							// local time, _ftime() cause memory/resource leaks

		centisec = ts.wMilliseconds/10+ts.wSecond*100+ts.wMinute*6000+ts.wHour*360000;
		Chipset.Keyboard.ram[0x2d] = (BYTE) (centisec & 0xFF);
		Chipset.Keyboard.ram[0x2e] = (BYTE) ((centisec >> 8) & 0xFF);
		Chipset.Keyboard.ram[0x2f] = (BYTE) ((centisec >> 16) & 0xFF);

		// calculate days: today from 01.01.1970 for pascal 3 & basic 2
		// calculate days: today from 01.01.1900 for basic 5
		dw = (DWORD) ts.wMonth;
		if (dw > 2)
			dw -= 3L;
		else {
			dw += 9L;
			--ts.wYear;
		}
		dw = (DWORD) ts.wDay + (153L * dw + 2L) / 5L;
		dw += (146097L * (((DWORD) ts.wYear) / 100L)) / 4L;
		dw +=   (1461L * (((DWORD) ts.wYear) % 100L)) / 4L;

		if (system == 2) 
			dw -= 719469L;										// for pascal (from 01.01.1970)

		if (system == 1) 
			dw -= 693961L;										// for basic  (from 01.01.1900)

		Chipset.Keyboard.ram[0x30] = (BYTE) (dw & 0xFF);
		Chipset.Keyboard.ram[0x31] = (BYTE) ((dw >> 8) & 0xFF);
	}
}

//
// adjust delay variables from speed settings
//
VOID SetSpeed(WORD wAdjust)					// set emulation speed
{
	LARGE_INTEGER lTime;				// sample timer ticks

	switch (wAdjust) {
		case 0:									// max speed
			dwOldCyc = Chipset.cycles;
			QueryPerformanceCounter(&lTime);	// get timer ticks
			dwSpeedRef = lTime.LowPart;			// save reference time
			break;
		default:
		case 1:									// 8MHz
			dwBaseRef = 16000;
			dwVsyncRef = 133333;
			wDivRef = 1;
			break;
		case 2:									// 16MHz
			dwBaseRef = 32000;
			dwVsyncRef = 266667;
			wDivRef = 2;
			break;
		case 3:									// 24MHz
			dwBaseRef = 48000;
			dwVsyncRef = 400000;
			wDivRef = 3;
			break;
		case 4:									// 32MHz
			dwBaseRef = 64000;
			dwVsyncRef = 533333;
			wDivRef = 4;
			break;
		case 5:									// 40MHz
			dwBaseRef = 80000;
			dwVsyncRef = 666667;
			wDivRef = 5;
			break;
	}
	bCpuSlow = (wAdjust != 0);						// save emulation speed 
}

//
// change thread state
//
UINT SwitchToState(UINT nNewState)
{
	UINT nOldState = nState;

	if (nState == nNewState) return nOldState;
	switch (nState) {
		case SM_RUN: // Run
			switch (nNewState) {
				case SM_INVALID: // -> Invalid
					nNextState = SM_INVALID;
					bInterrupt = TRUE;				// leave inner emulation loop
					SuspendDebugger();				// suspend debugger
					while (nState!=nNextState) Sleep(0);
					UpdateWindowStatus();
					break;
				case SM_RETURN: // -> Return
					DisableDebugger();				// disable debugger
					nNextState = SM_INVALID;
					bInterrupt = TRUE;				// leave inner emulation loop
					while (nState!=nNextState) Sleep(0);
					nNextState = SM_RETURN;
					SetEvent(hEventShutdn);
					WaitForSingleObject(hThread,INFINITE);
					UpdateWindowStatus();
					break;
				}
			break;
		case SM_INVALID: // Invalid
			switch (nNewState) {
				case SM_RUN: // -> Run
					nNextState = SM_RUN;
					// don't enter opcode loop on interrupt request
					bInterrupt = FALSE;
					ResumeDebugger();
					SetEvent(hEventShutdn);
					while (nState!=nNextState) Sleep(0);
					UpdateWindowStatus();
					break;
				case SM_RETURN: // -> Return
					DisableDebugger();				// disable debugger
					nNextState = SM_RETURN;
					SetEvent(hEventShutdn);
					WaitForSingleObject(hThread,INFINITE);
					break;
				}
			break;
	}
	return nOldState;
}

UINT WorkerThread(LPVOID pParam)
{
	LARGE_INTEGER lDummyInt;				// sample timer ticks
	BYTE newI210 = 0;						// new interrupt level


	QueryPerformanceFrequency(&lDummyInt);
	lDummyInt.QuadPart /= 1000;				// calculate 1ms ticks
	dwHp1000 = lDummyInt.LowPart;			
	QueryPerformanceFrequency(&lDummyInt);
	lDummyInt.QuadPart /= 500;				// calculate 2ms ticks
	dwHp500 = lDummyInt.LowPart;
	QueryPerformanceFrequency(&lDummyInt);
	lDummyInt.QuadPart /= 1000;				// calculate sample ticks
	dwTickRef = lDummyInt.LowPart;			// sample timer ticks 
	_ASSERT(dwTickRef);						// tick resolution error */

/*	QueryPerformanceFrequency(&lDummyInt);	// init timer ticks
	{
		TCHAR buffer[256];
		int i = wsprintf(buffer,_T("Performance frequency : (%08lX, %08lX) dwHp500 %d\n"), lDummyInt.HighPart, lDummyInt.LowPart, dwHp500);
		buffer[i++] = 0;
		OutputDebugString(buffer);
	} 
*/
	initOP();								// init mc68000 op table

	byLastOp = 0x4e71;						// start with a NOP as previous op :)

loop:
	while (nNextState == SM_INVALID) {		// go into invalid state
		nState = SM_INVALID;					// in invalid state
		WaitForSingleObject(hEventShutdn,INFINITE);
		if (nNextState == SM_RETURN) {			// go into return state
			nState = SM_RETURN;					// in return state
			return 0;							// kill thread
		}
	}
	while (nNextState == SM_RUN) {
		if (nState != SM_RUN) {
			nState = SM_RUN;
			UpdateAnnunciators(FALSE);

			if (Chipset.keeptime) SetHPTime();			// update HP time & date

			// init speed reference
			dwOldCyc = (DWORD) (Chipset.cycles & 0xFFFFFFFF);
			QueryPerformanceCounter(&lDummyInt);
			dwSpeedRef = lDummyInt.LowPart;
			dwHpCount500 = lDummyInt.LowPart;
			dwHpCount1000 = lDummyInt.LowPart;
		}
		byLastOp = 0x4e71;
		while (!bInterrupt) {
			if (nDbgState > DBG_OFF) {		// debugger active 
				Debugger();

				// if suspended skip next opcode execution
				if (nDbgState == DBG_SUSPEND)
					continue;
			} 
			
			Chipset.dcycles = 0;
			EvalOpcode(newI210);			// execute opcode with interrupt if neede
			Chipset.cycles += (DWORD)(Chipset.dcycles);
			Chipset.ccycles += (DWORD)(Chipset.dcycles);
			Chipset.Cpu.countOp++;			// inc opcode done
			Chipset.Cpu.countOp &= 0x3F;	// wrap at 64

			if (Chipset.Cpu.reset) {
				Chipset.Cpu.reset = 0;
				Reset_Keyboard();
			}

			// display
			if (Chipset.type == 35)
				do_display_timers36c();
			else if (Chipset.type == 37) {
				if (Chipset.Display37.lm_go) do_display37();	// do the line mover
				do_display_timers37();
			} else
				do_display_timers16(); 

			// internal floppy
			if ((Chipset.type == 36) || (Chipset.type == 35))
				DoHp9130();
			
			// HPIB controller
			DoHPIB();
			// HPIB instruments
			if (Chipset.Hpib70x == 3)
				DoHp9122(&Chipset.Hp9122_0);
			else if (Chipset.Hpib70x != 0)
				DoHp9121(&Chipset.Hp9121_0);
			if (Chipset.Hpib72x == 3)
				DoHp9122(&Chipset.Hp9122_1);
			else if (Chipset.Hpib72x != 0)
				DoHp9121(&Chipset.Hp9121_1);
			if (Chipset.Hpib73x != 0)
				DoHp7908(&Chipset.Hp7908_0);
			if (Chipset.Hpib74x != 0)
				DoHp7908(&Chipset.Hp7908_1);
			if (Chipset.Hpib71x != 0)
				DoHp2225(&Chipset.Hp2225);

			// keyboard
			Do_Keyboard();

			newI210 = (Chipset.Keyboard.int68000 == 7) ? 7: 
					  (Chipset.Hpib.h_int) ? 3 :
					  (Chipset.Hp9130.xstatus & 0x08) ? 2 :
					  (Chipset.Keyboard.int68000 == 1) ? 1 : 0;

			if ( !bCpuSlow && (nDbgState <= DBG_RUN)) {	// compute elapsed time with High Performance counter ?
				if (Chipset.ccycles > dwCyclesTimer) {				// enough MC68000 cycles for 2 ms ?											
					if (DoTimers(Chipset.ccycles))						// do it !
						Chipset.ccycles -= dwCyclesTimer;
				}
			} else													// slow mode at x MHz with cycles
				if (Chipset.ccycles > dwBaseRef)	{					// at least x*2000 MC68000 cycles ?
					DoTimers(Chipset.ccycles);
					Chipset.ccycles -= dwBaseRef;
				}
			if (bCpuSlow && (nDbgState <= DBG_RUN))			// emulation slow down
				AdjustSpeed();						// adjust emulation speed 
		}
		bInterrupt = FALSE;					// be sure to reenter opcode loop
	} 
	_ASSERT(nNextState != SM_RUN);

	goto loop;
	UNREFERENCED_PARAMETER(pParam);
}
