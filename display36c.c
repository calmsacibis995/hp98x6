/*
 *   Display36c.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
// Display driver. Update is done in real-time at each acces in video-memory.
// CRT refresh frequency is emulated with a a timer based on 68000 cycle
// counter on real-speed and with High performance counter otherwise at 1/60 sec.
//
// HP 9836C
// 
// see Display36.c for most comments
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "resource.h"
#include "kml.h"
#include "mops.h"

//#define DEBUG_DISPLAY36						// switch for DISPLAY debug purpose
//#define DEBUG_DISPLAY36D					// switch for alpha DISPLAY debug purpose
//#define DEBUG_DISPLAY36G					// switch for graph DISPLAY debug purpose
//#define DEBUG_DISPLAY36GH					// switch for graph DISPLAY debug purpose
//#define DEBUG_DISPLAY36S					// switch for reg DISPLAY debug purpose
//#define DEBUG_DISPLAY36H					// switch for DISPLAY debug purpose
#if defined(DEBUG_DISPLAY36H) || defined(DEBUG_DISPLAY36) || defined(DEBUG_DISPLAY36) || defined(DEBUG_DISPLAY36G) || defined(DEBUG_DISPLAY36A) || defined(DEBUG_DISPLAY36R) 
	TCHAR buffer[256];
	int k;
//	unsigned long l;
#endif

#define ALPHASIZE 4096

// offsets for colors in font bitmap		   Blue Green Red and green is inverted ..... (see schematics)
static WORD colors[8] = {  2 * 60,			//  0     0    0 -> 010 = 2
 						   3 * 60,			//  0     0    1 -> 011 = 3
						   0 * 60,			//  0     1    0 -> 000 = 0
						   1 * 60,			//  0     1    1 -> 001 = 1
						   6 * 60,			//  1     0    0 -> 110 = 6
						   7 * 60,			//  1     0    1 -> 111 = 7
						   4 * 60,			//  1     1    0 -> 100 = 4
						   5 * 60			//  1     1    1 -> 101 = 5
};
// map 4 bits per components clut(hp) to 8 bits per components clut (pc)
static BYTE ncolors[16] = {255, 238, 221, 204, 187, 170, 153, 136, 119, 102, 85, 68, 51, 34, 17, 0};

//
// Display for writing byte a at video memory address d for alpha part dual ported ram address with s effect
//
static VOID WriteToMainDisplay36c(BYTE a, WORD d, BYTE s)
{
	INT  x0, xmin, xmax;
	INT  y0, ymin, ymax;
	DWORD	dwRop;
	WORD	dy;
	WORD	delta;						// distance from start in byte
	
	d &= 0x07FF;

	if (d >= (Chipset.Display36c.start & 0x07FF)) {
		delta = d - (Chipset.Display36c.start & 0x07FF);
	} else {
		delta = d + (0x0800 - (Chipset.Display36c.start & 0x07FF));
	}

	#if defined DEBUG_DISPLAY36D
//	if ((delta > 1920) && (delta < 2000)) {
	{
		byte c = ' ';
		if ((a>31) && (a < 127)) c = a;
		wsprintf(buffer,_T("%06X: Write Main Display %04X(%d) : %02X(%c) (%02X)\n"),Chipset.Cpu.PC, d, delta, a, c, s);
		OutputDebugString(buffer);
	}
	#endif

	d = Chipset.Display36c.start + delta;
	d &= 0x3FFF;
	if (delta < 2000) {										// visible 
		y0 = (delta / 80) * Chipset.Display36c.alpha_char_h + nLcdY;												// where on Y
		x0 = (delta % 80) * Chipset.Display36c.alpha_char_w + nLcdX;												// where on X
		xmin = xmax = x0;											// min box
		ymin = ymax = y0;
		dy = colors[(s >> 4) & 0x07];
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
			dy += 2*Chipset.Display36c.alpha_char_h;
		}
		if (s & 0x01) {		// inverse video
			dy += Chipset.Display36c.alpha_char_h;
		}

		BitBlt(hAlpha1DC, x0, y0, Chipset.Display36c.alpha_char_w, Chipset.Display36c.alpha_char_h, hFontDC, (int)(a) * Chipset.Display36c.alpha_char_w, dy, dwRop);
		xmax += Chipset.Display36c.alpha_char_w;												// adjust max box
		ymax += Chipset.Display36c.alpha_char_h;

		if (xmax > Chipset.Display36c.a_xmax) Chipset.Display36c.a_xmax = xmax;
		if (xmin < Chipset.Display36c.a_xmin) Chipset.Display36c.a_xmin = xmin;
		if (ymax > Chipset.Display36c.a_ymax) Chipset.Display36c.a_ymax = ymax;
		if (ymin < Chipset.Display36c.a_ymin) Chipset.Display36c.a_ymin = ymin;
	} else {
		#if defined DEBUG_DISPLAY36A
			wsprintf(buffer,_T("%06X: Write Main Display %02X - %04X : %04X : %04X\n"), Chipset.Cpu.PC, a, Chipset.Display.start, d, Chipset.Display.end);
			OutputDebugString(buffer);
		#endif
	} 
}

static VOID Cursor_Remove36c(VOID)
{
	WORD d;
	INT x0, y0;

	if (Chipset.Display36c.cursor_last) {								// was on
		d = Chipset.Display36c.cursor;
		if (d < Chipset.Display36c.start) d += 0x0800;
		if ((d >= Chipset.Display36c.start) && (d < Chipset.Display36c.end)) {	// visible 
			y0 = ((d - Chipset.Display36c.start) / 80) * Chipset.Display36c.alpha_char_h + nLcdY;											// where on Y
			x0 = ((d - Chipset.Display36c.start) % 80) * Chipset.Display36c.alpha_char_w + nLcdX;											// where on X
//			EnterCriticalSection(&csGDILock);						// blit the min-max box on window display
//			{
				StretchBlt(hWindowDC, x0, y0*2, Chipset.Display36c.alpha_char_w, Chipset.Display36c.alpha_char_h*2, hAlpha1DC, x0, y0, Chipset.Display36c.alpha_char_w, Chipset.Display36c.alpha_char_h, SRCCOPY);
				if (Chipset.Display36c.graph_on) {
					StretchBlt(hWindowDC, x0, y0*2, Chipset.Display36c.alpha_char_w, Chipset.Display36c.alpha_char_h*2, hGraphDC, x0/2, y0, Chipset.Display36c.alpha_char_w/2, Chipset.Display36c.alpha_char_h, SRCPAINT);
				}
				GdiFlush();
//			}
//			LeaveCriticalSection(&csGDILock);
		}
	} 
}

static VOID Cursor_Display36c(VOID)
{
	WORD	d;
	INT		x0, y0;
	WORD	dy;
	DWORD	dwRop;

	Chipset.Display36c.cursor_last = 0;									// actually not displayed
	Chipset.Display36c.cursor = Chipset.Display36c.cursor_new;	
	if (Chipset.Display36c.cursor_blink & Chipset.Display36c.cnt)				// should be off, return
		return;
	if (Chipset.Display36c.cursor_b < Chipset.Display36c.cursor_t)			// no size !!
		return;

	d = Chipset.Display36c.cursor;
	if (d < Chipset.Display36c.start) d += 0x0800;
	dy = colors[(Chipset.Display36c.alpha[(Chipset.Display36c.cursor & 0x07FF) << 1] >> 4) & 0x07];
	if ((d >= Chipset.Display36c.start) && (d < Chipset.Display36c.end)) {	// visible 
		y0 = ((d - Chipset.Display36c.start) / 80) * Chipset.Display36c.alpha_char_h + nLcdY + Chipset.Display36c.cursor_t;			// where on Y
		x0 = ((d - Chipset.Display36c.start) % 80) * Chipset.Display36c.alpha_char_w + nLcdX;										// where on X
		switch (d >> 12) {
			case 0:	dwRop = BLACKNESS;
				break;
			case 1: dwRop = (d & 0x0800) ? SRCCOPY : BLACKNESS;
				break;
			case 2:	dwRop = (d & 0x0800) ? BLACKNESS : SRCCOPY;
					dy += (d & 0x0800) ? 0 : Chipset.Display36c.alpha_char_h;
				break;
			default: dwRop = SRCCOPY;
					dy += (d & 0x0800) ? Chipset.Display36c.alpha_char_h : 0;
				break;
		}
		StretchBlt(hWindowDC, x0, y0*2, Chipset.Display36c.alpha_char_w, Chipset.Display36c.cursor_h*2, 
			       hFontDC, 256*Chipset.Display36c.alpha_char_w, dy, Chipset.Display36c.alpha_char_w, Chipset.Display36c.cursor_h, dwRop);
		if (Chipset.Display36c.graph_on) {
			StretchBlt(hWindowDC, x0, y0*2, Chipset.Display36c.alpha_char_w, Chipset.Display36c.cursor_h*2, 
				       hGraphDC, x0/2, y0, Chipset.Display36c.alpha_char_w/2, Chipset.Display36c.cursor_h, SRCPAINT);
		}
		GdiFlush();
		Chipset.Display36c.cursor_last = 1;											// cursor is shown		
	} 
}

//################
//#
//#    Public functions
//#
//################

//
// write in IO display space
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

BYTE Write_Display36c(BYTE *a, WORD d, BYTE s)
{
	WORD start_new;
	BYTE graph_on;
	WORD dd = d & 0x7FFF;								// for basic and pascal ...
	WORD clut = ((dd & 0x7C00) == 0x7800) ? 1 : 0;		// clut access

	if ((d == 0xFFFF) || (d == 0xFFFE))	{						// write on crtID
		#if defined DEBUG_DISPLAY36
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write %04X(%d) ... %02X\n"), Chipset.Cpu.PC, d, s, *(a-1));
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		return BUS_OK;	
	}
	if ((d == 0xFFFC) || (d == 0xFFFD)) {											// graph on or off
		if (s == 2) a--;
		graph_on = ((*(--a)) & 0x01) ? 1 : 0;
		if (graph_on && !Chipset.Display36c.graph_on) {			// should redisplay all graph
			Chipset.Display36c.g_xmax = Chipset.Display36c.graph_width;
			Chipset.Display36c.g_xmin = 0;
			Chipset.Display36c.g_ymax = Chipset.Display36c.graph_height;
			Chipset.Display36c.g_ymin = 0;
		}
		if (!graph_on && Chipset.Display36c.graph_on) {			// should redisplay all alpha
			Chipset.Display36c.a_xmax = Chipset.Display36c.alpha_width;
			Chipset.Display36c.a_xmin = 0;
			Chipset.Display36c.a_ymax = Chipset.Display36c.alpha_height;
			Chipset.Display36c.a_ymin = 0;
		}
		#if defined DEBUG_DISPLAY36
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write %04X(%d) graph <- %02X\n"), Chipset.Cpu.PC, d, s, *(a-1));
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		if (graph_on ^ Chipset.Display36c.graph_on) {
			#if defined DEBUG_DISPLAY36
				k = wsprintf(buffer,_T("%06X: DISPLAY36C : graph on %02d -> %02X\n"), Chipset.Cpu.PC, Chipset.Display36c.graph_on, graph_on);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
		}
		Chipset.Display36c.graph_on = graph_on;
		return BUS_OK;
	}
	if ((dd >= 0x2000) && (dd <= 0x2000+(ALPHASIZE<<1)-s)) {
		dd -= 0x2000;															// base at 0
		while (dd >= (WORD) (ALPHASIZE)) dd -= (WORD) (ALPHASIZE);				// wrap address
		while (s) {
			Chipset.Display36c.alpha[dd] = *(--a); 
			#if defined DEBUG_DISPLAY36HA
				k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write %04X(%d) <- %02X\n"), Chipset.Cpu.PC, d, s, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			s--;
			if ((dd & 0x0001) == 0x0001) 
				WriteToMainDisplay36c(Chipset.Display36c.alpha[dd], dd >> 1, Chipset.Display36c.alpha[dd-1]);
			else {
				Chipset.Display36c.alpha[dd] &= 0x7F;	// remove last bit for highlights
				WriteToMainDisplay36c(Chipset.Display36c.alpha[dd+1], dd >> 1, Chipset.Display36c.alpha[dd]);
			}
			dd++;
		}
		return BUS_OK;
	}
	if ((dd == 0x0000) || (dd == 0x0001)) {							// 6845 reg
		if (s == 2) *(--a) = 0x00;
		Chipset.Display36c.reg = *(--a);
		#if defined DEBUG_DISPLAY36R
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write(%d) reg <- %02d\n"), Chipset.Cpu.PC, s, Chipset.Display36c.reg);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		return BUS_OK;		
	}  
	if ((dd == 0x0002) || (dd == 0x0003)) {							// 6845 reg write
		if (s == 2) *(--a) = 0x00;
		Chipset.Display36c.regs[Chipset.Display36c.reg] = *(--a);
		#if defined DEBUG_DISPLAY36R
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write(%d) reg %02d <- %02X\n"), Chipset.Cpu.PC, s, Chipset.Display36c.reg, *a);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		switch (Chipset.Display36c.reg) {
			case 10:
				Chipset.Display36c.cursor_t = (*a) & 0x1F;
				switch (((*a) & 0x60) >> 5) {
					case 0:												
						Chipset.Display36c.cursor_blink = 0x00;			//always on
						break;
					case 1:
						Chipset.Display36c.cursor_blink = 0xFF;			// off
						break;
					case 2:
						Chipset.Display36c.cursor_blink = 0x20;			// blink 1/16
						break;
					default:
						Chipset.Display36c.cursor_blink = 0x10;			// blink 1/32
						break;
				}
				if (Chipset.Display36c.cursor_b >= Chipset.Display36c.cursor_t) 
					Chipset.Display36c.cursor_h = Chipset.Display36c.cursor_b - Chipset.Display36c.cursor_t + 1;
				break;
			case 11:
				Chipset.Display36c.cursor_b = (*a) & 0x3F;
				if (Chipset.Display36c.cursor_b >= Chipset.Display36c.cursor_t) 
					Chipset.Display36c.cursor_h = Chipset.Display36c.cursor_b - Chipset.Display36c.cursor_t + 1;
				break;
			case 12:
			case 13:
				start_new = ((Chipset.Display36c.regs[12] << 8) + Chipset.Display36c.regs[13]) & 0x3FFF;
				if (start_new != Chipset.Display36c.start) {
					Chipset.Display36c.start = start_new;
					Chipset.Display36c.end = (start_new + 80*25) & 0x3FFF;
					UpdateMainDisplay36c(FALSE);		// change of start
				}
				break;
			case 14:
			case 15:
				Chipset.Display36c.cursor_new = ((Chipset.Display36c.regs[14] << 8) + Chipset.Display36c.regs[15]) & 0x3FFF;
				break;
			default:
				break;
		}
		return BUS_OK;
	} 
	if (clut) {	// color map
		if (s == 1) _ASSERT(0);
		else {
			WORD rgb;
			BYTE col = (d & 0x001F) >> 1;
			
			while (s) {
				rgb = ((*(--a)) & 0x0F) << 8;
				rgb |= *(--a);
				if (Chipset.Display36c.cmap[col] != rgb) {						// color changed ?
					if (Chipset.Display36c.graph_on) {								// graph displayed ?
						Chipset.Display36c.g_xmax = Chipset.Display36c.graph_width;		// redraw all graph
						Chipset.Display36c.g_xmin = 0;
						Chipset.Display36c.g_ymax = Chipset.Display36c.graph_height;
						Chipset.Display36c.g_ymin = 0; 
					}
					Chipset.Display36c.cmap[col] = rgb;
					SetLcdColor((col + 1) * 8, ncolors[(rgb & 0x0F00) >> 8], ncolors[(rgb & 0x00F0) >> 4], ncolors[rgb & 0x000F]);
					SetGraphColor((col + 1) * 8);		
					#if defined DEBUG_DISPLAY36
						k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write CLUT %04X(%d) <- %04X\n"), Chipset.Cpu.PC, d, s, Chipset.Display36c.cmap[col]);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
				}
				col++;
				d += 2;
				s -= 2;
			}
			return BUS_OK;
		}
	}
	#if defined DEBUG_DISPLAY36
		k = wsprintf(buffer,_T("%06X: DISPLAY36C : Write outside %04X(%d) <- %02X\n"), Chipset.Cpu.PC, d, s, *(a-1));
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif
	_ASSERT(0);
	return BUS_OK;
}

//
// read in IO display space
// use a CRT configuration identification register
//
BYTE Read_Display36c(BYTE *a, WORD d, BYTE s)
{
	WORD dd = d & 0x07FFF;	// for basic 5 and pascal
	WORD clut = ((dd & 0x7C00) == 0x7800) ? 1 : 0;

	if ((d == 0xFFFC) || (d == 0xFFFD)) {		// vbl ?
		if (s == 2) *(--a) = 0x00;
		if (Chipset.Display36c.cycles < (dwVsyncRef >> 8))
			*(--a) = 0x00;
		else 
			*(--a) = 0x01;
		#if defined DEBUG_DISPLAY36V
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Read VBL %04X(%d) = %02X\n"), Chipset.Cpu.PC, d, s, *a);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		return BUS_OK;
	}

	if ((d == 0xFFFE) || (d == 0xFFFF)) {		// CRT configuration identification register pas3: p.337
		if (s == 2) {
			*(--a) = 0x0A;							//  no self init CRT, mapping char rom, 4 memory planes at 520000 , highlight 512*390
			*(--a) = 0x30;							// 	236C monitor, 60 Hz 
		} else {
			if (d == 0xFFFE) 
				*(--a) = 0x0A;
			else 
				*(--a) = 0x30;										// 	236C monitor, 60 Hz 
		}
		#if defined DEBUG_DISPLAY36
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Read CRTID %04X(%d) = %02X\n"), Chipset.Cpu.PC, d, s, *a);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		return BUS_OK;
	}
	if ((dd >= 0x2000) && (dd <= 0x2000 + (ALPHASIZE<<1)-s)) {	// DUAL port alpha video ram
		dd -= 0x2000;
		while (dd >= (WORD) (ALPHASIZE)) dd -= (WORD) (ALPHASIZE);
		while (s) {
			*(--a) = Chipset.Display36c.alpha[dd++]; 
			#if defined DEBUG_DISPLAY36A
				k = wsprintf(buffer,_T("%06X: DISPLAY36C : Read alpha %04X(%d) = %02X\n"), Chipset.Cpu.PC, dd, s, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			s--;
		}
		return BUS_OK;
	}
	if (clut) {						// color maps
		#if defined DEBUG_DISPLAY36
			k = wsprintf(buffer,_T("%06X: DISPLAY36C : Read CLUT %04X(s) = %04X\n"), Chipset.Cpu.PC, d, s, Chipset.Display36c.cmap[(d & 0x001F) >> 1]);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		if (s == 2) {
			*(--a) = (BYTE) (Chipset.Display36c.cmap[(d & 0x001F) >> 1] >> 8);
			*(--a) = (BYTE) (Chipset.Display36c.cmap[(d & 0x001F) >> 1] & 0x00FF);
		} else
			_ASSERT(0);
		return BUS_OK;
	} 
	if ((dd == 0x0002) || (dd == 0x0003)) {							// 6845 regs
		if (s == 2) *(--a) = 0x00;
		*(--a) = Chipset.Display36c.regs[Chipset.Display36c.reg];
		#if defined DEBUG_DISPLAY36R
			k = wsprintf(buffer,_T("%06X: DISPLAY : Read(%d) reg %02d = %02X\n"), Chipset.Cpu.PC, Chipset.Display36c.reg, Chipset.Display36c.regs[Chipset.Display36c.reg]);
			OutputDebugString(buffer); buffer[0] = 0x00;
		#endif
		return BUS_OK;
	} 
	#if defined DEBUG_DISPLAY36
		k = wsprintf(buffer,_T("%06X: DISPLAY36C : Read outside %04X(%d) ...\n"), Chipset.Cpu.PC, d, s);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif
	return BUS_OK;
}

//
// read in graph mem
//
BYTE Read_Graph36c(BYTE *a, DWORD d, BYTE s)
{
	while (s) {
		*(--a) = Chipset.Display36c.graph[d]; 
		#if defined DEBUG_DISPLAY36GH
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

//
// write in graph mem
//
BYTE Write_Graph36c(BYTE *a, DWORD d, BYTE s)
{
	WORD x0, y0;

	while (s) {
		Chipset.Display36c.graph[d] = (*(--a)) & 0x0F;					// in graph mem
		if (d < 199680) {												// visible ?
			pbyGraph[d] = (Chipset.Display36c.graph[d] + 1) * 8;			// in graph bitmap 
			if (Chipset.Display36c.graph_on) {							
				x0 = (WORD) (d % 512);
				y0 = (WORD) (d / 512);
				if (x0+1 > Chipset.Display36c.g_xmax) 
					Chipset.Display36c.g_xmax = x0+1;
				if (x0 < Chipset.Display36c.g_xmin) 
					Chipset.Display36c.g_xmin = x0;
				if (y0+1 > Chipset.Display36c.g_ymax) 
					Chipset.Display36c.g_ymax = y0+1;
				if (y0 < Chipset.Display36c.g_ymin) 
					Chipset.Display36c.g_ymin = y0;
			}
		}
		#if defined DEBUG_DISPLAY36G
			if ((Chipset.Cpu.PC > 0x010000) && bDebugOn) {
				k = wsprintf(buffer,_T("%06X: GRAPH36C(%d) : Write %06X <- %02X\n"), Chipset.Cpu.PC, Chipset.Display36c.graph_on, d, *a);
				OutputDebugString(buffer); buffer[0] = 0x00;
			}
		#endif
		d++;
		s--;
	} 
	return BUS_OK; 
}

//
// reload graph bitmap from graph mem and clut too
//
VOID Reload_Graph36c(VOID)
{
	DWORD ad;
	BYTE s;
	WORD d;

	for (ad = 0; ad < 199680; ad++) {
		pbyGraph[ad] = (Chipset.Display36c.graph[ad] + 1) * 8;
	}

	if (Chipset.Display36c.graph_on) {
		Chipset.Display36c.g_xmax = Chipset.Display36c.graph_width;
		Chipset.Display36c.g_xmin = 0;
		Chipset.Display36c.g_ymax = Chipset.Display36c.graph_height;
		Chipset.Display36c.g_ymin = 0; 
	}

	for (s = 0; s < 16; s++) {
		d = Chipset.Display36c.cmap[s];
		SetLcdColor((s + 1) * 8, ncolors[(d & 0x0F00) >> 8], ncolors[(d & 0x00F0) >> 4], ncolors[d & 0x000F]);
	}
	UpdateClut();		
}

//
// update alpha display (when start change only)
//
VOID UpdateMainDisplay36c(BOOL bForce)
{
	WORD	i, n_lines;
	INT		x0;			
	INT		y0;
	WORD	dVid;
	WORD	d;
	DWORD	dwRop;
	WORD	dy;
	BYTE	s;

	#if defined DEBUG_DISPLAY36S
		wsprintf(buffer,_T("%06X: Update Main Display, %d\n"), Chipset.Cpu.PC, bForce);
		OutputDebugString(buffer);
	#endif

	d = Chipset.Display36c.start;									// adress of begening of screen

	n_lines = 25;												// 25 to redraw
	dVid = ((d & 0x07FF) << 1) + 1;									// address (11 bits) in video ram (2 bytes per char)
	y0 = nLcdY;													
	x0 = nLcdX;														// character of start of update (always 0)

	n_lines *= 80;												// number of characters to update
	for (i = 0; i < n_lines; i++)
	{	
		s = Chipset.Display36c.alpha[dVid-1];
		dy = colors[(s >> 4) & 0x07];
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
			dy += 2*Chipset.Display36c.alpha_char_h;
		}
		if (s & 0x01) {		// inverse video
			dy += Chipset.Display36c.alpha_char_h;
		}
		BitBlt(hAlpha1DC, x0, y0, Chipset.Display36c.alpha_char_w, Chipset.Display36c.alpha_char_h, hFontDC, Chipset.Display36c.alpha[dVid] * Chipset.Display36c.alpha_char_w, dy, dwRop);
		dVid += 2;
		dVid &= 0x0FFF;				// wrap at 4k
		x0 += Chipset.Display36c.alpha_char_w;
		if (x0 == (nLcdX + 80*Chipset.Display36c.alpha_char_w)) {
			y0 += Chipset.Display36c.alpha_char_h;
			x0 = nLcdX;
		}
		d++;
		d &= 0x3FFF;				// wrap at 16k
	}

	Chipset.Display36c.a_xmin = 0;
	Chipset.Display36c.a_xmax = Chipset.Display36c.alpha_width;
	Chipset.Display36c.a_ymin = 0;
	Chipset.Display36c.a_ymax = Chipset.Display36c.alpha_height;
}


VOID Refresh_Display36c(BOOL bForce)
{
	BYTE do_flush = 0;

	if (bForce) {
		Chipset.Display36c.a_xmin = 0;
		Chipset.Display36c.a_xmax = Chipset.Display36c.alpha_width;
		Chipset.Display36c.a_ymin = 0;
		Chipset.Display36c.a_ymax = Chipset.Display36c.alpha_height;
		Chipset.Display36c.g_xmin = Chipset.Display36c.graph_width;
		Chipset.Display36c.g_xmax = 0;
		Chipset.Display36c.g_ymin = Chipset.Display36c.graph_height;
		Chipset.Display36c.g_ymax = 0;
	}
	if ((Chipset.Display36c.g_xmax > Chipset.Display36c.g_xmin) && Chipset.Display36c.graph_on) {	// something to draw in graph part
		BitBlt(hScreenDC, Chipset.Display36c.g_xmin*2, Chipset.Display36c.g_ymin, 
					      (Chipset.Display36c.g_xmax-Chipset.Display36c.g_xmin)*2, Chipset.Display36c.g_ymax-Chipset.Display36c.g_ymin, 
			   hAlpha1DC, Chipset.Display36c.g_xmin*2, Chipset.Display36c.g_ymin, 
			   SRCCOPY);
		StretchBlt(hScreenDC, Chipset.Display36c.g_xmin*2, Chipset.Display36c.g_ymin, 
			                  (Chipset.Display36c.g_xmax-Chipset.Display36c.g_xmin)*2, Chipset.Display36c.g_ymax-Chipset.Display36c.g_ymin, 
	               hGraphDC,  Chipset.Display36c.g_xmin, Chipset.Display36c.g_ymin, 
				              Chipset.Display36c.g_xmax-Chipset.Display36c.g_xmin, Chipset.Display36c.g_ymax-Chipset.Display36c.g_ymin, 
				   SRCPAINT);
		StretchBlt(hWindowDC, Chipset.Display36c.g_xmin*2, Chipset.Display36c.g_ymin*2, 
			                  (Chipset.Display36c.g_xmax-Chipset.Display36c.g_xmin)*2, (Chipset.Display36c.g_ymax-Chipset.Display36c.g_ymin)*2, 
		           hScreenDC, Chipset.Display36c.g_xmin*2, Chipset.Display36c.g_ymin, 
				              (Chipset.Display36c.g_xmax-Chipset.Display36c.g_xmin) * 2, Chipset.Display36c.g_ymax-Chipset.Display36c.g_ymin, 
				   SRCCOPY);
		do_flush = 1;
		Chipset.Display36c.g_xmin = Chipset.Display36c.graph_width;
		Chipset.Display36c.g_xmax = 0;
		Chipset.Display36c.g_ymin = Chipset.Display36c.graph_height;
		Chipset.Display36c.g_ymax = 0;
	}
	if (Chipset.Display36c.a_xmax > Chipset.Display36c.a_xmin) {									// something to draw in alpha part
		BitBlt(hScreenDC, Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymin, 
			              Chipset.Display36c.a_xmax-Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymax-Chipset.Display36c.a_ymin, 
			   hAlpha1DC, Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymin, 
			   SRCCOPY);
		if (Chipset.Display36c.graph_on) {							// add graph part if needed
			StretchBlt(hScreenDC, Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymin, 
				                 Chipset.Display36c.a_xmax-Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymax-Chipset.Display36c.a_ymin, 
					  hGraphDC,  Chipset.Display36c.a_xmin/2, Chipset.Display36c.a_ymin, 
								 (Chipset.Display36c.a_xmax-Chipset.Display36c.a_xmin)/2, Chipset.Display36c.a_ymax-Chipset.Display36c.a_ymin,  
					  SRCPAINT);
		}
		StretchBlt(hWindowDC, Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymin*2, 
			                  Chipset.Display36c.a_xmax-Chipset.Display36c.a_xmin, (Chipset.Display36c.a_ymax-Chipset.Display36c.a_ymin)*2, 
				   hScreenDC, Chipset.Display36c.a_xmin, Chipset.Display36c.a_ymin, Chipset.Display36c.a_xmax-Chipset.Display36c.a_xmin, 
				              Chipset.Display36c.a_ymax-Chipset.Display36c.a_ymin, 
				   SRCCOPY);
		do_flush = 1;
		Chipset.Display36c.a_xmin = Chipset.Display36c.alpha_width;
		Chipset.Display36c.a_xmax = 0;
		Chipset.Display36c.a_ymin = Chipset.Display36c.alpha_height;
		Chipset.Display36c.a_ymax = 0;
	}
	if (do_flush)
		GdiFlush();
}

//
// do VBL
//
VOID do_display_timers36c(VOID)							
{
	Chipset.Display36c.cycles += Chipset.dcycles;
	if (Chipset.Display36c.cycles >= dwVsyncRef) {		// one vsync
		Chipset.Display36c.cycles -= dwVsyncRef;
		Chipset.Display36c.cnt ++;
		Chipset.Display36c.cnt &= 0x3F;
		Refresh_Display36c(FALSE);						// just rects
		Chipset.Display36c.whole = 0;
		UpdateAnnunciators(FALSE);
		Cursor_Remove36c();
		Cursor_Display36c();
	}
}
