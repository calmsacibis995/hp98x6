/*
 *   Hp-9130.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   HP9130 internal floppy disk with 2 units 270 KB capacity 256 bytes per sector
//	 only LOCAL mode done for a wd 1793 warning, no side in the controller, side select external
//	 or mb8876

#include "StdAfx.h"
#include "HP98x6.h"
#include "kml.h"
#include "mops.h"								// I/O definitions

//#define DEBUG_HP9130						// debug flag
//#define DEBUG_HP9130H						// debug flag
#if defined (DEBUG_HP9130) || defined (DEBUG_HP9130H)
	TCHAR buffer[256];
	int k;
#endif

// hp9130 geometry

#define HEADS 2
#define CYLINDERS 33
#define SECTORS 16

#define DISK_BYTES ((HEADS * CYLINDERS * SECTORS) << 8)

//################
//#
//#    Low level subroutines
//#
//################

//
// get lif name from volume in ram
//
static VOID GetLifName(BYTE unit)
{
	BYTE i = 0;

	if (Chipset.Hp9130.disk[unit] == NULL) {
		Chipset.Hp9130.lifname[unit][0] = 0x00;
		return;
	}
	while (i < 6) {
		if (Chipset.Hp9130.disk[unit][i+2] == 0x00) {
			Chipset.Hp9130.lifname[unit][i] = 0x00;
			i = 7;
		} else {
			Chipset.Hp9130.lifname[unit][i] = Chipset.Hp9130.disk[unit][i+2];
			i++;
		}
	}
	if (i == 6) Chipset.Hp9130.lifname[unit][i] = 0x00;
}

//
// adjust address of sector 
//
static VOID hp9130_raj_addr(VOID)
{
	while (Chipset.Hp9130.sector >= SECTORS)
		Chipset.Hp9130.sector -= SECTORS;
	// while (Chipset.Hp9130.head >= HEADS)
	//	Chipset.Hp9130.head -= HEADS;
	while (Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] >= CYLINDERS)
		Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] -= CYLINDERS;
	Chipset.Hp9130.addr = (Chipset.Hp9130.sector + 
							  Chipset.Hp9130.head * SECTORS + 
							  Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] * HEADS * SECTORS
							 ) << 8;
}

//
// increment sector address
// head is done directly 
//
static VOID hp9130_inc_addr(VOID)
{
	Chipset.Hp9130.sector++;
	if (Chipset.Hp9130.sector >= SECTORS) {
		Chipset.Hp9130.sector = 0;
		Chipset.Hp9130.cylinder[Chipset.Hp9130.unit]++;
		if (Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] >= CYLINDERS)
			Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] = 0;	// wrap
	}
	hp9130_raj_addr();
}

//
// increment track and adjust address
//
static VOID hp9130_inc_track(VOID)
{
	Chipset.Hp9130.cylinder[Chipset.Hp9130.unit]++;
	if (Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] >= CYLINDERS)
		Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] = 0;	// wrap
	hp9130_raj_addr();
}

//
// decrement track and adjust address
//
static VOID hp9130_dec_track(VOID)
{
	Chipset.Hp9130.cylinder[Chipset.Hp9130.unit]--;
	if (Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] == 0xFF)
		Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] = CYLINDERS - 1;	// wrap
	hp9130_raj_addr();
}

//################
//#
//#    Public functions
//#
//################

//
// write in disk IO space
//
BYTE Write_9130(BYTE *a, WORD d, BYTE s)
{
	if (s != 1) {
		InfoMessage(_T("9130 write acces not in byte !!"));
		return BUS_ERROR;
	}

	a--;		// correct the little endianness

	if ((d >= 0xE000) && (d <= 0xE200-s)) {			// on board ram
		if (!(d & 0x0001)) {
			// Chipset.Hp9130.last_ram = (BYTE) ((d-0xE000)>>1);
			Chipset.Hp9130.ob_ram[(d-0xE000)>>1] = *a;
		}
		return BUS_OK;
	}

	#if defined DEBUG_HP9130
		k = wsprintf(buffer,_T("%06X: HP9130 : %02X->%04X:%d\n"), Chipset.Cpu.PC, *a, d,s);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif

	switch (d) {
		case 0x5000:										// extended COM
			Chipset.Hp9130.xcom = *a;
			if (!(Chipset.Hp9130.xcom & 0x08)) {							// reset floppy
				#if defined DEBUG_HP9130
					k = wsprintf(buffer,_T("%06X: HP9130 : Reset fdc ...\n"), Chipset.Cpu.PC);
					OutputDebugString(buffer); buffer[0] = 0x00;
				#endif
				Chipset.Hp9130.boot = 1;
			} else {
				if (Chipset.Hp9130.boot) { // end of reset, make the seek
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("%06X: HP9130 : Reset fdc ... go\n"), Chipset.Cpu.PC);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					Chipset.Hp9130.unit = (Chipset.Hp9130.xcom >> 6) & 0x01;										// addressed unit
					Chipset.Hp9130.status = (Chipset.Hp9130.disk[Chipset.Hp9130.unit] != NULL) ? 0x00 : 0x80;		// not ready if no disk loaded
					Chipset.Hp9130.local = (Chipset.Hp9130.xcom & 0x20) ? 1 : 0;											// local mode for transferts
					Chipset.Hp9130.motor = (Chipset.Hp9130.xcom & 0x01) ? 1 : 0;
					Chipset.Hp9130.head = (Chipset.Hp9130.xcom & 0x04) ? 1 : 0; 
					Chipset.Hp9130.rw = (Chipset.Hp9130.xcom & 0x10) ? 1 : 0;					// not usefull
					Chipset.Hp9130.boot = 0;
					Chipset.Hp9130.com = 0x03;
					Chipset.Hp9130.status |= 0x01;									// busy 
					Chipset.Hp9130.st9130 = 10;
				}
			}	
			Chipset.Hp9130.unit = (Chipset.Hp9130.xcom >> 6) & 0x01;										// addressed unit
			Chipset.Hp9130.status = (Chipset.Hp9130.disk[Chipset.Hp9130.unit] != NULL) ? 0x00 : 0x80;		// not ready if no disk loaded
			Chipset.Hp9130.local = (Chipset.Hp9130.xcom & 0x20) ? 1 : 0;											// local mode for transferts
			Chipset.Hp9130.motor = (Chipset.Hp9130.xcom & 0x01) ? 1 : 0;
			Chipset.Hp9130.head = (Chipset.Hp9130.xcom & 0x04) ? 1 : 0; 
			Chipset.Hp9130.rw = (Chipset.Hp9130.xcom & 0x10) ? 1 : 0;					// not usefull
			hp9130_raj_addr();
			#if defined DEBUG_HP9130
			k = wsprintf(buffer,_T("%06X: HP9130 : XCOM: %02X, unit: %d head: %d local: %d\n"), Chipset.Cpu.PC, *a, Chipset.Hp9130.unit, Chipset.Hp9130.head, Chipset.Hp9130.local);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0x5400:										// clear xstatus
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : clear XSTATUS\n"),Chipset.Cpu.PC);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Hp9130.xstatus &= 0xFD;					// clear only media change
			break;
		case 0xC000:										// COM
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : COM : %02X\n"),Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Hp9130.com = *a;
			Chipset.Hp9130.xstatus &= 0xF7;					// clear interrupt
			if ((Chipset.Hp9130.status & 0x01) && ((Chipset.Hp9130.com & 0xF0) != 0xD0))
				InfoMessage(_T("9130 COM while busy !!"));
			else 
			{	
				Chipset.Hp9130.status = 0x01;									// busy 
				Chipset.Hp9130.st9130 = 10;														// do COM
			}
			break;
		case 0xC002:										// track
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : TRACK : %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Hp9130.track = (*a);
			//Chipset.Hp9130.cylinder = *a;
			hp9130_raj_addr();
			break;
		case 0xC004:										// sector
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : SECTOR : %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Hp9130.sector = *a;
			hp9130_raj_addr();
			break;
		case 0xC006:										// data
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : DATA : %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Hp9130.data = *a;
			//Chipset.Hp9130.data_loaded = 1;
			break;
		default: 
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : %02X -> %04X:%d -> BUS ERROR\n"), Chipset.Cpu.PC, *a, d, s);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			InfoMessage(_T("9130 write acces bus error !!"));
			return BUS_ERROR;
			break;
	}	
	return BUS_OK;
}

//
// read in disk IO space
//
BYTE Read_9130(BYTE *a, WORD d, BYTE s)
{
	if (s != 1) {
		InfoMessage(_T("9130 read acces not in byte !!"));
		return BUS_ERROR;
	}

	a--;		// correct the little endianness

	if ((d >= 0xE000) && (d <= 0xE200-s)) {			// on board ram
		if (!(d & 0x001)) {
			Chipset.Hp9130.last_ram = (BYTE) ((d-0xE000)>>1);		// last address accessed on read ...
			*a = Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram];
		}
		return BUS_OK;
	}

	switch (d) {
		case 0x5000:										// extended STATUS
			*a = Chipset.Hp9130.xstatus;									// b3: interrupt b2: margin error b1: media change b0: data request
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : XSTATUS = %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0x5400:										// read clear XSTATUS for clr.b 68000 compatibiliry
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : read CLXSTATUS (%02X)\n"), Chipset.Cpu.PC, Chipset.Hp9130.xstatus);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Hp9130.xstatus &= 0xFD;							// only media change
			*a = 0x00;												// who cares
			break;
		case 0xC000:										// status
			*a = Chipset.Hp9130.status;									
			Chipset.Hp9130.xstatus &= 0xF7;					// clear interrupt
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : STATUS = %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xC002:										// track
			*a = Chipset.Hp9130.track;
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : TRACK = %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xC004:										// sector
			*a = Chipset.Hp9130.sector;
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : SECTOR = %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xC006:										// data
			*a = Chipset.Hp9130.data;
			// Chipset.Hp9130.data_read = 1;
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : DATA = %02X\n"), Chipset.Cpu.PC, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		default: 
			#if defined DEBUG_HP9130
				k = wsprintf(buffer,_T("%06X: HP9130 : %04X:%d = %02X -> BUS ERROR\n"), Chipset.Cpu.PC, d, s, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			InfoMessage(_T("9130 read acces bus error !!"));
			return BUS_ERROR;
			break;
	}
	return BUS_OK;
}
//
//   FSA to simulate the disc controller 
//

VOID DoHp9130(VOID)
{
	if (Chipset.Hp9130.st9130 > 1) {
		Chipset.annun &= ~(1 << (2 + 0*2));		// unit 0
		Chipset.annun &= ~(1 << (2 + 1*2));		// unit 1
		Chipset.annun |= (1 << (2 + Chipset.Hp9130.unit*2));
	}

	switch (Chipset.Hp9130.st9130)
	{

		case 0:									// IDLE STATE
			Chipset.annun &= ~(1 << (2 + 0*2));		// unit 0
			Chipset.annun &= ~(1 << (2 + 1*2));		// unit 1
			Chipset.Hp9130.status &= 0xFE;							// clear busy
			Chipset.Hp9130.st9130++;
			break;
		case 1:
			break;
		case 10:									// do Com
			switch(Chipset.Hp9130.com & 0xF0) {
				case 0x00:						// restore Com					type I
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Restore Com\n"));
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					Chipset.Hp9130.sector = 0;
					Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] = 0;
					Chipset.Hp9130.track = 0;
					hp9130_raj_addr();
					Chipset.Hp9130.st9130 = 20;
					break;
				case 0x10:						// seek Com						type I
					Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] = Chipset.Hp9130.data; 
					Chipset.Hp9130.track = Chipset.Hp9130.data; 
					hp9130_raj_addr();
					Chipset.Hp9130.st9130 = 20;
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Seek Com : %03X : %08X\n"), Chipset.Hp9130.data, Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0x20:						// step no update Com			type I
				case 0x30:						// step with update Com			type I
					if (Chipset.Hp9130.step) 
						hp9130_inc_track();
					else hp9130_dec_track();
					if (Chipset.Hp9130.com & 0x10) {				// update
						Chipset.Hp9130.track = Chipset.Hp9130.cylinder[Chipset.Hp9130.unit];
					}
					Chipset.Hp9130.st9130 = 20;
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Step Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0x40:						// step in no update Com		type I
				case 0x50:						// step in with update Com		type I
					Chipset.Hp9130.step = 1;
					hp9130_inc_track();
					if (Chipset.Hp9130.com & 0x10) {				// update
						Chipset.Hp9130.track = Chipset.Hp9130.cylinder[Chipset.Hp9130.unit];
					}
					Chipset.Hp9130.st9130 = 20;
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Step in Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0x60:						// step out no update Com		type I
				case 0x70:						// step out with update Com		type I
					Chipset.Hp9130.step = 0;
					hp9130_dec_track();
					if (Chipset.Hp9130.com & 0x10) {				// update
						Chipset.Hp9130.track = Chipset.Hp9130.cylinder[Chipset.Hp9130.unit];
					}
					Chipset.Hp9130.st9130 = 20;
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Step out Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0x80:						// read sector Com				type II
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Read Sector Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					// if (!hp9130_local) InfoMessage(_T("9130 read sector non local !!"));
					if (Chipset.Hp9130.com & 0x02) {				// check side
						if (((Chipset.Hp9130.com >> 3) & 0x01) != Chipset.Hp9130.head) {
							Chipset.Hp9130.status |= 0x80;					// record-not-found
//							Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
						}
					}
					if (Chipset.Hp9130.disk[Chipset.Hp9130.unit] != NULL) {
						memcopy(Chipset.Hp9130.ob_ram, &Chipset.Hp9130.disk[Chipset.Hp9130.unit][Chipset.Hp9130.addr], 256);
//						Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
					} else {
						Chipset.Hp9130.status |= 0x80;					// record-not-found
//						Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
					}
					Chipset.Hp9130.cycles = 0;
					Chipset.Hp9130.st9130 = 30;						// wait a bit
					break;
				case 0x90:						// read sector multiple Com		type II
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : read sector multiple Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					InfoMessage(_T("9130 read sector  multiple !!"));
					break;
				case 0xA0:						// write sector Com				type II
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Write Sector Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					//if (!hp9130_local) InfoMessage(_T("9130 write sector non local !!"));
					if (Chipset.Hp9130.disk[Chipset.Hp9130.unit] != NULL) {
						memcopy(&Chipset.Hp9130.disk[Chipset.Hp9130.unit][Chipset.Hp9130.addr], Chipset.Hp9130.ob_ram, 256);
						if (Chipset.Hp9130.addr == 0x00000000) {
							GetLifName(Chipset.Hp9130.unit);
							kmlButtonText6(1+Chipset.Hp9130.unit, Chipset.Hp9130.lifname[Chipset.Hp9130.unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3
						}
//						Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
					} else {
						Chipset.Hp9130.status |= 0x10;					// record-not-found
//						Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
					}
					Chipset.Hp9130.cycles = 0;
					Chipset.Hp9130.st9130 = 30;						// wait a bit
					break;
				case 0xB0:						// write sector multiple Com	type II
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : write sector multiple Com : %08X\n"), Chipset.Hp9130.addr);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					InfoMessage(_T("9130 write sector  multiple !!"));
					break;
				case 0xC0:						// read address Com				type III
					// InfoMessage(_T("9130 read address !!"));
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("------: HP9130 : Read Address Com : track: %02X, side: %d\n"), Chipset.Hp9130.cylinder[Chipset.Hp9130.unit], Chipset.Hp9130.head);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					if (Chipset.Hp9130.disk[Chipset.Hp9130.unit] != NULL) {
						Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram++] = Chipset.Hp9130.cylinder[Chipset.Hp9130.unit];	// track addr
						Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram++] = Chipset.Hp9130.head;								// side
						Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram++] = 0;												// sector
						Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram++] = 2;												// sector length
						Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram++] = 0xFF;											// crc1
						Chipset.Hp9130.ob_ram[Chipset.Hp9130.last_ram++] = 0xFF;											// crc2
//						Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
					} else {
						Chipset.Hp9130.status |= 0x10;					// record-not-found
//						Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
					}
					Chipset.Hp9130.cycles = 0;
					Chipset.Hp9130.st9130 = 30;						// wait a bit
					break;
				case 0xD0:						// force interrupt Com			type IV
					#if defined DEBUG_HP9130
						k = wsprintf(buffer,_T("%06X: HP9130 : Force interrupt com %02X (was %04X)\n"), Chipset.Cpu.PC, Chipset.Hp9130.com & 0x0F, Chipset.Hp9130.st9130);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					Chipset.Hp9130.xstatus &= 0xF7;					// clear interrupt
					//InfoMessage(_T("9130 force interrupt  !!"));
					Chipset.Hp9130.st9130 = 0;
					break;
				case 0xE0:						// read track Com				type III
					InfoMessage(_T("9130 read track !!"));
					break;
				case 0xF0:						// write track Com				type III
					InfoMessage(_T("9130 write track !!"));
					break;
				default:
					InfoMessage(_T("9130 unknow Com !!"));
					break;
			}
			break;
		case 20:									// status for type I
			if ((Chipset.Hp9130.cylinder[Chipset.Hp9130.unit] == 0) && (Chipset.Hp9130.disk[Chipset.Hp9130.unit] != NULL))
				Chipset.Hp9130.status |= 0x04;	// TRACK 0
			else Chipset.Hp9130.status &= 0xFB;
			if (Chipset.Hp9130.com & 0x04) { // verify flag
				if (Chipset.Hp9130.disk[Chipset.Hp9130.unit] == NULL) // no disk
					Chipset.Hp9130.status |= 0x80;							// not ready
				else {
					if (Chipset.Hp9130.track == Chipset.Hp9130.cylinder[Chipset.Hp9130.unit])
						Chipset.Hp9130.status &= 0x6F;							// ready and no error
					else 
						Chipset.Hp9130.status |= 0x10;							// seek error
				}
			}
			Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
			Chipset.Hp9130.st9130 = 0;
			break;
		case 30:									// wait for completion of read and write op
			if (Chipset.Hp9130.cycles > 30000) {
				Chipset.Hp9130.xstatus |= 0x08;					// set interrupt
				Chipset.Hp9130.st9130 = 0;
			} else Chipset.Hp9130.cycles += Chipset.dcycles;
			break;
	}
}

//
// load a volume in unit in ram
//
BOOL hp9130_load(BYTE unit, LPCTSTR szFilename)
{
	HANDLE  hDiskFile = NULL;
	DWORD dwFileSize, dwRead;
	LPBYTE pDisk = NULL;

	hDiskFile = CreateFile(szFilename,
						   GENERIC_READ,
						   FILE_SHARE_READ,
						   NULL,
						   OPEN_EXISTING,
						   FILE_FLAG_SEQUENTIAL_SCAN,
						   NULL);
	if (hDiskFile == INVALID_HANDLE_VALUE)
	{
		hDiskFile = NULL;
		return FALSE;
	}
	dwFileSize = GetFileSize(hDiskFile, NULL);
	if (dwFileSize != (DWORD)(DISK_BYTES))
	{ // file is too big.
		CloseHandle(hDiskFile);
		return FALSE;
	}

	pDisk = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, DISK_BYTES);
	if (pDisk == NULL)
	{
		CloseHandle(hDiskFile);
		return FALSE;
	}

	// load file content)
	ReadFile(hDiskFile, pDisk, dwFileSize, &dwRead,NULL);
	CloseHandle(hDiskFile);
	_ASSERT(dwFileSize == dwRead);

	if (Chipset.Hp9130.disk[unit] != NULL)
		HeapFree(hHeap, 0, Chipset.Hp9130.disk[unit]);
	Chipset.Hp9130.disk[unit] = pDisk;

	Chipset.Hp9130.xstatus |= 0x02;

	lstrcpy(Chipset.Hp9130.name[unit], szFilename);

	Chipset.annun |= (1 << (3 + unit*2));					// annun 4 & 6
	UpdateAnnunciators(FALSE);

	GetLifName(unit);
	kmlButtonText6(1+unit, Chipset.Hp9130.lifname[unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3

	return TRUE;
}

//
// save a volume in unit in ram
//
BOOL hp9130_save(BYTE unit, LPCTSTR szFilename)
{
	HANDLE  hDiskFile = NULL;
	DWORD dwWritten;

	if (Chipset.Hp9130.disk[unit] == NULL)
		return FALSE;

	hDiskFile = CreateFile(szFilename,
						   GENERIC_WRITE,
						   FILE_SHARE_READ,
						   NULL,
						   CREATE_ALWAYS,
						   FILE_FLAG_SEQUENTIAL_SCAN,
						   NULL);
	if (hDiskFile == INVALID_HANDLE_VALUE)
	{
		hDiskFile = NULL;
		return FALSE;
	}

	// save file content
	WriteFile(hDiskFile, Chipset.Hp9130.disk[unit], DISK_BYTES, &dwWritten,NULL);
	CloseHandle(hDiskFile);
	lstrcpy(Chipset.Hp9130.name[unit], szFilename);

	_ASSERT((DWORD)(DISK_BYTES) == dwWritten);

	return TRUE;
}

// 
// eject a volume in unit from ram (no save done)
//
BOOL hp9130_eject(BYTE unit)
{
	if (Chipset.Hp9130.disk[unit] != NULL)
		HeapFree(hHeap, 0, Chipset.Hp9130.disk[unit]);
	Chipset.Hp9130.name[unit][0] = 0x00;
	Chipset.Hp9130.disk[unit] = NULL;

	Chipset.Hp9130.xstatus |= 0x02;			// media changed

	Chipset.annun &= ~(1 << (3 + unit*2));			// anun 4 & 6
	UpdateAnnunciators(FALSE);
	
	Chipset.Hp9130.lifname[unit][0] = 0x00;
	kmlButtonText6(1+unit, Chipset.Hp9130.lifname[unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3

	return TRUE;
}

//
// wait for controller idle state
//
BOOL hp9130_widle(VOID)
{
	DWORD dwRefTime;

	SuspendDebugger();									// suspend debugger

	dwRefTime = timeGetTime();
//	Chipset.idle = FALSE;
	Sleep(10);
	// wait for the Iddle state with 5 sec timeout
	while (((timeGetTime() - dwRefTime) < 5000L) && !(Chipset.Hp9130.st9130 < 2))
		Sleep(10);

	if (Chipset.Hp9130.st9130 < 2)									// not timeout, hp9130 is idle
		return FALSE;									// idle state
	else
		ResumeDebugger();								// timeout, resume to debugger

	return TRUE;										// hp9130 was busy
}

//
// reset controller
//
VOID hp9130_reset(VOID)
{
	BYTE i;

	for (i = 0; i < 2; i++) {
		if (Chipset.Hp9130.name[i][0] != 0x00) {
			hp9130_load(i, Chipset.Hp9130.name[i]);
//			_ASSERT(Chipset.Hp9130.disk[i] != NULL);
		}
		Chipset.Hp9130.cylinder[i] = 0;
		if (Chipset.Hp9130.disk[i] != NULL) {
			Chipset.annun |= (1 << (3 + i*2));
		}
		else 
			Chipset.annun &= ~(1 << (3 + i*2));
	}
	Chipset.Hp9130.head = 0;
	Chipset.Hp9130.sector = 0;
	Chipset.Hp9130.addr = 0;
	Chipset.Hp9130.motor = 0;
	Chipset.Hp9130.unit = 0;
	Chipset.Hp9130.xcom = 0x00;
	Chipset.Hp9130.xstatus = 0x02;
	Chipset.Hp9130.com = 0x00;
	Chipset.Hp9130.status = 0x00;
	Chipset.Hp9130.boot = 0;
	Chipset.Hp9130.st9130 = 0;								// state of hp9130 controller
}

//
// remove all floppy
//
VOID hp9130_stop(VOID)						
{
	int unit;
	if ((Chipset.type != 16) && (Chipset.type != 37)) {
		for (unit = 0; unit < 2; unit++) {
			if (Chipset.Hp9130.disk[unit] != NULL)
				HeapFree(hHeap, 0, Chipset.Hp9130.disk[unit]);
			Chipset.Hp9130.disk[unit] = NULL;

			Chipset.Hp9130.xstatus |= 0x02;			// media changed

			Chipset.annun &= ~(1 << (3 + unit*2));			// anun 4 & 6
			UpdateAnnunciators(FALSE);
			
			Chipset.Hp9130.lifname[unit][0] = 0x00;
			kmlButtonText6(1+unit, Chipset.Hp9130.lifname[unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3
		}
	}
}
