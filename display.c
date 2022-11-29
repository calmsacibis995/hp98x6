/*
 *   Display.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
// Common part of display drivers, create bitmaps and all
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "resource.h"
#include "kml.h"
#include "mops.h"

//#define DEBUG_DISPLAY						// switch for DISPLAY debug purpose
//#define DEBUG_DISPLAYA					// switch for alpha DISPLAY debug purpose
//#define DEBUG_DISPLAYG					// switch for graph DISPLAY debug purpose
//#define DEBUG_DISPLAYGH					// switch for graph DISPLAY debug purpose
//#define DEBUG_DISPLAYS					// switch for reg DISPLAY debug purpose
//#define DEBUG_DISPLAYH					// switch for DISPLAY debug purpose
#if defined(DEBUG_DISPLAYH) || defined(DEBUG_DISPLAY) || defined(DEBUG_DISPLAY) || defined(DEBUG_DISPLAYG) || defined(DEBUG_DISPLAYA) || defined(DEBUG_DISPLAYR) 
	TCHAR buffer[256];
	int k;
//	unsigned long l;
#endif

HDC		hFontDC = NULL;				// handle of alpha font
HDC     hMainDC = NULL;				// handle of main bitmap for border display
HDC		hAlpha1DC = NULL;			// handle of display alpha 1
HDC     hAlpha2DC = NULL;			// handle of display alpha 2 for blink
HDC		hGraphDC = NULL;			// handle of display graph
HDC		hScreenDC = NULL;			// handle for screen in 8 bit

HBITMAP	hFontBitmap;
HBITMAP	hAlpha1Bitmap;
HBITMAP	hAlpha2Bitmap;
HBITMAP	hGraphBitmap;
HBITMAP hMainBitmap;
HBITMAP hScreenBitmap;

LPBYTE	pbyAlpha1;
LPBYTE	pbyAlpha2;
LPBYTE	pbyGraph;
LPBYTE  pbyScreen;

#define NOCOLORS	256						// work on 8 bits bitmap (easier to do the priority effect on alpha and graph planes)

#define B 0x00000000						// black
#define W 0x00FFFFFF						// white
#define I 0xFFFFFFFF						// ignore
											//					  9816			  9826			  9837
static DWORD dwKMLColor[256] =				// color table loaded by KML script (not really usefull)
{
	W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,
	B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,
	B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,
	B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,
	B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I
};

typedef struct
{
	BITMAPINFOHEADER Lcd_bmih;
	RGBQUAD bmiColors[NOCOLORS]; 
} BMI;

static BMI bmiAlpha1 = {{0x28,800,-300,1,8,BI_RGB,0,0,0,NOCOLORS,0}};			// bitmap for alpha1 (normal alpha)
static BMI bmiAlpha2 = {{0x28,800,-300,1,8,BI_RGB,0,0,0,NOCOLORS,0}};			// bitmap for alpha2 (blink alpha, not done actally)
static BMI bmiGraph = {{0x28,400,-300,1,8,BI_RGB,0,0,0,NOCOLORS,0}};			// bitmap for graph
static BMI bmiScreen = {{0x28,400,-300,1,8,BI_RGB,0,0,0,NOCOLORS,0}};			// bitmap for screen (alpha(+)graph) (will be copied on main window bitmap later)

UINT    nBackgroundX = 0;					// offset of background X (from KML script)
UINT    nBackgroundY = 0;					// offset of background Y (from KML script)
UINT    nBackgroundW = 0;					// width of background (from KML script)
UINT    nBackgroundH = 0;					// height of background (from KML script)
UINT    nLcdX = 0;							// offset of screen X (from KML script)
UINT    nLcdY = 0;							// offset of screen Y (from KML script)

//################
//#
//#    Low level subroutines
//#
//################

//
// update the clut for display
//
VOID UpdateClut(VOID)
{	
	if (Chipset.type == 37) {									// for 9837 : monochrome
		bmiGraph.bmiColors[0] = *(RGBQUAD*)&dwKMLColor[0];			// pixel color 0
		bmiGraph.bmiColors[1] = *(RGBQUAD*)&dwKMLColor[1];			// pixel color 1
	} else if (Chipset.type == 35) {							// for 9836C : 136 colors
		BYTE i;

		for (i = 0; i < 136; i++) {
			bmiAlpha1.bmiColors[i] = *(RGBQUAD*)&dwKMLColor[i];		// pixel color for alpha1
			bmiAlpha2.bmiColors[i] = *(RGBQUAD*)&dwKMLColor[i];		// pixel color for alpha2
			bmiGraph.bmiColors[i] = *(RGBQUAD*)&dwKMLColor[i];		// pixel color for graph
			bmiScreen.bmiColors[i] = *(RGBQUAD*)&dwKMLColor[i];		// pixel color for screen
		}
	} else if ((Chipset.type == 36) || (Chipset.type == 16)) {	// for 9816 and 9836A
		bmiAlpha1.bmiColors[0] = *(RGBQUAD*)&dwKMLColor[0];			// pixel on on alpha1
		bmiAlpha1.bmiColors[1] = *(RGBQUAD*)&dwKMLColor[1];			// pixel off on alpha1
		bmiAlpha1.bmiColors[2] = *(RGBQUAD*)&dwKMLColor[2];			// pixel on semi on alpha1
		bmiAlpha1.bmiColors[3] = *(RGBQUAD*)&dwKMLColor[3];			// pixel off semi on alpha1
		bmiAlpha2.bmiColors[0] = *(RGBQUAD*)&dwKMLColor[0];		
		bmiAlpha2.bmiColors[1] = *(RGBQUAD*)&dwKMLColor[1];		
		bmiAlpha2.bmiColors[2] = *(RGBQUAD*)&dwKMLColor[2];		
		bmiAlpha2.bmiColors[3] = *(RGBQUAD*)&dwKMLColor[3];		
		bmiGraph.bmiColors[0] = *(RGBQUAD*)&dwKMLColor[0];		
		bmiGraph.bmiColors[1] = *(RGBQUAD*)&dwKMLColor[1];		
		bmiGraph.bmiColors[2] = *(RGBQUAD*)&dwKMLColor[2];		
		bmiGraph.bmiColors[3] = *(RGBQUAD*)&dwKMLColor[3];		
		bmiScreen.bmiColors[0] = *(RGBQUAD*)&dwKMLColor[0];		
		bmiScreen.bmiColors[1] = *(RGBQUAD*)&dwKMLColor[1];		
		bmiScreen.bmiColors[2] = *(RGBQUAD*)&dwKMLColor[2];		
		bmiScreen.bmiColors[3] = *(RGBQUAD*)&dwKMLColor[3];		
	}
	// update palette information
	if (Chipset.type == 37) {									// for 9837, only graph palette
		_ASSERT(hGraphDC);
		SetDIBColorTable(hGraphDC,0,ARRAYSIZEOF(bmiGraph.bmiColors),bmiGraph.bmiColors); 
		SetDIBColorTable(hScreenDC,0,ARRAYSIZEOF(bmiScreen.bmiColors),bmiScreen.bmiColors);
	} else {													// other, alpha1, alpha2, graph and screen palette
		_ASSERT(hAlpha1DC);
		_ASSERT(hAlpha2DC);
		_ASSERT(hGraphDC);
		_ASSERT(hScreenDC);
		SetDIBColorTable(hAlpha1DC,0,ARRAYSIZEOF(bmiAlpha1.bmiColors),bmiAlpha1.bmiColors); 
		SetDIBColorTable(hAlpha2DC,0,ARRAYSIZEOF(bmiAlpha2.bmiColors),bmiAlpha2.bmiColors); 
		SetDIBColorTable(hGraphDC,0,ARRAYSIZEOF(bmiGraph.bmiColors),bmiGraph.bmiColors); 
		SetDIBColorTable(hScreenDC,0,ARRAYSIZEOF(bmiScreen.bmiColors),bmiScreen.bmiColors);
	}
}

//################
//#
//#    Public functions
//#
//################

//
// settings for palette colors
//
VOID SetLcdColor(UINT nId, UINT nRed, UINT nGreen, UINT nBlue)
{
	dwKMLColor[nId & 0xFF] = ((nRed&0xFF)<<16)|((nGreen&0xFF)<<8)|(nBlue&0xFF);
}

VOID SetGraphColor(BYTE col)
{
	bmiAlpha1.bmiColors[col] = *(RGBQUAD*)&dwKMLColor[col];		
	bmiAlpha2.bmiColors[col] = *(RGBQUAD*)&dwKMLColor[col];		
	bmiScreen.bmiColors[col] = *(RGBQUAD*)&dwKMLColor[col];		
	bmiGraph.bmiColors[col] = *(RGBQUAD*)&dwKMLColor[col];		
	SetDIBColorTable(hAlpha1DC,0,ARRAYSIZEOF(bmiAlpha1.bmiColors),bmiAlpha1.bmiColors); 
	SetDIBColorTable(hAlpha2DC,0,ARRAYSIZEOF(bmiAlpha2.bmiColors),bmiAlpha2.bmiColors); 
	SetDIBColorTable(hScreenDC,0,ARRAYSIZEOF(bmiScreen.bmiColors),bmiScreen.bmiColors); 
	SetDIBColorTable(hGraphDC,0,ARRAYSIZEOF(bmiGraph.bmiColors),bmiGraph.bmiColors); 
}

//
// allocate screen bitmaps and font and find mother board switches ... (I know, strange place to do that)
//
VOID CreateLcdBitmap(VOID)
{
//	BYTE i;

// HP9816
//
// SW1				/---------------------- CRT Hz setting
//                  |/--------------------- HP-IB System controller
//                  ||/-------------------- Continuous test
//					|||/------------------- Remote Keyboard status
//                  ||||/--\--------------- Baud rate
//					76543210
// default          11100101				0xE5 
// SWITCH1		0xE5
//
// SW2				/\--------------------- Character length
//                  ||/-------------------- Stop bits
//                  |||/------------------- Parity Enable
//					||||/\----------------- Parity
//                  ||||||/\--------------- Handshake
//					76543210
// default          11000000				0xC0
// SWITCH2		0xC0
//
// HP9836
//
// SW1				/---------------------- CRT Hz setting
//                  |/--------------------- HP-IB System controller
//                  ||/-------------------- Continuous test
//					|||/------------------- Remote Keyboard status
//                  ||||/--\--------------- Baud rate
//					76543210
// default          11100101				0x00
// SWITCH1		0xE0
//
// HP9837 ?? so take the same as HP9836
//

	Chipset.type = nCurrentRomType;		// not already assigned, do it

	if (Chipset.type == 16) {										// 9816
		Chipset.switch1 = 0xE5;
		Chipset.switch2 = 0xC0;
		Chipset.Display16.alpha_width = 800;							// width in pc pixels
		Chipset.Display16.alpha_height = 300;							// height in native pixel (will be stretch later)
		Chipset.Display16.alpha_char_w = 10;
		Chipset.Display16.alpha_char_h = 12;
		Chipset.Display16.graph_width = 400;							// width in native pixels (will be stretch later)
		Chipset.Display16.graph_bytes = 50;								// width in byte 50*8=400
		Chipset.Display16.graph_height = 300;							// height in native pixel (will be stretch later)
		Chipset.Display16.graph_visible = 30000;						// last address byte visible : 400x300/8 x 2 (only odd bytes)
	} else if (Chipset.type == 36) {								// 9836A
		Chipset.switch1 = 0xE0;
		Chipset.switch2 = 0x00;
		Chipset.Display16.alpha_width = 1024;							// width in pc pixels 
		Chipset.Display16.alpha_height = 390;							// height in native pixels 
		Chipset.Display16.alpha_char_w = 12;
		Chipset.Display16.alpha_char_h = 15;
		Chipset.Display16.graph_width = 512;							// width in native pixels (will be stretch later)
		Chipset.Display16.graph_bytes = 64;								// width in bytes 64*8=512
		Chipset.Display16.graph_height = 390;							// height in native pixel (will be stretch later)
		Chipset.Display16.graph_visible = 24960;						// last address byte visible : 512*390/8 (every bytes)
	} else if (Chipset.type == 35) {								// 9836C
		Chipset.switch1 = 0xE0;
		Chipset.switch2 = 0x00;
		Chipset.Display36c.alpha_width = 1024;							// width in pc pixels 
		Chipset.Display36c.alpha_height = 390;							// height in native pixels
		Chipset.Display36c.alpha_char_w = 12;
		Chipset.Display36c.alpha_char_h = 15;
		Chipset.Display36c.graph_width = 512;
		Chipset.Display36c.graph_bytes = 64;
		Chipset.Display36c.graph_height = 390;
	} else if (Chipset.type == 37) {	// 9837
		Chipset.switch1 = 0xE0;
		Chipset.switch2 = 0x00;
		Chipset.Display37.graph_width = 1024;							// only graph plane
		Chipset.Display37.graph_height = 768;							// visible part
	}

	switch (Chipset.type) {
		case 16:
		case 35:
		case 36:
			// create alpha1 bitmap
			hAlpha1DC = CreateCompatibleDC(hWindowDC);
			if (Chipset.type == 35) {
				bmiAlpha1.Lcd_bmih.biWidth = Chipset.Display36c.alpha_width;
				bmiAlpha1.Lcd_bmih.biHeight = - Chipset.Display36c.alpha_height;
			} else {
				bmiAlpha1.Lcd_bmih.biWidth = Chipset.Display16.alpha_width;
				bmiAlpha1.Lcd_bmih.biHeight = - Chipset.Display16.alpha_height;
			}
			hAlpha1Bitmap = CreateDIBSection(hAlpha1DC, (BITMAPINFO*)&bmiAlpha1,DIB_RGB_COLORS, (LPVOID*)&pbyAlpha1, NULL, 0);
			hAlpha1Bitmap = SelectObject(hAlpha1DC, hAlpha1Bitmap);
			SelectPalette(hAlpha1DC, hPalette, FALSE);	
			RealizePalette(hAlpha1DC);					
			// create alpha2 bitmap
			hAlpha2DC = CreateCompatibleDC(hWindowDC);
			if (Chipset.type == 35) {
				bmiAlpha2.Lcd_bmih.biWidth = Chipset.Display36c.alpha_width;
				bmiAlpha2.Lcd_bmih.biHeight = - Chipset.Display36c.alpha_height;
			} else {
				bmiAlpha2.Lcd_bmih.biWidth = Chipset.Display16.alpha_width;
				bmiAlpha2.Lcd_bmih.biHeight = - Chipset.Display16.alpha_height;
			}
			hAlpha2Bitmap = CreateDIBSection(hAlpha2DC, (BITMAPINFO*)&bmiAlpha2,DIB_RGB_COLORS, (LPVOID*)&pbyAlpha2, NULL, 0);
			hAlpha2Bitmap = SelectObject(hAlpha2DC, hAlpha2Bitmap);
			SelectPalette(hAlpha2DC, hPalette, FALSE);	
			RealizePalette(hAlpha2DC);					
			// create graph bitmap
			hGraphDC = CreateCompatibleDC(hWindowDC);
			if (Chipset.type == 35) {
				bmiGraph.Lcd_bmih.biWidth = Chipset.Display36c.graph_width;
				bmiGraph.Lcd_bmih.biHeight = - Chipset.Display36c.graph_height;
			} else {
				bmiGraph.Lcd_bmih.biWidth = Chipset.Display16.graph_width;
				bmiGraph.Lcd_bmih.biHeight = - Chipset.Display16.graph_height;
			}
			hGraphBitmap = CreateDIBSection(hGraphDC, (BITMAPINFO*)&bmiGraph,DIB_RGB_COLORS, (LPVOID*)&pbyGraph, NULL, 0);
			hGraphBitmap = SelectObject(hGraphDC, hGraphBitmap);
			SelectPalette(hGraphDC, hPalette, FALSE);	
			RealizePalette(hGraphDC); 
			// create screen bitmap
			hScreenDC = CreateCompatibleDC(hWindowDC);
			if (Chipset.type == 35) {
				bmiScreen.Lcd_bmih.biWidth = Chipset.Display36c.alpha_width;
				bmiScreen.Lcd_bmih.biHeight = - Chipset.Display36c.alpha_height;
			} else {
				bmiScreen.Lcd_bmih.biWidth = Chipset.Display16.alpha_width;
				bmiScreen.Lcd_bmih.biHeight = - Chipset.Display16.alpha_height;
			}
			hScreenBitmap = CreateDIBSection(hScreenDC, (BITMAPINFO*)&bmiScreen,DIB_RGB_COLORS, (LPVOID*)&pbyScreen, NULL, 0);
			hScreenBitmap = SelectObject(hScreenDC, hScreenBitmap);
			SelectPalette(hScreenDC, hPalette, FALSE);	
			RealizePalette(hScreenDC); 
			// font bitmap
			hFontDC = CreateCompatibleDC(hWindowDC);
			_ASSERT(hFontDC != NULL);

			if (Chipset.type == 16) {
				hFontBitmap = LoadImage(hApp,MAKEINTRESOURCE(IDB_216_FONTMAP1), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);	// Load bitmap font
			} else if (Chipset.type == 36) {
				hFontBitmap = LoadImage(hApp,MAKEINTRESOURCE(IDB_236_FONTMAP1), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);	// Load bitmap font
			} else if (Chipset.type == 35) {
				hFontBitmap = LoadImage(hApp,MAKEINTRESOURCE(IDB_236C_FONTMAP1), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);	// Load bitmap font
			}
			_ASSERT(hFontBitmap != NULL);
			hFontBitmap = SelectObject(hFontDC, hFontBitmap);
			// make a full redraw at start
			if (Chipset.type == 35) {			
				Chipset.Display36c.a_xmin = 0;
				Chipset.Display36c.a_xmax = Chipset.Display36c.alpha_width;
				Chipset.Display36c.a_ymin = 0;
				Chipset.Display36c.a_ymax = Chipset.Display36c.alpha_height;
				Chipset.Display36c.g_xmin = Chipset.Display36c.graph_width;
				Chipset.Display36c.g_xmax = 0;
				Chipset.Display36c.g_ymin = Chipset.Display36c.graph_height;
				Chipset.Display36c.g_ymax = 0;
			} else {
				Chipset.Display16.a_xmin = 0;
				Chipset.Display16.a_xmax = Chipset.Display16.alpha_width;
				Chipset.Display16.a_ymin = 0;
				Chipset.Display16.a_ymax = Chipset.Display16.alpha_height;
				Chipset.Display16.g_xmin = Chipset.Display16.graph_width;
				Chipset.Display16.g_xmax = 0;
				Chipset.Display16.g_ymin = Chipset.Display16.graph_height;
				Chipset.Display16.g_ymax = 0;
			}
			break;
		case 37:
			// create graph bitmap
			hGraphDC = CreateCompatibleDC(hWindowDC);
			bmiGraph.Lcd_bmih.biWidth = Chipset.Display37.graph_width;
			bmiGraph.Lcd_bmih.biHeight = - Chipset.Display37.graph_width;
			hGraphBitmap = CreateDIBSection(hGraphDC, (BITMAPINFO*)&bmiGraph,DIB_RGB_COLORS, (LPVOID*)&pbyGraph, NULL, 0);
			FillMemory(pbyGraph, 1024*1024, 1);
			hGraphBitmap = SelectObject(hGraphDC, hGraphBitmap);
			SelectPalette(hGraphDC, hPalette, FALSE);	
			RealizePalette(hGraphDC);
			// font bitmap
			hFontDC = CreateCompatibleDC(hWindowDC);
			_ASSERT(hFontDC != NULL);

			//hFontBitmap = LoadImage(hApp,MAKEINTRESOURCE(IDB_237_FONTMAP1), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);	// Load bitmap font, no more needed

			_ASSERT(hFontBitmap != NULL);
			hFontBitmap = SelectObject(hFontDC, hFontBitmap);

			// make a full redraw at start
			Chipset.Display37.g_xmax = Chipset.Display37.graph_width;
			Chipset.Display37.g_xmin = 0;
			Chipset.Display37.g_ymax = Chipset.Display37.graph_height;
			Chipset.Display37.g_ymin = 0;
			break;
	}
	UpdateClut();

}

//
// free screen bitmap and font
//
VOID DestroyLcdBitmap(VOID)
{
	// set contrast palette to startup colors
	WORD i = 0;   
	while(i < 16) dwKMLColor[i++] = W;
	while(i < 32) dwKMLColor[i++] = B;
	while(i < 64) dwKMLColor[i++] = I;
	while(i < 80) dwKMLColor[i++] = W;
	while(i < 96) dwKMLColor[i++] = B;
	while(i < 128) dwKMLColor[i++] = I;
	while(i < 144) dwKMLColor[i++] = W;
	while(i < 160) dwKMLColor[i++] = B;
	while(i < 192) dwKMLColor[i++] = I;
	while(i < 208) dwKMLColor[i++] = W;
	while(i < 224) dwKMLColor[i++] = B;
	while(i < 256) dwKMLColor[i++] = I;

	switch (Chipset.type) {
		case 16:
		case 35:
		case 36:
			if (hGraphDC != NULL) {
				// destroy graph bitmap
				SelectObject(hGraphDC, hGraphBitmap);
				DeleteObject(hGraphBitmap);
				DeleteDC(hGraphDC);
				hGraphDC = NULL;
				hGraphBitmap = NULL;
			}
			if (hAlpha1DC != NULL) {
				// destroy screen bitmap
				SelectObject(hAlpha1DC, hAlpha1Bitmap);
				DeleteObject(hAlpha1Bitmap);
				DeleteDC(hAlpha1DC);
				hAlpha1DC = NULL;
				hAlpha1Bitmap = NULL;
			}
			if (hAlpha2DC != NULL) {
				// destroy screen bitmap
				SelectObject(hAlpha2DC, hAlpha2Bitmap);
				DeleteObject(hAlpha2Bitmap);
				DeleteDC(hAlpha2DC);
				hAlpha2DC = NULL;
				hAlpha2Bitmap = NULL;
			}
			if (hFontDC != NULL) {
				// destroy font bitmap
				SelectObject(hFontDC, hFontBitmap);
				DeleteObject(hFontBitmap);
				DeleteDC(hFontDC);
				hFontDC = NULL;
				hFontBitmap = NULL;
			}
			if (hScreenDC != NULL) {
				// destroy screen bitmap
				SelectObject(hScreenDC, hScreenBitmap);
				DeleteObject(hScreenBitmap);
				DeleteDC(hScreenDC);
				hScreenDC = NULL;
				hScreenBitmap = NULL;
			}
			break;
		case 37:
			if (hGraphDC != NULL) {
				// destroy graph bitmap
				SelectObject(hGraphDC, hGraphBitmap);
				DeleteObject(hGraphBitmap);
				DeleteDC(hGraphDC);
				hGraphDC = NULL;
				hGraphBitmap = NULL;
			}
			if (hFontDC != NULL) {
				// destroy font bitmap
				SelectObject(hFontDC, hFontBitmap);
				DeleteObject(hFontBitmap);
				DeleteDC(hFontDC);
				hFontDC = NULL;
				hFontBitmap = NULL;
			}
			break;
	}
}

//
// allocate main bitmap for border infos
//
BOOL CreateMainBitmap(LPCTSTR szFilename)
{
	HPALETTE hAssertPalette;

	_ASSERT(hWindowDC != NULL);
	hMainDC = CreateCompatibleDC(hWindowDC);
	_ASSERT(hMainDC != NULL);
	if (hMainDC == NULL) return FALSE;		// quit if failed
	hMainBitmap = LoadBitmapFile(szFilename);
	if (hMainBitmap == NULL) {
		DeleteDC(hMainDC);
		return FALSE;
	}
	hMainBitmap = SelectObject(hMainDC, hMainBitmap);
	_ASSERT(hPalette != NULL);
	hAssertPalette = SelectPalette(hMainDC, hPalette, FALSE);
	_ASSERT(hAssertPalette != NULL);
	RealizePalette(hMainDC);
	return TRUE;
}

//
// free main bitmap
//
VOID DestroyMainBitmap(VOID)
{
	if (hMainDC != NULL) {
		// destroy Main bitmap
		SelectObject(hMainDC, hMainBitmap);
		DeleteObject(hMainBitmap);
		DeleteDC(hMainDC);
		hMainDC = NULL;
		hMainBitmap = NULL;
	}
}

//
// update the main window bitmap (border & all)
//
VOID UpdateMainDisplay(BOOL bForce)
{
	if (Chipset.type == 37)
		UpdateMainDisplay37(bForce);
	else if (Chipset.type == 16)
		UpdateMainDisplay16(bForce);
	else if (Chipset.type == 36)
		UpdateMainDisplay16(bForce);
	else if (Chipset.type == 35)
		UpdateMainDisplay36c(bForce);
	else _ASSERT(0);
}

//
// refresh only the display part (with rectangle)
//
VOID Refresh_Display(BOOL bForce)
{
	if (Chipset.type == 37)
		Refresh_Display37(bForce);
	else if (Chipset.type == 16)
		Refresh_Display16(bForce);
	else if (Chipset.type == 36)
		Refresh_Display16(bForce);
	else if (Chipset.type == 35)
		Refresh_Display36c(bForce);
	else _ASSERT(0);
}

// 
// reload graph bitmap from graph mem (9837) 
//
VOID Reload_Graph(VOID)
{
	if (Chipset.type == 37)
		Reload_Graph37();
	else if (Chipset.type == 16)
		Reload_Graph16();
	else if (Chipset.type == 36)
		Reload_Graph16();
	else if (Chipset.type == 35)
		Reload_Graph36c();
	else _ASSERT(0);
}

//
// update display annuciators (via kml.c)
//
VOID UpdateAnnunciators(BOOL bForce)
{
	int i;
	DWORD j=1;

	if (bForce)					// force redraw all annun ?
		Chipset.pannun = ~Chipset.annun;		// complement state
	for (i = 1; i < 33; ++i) {
		if ((Chipset.annun ^ Chipset.pannun) & j) {	// only if state changed
			if (Chipset.annun & j)
				DrawAnnunciator(i, TRUE);
			else
				DrawAnnunciator(i, FALSE);
		}
		j <<= 1;
	}
	Chipset.pannun = Chipset.annun;
}

//
// Main window resize
//
VOID ResizeWindow(VOID)
{
	RECT rectWindow;
	RECT rectClient;

	if (hWnd == NULL) return;				// return if window closed

	rectWindow.left   = 0;
	rectWindow.top    = 0;
	rectWindow.right  = nBackgroundW;
	rectWindow.bottom = nBackgroundH;
	AdjustWindowRect(&rectWindow, WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED, TRUE);
	SetWindowPos (hWnd, (HWND)NULL, 0, 0,
		rectWindow.right  - rectWindow.left,
		rectWindow.bottom - rectWindow.top,
		SWP_NOMOVE | SWP_NOZORDER);
	GetClientRect(hWnd, &rectClient);
	AdjustWindowRect(&rectClient, WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED, TRUE);
	if (rectClient.bottom < rectWindow.bottom) {
		rectWindow.bottom += (rectWindow.bottom - rectClient.bottom);
		SetWindowPos (hWnd, (HWND)NULL, 0, 0,
			rectWindow.right  - rectWindow.left,
			rectWindow.bottom - rectWindow.top,
			SWP_NOMOVE | SWP_NOZORDER);
	}

	_ASSERT(hWindowDC);						// move destination window
	SetWindowOrgEx(hWindowDC, nBackgroundX, nBackgroundY, NULL);
	InvalidateRect(hWnd,NULL,TRUE);
}

