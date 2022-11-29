/*
 *   Display16.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
// Display driver. Update is done in real-time at each acces in video-memory in the graph or alpha bitmap.
// CRT refresh frequency is emulated with a a timer based on 68000 cycle will create screen bitmap and copy it to main display
// counter on real-speed and with High performance counter otherwise at 1/60 sec.
//
// HP 98204A like 9816
// HP 98204B like 9836
// 9826 not done
// 
// HP9816  with 09816-66582 enhancement
// basic graphic 400x300 at zoom 2,2		16 Kb 
// basic alpha 800x300 at zoom 1,2 80x25	2kb with effect (not blink, could be done)
//
// HP9836
// basic graphic 512x390 at zoom 2,2		32 Kb
// alpha alpha 960x780 at zoom 1,1			4kb with effect (not blink, could be done)
//
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "resource.h"
#include "kml.h"
#include "mops.h"

//#define DEBUG_DISPLAY16						// switch for DISPLAY debug purpose
//#define DEBUG_DISPLAY16A					// switch for alpha DISPLAY debug purpose
//#define DEBUG_DISPLAY16G					// switch for graph DISPLAY debug purpose
//#define DEBUG_DISPLAY16GH					// switch for graph DISPLAY debug purpose
//#define DEBUG_DISPLAY16S					// switch for reg DISPLAY debug purpose
//#define DEBUG_DISPLAY16H					// switch for DISPLAY debug purpose
#if defined(DEBUG_DISPLAY16H) || defined(DEBUG_DISPLAY16) || defined(DEBUG_DISPLAY16) || defined(DEBUG_DISPLAY16G) || defined(DEBUG_DISPLAY16A) || defined(DEBUG_DISPLAY16R) 
	TCHAR buffer[256];
	int k;
//	unsigned long l;
#endif

#define ALPHASIZE 4096

//
// Display for writing byte a at video memory address d for alpha part dual ported ram address with s effect byte
//
static VOID WriteToMainDisplay16(BYTE a, WORD d, BYTE s)
{
	INT  x0, xmin, xmax;
	INT  y0, ymin, ymax;
	DWORD	dwRop;
	WORD	dy;
	WORD	delta;						// distance from display start in byte

	#if defined DEBUG_DISPLAY16H
	if ((d == 159) || (d == 79) || (d == 78) || (d == 158) || (d == 80) || (d == 0) || (d == 81) || (d == 1)) {
		wsprintf(buffer,_T("%06X: Write Main Display %04X : %02X (%02X)\n"),Chipset.Cpu.PC, d, a, s);
		OutputDebugString(buffer);
	}
	#endif
	
	d &= 0x07FF;

	if (d >= (Chipset.Display16.start & 0x07FF)) {
		delta = d - (Chipset.Display16.start & 0x07FF);
	} else {
		delta = d + (0x0800 - (Chipset.Display16.start & 0x07FF));
	}
	d = Chipset.Display16.start + delta;					// where to write
	d &= 0x3FFF;
	if (delta < 2000) {										// visible ?
		y0 = (delta / 80) * Chipset.Display16.alpha_char_h + nLcdY;												// where on Y
		x0 = (delta % 80) * Chipset.Display16.alpha_char_w + nLcdX;												// where on X
		xmin = xmax = x0;											// min box
		ymin = ymax = y0;
		dy = 0;
		switch (d >> 12) {										// get TEXT (bit13), KEYS (bit12)
			case 0:	dwRop = BLACKNESS;								// nothing displayed
				break;												
			case 1: dwRop = (d & 0x0800) ? SRCCOPY : BLACKNESS;		// keys on, text off
				break;
			case 2:	dwRop = (d & 0x0800) ? BLACKNESS : SRCCOPY;		// keys off, text on
				break;
			default: dwRop = SRCCOPY;								// keys on, text on
				break;
		}
		if (s & 0x04) {		// underline
			dy += 4*Chipset.Display16.alpha_char_h;						// change line in font bitmap
		}
		if (s & 0x08) {		// half bright
			dy += 2*Chipset.Display16.alpha_char_h;						// change line in font bitmap
		}
		if (s & 0x01) {		// inverse video
			dy += Chipset.Display16.alpha_char_h;						// change line in font bitmap
		}
		// copy the font bitmap part to alpha1 bitmap
		BitBlt(hAlpha1DC, x0, y0, Chipset.Display16.alpha_char_w, Chipset.Display16.alpha_char_h, hFontDC, (int)(a) * Chipset.Display16.alpha_char_w, dy, dwRop);
		xmax += Chipset.Display16.alpha_char_w;												// adjust max box
		ymax += Chipset.Display16.alpha_char_h;
		// adjust rectangle
		if (xmax > Chipset.Display16.a_xmax) Chipset.Display16.a_xmax = xmax;
		if (xmin < Chipset.Display16.a_xmin) Chipset.Display16.a_xmin = xmin;
		if (ymax > Chipset.Display16.a_ymax) Chipset.Display16.a_ymax = ymax;
		if (ymin < Chipset.Display16.a_ymin) Chipset.Display16.a_ymin = ymin;
	} else {
		#if defined DEBUG_DISPLAY16A
			wsprintf(buffer,_T("%06X: Write Main Display %02X - %04X : %04X : %04X\n"), Chipset.Cpu.PC, a, Chipset.Display.start, d, Chipset.Display.end);
			OutputDebugString(buffer);
		#endif
	} 
}

//
// remove the blinking cursor if last on directly on main display bitmap
//
static VOID Cursor_Remove16(VOID)
{
	WORD d;
	INT x0, y0;

	if (Chipset.Display16.cursor_last) {								// was on
		d = Chipset.Display16.cursor;
		if (d < Chipset.Display16.start) d += 0x0800;
		if ((d >= Chipset.Display16.start) && (d < Chipset.Display16.end)) {	// visible 
			y0 = ((d - Chipset.Display16.start) / 80) * Chipset.Display16.alpha_char_h + nLcdY;											// where on Y
			x0 = ((d - Chipset.Display16.start) % 80) * Chipset.Display16.alpha_char_w + nLcdX;											// where on X
			StretchBlt(hWindowDC, x0, y0*2, Chipset.Display16.alpha_char_w, Chipset.Display16.alpha_char_h*2, 
				       hAlpha1DC, x0, y0, Chipset.Display16.alpha_char_w, Chipset.Display16.alpha_char_h, SRCCOPY);
			if (Chipset.Display16.graph_on)									// put graph part again if it was on
				StretchBlt(hWindowDC, x0, y0*2, Chipset.Display16.alpha_char_w, Chipset.Display16.alpha_char_h*2, 
				           hGraphDC, x0/2, y0, Chipset.Display16.alpha_char_w/2, Chipset.Display16.alpha_char_h, SRCPAINT);
			GdiFlush();
		}
	} 
}

//
// display the blinking cursor if needed
//
static VOID Cursor_Display16(VOID)
{
	WORD	d;
	INT		x0, y0;
	WORD	dy = 0;
	DWORD	dwRop;

	Chipset.Display16.cursor_last = 0;									// actually not displayed
	Chipset.Display16.cursor = Chipset.Display16.cursor_new;				// new location on screen	
	if (Chipset.Display16.cursor_blink & Chipset.Display16.cnt)				// should be off, return
		return;
	if (Chipset.Display16.cursor_b < Chipset.Display16.cursor_t)			// no size, return
		return;

	d = Chipset.Display16.cursor;
	if (d < Chipset.Display16.start) d += 0x0800;
	if ((d >= Chipset.Display16.start) && (d < Chipset.Display16.end)) {	// visible ?
		y0 = ((d - Chipset.Display16.start) / 80) * Chipset.Display16.alpha_char_h + nLcdY + Chipset.Display16.cursor_t;			// where on Y
		x0 = ((d - Chipset.Display16.start) % 80) * Chipset.Display16.alpha_char_w + nLcdX;											// where on X
		switch (d >> 12) {		// do the effects
			case 0:	dwRop = BLACKNESS;
				break;
			case 1: dwRop = (d & 0x0800) ? SRCCOPY : BLACKNESS;
				break;
			case 2:	dwRop = (d & 0x0800) ? BLACKNESS : SRCCOPY;
					dy = (d & 0x0800) ? 0 : Chipset.Display16.alpha_char_h;
				break;
			default: dwRop = SRCCOPY;
					dy = (d & 0x0800) ? Chipset.Display16.alpha_char_h : 0;
				break;
		}
		StretchBlt(hWindowDC, x0, y0*2, Chipset.Display16.alpha_char_w, Chipset.Display16.cursor_h*2,
			       hFontDC, 256*Chipset.Display16.alpha_char_w, 0, Chipset.Display16.alpha_char_w, Chipset.Display16.cursor_h, dwRop);
		if (Chipset.Display16.graph_on)
			StretchBlt(hWindowDC, x0, y0*2, Chipset.Display16.alpha_char_w, Chipset.Display16.cursor_h*2, 
			           hGraphDC, x0/2, y0, Chipset.Display16.alpha_char_w/2, Chipset.Display16.cursor_h, SRCPAINT);
		GdiFlush();
		Chipset.Display16.cursor_last = 1;											// cursor is shown		
	} 
}

//################
//#
//#    Public functions
//#
//################

//
// write in display IO space
// 
// 6845 default regs for 9836 (80x25)
// R00 : 114
// R01 :  80
// R02 :  76
// R03 :   7
// R04 :  26
// R05 :  10
// R06 :  25
// R07 :  25
// R08 :   0
// R09 :  14
// R10 :  76	cursor shape : start at 14d, blink a 1/32
// R11 :  13	end at 19d
// 
// R11 :  $0D
// R12 :  $30 display pos : $30D0 : both area displayed (3) with 2 lines of soft keys (D0)
// R13 :  $D0  ""
// R14 :  $30 cursor pos : $30CF
// R15 :  $CF  ""

BYTE Write_Display16(BYTE *a, WORD d, BYTE s)
{
	WORD	start_new;

	d &= 0x3FFF;

	if ((d >= 0x2000) && (d <= 0x2000+(ALPHASIZE<<1)-s)) {
		d -= 0x2000;															// base at 0
		while (d >= (WORD) (ALPHASIZE)) d -= (WORD) (ALPHASIZE);				// wrap address
//		if ((s == 1) && !(d & 0x0001))
//			return BUS_ERROR;													// no blink and all
		while (s) {
			Chipset.Display16.alpha[d] = *(--a); 
			#if defined DEBUG_DISPLAY16
			if (d > 4095) {
				k = wsprintf(buffer,_T("%06X: DISPLAY16 : Write MEM reg %06X = %02X\n"), Chipset.Cpu.PC, d, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			}
			#endif
			s--;
			if ((d & 0x0001) == 0x0001)  WriteToMainDisplay16(*a, d >> 1, Chipset.Display16.alpha[d-1]);
			d++;
			// else return BUS_ERROR;
		}
		return BUS_OK;
	}
	if (s == 2) {						// acces in word
		if (d == 0x0000) {					// 6845 reg
			--a;
			Chipset.Display16.reg = *(--a);
			return BUS_OK;
		}
		else if (d == 0x0002) {				// 6845 regs
			--a;
			Chipset.Display16.regs[Chipset.Display16.reg] = *(--a);
			switch (Chipset.Display16.reg) {
				case 10:
					Chipset.Display16.cursor_t = (*a) & 0x1F;
					switch (((*a) & 0x60) >> 5) {
						case 0:												
							Chipset.Display16.cursor_blink = 0x00;			//always on
							break;
						case 1:
							Chipset.Display16.cursor_blink = 0xFF;			// off
							break;
						case 2:
							Chipset.Display16.cursor_blink = 0x20;			// blink 1/16
							break;
						default:
							Chipset.Display16.cursor_blink = 0x10;			// blink 1/32
							break;
					}
					if (Chipset.Display16.cursor_b >= Chipset.Display16.cursor_t) 
						Chipset.Display16.cursor_h = Chipset.Display16.cursor_b - Chipset.Display16.cursor_t + 1;
					break;
				case 11:
					Chipset.Display16.cursor_b = (*a) & 0x3F;
					if (Chipset.Display16.cursor_b >= Chipset.Display16.cursor_t) 
						Chipset.Display16.cursor_h = Chipset.Display16.cursor_b - Chipset.Display16.cursor_t + 1;
					break;
				case 12:
				case 13:
					start_new = ((Chipset.Display16.regs[12] << 8) + Chipset.Display16.regs[13]) & 0x3FFF;
					#if defined DEBUG_DISPLAY16S
						k = wsprintf(buffer,_T("%06X: DISPLAY : Write.w reg %02d = %02X %04X -> %04X\n"), Chipset.Cpu.PC, Chipset.Display.reg, *a, Chipset.Display.start, start_new);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					if (start_new != Chipset.Display16.start) {
						Chipset.Display16.start = start_new;
						Chipset.Display16.end = (start_new + 80*25) & 0x3FFF;
						UpdateMainDisplay16(FALSE);		// change of start
					}
					break;
				case 14:
				case 15:
					Chipset.Display16.cursor_new = ((Chipset.Display16.regs[14] << 8) + Chipset.Display16.regs[15]) & 0x3FFF;
					break;
				default:
					break;
			}
			#if defined DEBUG_DISPLAY16R
				k = wsprintf(buffer,_T("%06X: DISPLAY : Write.w reg %02d = %02X\n"), Chipset.Cpu.PC, Chipset.Display.reg, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			return BUS_OK;
		} 
	} else if (s == 1) {				// acces in byte
		if (d == 0x0001) {					// 6845 regs
			Chipset.Display16.reg = *(--a);
			return BUS_OK;
		}
		else if (d == 0x0003) {				// 6845 regs
			Chipset.Display16.regs[Chipset.Display16.reg] = *(--a);
			switch (Chipset.Display16.reg) {
				case 10:
					Chipset.Display16.cursor_t = (*a) & 0x1F;
					switch (((*a) & 0x60) >> 5) {
						case 0:												
							Chipset.Display16.cursor_blink = 0x00;			//always on
							break;
						case 1:
							Chipset.Display16.cursor_blink = 0xFF;			// off
							break;
						case 2:
							Chipset.Display16.cursor_blink = 0x20;			// blink 1/16
							break;
						default:
							Chipset.Display16.cursor_blink = 0x10;			// blink 1/32
							break;
					}
					if (Chipset.Display16.cursor_b >= Chipset.Display16.cursor_t) 
						Chipset.Display16.cursor_h = Chipset.Display16.cursor_b - Chipset.Display16.cursor_t + 1;
					break;
				case 11:
					Chipset.Display16.cursor_b = (*a) & 0x3F;
					if (Chipset.Display16.cursor_b >= Chipset.Display16.cursor_t) 
						Chipset.Display16.cursor_h = Chipset.Display16.cursor_b - Chipset.Display16.cursor_t + 1;
					break;
				case 12:
				case 13:
					start_new = ((Chipset.Display16.regs[12] << 8) + Chipset.Display16.regs[13]) & 0x3FFF;
					#if defined DEBUG_DISPLAY16S
						k = wsprintf(buffer,_T("%06X: DISPLAY : Write.w reg %02d = %02X %04X -> %04X\n"), Chipset.Cpu.PC, Chipset.Display.reg, *a, Chipset.Display.start, start_new);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					if (start_new != Chipset.Display16.start) {
						Chipset.Display16.start = start_new;
						Chipset.Display16.end = (start_new + 80*25) & 0x3FFF;
						UpdateMainDisplay16(FALSE);		// change of start
					}
					break;
				case 14:
				case 15:
					Chipset.Display16.cursor_new = ((Chipset.Display16.regs[14] << 8) + Chipset.Display16.regs[15]) & 0x3FFF;
					break;
				default:
					break;
			}
			#if defined DEBUG_DISPLAY16R
				k = wsprintf(buffer,_T("%06X: DISPLAY : Write.b reg %02d = %02X\n"), Chipset.Cpu.PC, Chipset.Display.reg, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			return BUS_OK;
		}
	}
	return BUS_ERROR;
}

//
// read in display IO space (too wide alpha ram, but nobody cares ...)
// no CRT configuration reg for 9816 and 9836A
//
BYTE Read_Display16(BYTE *a, WORD d, BYTE s)
{
	d &= 0x3FFF;
	if ((d >= 0x2000) && (d <= 0x2000 + (ALPHASIZE<<1)-s)) {	// DUAL port alpha video ram
		d -= 0x2000;
		while (d >= (WORD) (ALPHASIZE)) d -= (WORD) (ALPHASIZE);
		while (s) {
			*(--a) = Chipset.Display16.alpha[d++]; 
			s--;
		}
		return BUS_OK;		
	}
	if ((d == 0x0003) && (s == 1)) {				// 6845 regs
		*(--a) = Chipset.Display16.regs[Chipset.Display16.reg];
		#if defined DEBUG_DISPLAY16R
			k = wsprintf(buffer,_T("%06X: DISPLAY : Read reg %02d = %02X\n"), Chipset.Cpu.PC, Chipset.Display.reg, Chipset.Display.regs[Chipset.Display.reg]);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		return BUS_OK;
	}
	return BUS_ERROR;
}

//
// read in graph mem space 
//
BYTE Read_Graph16(BYTE *a, WORD d, BYTE s)
{
	BYTE graph_on;

	graph_on = (d & 0x8000) ? 0 : 1;					// address of read access can ON/OFF the graph plane display
	if (graph_on && ! Chipset.Display16.graph_on) {			// was off and now on -> should redisplay all graph
		Chipset.Display16.g_xmax = Chipset.Display16.graph_width;
		Chipset.Display16.g_xmin = 0;
		Chipset.Display16.g_ymax = Chipset.Display16.graph_height;
		Chipset.Display16.g_ymin = 0;
	}
	if (!graph_on && Chipset.Display16.graph_on) {			// was on and now off -> should redisplay all alpha
		Chipset.Display16.a_xmax = Chipset.Display16.alpha_width;
		Chipset.Display16.a_xmin = 0;
		Chipset.Display16.a_ymax = Chipset.Display16.alpha_height;
		Chipset.Display16.a_ymin = 0;
	}
	if (graph_on ^ Chipset.Display16.graph_on) {			// changed 
		#if defined DEBUG_DISPLAY16G
			k = wsprintf(buffer,_T("%06X: DISPLAY : graph on %02d -> %02X\n"), Chipset.Cpu.PC, Chipset.Display.graph_on, graph_on);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
	}
	Chipset.Display16.graph_on = graph_on;				// new graph state

	while (s) {			// do the read with endianness
		d &= 0x7FFF;
		*(--a) = Chipset.Display16.graph[d]; 
		#if defined DEBUG_DISPLAY16GH
			if (Chipset.Cpu.PC > 0x010000) {
				k = wsprintf(buffer,_T("%06X: GRAPH(%d) : Read %04X = %02X\n"), Chipset.Cpu.PC, Chipset.Display.graph_on, d, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			}
		#endif
		d++;
		s--;
	}
	return BUS_OK;
}

BYTE Write_Graph16(BYTE *a, WORD d, BYTE s)
{
	LPBYTE	ad;
	WORD x0, y0;
	BYTE graph_on;

	graph_on = (d & 0x8000) ? 0 : 1;					// address of read access can ON/OFF the graph plane display
	if (graph_on && !Chipset.Display16.graph_on) {			// was off and now on -> should redisplay all graph
		Chipset.Display16.g_xmax = Chipset.Display16.graph_width;
		Chipset.Display16.g_xmin = 0;
		Chipset.Display16.g_ymax = Chipset.Display16.graph_height;
		Chipset.Display16.g_ymin = 0;
	}
	if (!graph_on && Chipset.Display16.graph_on) {			// was on and now of -> should redisplay all alpha
		Chipset.Display16.a_xmax = Chipset.Display16.alpha_width;
		Chipset.Display16.a_xmin = 0;
		Chipset.Display16.a_ymax = Chipset.Display16.alpha_height;
		Chipset.Display16.a_ymin = 0;
	}
	if (graph_on ^ Chipset.Display16.graph_on) {
		#if defined DEBUG_DISPLAY16G
			k = wsprintf(buffer,_T("%06X: DISPLAY : graph on %02d -> %02X\n"), Chipset.Cpu.PC, Chipset.Display.graph_on, graph_on);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
	}
	Chipset.Display16.graph_on = graph_on;				// new graph state

	while (s) {											// do the write on mem and bitmap
		d &= 0x7FFF;
		Chipset.Display16.graph[d] = *(--a);				// write even for even byte for 9816... nobody cares
		#if defined DEBUG_DISPLAY16GH
			if (Chipset.Cpu.PC > 0x010000) {
				k = wsprintf(buffer,_T("%06X: GRAPH(%d) : Write %04X <- %02X\n"), Chipset.Cpu.PC, Chipset.Display.graph_on, d, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			}
		#endif
		if (Chipset.type == 16) {						// for 9816
			if ((d < Chipset.Display16.graph_visible) && (d & 0x0001)) {			// visible and odd byte only
				x0 = ((d>>1) % Chipset.Display16.graph_bytes)*8;
				y0 = ((d>>1) / Chipset.Display16.graph_bytes);
				ad = pbyGraph + (d>>1)*8;												// adress in graph bitmap (one byte per pixel)
				*ad++ = (*a & 0x80) ? 1 : 0;											// do the bits on/off	
				*ad++ = (*a & 0x40) ? 1 : 0;
				*ad++ = (*a & 0x20) ? 1 : 0;
				*ad++ = (*a & 0x10) ? 1 : 0;
				*ad++ = (*a & 0x08) ? 1 : 0;
				*ad++ = (*a & 0x04) ? 1 : 0;
				*ad++ = (*a & 0x02) ? 1 : 0;
				*ad = (*a & 0x01) ? 1 : 0;
				if (x0+8 > Chipset.Display16.g_xmax)									// adjust rectangle
					Chipset.Display16.g_xmax = x0+8;
				if (x0 < Chipset.Display16.g_xmin) 
					Chipset.Display16.g_xmin = x0;
				if (y0+1 > Chipset.Display16.g_ymax) 
					Chipset.Display16.g_ymax = y0+1;
				if (y0 < Chipset.Display16.g_ymin) 
					Chipset.Display16.g_ymin = y0;
			}
		} else {										// for 9836A
			if (d < Chipset.Display16.graph_visible) {								// visible and all bytes
				x0 = (d % Chipset.Display16.graph_bytes)*8;
				y0 = (d / Chipset.Display16.graph_bytes);
				ad = pbyGraph + d*8;
				*ad++ = (*a & 0x80) ? 1 : 0;
				*ad++ = (*a & 0x40) ? 1 : 0;
				*ad++ = (*a & 0x20) ? 1 : 0;
				*ad++ = (*a & 0x10) ? 1 : 0;
				*ad++ = (*a & 0x08) ? 1 : 0;
				*ad++ = (*a & 0x04) ? 1 : 0;
				*ad++ = (*a & 0x02) ? 1 : 0;
				*ad = (*a & 0x01) ? 1 : 0;
				if (x0+8 > Chipset.Display16.g_xmax) 
					Chipset.Display16.g_xmax = x0+8;
				if (x0 < Chipset.Display16.g_xmin) 
					Chipset.Display16.g_xmin = x0;
				if (y0+1 > Chipset.Display16.g_ymax) 
					Chipset.Display16.g_ymax = y0+1;
				if (y0 < Chipset.Display16.g_ymin) 
					Chipset.Display16.g_ymin = y0;
			}
		}
		d++;
		s--;
	} 
	return BUS_OK; 
}

//
// reload graph bitmap from graph mem
//
VOID Reload_Graph16(VOID)
{
	LPBYTE ad;
	BYTE s;
	WORD d;

	for (d = 0; d < Chipset.Display16.graph_visible; d++) {
		s = Chipset.Display16.graph[d];
		if (Chipset.type == 16) {
			if (d & 0x0001) {			// draw on bitmap
				ad = pbyGraph + (d>>1)*8;
				*ad++ = (s & 0x80) ? 1 : 0;
				*ad++ = (s & 0x40) ? 1 : 0;
				*ad++ = (s & 0x20) ? 1 : 0;
				*ad++ = (s & 0x10) ? 1 : 0;
				*ad++ = (s & 0x08) ? 1 : 0;
				*ad++ = (s & 0x04) ? 1 : 0;
				*ad++ = (s & 0x02) ? 1 : 0;
				*ad = (s & 0x01) ? 1 : 0;
			}
		} else {
			ad = pbyGraph + d*8;
			*ad++ = (s & 0x80) ? 1 : 0;
			*ad++ = (s & 0x40) ? 1 : 0;
			*ad++ = (s & 0x20) ? 1 : 0;
			*ad++ = (s & 0x10) ? 1 : 0;
			*ad++ = (s & 0x08) ? 1 : 0;
			*ad++ = (s & 0x04) ? 1 : 0;
			*ad++ = (s & 0x02) ? 1 : 0;
			*ad = (s & 0x01) ? 1 : 0;
		}
	}

	if (Chipset.Display16.graph_on) {
		Chipset.Display16.g_xmax = Chipset.Display16.graph_width;
		Chipset.Display16.g_xmin = 0;
		Chipset.Display16.g_ymax = Chipset.Display16.graph_height;
		Chipset.Display16.g_ymin = 0; 
	}
}

//
// update alpha display (bForce not used) when start change only
//
VOID UpdateMainDisplay16(BOOL bForce)
{
	WORD	i, n_lines;
	INT		x0;			
	INT		y0;
	WORD	dVid;
	WORD	d;
	DWORD	dwRop;
	WORD	dy;
	BYTE	s;

	#if defined DEBUG_DISPLAY16S
		wsprintf(buffer,_T("%06X: Update Main Display, %d\n"), Chipset.Cpu.PC, bForce);
		OutputDebugString(buffer);
	#endif

	d = Chipset.Display16.start;								// adress of begining of screen

	n_lines = 25;												// 25 lines to redraw
	dVid = ((d & 0x07FF) << 1) + 1;								// address (11 bits) in video ram (2 bytes per char)
	y0 = nLcdY;													
	x0 = nLcdX;													

	n_lines *= 80;												// number of characters to update
	for (i = 0; i < n_lines; i++) {	
		s = Chipset.Display16.alpha[dVid-1];						// effect byte
		dy = 0;
		switch (d >> 12) {											// get TEXT (bit13), KEYS (bit12)
			case 0:	dwRop = BLACKNESS;									// nothing displayed
				break;												
			case 1: dwRop = (d & 0x0800) ? SRCCOPY : BLACKNESS;			// keys on, text off
				break;
			case 2:	dwRop = (d & 0x0800) ? BLACKNESS : SRCCOPY;			// keys off, text on
				break;
			default: dwRop = SRCCOPY;									// keys on, text on
				break;
		}
		if (s & 0x04) {		// underline
			dy += 4*Chipset.Display16.alpha_char_h;
		}
		if (s & 0x08) {		// half bright
			dy += 2*Chipset.Display16.alpha_char_h;
		}
		if (s & 0x01) {		// inverse video
			dy += Chipset.Display16.alpha_char_h;
		}
		BitBlt(hAlpha1DC, x0, y0, Chipset.Display16.alpha_char_w, Chipset.Display16.alpha_char_h, hFontDC, Chipset.Display16.alpha[dVid] * Chipset.Display16.alpha_char_w, dy, dwRop);
		dVid += 2;
		dVid &= 0x0FFF;				// wrap at 4k
		x0 += Chipset.Display16.alpha_char_w;
		if (x0 == (nLcdX + 80*Chipset.Display16.alpha_char_w)) {
			y0 += Chipset.Display16.alpha_char_h;
			x0 = nLcdX;
		}
		d++;
		d &= 0x3FFF;				// wrap at 16k
	}
	// force a full copy at next vbl
	Chipset.Display16.a_xmin = 0;
	Chipset.Display16.a_xmax = Chipset.Display16.alpha_width;
	Chipset.Display16.a_ymin = 0;
	Chipset.Display16.a_ymax = Chipset.Display16.alpha_height;
	Chipset.Display16.g_xmin = Chipset.Display16.graph_width;
	Chipset.Display16.g_xmax = 0;
	Chipset.Display16.g_ymin = Chipset.Display16.graph_height;
	Chipset.Display16.g_ymax = 0; 
}

//
// only copy wat is needed from alpha1 and graph to screen
//
VOID Refresh_Display16(BOOL bForce)
{
	BYTE do_flush = 0;

	if (bForce) {			// adjust rectangles to do a full redraw
		Chipset.Display16.a_xmin = 0;
		Chipset.Display16.a_xmax = Chipset.Display16.alpha_width;
		Chipset.Display16.a_ymin = 0;
		Chipset.Display16.a_ymax = Chipset.Display16.alpha_height;
		Chipset.Display16.g_xmin = Chipset.Display16.graph_width;
		Chipset.Display16.g_xmax = 0;
		Chipset.Display16.g_ymin = Chipset.Display16.graph_height;
		Chipset.Display16.g_ymax = 0;
	}
	if (Chipset.Display16.a_xmax > Chipset.Display16.a_xmin) {									// something to draw in alpha part
		BitBlt(hScreenDC, Chipset.Display16.a_xmin, Chipset.Display16.a_ymin, 
			              Chipset.Display16.a_xmax-Chipset.Display16.a_xmin, Chipset.Display16.a_ymax-Chipset.Display16.a_ymin, 
			   hAlpha1DC, Chipset.Display16.a_xmin, Chipset.Display16.a_ymin, 
			   SRCCOPY);
		if (Chipset.Display16.graph_on) {							// add graph part if needed
			StretchBlt(hScreenDC, Chipset.Display16.a_xmin, Chipset.Display16.a_ymin, 
				                 Chipset.Display16.a_xmax-Chipset.Display16.a_xmin, Chipset.Display16.a_ymax-Chipset.Display16.a_ymin, 
					  hGraphDC,  Chipset.Display16.a_xmin/2, Chipset.Display16.a_ymin, 
								 (Chipset.Display16.a_xmax-Chipset.Display16.a_xmin)/2, Chipset.Display16.a_ymax-Chipset.Display16.a_ymin,  
					  SRCPAINT);
		}
		StretchBlt(hWindowDC, Chipset.Display16.a_xmin, Chipset.Display16.a_ymin*2, 
			                  Chipset.Display16.a_xmax-Chipset.Display16.a_xmin, (Chipset.Display16.a_ymax-Chipset.Display16.a_ymin)*2, 
				   hScreenDC, Chipset.Display16.a_xmin, Chipset.Display16.a_ymin, 
					          Chipset.Display16.a_xmax-Chipset.Display16.a_xmin, Chipset.Display16.a_ymax-Chipset.Display16.a_ymin, 
				   SRCCOPY);
		do_flush = 1;
		Chipset.Display16.a_xmin = Chipset.Display16.alpha_width;		// nothing more to re draw at next vbl
		Chipset.Display16.a_xmax = 0;
		Chipset.Display16.a_ymin = Chipset.Display16.alpha_height;
		Chipset.Display16.a_ymax = 0;
	}
	if ((Chipset.Display16.g_xmax > Chipset.Display16.g_xmin) && Chipset.Display16.graph_on) {	// something to draw in graph part
		BitBlt(hScreenDC, Chipset.Display16.g_xmin*2, Chipset.Display16.g_ymin, 
					      (Chipset.Display16.g_xmax-Chipset.Display16.g_xmin)*2, Chipset.Display16.g_ymax-Chipset.Display16.g_ymin, 
			   hAlpha1DC, Chipset.Display16.g_xmin*2, Chipset.Display16.g_ymin, 
			   SRCCOPY);
		StretchBlt(hScreenDC, Chipset.Display16.g_xmin*2, Chipset.Display16.g_ymin, 
			                  (Chipset.Display16.g_xmax-Chipset.Display16.g_xmin)*2, Chipset.Display16.g_ymax-Chipset.Display16.g_ymin, 
	               hGraphDC,  Chipset.Display16.g_xmin, Chipset.Display16.g_ymin, 
				              Chipset.Display16.g_xmax-Chipset.Display16.g_xmin, Chipset.Display16.g_ymax-Chipset.Display16.g_ymin, 
				   SRCPAINT);
		StretchBlt(hWindowDC, Chipset.Display16.g_xmin*2, Chipset.Display16.g_ymin*2, 
			                  (Chipset.Display16.g_xmax-Chipset.Display16.g_xmin)*2, (Chipset.Display16.g_ymax-Chipset.Display16.g_ymin)*2, 
		           hScreenDC, Chipset.Display16.g_xmin*2, Chipset.Display16.g_ymin, 
				              (Chipset.Display16.g_xmax-Chipset.Display16.g_xmin) * 2, Chipset.Display16.g_ymax-Chipset.Display16.g_ymin, 
				   SRCCOPY);

		do_flush = 1;
		Chipset.Display16.g_xmin = Chipset.Display16.graph_width;		// nothing more to re draw at next vbl
		Chipset.Display16.g_xmax = 0;
		Chipset.Display16.g_ymin = Chipset.Display16.graph_height;
		Chipset.Display16.g_ymax = 0;
	}
	if (do_flush) 
		GdiFlush();
}

//
// do VBL
//
VOID do_display_timers16(VOID)							
{
	Chipset.Display16.cycles += Chipset.dcycles;
	if (Chipset.Display16.cycles >= dwVsyncRef) {		// one vsync
		Chipset.Display16.cycles -= dwVsyncRef;
		Chipset.Display16.cnt ++;
		Chipset.Display16.cnt &= 0x3F;
		Refresh_Display16(FALSE);						// just rects
		UpdateAnnunciators(FALSE);						// changed annunciators only
		Cursor_Remove16();								// do the blink 
		Cursor_Display16();
	}
}
