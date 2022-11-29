/*
 *   Hp-2225.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   HP2225A printer as ADDRESS 1 simulation
//
//   no docs about protocol, just some trials and error ....
//   dump on a file (you can open it to copy & paste when the emulator run)
//

#include "StdAfx.h"
#include "HP98x6.h"

//#define DEBUG_HP2225						// debug flag

#if defined DEBUG_HP2225
	TCHAR buffer[256];
	int k;
#endif

#define DELAY_CMD 50						// delay for command response

//################
//#
//#    Low level subroutines
//#
//################

static BOOL pop_c(HP2225 *ctrl, BYTE *c)
{
	if (ctrl->hc_hi == ctrl->hc_lo)
		return FALSE;
	if (ctrl->hc_t[ctrl->hc_lo] != 0) {		// wait more
		ctrl->hc_t[ctrl->hc_lo]--;
		return FALSE;
	}
	*c = ctrl->hc[ctrl->hc_lo++];
	ctrl->hc_lo &= 0x1FF;
	return TRUE;
}

static BOOL pop_d(HP2225 *ctrl, BYTE *c, BYTE *eoi)
{
	if (ctrl->hd_hi == ctrl->hd_lo)
		return FALSE;
	if (ctrl->hd_t[ctrl->hd_lo] != 0) {		// wait more
		ctrl->hd_t[ctrl->hd_lo]--;
		return FALSE;
	}
	*c = (BYTE) (ctrl->hd[ctrl->hd_lo] & 0x00FF);
	*eoi = (BYTE) (ctrl->hd[ctrl->hd_lo++] >> 8);
	ctrl->hd_lo &= 0x1FF;
	return TRUE;
}

BOOL hp2225_push_c(VOID *controler, BYTE c)	// push on stack ctrl->hc
{
	HP2225 *ctrl = (HP2225 *) controler;			// cast controler
	
	c &= 0x7F;										// remove parity

	if (c == 0x20 + ctrl->hpibaddr) {				// my listen address
		ctrl->listen = TRUE;
		ctrl->untalk = FALSE;
	} else if (c == 0x40 + ctrl->hpibaddr) {		// my talk address
		ctrl->talk = TRUE;
		ctrl->untalk = FALSE;
	} else if (c == 0x3F) {							// unlisten
			ctrl->listen = FALSE;
			ctrl->untalk = FALSE;
	} else if (c == 0x5F) {							// untalk
			ctrl->talk = FALSE;
			ctrl->untalk = TRUE;
	} else if ((c & 0x60) == 0x20) {				// listen other, skip
		ctrl->untalk = FALSE;
	} else if ((c & 0x60) == 0x40) {				// talk other, skip
		ctrl->untalk = FALSE;
	} else {										// other
		if ((c == 0x60 + ctrl->hpibaddr) && (ctrl->untalk)) {			// my secondary address after untak -> identify
			ctrl->untalk = FALSE;
		} else {
			ctrl->untalk = FALSE;
			if (ctrl->talk || ctrl->listen) {				// ok
				#if defined DEBUG_HP2225
				{
					k = wsprintf(buffer,_T("%06X: HP2225:%d: got %02X command\n"), Chipset.Cpu.PC, ctrl->hpibaddr, c );
					OutputDebugString(buffer);
				}
				#endif
				ctrl->hc_t[ctrl->hc_hi] = 5;					// 20 mc68000 cycles of transmission delay
				ctrl->hc[ctrl->hc_hi++] = c;
				ctrl->hc_hi &= 0x1FF;
				_ASSERT(ctrl->hc_hi != ctrl->hc_lo);
			}
		}
	}

	return TRUE;
}

BOOL hp2225_push_d(VOID *controler, BYTE d, BYTE eoi)	// push on stack ctrl->hd
{
	HP2225 *ctrl = (HP2225 *) controler;

	if (ctrl->listen || ctrl->talk) {					// only if listen or talk state
		if (((ctrl->hd_hi + 1) & 0x1FF) == ctrl->hd_lo)		// if full, not done
			return FALSE;
		else {
			ctrl->hd_t[ctrl->hd_hi] = 5;					// 5 mc68000 op cycles of transmission delay
			ctrl->hd[ctrl->hd_hi++] = (WORD) (d | (eoi << 8));
			ctrl->hd_hi &= 0x1FF;
			_ASSERT(ctrl->hd_hi != ctrl->hd_lo);
			return TRUE;
		}
	} else 
		return TRUE;
}

//################
//#
//#    Public functions
//#
//################

VOID DoHp2225(HP2225 *ctrl)
{
BYTE c, eoi;
int l;

	if (pop_c(ctrl, &c)) {
		if (c == 0x04) {			// IFC
//			hp2225_save(ctrl);
//			hp2225_reset((void *) ctrl);
		}
	}
	if (pop_d(ctrl, &c, &eoi)) {
		if (ctrl->listen) {
			if (ctrl->hfile != NULL) {
				WriteFile(ctrl->hfile, &c, 1, &l, NULL);
			}
		}
	}
}

BOOL hp2225_save(HP2225 *ctrl)
{
	if (ctrl->hfile != NULL) {
		CloseHandle(ctrl->hfile);
		ctrl->hfile = NULL;
		ctrl->fn++;
	}
	return TRUE;
}

BOOL hp2225_eject(HP2225 *ctrl)
{
	hp2225_save(ctrl);
	return TRUE;
}

BOOL hp2225_widle(HP2225 *ctrl)
{
	return FALSE;										// hp2225 is iddle
}

VOID hp2225_reset(void *controler)
{
	HP2225 *ctrl = (HP2225 *) controler;
	DWORD k;

//	ctrl->st2225 = 0;							// state of hp2225 controller
	ctrl->talk = FALSE;							// MTA received ?
	ctrl->listen = FALSE;						// MLA received ?
	ctrl->ppol_e = TRUE;						// parallel poll enabled
	ctrl->hc_hi = 0;							// hi mark
    ctrl->hc_lo = 0;							// lo mark
	ctrl->hd_hi = 0;							// hi mark
	ctrl->hd_lo = 0;							// lo mark

	if (ctrl->hfile == NULL) {
		k = wsprintf(ctrl->name, _T("printer-%03d.txt"), ctrl->fn);
		ctrl->name[k] = 0x00;
		ctrl->hfile = CreateFile(ctrl->name,
					   GENERIC_WRITE,
					   FILE_SHARE_READ,
					   NULL,
					   CREATE_ALWAYS,
					   FILE_FLAG_SEQUENTIAL_SCAN,
					   NULL);
		if (ctrl->hfile == INVALID_HANDLE_VALUE) {
			ctrl->hfile = NULL;
			return;
		}
	}
}

VOID hp2225_stop(HP2225 *ctrl)
{
	hp2225_save(ctrl);
}
