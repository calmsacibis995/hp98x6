/*
 *   Keyboard.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//  nimitz keyboard emulation
//
//	done a french keymap (use alt-graph) and a us one (no use of alt-graph)
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "mops.h"

//#define DEBUG_KEYBOARDH
//#define DEBUG_KEYBOARD
#if defined(DEBUG_KEYBOARD) || defined(DEBUG_KEYBOARDH)
TCHAR buffer[256];
	int k;
//	unsigned long l;
#endif

//status   76543210
//         |||||||\----------	service need (interrupt level one done)
//         ||||||\-----------	8041 busy
//         |||||\------------	cause of NMI : 0:RESET key, 1:timeout
//         ||||\-------------	??
//         |||\-------------- b7=1	/ctrl state			0,5,6:	not used
//         ||\--------------- b7=1	/shift state			1:	10msec periodic system interrupt (PSI)	
//         |\---------------- b7=1	knob count in data		2:	interrupt from one of the special timers ?
//         \-----------------		keycode in data			3:	both PSI and special timers interrupt
//															4:	the data contain a byte request by the 68000
//															7:	power-up reset and self test done
//															8:	data contain keycode (shift & ctrl)
//															9:	data contain keycode (ctrl)
//														   10:	data contain keycode (shift)
//														   11:	data contain keycode
//														   12:	data contain knob count (shift & ctrl)
//														   13:	data contain knob count (ctrl)
//														   14:	data contain knob count (shift)
//														   15:	data contain knob count
//								
// 8041 registers
//	0, 1		scratch and pointer registers
//	2			flags
//	3			scratch
//	4			flags
//	5			flags
//	6			fifo
//	7			keycode of current key or 0
//	8, f		stack space
//	10			reset debounce counter
//	11			configuration jumper code
//	12			language jumper code
//	13, 17		timer outmput buffer (lsb is 13)
//	18, 19		scratch and pointer
//	1a			scratch
//	1b			timer status bits
//	1c			current knob pulse count (non hpil)
//	1d			pointer register to put data sent by 68000
//	1e			pointer register for data to send to 68000
//	1f			six counter for 804x timer interrupt
//	20			auto repeat delay
//	21			auto repeat timer
//	22			auto repeat rate
//	23			beep frequency (8041)
//	24			beep timer (counts up to zero)
//	25			knob timer (non hpil)
//	26			knob interrupt rate (non hpil)
//	27			if bit 6 is 1 the 804x timer has interrupted
//	28, 2c		not used
//	2d, 2f		time of day (3 bytes)
//	30, 31		days integer (2 bytes)
//	32, 33		fast-handshake timer (2 bytes)
//	34, 36		real time to match (3 bytes)
//	37, 39		delay timer (3 bytes)
//	3a, 3c		Cycle timer (3 bytes)
//	3d, 3f		Cycle timer save (3 bytes)
//
// language jumper code $11		0 : US normal		1 : French			2 : German
//								3 : Swedish/Finish	4 : Spanish			5 : Katakana
//								6 : 
// configuration jumper $12		bit 0 : 0 98203B keyboard, 1 98203A keyboard
//								bit 6 : 0 old code, 1 new revisions
//

// french keymap
// normal shift altgr alt shif-alt if needed
// add 128 to change the shift state
//
static BYTE frkeycode[256][5] = {
		0,		0,		0,		0,		0,			// 0x00 
		0,		0,		0,		0,		0,			// 0x01
		0,		0,		0,		0,		0,			// 0x02
		0,		0,		0,		0,		0,			// 0x03
		0,		0,		0,		0,		0,			// 0x04
		0,		0,		0,		0,		0,			// 0x05
		0,		0,		0,		0,		0,			// 0x06
		0,		0,		0,		0,		0,			// 0x07
		46,		0,		0,		0,		0,			// 0x08 VK_BACK				BACK SPACE
		25,		0,		0,		0,		0,			// 0x09 VK_TAB				TAB
		0,		0,		0,		0,		0,			// 0x0a
		0,		0,		0,		0,		0,			// 0x0b
		0,		0,		0,		0,		0,			// 0x0c VK_CLEAR
		57,		0,		0,		59,		0,			// 0x0d	VK_RETURN			ENTER				EXECUTE
		0,		0,		0,		0,		0,			// 0x0e
		0,		0,		0,		0,		0,			// 0x0f
		0,		0,		0,		0,		0,			// 0x10 VK_SHIFT			not used here
		0,		0,		0,		0,		0,			// 0x11 VK_CONTROL			not used here
		0,		0,		0,		0,		0,			// 0x12 VK_MENU				not used here
		56,		0,		0,		58,		0,			// 0x13 VK_PAUSE			PAUSE				CONTINUE
		24,		0,		0,		0,		0,			// 0x14 VK_CAPITAL
		0,		0,		0,		0,		0,			// 0x15
		0,		0,		0,		0,		0,			// 0x16
		0,		0,		0,		0,		0,			// 0x17
		0,		0,		0,		0,		0,			// 0x18
		0,		0,		0,		0,		0,			// 0x19
		0,		0,		0,		0,		0,			// 0x1a
		55,		0,		0,		0,		0,			// 0x1b VK_ESCAPE			CLR I/O (STOP)
		0,		0,		0,		0,		0,			// 0x1c
		0,		0,		0,		0,		0,			// 0x1d	
		0,		0,		0,		0,		0,			// 0x1e
		0,		0,		0,		0,		0,			// 0x1f
		99,		0,		0,		0,		0,			// 0x20 VK_SPACE
	128+34,		0,		0,		0,		0,			// 0x21 VK_PRIOR			shift up
	128+35,		0,		0,		0,		0,			// 0x22 VK_NEXT				shift down
		47,		0,		0,		0,		0,			// 0x23 VK_END				RUN
		45,		0,		0,		42,		0,			// 0x24 VK_HOME				CLEAR->END	 		RECALL	
		38,		0,		0,		0,		0,			// 0x25 VK_LEFT			
		35,		0,		0,		0,		0,			// 0x26 VK_UP
		39,		0,		0,		0,		0,			// 0x27 VK_RIGHT
		34,		0,		0,		0,		0,			// 0x28 VK_DOWN
		0,		0,		0,		0,		0,			// 0x29 VK_SELECT
		0,		0,		0,		0,		0,			// 0x2a
		0,		0,		0,		0,		0,			// 0x2b VK_EXECUTE
		54,		0,		0,		50,		0,			// 0x2c VK_SNAPSHOT			PRT ALL				GRAPHICS
		43,		0,		0,		40,		0,			// 0x2d	VK_INSERT			INS CHR				INS LN
		44,		0,		0,		41,		0,			// 0x2e VK_DELETE			DEL CHR				DEL LN
		0,		0,		0,		0,		0,			// 0x2f VK_HELP
		0,	128+89,	128+81,		0,		0,			// 0x30 VK_0	@				0	@
	128+86,	128+80,		0,		0,		0,			// 0x31 VK_1				&	1				
		0,	128+81,		0,		0,		0,			// 0x32 VK_2	~				2
	128+95,	128+82,	128+82,		0,		0,			// 0x33 VK_3	#			"	3	#					
		95,	128+83,	128+92,		0,		0,			// 0x34 VK_4	{			'	4	{
		77,	128+84,		92,		0,		0,			// 0x35 VK_5	[			(	5	[
		90,	128+85,		0,		0,		0,			// 0x36 VK_6				-	6
		0,	128+86,		0,		0,		0,			// 0x37 VK_7	`				7
	128+90,	128+87,		0,		0,		0,			// 0x38 VK_8	\			_	8
		0,	128+88,	128+85,		0,		0,			// 0x39 VK_9					9	^
		0,		0,		0,		0,		0,			// 0x3a
		0,		0,		0,		0,		0,			// 0x3b
		0,		0,		0,		0,		0,			// 0x3c
		0,		0,		0,		0,		0,			// 0x3d	
		0,		0,		0,		0,		0,			// 0x3e
		0,		0,		0,		0,		0,			// 0x3f
		0,		0,		0,		0,		0,			// 0x40
		112,	0,		0,		0,		0,			// 0x41 VK_A
		124,	0,		0,		0,		0,			// 0x42 VK_B
		122,	0,		0,		0,		0,			// 0x43 VK_C
		114,	0,		0,		0,		0,			// 0x44 VK_D
		106,	0,		0,		0,		0,			// 0x45 VK_E
		115,	0,		0,		0,		0,			// 0x46 VK_F
		116,	0,		0,		0,		0,			// 0x47 VK_G
		117,	0,		0,		0,		0,			// 0x48 VK_H
		111,	0,		0,		0,		0,			// 0x49 VK_I
		118,	0,		0,		0,		0,			// 0x4a VK_J
		102,	0,		0,		0,		0,			// 0x4b VK_K
		103,	0,		0,		0,		0,			// 0x4c VK_L
		119,	0,		0,		0,		0,			// 0x4d	VK_M
		125,	0,		0,		0,		0,			// 0x4e VK_N
		100,	0,		0,		0,		0,			// 0x4f VK_O
		101,	0,		0,		0,		0,			// 0x50 VK_P
		104,	0,		0,		0,		0,			// 0x51 VK_Q
		107,	0,		0,		0,		0,			// 0x52 VK_R
		113,	0,		0,		0,		0,			// 0x53 VK_S
		108,	0,		0,		0,		0,			// 0x54 VK_T
		110,	0,		0,		0,		0,			// 0x55 VK_U
		123,	0,		0,		0,		0,			// 0x56 VK_V
		105,	0,		0,		0,		0,			// 0x57 VK_W
		121,	0,		0,		0,		0,			// 0x58 VK_X
		109,	0,		0,		0,		0,			// 0x59 VK_Y
		120,	0,		0,		0,		0,			// 0x5a VK_Z
		0,		0,		0,		0,		0,			// 0x5b VK_LWIN				
		0,		0,		0,		0,		0,			// 0x5c VK_RWIN
		0,		0,		0,		0,		0,			// 0x5d	VK_APPS			
		0,		0,		0,		0,		0,			// 0x5e
		0,		0,		0,		0,		0,			// 0x5f
		60,		0,		0,		0,		0,			// 0x60	VK_NUMPAD0
		64,		0,		0,		0,		0,			// 0x61	VK_NUMPAD1
		65,		0,		0,		0,		0,			// 0x62	VK_NUMPAD2
		66,		0,		0,		0,		0,			// 0x63	VK_NUMPAD3
		68,		0,		0,		0,		0,			// 0x64	VK_NUMPAD4
		69,		0,		0,		0,		0,			// 0x65	VK_NUMPAD5
		70,		0,		0,		0,		0,			// 0x66	VK_NUMPAD6
		72,		0,		0,		0,		0,			// 0x67	VK_NUMPAD7
		73,		0,		0,		0,		0,			// 0x68 VK_NUMPAD8
		74,		0,		0,		0,		0,			// 0x69 VK_NUMPAD9
		71,		0,		0,		0,		0,			// 0x6a VK_MULTIPLY
		63,		0,		0,		0,		0,			// 0x6b VK_ADD
		0,		0,		0,		0,		0,			// 0x6c VK_SEPARATOR
		67,		0,		0,		0,		0,			// 0x6d	VK_SUBTRACT
		61,		0,		0,		0,		0,			// 0x6e VK_DECIMAL
		75,		0,		0,		0,		0,			// 0x6f VK_DIVIDE
		26,		0,		0,		0,		0,			// 0x70 VK_F1			
		27,		0,		0,		0,		0,			// 0x71 VK_F2			
		28,		0,		0,		0,		0,			// 0x72 VK_F3			
		32,		0,		0,		0,		0,			// 0x73 VK_F4			
		33,		0,		0,		0,		0,			// 0x74 VK_F5			
		29,		0,		0,		0,		0,			// 0x75 VK_F6			
		30,		0,		0,		0,		0,			// 0x76 VK_F7			
		31,		0,		0,		0,		0,			// 0x77 VK_F8
		36,		0,		0,		0,		0,			// 0x78 VK_F9			
		37,		0,		0,		0,		0,			// 0x79 VK_F10					
		52,		0,		0,		48,		0,			// 0x7a VK_F11				CLR LN					EDIT	
		53,		0,		0,		49,		0,			// 0x7b VK_F12				RESULT					ALPHA
		0,		0,		0,		0,		0,			// 0x7c VK_F13
		0,		0,		0,		0,		0,			// 0x7d VK_F14
		0,		0,		0,		0,		0,			// 0x7e VK_F15
		0,		0,		0,		0,		0,			// 0x7f VK_F16
		0,		0,		0,		0,		0,			// 0x80 VK_F17
		0,		0,		0,		0,		0,			// 0x81 VK_F18
		0,		0,		0,		0,		0,			// 0x82 VK_F19
		0,		0,		0,		0,		0,			// 0x83 VK_F20
		0,		0,		0,		0,		0,			// 0x84 VK_F21
		0,		0,		0,		0,		0,			// 0x85 VK_F22
		0,		0,		0,		0,		0,			// 0x86 VK_F23
		0,		0,		0,		0,		0,			// 0x87 VK_F24
		0,		0,		0,		0,		0,			// 0x88
		0,		0,		0,		0,		0,			// 0x89
		0,		0,		0,		0,		0,			// 0x8a
		0,		0,		0,		0,		0,			// 0x8b
		0,		0,		0,		0,		0,			// 0x8c
		0,		0,		0,		0,		0,			// 0x8d	
		0,		0,		0,		0,		0,			// 0x8e
		0,		0,		0,		0,		0,			// 0x8f
		0,		0,		0,		0,		0,			// 0x90 VK_NUMLOCK		
		54,		0,		0,		50,		0,			// 0x91 VK_SCROLL			PRT ALL				GRAPHICS
		0,		0,		0,		0,		0,			// 0x92
		0,		0,		0,		0,		0,			// 0x93
		0,		0,		0,		0,		0,			// 0x94
		0,		0,		0,		0,		0,			// 0x95
		0,		0,		0,		0,		0,			// 0x96
		0,		0,		0,		0,		0,			// 0x97
		0,		0,		0,		0,		0,			// 0x98
		0,		0,		0,		0,		0,			// 0x99
		0,		0,		0,		0,		0,			// 0x9a
		0,		0,		0,		0,		0,			// 0x9b
		0,		0,		0,		0,		0,			// 0x9c
		0,		0,		0,		0,		0,			// 0x9d	
		0,		0,		0,		0,		0,			// 0x9e
		0,		0,		0,		0,		0,			// 0x9f
		0,		0,		0,		0,		0,			// 0xa0	VK_LSHIFT
		0,		0,		0,		0,		0,			// 0xa1	VK_RSHIFT
		0,		0,		0,		0,		0,			// 0xa2 VK_LCONTROL
		0,		0,		0,		0,		0,			// 0xa3	VK_RCONTROL
		0,		0,		0,		0,		0,			// 0xa4 VK_ALT
		0,		0,		0,		0,		0,			// 0xa5
		0,		0,		0,		0,		0,			// 0xa6
		0,		0,		0,		0,		0,			// 0xa7
		0,		0,		0,		0,		0,			// 0xa8
		0,		0,		0,		0,		0,			// 0xa9
		0,		0,		0,		0,		0,			// 0xaa
		0,		0,		0,		0,		0,			// 0xab
		0,		0,		0,		0,		0,			// 0xac
		0,		0,		0,		0,		0,			// 0xad	
		0,		0,		0,		0,		0,			// 0xae
		0,		0,		0,		0,		0,			// 0xaf
		0,		0,		0,		0,		0,			// 0xb0
		0,		0,		0,		0,		0,			// 0xb1
		0,		0,		0,		0,		0,			// 0xb2
		0,		0,		0,		0,		0,			// 0xb3
		0,		0,		0,		0,		0,			// 0xb4
		0,		0,		0,		0,		0,			// 0xb5
		0,		0,		0,		0,		0,			// 0xb6
		0,		0,		0,		0,		0,			// 0xb7
		0,		0,		0,		0,		0,			// 0xb8
		0,		0,		0,		0,		0,			// 0xb9
	128+83,		0,		0,		0,		0,			// 0xba	VK $ £ ($* mac os)		$
		91,		0,	128+93,		0,		0,			// 0xbb VK = + (-_ mac os)		=	+	}
		96,		98,		0,		0,		0,			// 0xbc VK , ?					,	?
		0,		0,		0,		0,		0,			// 0xbd
		94,	128+97,		0,		0,		0,			// 0xbe VK ; .
	128+94,	128+98,		0,		0,		0,			// 0xbf VK : /
		0,		84,		0,		0,		0,			// 0xc0 VK ù %
		0,		0,		0,		0,		0,			// 0xc1
		0,		0,		0,		0,		0,			// 0xc2
		0,		0,		0,		0,		0,			// 0xc3
		0,		0,		0,		0,		0,			// 0xc4
		0,		0,		0,		0,		0,			// 0xc5
		0,		0,		0,		0,		0,			// 0xc6
		0,		0,		0,		0,		0,			// 0xc7
		0,		0,		0,		0,		0,			// 0xc8
		0,		0,		0,		0,		0,			// 0xc9
		0,		0,		0,		0,		0,			// 0xca
		0,		0,		0,		0,		0,			// 0xcb
		0,		0,		0,		0,		0,			// 0xcc
		0,		0,		0,		0,		0,			// 0xcd	
		0,		0,		0,		0,		0,			// 0xce
		0,		0,		0,		0,		0,			// 0xcf
		0,		0,		0,		0,		0,			// 0xd0
		0,		0,		0,		0,		0,			// 0xd1
		0,		0,		0,		0,		0,			// 0xd2
		0,		0,		0,		0,		0,			// 0xd3
		0,		0,		0,		0,		0,			// 0xd4
		0,		0,		0,		0,		0,			// 0xd5
		0,		0,		0,		0,		0,			// 0xd6
		0,		0,		0,		0,		0,			// 0xd7
		0,		0,		0,		0,		0,			// 0xd8
		0,		0,		0,		0,		0,			// 0xd9
		0,		0,		0,		0,		0,			// 0xda
		78,		0,		93,		0,		0,			// 0xdb	VK ) ] ()° macos)		)		]
		71,		0,		0,		0,		0,			// 0xdc	VK * µ (` £ mac os)		*
		79,		0,		0,		0,		0,			// 0xdd	VK ^¨					^ 
		51,		0,		0,		0,		0,			// 0xde VK ² (@# mac os)		STEP	
	128+80,		0,		0,		0,		0,			// 0xdf VK ! § (=+ mac os)		!	
		0,		0,		0,		0,		0,			// 0xe0
		0,		0,		0,		0,		0,			// 0xe1
	128+96,		97,		0,		0,		0,			// 0xe2	VK > <					<	>
		0,		0,		0,		0,		0,			// 0xe3
		0,		0,		0,		0,		0,			// 0xe4
		0,		0,		0,		0,		0,			// 0xe5
		0,		0,		0,		0,		0,			// 0xe6
		0,		0,		0,		0,		0,			// 0xe7
		0,		0,		0,		0,		0,			// 0xe8
		0,		0,		0,		0,		0,			// 0xe9
		0,		0,		0,		0,		0,			// 0xea
		0,		0,		0,		0,		0,			// 0xeb
		0,		0,		0,		0,		0,			// 0xec
		0,		0,		0,		0,		0,			// 0xed	
		0,		0,		0,		0,		0,			// 0xee
		0,		0,		0,		0,		0,			// 0xef
		0,		0,		0,		0,		0,			// 0xf0
		0,		0,		0,		0,		0,			// 0xf1
		0,		0,		0,		0,		0,			// 0xf2
		0,		0,		0,		0,		0,			// 0xf3
		0,		0,		0,		0,		0,			// 0xf4
		0,		0,		0,		0,		0,			// 0xf5
		0,		0,		0,		0,		0,			// 0xf6 VK_ATTN
		0,		0,		0,		0,		0,			// 0xf7 VK_CRSEL
		0,		0,		0,		0,		0,			// 0xf8 VK_EXSEL
		0,		0,		0,		0,		0,			// 0xf9 VK_EREOF
		0,		0,		0,		0,		0,			// 0xfa VK_PLAY
		0,		0,		0,		0,		0,			// 0xfb VK_ZOOM
		0,		0,		0,		0,		0,			// 0xfc VK_NONAME
		0,		0,		0,		0,		0,			// 0xfd	VK_PA1
		0,		0,		0,		0,		0,			// 0xfe VK_OEM_CLEAR
		0,		0,		0,		0,		0			// 0xff
};

// us keymap
// normal shift altgr alt shif-alt if needed

static BYTE uskeycode[256][5] = {
		0,		0,		0,		0,		0,			// 0x00 
		0,		0,		0,		0,		0,			// 0x01
		0,		0,		0,		0,		0,			// 0x02
		0,		0,		0,		0,		0,			// 0x03
		0,		0,		0,		0,		0,			// 0x04
		0,		0,		0,		0,		0,			// 0x05
		0,		0,		0,		0,		0,			// 0x06
		0,		0,		0,		0,		0,			// 0x07
		46,		0,		0,		0,		0,			// 0x08 VK_BACK				BACK SPACE
		25,		0,		0,		0,		0,			// 0x09 VK_TAB				TAB
		0,		0,		0,		0,		0,			// 0x0a
		0,		0,		0,		0,		0,			// 0x0b
		0,		0,		0,		0,		0,			// 0x0c VK_CLEAR
		57,		0,		0,		59,		0,			// 0x0d	VK_RETURN			ENTER				EXECUTE
		0,		0,		0,		0,		0,			// 0x0e
		0,		0,		0,		0,		0,			// 0x0f
		0,		0,		0,		0,		0,			// 0x10 VK_SHIFT			not used here
		0,		0,		0,		0,		0,			// 0x11 VK_CONTROL			not used here
		0,		0,		0,		0,		0,			// 0x12 VK_MENU				not used here
		56,		0,		0,		58,		0,			// 0x13 VK_PAUSE			PAUSE				CONTINUE
		24,		0,		0,		0,		0,			// 0x14 VK_CAPITAL
		0,		0,		0,		0,		0,			// 0x15
		0,		0,		0,		0,		0,			// 0x16
		0,		0,		0,		0,		0,			// 0x17
		0,		0,		0,		0,		0,			// 0x18
		0,		0,		0,		0,		0,			// 0x19
		0,		0,		0,		0,		0,			// 0x1a
		55,		0,		0,		0,		0,			// 0x1b VK_ESCAPE			CLR I/O (STOP)
		0,		0,		0,		0,		0,			// 0x1c
		0,		0,		0,		0,		0,			// 0x1d	
		0,		0,		0,		0,		0,			// 0x1e
		0,		0,		0,		0,		0,			// 0x1f
		99,		0,		0,		0,		0,			// 0x20 VK_SPACE
	128+34,		0,		0,		0,		0,			// 0x21 VK_PRIOR			shift up
	128+35,		0,		0,		0,		0,			// 0x22 VK_NEXT				shift down
		47,		0,		0,		0,		0,			// 0x23 VK_END				RUN
		45,		0,		0,		42,		0,			// 0x24 VK_HOME				CLEAR->END	 		RECALL	
		38,		0,		0,		0,		0,			// 0x25 VK_LEFT			
		35,		0,		0,		0,		0,			// 0x26 VK_UP
		39,		0,		0,		0,		0,			// 0x27 VK_RIGHT
		34,		0,		0,		0,		0,			// 0x28 VK_DOWN
		0,		0,		0,		0,		0,			// 0x29 VK_SELECT
		0,		0,		0,		0,		0,			// 0x2a
		0,		0,		0,		0,		0,			// 0x2b VK_EXECUTE
		54,		0,		0,		50,		0,			// 0x2c VK_SNAPSHOT			PRT ALL				GRAPHICS
		43,		0,		0,		40,		0,			// 0x2d	VK_INSERT			INS CHR				INS LN
		44,		0,		0,		41,		0,			// 0x2e VK_DELETE			DEL CHR				DEL LN
		0,		0,		0,		0,		0,			// 0x2f VK_HELP
		89,		0,		0,		0,		0,			// 0x30 VK_0	@				0	@
		80,		0,		0,		0,		0,			// 0x31 VK_1				&	1				
		81,		0,		0,		0,		0,			// 0x32 VK_2	~				2
		82,		0,		0,		0,		0,			// 0x33 VK_3	#			"	3	#					
		83,		0,		0,		0,		0,			// 0x34 VK_4	{			'	4	{
		84,		0,		0,		0,		0,			// 0x35 VK_5	[			(	5	[
		85,		0,		0,		0,		0,			// 0x36 VK_6				-	6
		86,		0,		0,		0,		0,			// 0x37 VK_7	`				7
		87,		0,		0,		0,		0,			// 0x38 VK_8	\			_	8
		88,		0,		0,		0,		0,			// 0x39 VK_9					9	^
		0,		0,		0,		0,		0,			// 0x3a
		0,		0,		0,		0,		0,			// 0x3b
		0,		0,		0,		0,		0,			// 0x3c
		0,		0,		0,		0,		0,			// 0x3d	
		0,		0,		0,		0,		0,			// 0x3e
		0,		0,		0,		0,		0,			// 0x3f
		0,		0,		0,		0,		0,			// 0x40
		112,	0,		0,		0,		0,			// 0x41 VK_A
		124,	0,		0,		0,		0,			// 0x42 VK_B
		122,	0,		0,		0,		0,			// 0x43 VK_C
		114,	0,		0,		0,		0,			// 0x44 VK_D
		106,	0,		0,		0,		0,			// 0x45 VK_E
		115,	0,		0,		0,		0,			// 0x46 VK_F
		116,	0,		0,		0,		0,			// 0x47 VK_G
		117,	0,		0,		0,		0,			// 0x48 VK_H
		111,	0,		0,		0,		0,			// 0x49 VK_I
		118,	0,		0,		0,		0,			// 0x4a VK_J
		102,	0,		0,		0,		0,			// 0x4b VK_K
		103,	0,		0,		0,		0,			// 0x4c VK_L
		119,	0,		0,		0,		0,			// 0x4d	VK_M
		125,	0,		0,		0,		0,			// 0x4e VK_N
		100,	0,		0,		0,		0,			// 0x4f VK_O
		101,	0,		0,		0,		0,			// 0x50 VK_P
		104,	0,		0,		0,		0,			// 0x51 VK_Q
		107,	0,		0,		0,		0,			// 0x52 VK_R
		113,	0,		0,		0,		0,			// 0x53 VK_S
		108,	0,		0,		0,		0,			// 0x54 VK_T
		110,	0,		0,		0,		0,			// 0x55 VK_U
		123,	0,		0,		0,		0,			// 0x56 VK_V
		105,	0,		0,		0,		0,			// 0x57 VK_W
		121,	0,		0,		0,		0,			// 0x58 VK_X
		109,	0,		0,		0,		0,			// 0x59 VK_Y
		120,	0,		0,		0,		0,			// 0x5a VK_Z
		0,		0,		0,		0,		0,			// 0x5b VK_LWIN				
		0,		0,		0,		0,		0,			// 0x5c VK_RWIN
		0,		0,		0,		0,		0,			// 0x5d	VK_APPS			
		0,		0,		0,		0,		0,			// 0x5e
		0,		0,		0,		0,		0,			// 0x5f
		60,		0,		0,		0,		0,			// 0x60	VK_NUMPAD0
		64,		0,		0,		0,		0,			// 0x61	VK_NUMPAD1
		65,		0,		0,		0,		0,			// 0x62	VK_NUMPAD2
		66,		0,		0,		0,		0,			// 0x63	VK_NUMPAD3
		68,		0,		0,		0,		0,			// 0x64	VK_NUMPAD4
		69,		0,		0,		0,		0,			// 0x65	VK_NUMPAD5
		70,		0,		0,		0,		0,			// 0x66	VK_NUMPAD6
		72,		0,		0,		0,		0,			// 0x67	VK_NUMPAD7
		73,		0,		0,		0,		0,			// 0x68 VK_NUMPAD8
		74,		0,		0,		0,		0,			// 0x69 VK_NUMPAD9
		71,		0,		0,		0,		0,			// 0x6a VK_MULTIPLY
		63,		0,		0,		0,		0,			// 0x6b VK_ADD
		0,		0,		0,		0,		0,			// 0x6c VK_SEPARATOR
		67,		0,		0,		0,		0,			// 0x6d	VK_SUBTRACT
		61,		0,		0,		0,		0,			// 0x6e VK_DECIMAL
		75,		0,		0,		0,		0,			// 0x6f VK_DIVIDE
		26,		0,		0,		0,		0,			// 0x70 VK_F1			
		27,		0,		0,		0,		0,			// 0x71 VK_F2			
		28,		0,		0,		0,		0,			// 0x72 VK_F3			
		32,		0,		0,		0,		0,			// 0x73 VK_F4			
		33,		0,		0,		0,		0,			// 0x74 VK_F5			
		29,		0,		0,		0,		0,			// 0x75 VK_F6			
		30,		0,		0,		0,		0,			// 0x76 VK_F7			
		31,		0,		0,		0,		0,			// 0x77 VK_F8
		36,		0,		0,		0,		0,			// 0x78 VK_F9			
		37,		0,		0,		0,		0,			// 0x79 VK_F10					
		52,		0,		0,		48,		0,			// 0x7a VK_F11				CLR LN					EDIT	
		53,		0,		0,		49,		0,			// 0x7b VK_F12				RESULT					ALPHA
		0,		0,		0,		0,		0,			// 0x7c VK_F13
		0,		0,		0,		0,		0,			// 0x7d VK_F14
		0,		0,		0,		0,		0,			// 0x7e VK_F15
		0,		0,		0,		0,		0,			// 0x7f VK_F16
		0,		0,		0,		0,		0,			// 0x80 VK_F17
		0,		0,		0,		0,		0,			// 0x81 VK_F18
		0,		0,		0,		0,		0,			// 0x82 VK_F19
		0,		0,		0,		0,		0,			// 0x83 VK_F20
		0,		0,		0,		0,		0,			// 0x84 VK_F21
		0,		0,		0,		0,		0,			// 0x85 VK_F22
		0,		0,		0,		0,		0,			// 0x86 VK_F23
		0,		0,		0,		0,		0,			// 0x87 VK_F24
		0,		0,		0,		0,		0,			// 0x88
		0,		0,		0,		0,		0,			// 0x89
		0,		0,		0,		0,		0,			// 0x8a
		0,		0,		0,		0,		0,			// 0x8b
		0,		0,		0,		0,		0,			// 0x8c
		0,		0,		0,		0,		0,			// 0x8d	
		0,		0,		0,		0,		0,			// 0x8e
		0,		0,		0,		0,		0,			// 0x8f
		0,		0,		0,		0,		0,			// 0x90 VK_NUMLOCK		
		0,		0,		0,		0,		0,			// 0x91 VK_SCROLL		
		0,		0,		0,		0,		0,			// 0x92
		0,		0,		0,		0,		0,			// 0x93
		0,		0,		0,		0,		0,			// 0x94
		0,		0,		0,		0,		0,			// 0x95
		0,		0,		0,		0,		0,			// 0x96
		0,		0,		0,		0,		0,			// 0x97
		0,		0,		0,		0,		0,			// 0x98
		0,		0,		0,		0,		0,			// 0x99
		0,		0,		0,		0,		0,			// 0x9a
		0,		0,		0,		0,		0,			// 0x9b
		0,		0,		0,		0,		0,			// 0x9c
		0,		0,		0,		0,		0,			// 0x9d	
		0,		0,		0,		0,		0,			// 0x9e
		0,		0,		0,		0,		0,			// 0x9f
		0,		0,		0,		0,		0,			// 0xa0	VK_LSHIFT
		0,		0,		0,		0,		0,			// 0xa1	VK_RSHIFT
		0,		0,		0,		0,		0,			// 0xa2 VK_LCONTROL
		0,		0,		0,		0,		0,			// 0xa3	VK_RCONTROL
		0,		0,		0,		0,		0,			// 0xa4 VK_ALT
		0,		0,		0,		0,		0,			// 0xa5
		0,		0,		0,		0,		0,			// 0xa6
		0,		0,		0,		0,		0,			// 0xa7
		0,		0,		0,		0,		0,			// 0xa8
		0,		0,		0,		0,		0,			// 0xa9
		0,		0,		0,		0,		0,			// 0xaa
		0,		0,		0,		0,		0,			// 0xab
		0,		0,		0,		0,		0,			// 0xac
		0,		0,		0,		0,		0,			// 0xad	
		0,		0,		0,		0,		0,			// 0xae
		0,		0,		0,		0,		0,			// 0xaf
		0,		0,		0,		0,		0,			// 0xb0
		0,		0,		0,		0,		0,			// 0xb1
		0,		0,		0,		0,		0,			// 0xb2
		0,		0,		0,		0,		0,			// 0xb3
		0,		0,		0,		0,		0,			// 0xb4
		0,		0,		0,		0,		0,			// 0xb5
		0,		0,		0,		0,		0,			// 0xb6
		0,		0,		0,		0,		0,			// 0xb7
		0,		0,		0,		0,		0,			// 0xb8
		0,		0,		0,		0,		0,			// 0xb9
		94,		0,		0,		0,		0,			// 0xba	VK ; :
		91,		0,		0,		0,		0,			// 0xbb VK = + 
		96,		0,		0,		0,		0,			// 0xbc VK , <					
		90,		0,		0,		0,		0,			// 0xbd VK - _
		97,		0,		0,		0,		0,			// 0xbe VK . >
		98,		0,		0,		0,		0,			// 0xbf VK / ?
		51,		0,		0,		0,		0,			// 0xc0 VK ` ~			STEP
		0,		0,		0,		0,		0,			// 0xc1
		0,		0,		0,		0,		0,			// 0xc2
		0,		0,		0,		0,		0,			// 0xc3
		0,		0,		0,		0,		0,			// 0xc4
		0,		0,		0,		0,		0,			// 0xc5
		0,		0,		0,		0,		0,			// 0xc6
		0,		0,		0,		0,		0,			// 0xc7
		0,		0,		0,		0,		0,			// 0xc8
		0,		0,		0,		0,		0,			// 0xc9
		0,		0,		0,		0,		0,			// 0xca
		0,		0,		0,		0,		0,			// 0xcb
		0,		0,		0,		0,		0,			// 0xcc
		0,		0,		0,		0,		0,			// 0xcd	
		0,		0,		0,		0,		0,			// 0xce
		0,		0,		0,		0,		0,			// 0xcf
		0,		0,		0,		0,		0,			// 0xd0
		0,		0,		0,		0,		0,			// 0xd1
		0,		0,		0,		0,		0,			// 0xd2
		0,		0,		0,		0,		0,			// 0xd3
		0,		0,		0,		0,		0,			// 0xd4
		0,		0,		0,		0,		0,			// 0xd5
		0,		0,		0,		0,		0,			// 0xd6
		0,		0,		0,		0,		0,			// 0xd7
		0,		0,		0,		0,		0,			// 0xd8
		0,		0,		0,		0,		0,			// 0xd9
		0,		0,		0,		0,		0,			// 0xda
		92,		0,		0,		0,		0,			// 0xdb	VK [ {
		59,		0,		0,		0,		0,			// 0xdc	VK \ |		execute
		93,		0,		0,		0,		0,			// 0xdd	VK ] }				
		95,		0,		0,		0,		0,			// 0xde VK ' "	
		0,		0,		0,		0,		0,			// 0xdf VK 
		0,		0,		0,		0,		0,			// 0xe0
		0,		0,		0,		0,		0,			// 0xe1
		0,		0,		0,		0,		0,			// 0xe2	VK 
		0,		0,		0,		0,		0,			// 0xe3
		0,		0,		0,		0,		0,			// 0xe4
		0,		0,		0,		0,		0,			// 0xe5
		0,		0,		0,		0,		0,			// 0xe6
		0,		0,		0,		0,		0,			// 0xe7
		0,		0,		0,		0,		0,			// 0xe8
		0,		0,		0,		0,		0,			// 0xe9
		0,		0,		0,		0,		0,			// 0xea
		0,		0,		0,		0,		0,			// 0xeb
		0,		0,		0,		0,		0,			// 0xec
		0,		0,		0,		0,		0,			// 0xed	
		0,		0,		0,		0,		0,			// 0xee
		0,		0,		0,		0,		0,			// 0xef
		0,		0,		0,		0,		0,			// 0xf0
		0,		0,		0,		0,		0,			// 0xf1
		0,		0,		0,		0,		0,			// 0xf2
		0,		0,		0,		0,		0,			// 0xf3
		0,		0,		0,		0,		0,			// 0xf4
		0,		0,		0,		0,		0,			// 0xf5
		0,		0,		0,		0,		0,			// 0xf6 VK_ATTN
		0,		0,		0,		0,		0,			// 0xf7 VK_CRSEL
		0,		0,		0,		0,		0,			// 0xf8 VK_EXSEL
		0,		0,		0,		0,		0,			// 0xf9 VK_EREOF
		0,		0,		0,		0,		0,			// 0xfa VK_PLAY
		0,		0,		0,		0,		0,			// 0xfb VK_ZOOM
		0,		0,		0,		0,		0,			// 0xfc VK_NONAME
		0,		0,		0,		0,		0,			// 0xfd	VK_PA1
		0,		0,		0,		0,		0,			// 0xfe VK_OEM_CLEAR
		0,		0,		0,		0,		0			// 0xff
};

//
// current used keymap
//
static BYTE keycode[256][5];

VOID KnobRotate(SWORD knob)
{
	if ((Chipset.Keyboard.ram[0x1C] != 0x7F) && (knob < 0))
		Chipset.Keyboard.ram[0x1C] += knob;
	if ((Chipset.Keyboard.ram[0x1C] != 0x80) && (knob > 0))
		Chipset.Keyboard.ram[0x1C] += knob;
	#if defined DEBUG_KEYBOARDH
		k = wsprintf(buffer,_T("%06X: KEYBOARD : knob (%04X) = %04X\n"), Chipset.Cpu.PC, knob, Chipset.Keyboard.ram[0x1C]);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif
}

//
// Key released
//
VOID KeyboardEventUp(BYTE nId) //, BYTE shift, BYTE ctrl, BYTE altgr)
{
	WORD key;

	#if defined DEBUG_KEYBOARDH
		k = wsprintf(buffer,_T("%06X: KEYBOARD : up key %04X altgr %X\n"), Chipset.Cpu.PC, nId, Chipset.Keyboard.altgr);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif

	if (nState != SM_RUN)					// not in running state
		return;								// ignore key

	if (Chipset.Keyboard.altgr) {			// altgr
		key = keycode[nId][2];
	} else {
		key = keycode[nId][Chipset.Keyboard.shift + (Chipset.Keyboard.alt ? 3:0)];
		if (key == 0)
			key = keycode[nId][0 + (Chipset.Keyboard.alt ? 3:0)];
	}
	if (key != 0x00) {
		Chipset.Keyboard.ram[0x07] = 0x00;							// no more key pressed
	} 
}

//
// Key pressed
//
VOID KeyboardEventDown(BYTE nId) //, BYTE shift, BYTE ctrl, BYTE altgr)
{
	BYTE key;

	#if defined DEBUG_KEYBOARDH
		k = wsprintf(buffer,_T("%06X: KEYBOARD : down key %04X altgr %X\n"), Chipset.Cpu.PC, nId, Chipset.Keyboard.altgr);
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif

	if (nState != SM_RUN)				// not in running state ?
		return;								// ignore key

	if (Chipset.Keyboard.altgr) {			// altgr
		key = keycode[nId][2];
	} else {
		key = keycode[nId][Chipset.Keyboard.shift + (Chipset.Keyboard.alt ? 3:0)];
		if (key == 0)
			key = keycode[nId][0 + (Chipset.Keyboard.alt ? 3:0)];
	}
	if (key != 0x00) {
		Chipset.Keyboard.kc_buffer[Chipset.Keyboard.hi_b++] = key;
	}  
}

//
// for pasting key to keyboard (to be done later)
// 
BOOL WriteToKeyboard(LPBYTE lpData, DWORD dwSize)
{
/* LPBYTE lpAd = lpData;
DWORD  dwLen = dwSize;

	for ( ; dwLen > 0; dwLen--)
	{
		if (WaitForIdleState())
		{
			InfoMessage(_T("The emulator is busy."));
			return FALSE;
		}
		AsciiEventDown(*lpAd);
		bKeySlow = FALSE;
		Sleep(0);
		KeyboardEventUp(0x00);
		if (*lpAd == 0x0D)
		{
			if (lpAd[1] == 0x0A)
				lpAd++;
		}
		lpAd++;
	} */
	return TRUE;
}

//
// write to I/O space
//
BYTE Write_Keyboard(BYTE *a, WORD d, BYTE s)
{
	if (s != 1) {
		InfoMessage(_T("Keyboard write acces not in byte !!"));
		return BUS_ERROR;
	}
	switch (d) {
		case 0x8003:										// command
			if ((Chipset.Keyboard.status & 0x02) & (Chipset.Keyboard.stKeyb != 0)) {
				InfoMessage(_T("Command to Keyboard while busy !!"));
			} else {
				Chipset.Keyboard.stKeyb = 10;									// do a command;
				Chipset.Keyboard.command = *(--a);
				#if defined DEBUG_KEYBOARDH
					k = wsprintf(buffer,_T("%06X: KEYBOARD : write command = %02X\n"), Chipset.Cpu.PC, Chipset.Keyboard.command);
					OutputDebugString(buffer); buffer[0] = 0x00;
				#endif
			}
			return BUS_OK;
			break;
		case 0x8001:										// data from 68000 to 8041
			if (Chipset.Keyboard.status & 0x02) 
				InfoMessage(_T("Data to Keyboard while busy !!"));
			Chipset.Keyboard.datain = *(--a);
			Chipset.Keyboard.stdatain = TRUE;					// strobe :)
			Chipset.Keyboard.status |= 0x02;					// busy till take it
			Chipset.Keyboard.stKeyb = 1;						// go take it
			#if defined DEBUG_KEYBOARDH
				k = wsprintf(buffer,_T("%06X: KEYBOARD : write data = %02X\n"), Chipset.Cpu.PC, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			return BUS_OK;
			break;
		default: 
			return BUS_ERROR;
			break;
	}
	return BUS_OK;
}

//
// read in I/O space
//
BYTE Read_Keyboard(BYTE *a, WORD d, BYTE s)
{
	if (s != 1) {
		InfoMessage(_T("Keyboard read acces not in byte !!"));
		return BUS_ERROR;
	}
	switch (d) {
		case 0x8001:										// data from 8041 to 68000
			*(--a) = Chipset.Keyboard.dataout;									
			#if defined DEBUG_KEYBOARDH
				k = wsprintf(buffer,_T("%06X: KEYBOARD : read data = %02X (%03d) \n"), Chipset.Cpu.PC, Chipset.Keyboard.dataout, Chipset.Keyboard.dataout);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Keyboard.int68000 = 0;							// clear interrupt
			Chipset.Keyboard.status &= 0xFE;						// clear interrupt
			return BUS_OK;
			break;
		case 0x8003:										// status
			*(--a) = Chipset.Keyboard.status;
			Chipset.Keyboard.status_cycles = 0;					// read the status, keybord wait at least 1ms before doing something
			#if defined DEBUG_KEYBOARDH
				k = wsprintf(buffer,_T("%06X: KEYBOARD : read status = %02X\n"), Chipset.Cpu.PC, Chipset.Keyboard.status);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			return BUS_OK;
			break;
		default: 
			return BUS_ERROR;
			break;
	}
	return BUS_OK;
}

//
// init keyboard (copy the right keymap), set time if needed
//
VOID Init_Keyboard(VOID)
{ 
	WORD i,j;

	for (i = 0; i < 256; i++) {
		for (j = 0; j < 5; j++) {
			switch (Chipset.Keyboard.keymap) {
				default:	
					keycode[i][j] = frkeycode[i][j];
					break;
				case 1:		
					keycode[i][j] = uskeycode[i][j];
					break;
			}
		}
	}
	if (Chipset.keeptime) SetHPTime();
	#if defined DEBUG_KEYBOARDH
		k = wsprintf(buffer,_T("        : KEYBOARD : Init\n"));
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif
}

//
// reset keyboard
//
VOID Reset_Keyboard(VOID)
{ 
	BYTE map = Chipset.Keyboard.keymap;

	ZeroMemory((BYTE *) &Chipset.Keyboard, sizeof(KEYBOARD));
	Chipset.Keyboard.keymap = map;								// keep keymap

	Init_Keyboard();
	Chipset.Keyboard.stKeyb = 90;								// jump to reset state
	Chipset.Keyboard.hi_b = 0x00;								// empty buffer
	Chipset.Keyboard.lo_b = 0x00;
	// doMouse = 0;
	#if defined DEBUG_KEYBOARDH
		k = wsprintf(buffer,_T("        : KEYBOARD : Reset\n"));
		OutputDebugString(buffer); buffer[0] = 0x00;
	#endif
}

//
// keyboard timers in 2.457 MHz cycles
//
VOID Do_Keyboard_Timers(DWORD cycles)				// in 2.457MHz base clock
{ 
	Chipset.Keyboard.cycles += cycles;
	Chipset.Keyboard.status_cycles += cycles;
	if (Chipset.Keyboard.cycles >= 24570) {		// one centisecond elapsed
		Chipset.Keyboard.cycles -= 24570;

		if (Chipset.Keyboard.ram[0x07] != 0x00) {	// there is a key down ?
			if (Chipset.Keyboard.ram[0x22] != 0x00) {		// auto-repeat enabled ...
				Chipset.Keyboard.ram[0x21]++;					// auto-repeat timer
				if (Chipset.Keyboard.ram[0x21] == 0x00)	{			// time to repeat
					Chipset.Keyboard.ram[0x21] = Chipset.Keyboard.ram[0x22];		// reload
					Chipset.Keyboard.ram[0x04] |= 0x20;			
				}
			}
		}

		Chipset.Keyboard.ram[0x25]--;				// knob timer
		if (Chipset.Keyboard.ram[0x25] == 0x00) {
			if (Chipset.Keyboard.ram[0x1C] != 0x00)		// knob moved
				Chipset.Keyboard.ram[0x05] |= 0x10;			// time to knob
			Chipset.Keyboard.ram[0x25] = Chipset.Keyboard.ram[0x26];		// reload
		}

		Chipset.Keyboard.ram[0x05] |= 0x04;			// time to do a PSI at 10 msec

		Chipset.Keyboard.ram[0x2D]++;					// adjust clock
		if (Chipset.Keyboard.ram[0x2D] == 0) {
			if ((Chipset.keeptime) && (Chipset.Keyboard.ram[0x30] == 0) && (Chipset.Keyboard.ram[0x31] == 0))
				SetHPTime();		// set time after bootrom erased it at boot (256 centisecond elapsed) 
			Chipset.Keyboard.ram[0x2E]++;
			if ((Chipset.Keyboard.ram[0x2F] >= 0x83) && (Chipset.Keyboard.ram[0x2E] >= 0xD6)) {	// wrap on day
				Chipset.Keyboard.ram[0x2F] = 0x00;
				Chipset.Keyboard.ram[0x2E] = 0x00;
				Chipset.Keyboard.ram[0x30]++;		// one day more
				if (Chipset.Keyboard.ram[0x30] == 0)	// carry on
					Chipset.Keyboard.ram[0x31]++;
			} else {
				if (Chipset.Keyboard.ram[0x2E] == 0) {
					Chipset.Keyboard.ram[0x2F]++;
				}
			}
		}

		if (Chipset.Keyboard.ram[0x02] & 0x08) {			// non-maskable timer in use
			Chipset.Keyboard.ram[0x32]++;
			if (Chipset.Keyboard.ram[0x32] == 0x00) {
				Chipset.Keyboard.ram[0x33]++;
				if (Chipset.Keyboard.ram[0x33] == 0x00) {				// delay elapsed
					Chipset.Keyboard.ram[0x05] |= 0x80;						// time to do it
				}
			}
		}
	
		if (Chipset.Keyboard.ram[0x02] & 0x10) {			// cyclic timer in use
			Chipset.Keyboard.ram[0x3A]++;
			if (Chipset.Keyboard.ram[0x3A] == 0x00) {
				Chipset.Keyboard.ram[0x3B]++;
				if (Chipset.Keyboard.ram[0x3B] == 0x00) {
					Chipset.Keyboard.ram[0x3C]++;
					if (Chipset.Keyboard.ram[0x3C] == 0x00) {			// delay elapsed
						Chipset.Keyboard.ram[0x05] |= 0x08;						// time to do it
						if ((Chipset.Keyboard.ram[0x1B] & 0x1F) != 0x1F) {		// not satured, inc
							BYTE data = (Chipset.Keyboard.ram[0x1B] & 0x1F) + 1;

							Chipset.Keyboard.ram[0x1B] &= 0xE0;
							Chipset.Keyboard.ram[0x1B] |= data;
						}
						Chipset.Keyboard.ram[0x1B] |= 0x20;
						// reload
						Chipset.Keyboard.ram[0x3A] = Chipset.Keyboard.ram[0x3D];
						Chipset.Keyboard.ram[0x3B] = Chipset.Keyboard.ram[0x3E];
						Chipset.Keyboard.ram[0x3C] = Chipset.Keyboard.ram[0x3F];
					}
				}
			}
		}

		if (Chipset.Keyboard.ram[0x02] & 0x20) {			// delay timer in use
			Chipset.Keyboard.ram[0x37]++;
			if (Chipset.Keyboard.ram[0x37] == 0x00) {
				Chipset.Keyboard.ram[0x38]++;
				if (Chipset.Keyboard.ram[0x38] == 0x00) {
					Chipset.Keyboard.ram[0x39]++;
					if (Chipset.Keyboard.ram[0x39] == 0x00) {			// delay elapsed
						Chipset.Keyboard.ram[0x05] |= 0x08;						// time to do it
						Chipset.Keyboard.ram[0x1B] |= 0x40;			
					}
				}
			}
		}

		if (Chipset.Keyboard.ram[0x02] & 0x40) {			// match timer in use
			if (Chipset.Keyboard.ram[0x36] == Chipset.Keyboard.ram[0x2F]) {
				if (Chipset.Keyboard.ram[0x35] == Chipset.Keyboard.ram[0x2E]) {
					if (Chipset.Keyboard.ram[0x34] == Chipset.Keyboard.ram[0x2D]) {		// matched
						Chipset.Keyboard.ram[0x05] |= 0x08;						// time to do it
						Chipset.Keyboard.ram[0x1B] |= 0x80;						
					}
				}
			}
		}

		if (Chipset.Keyboard.ram[0x02] & 0x80) {			// beeper in use
		}
	}
}

//
// state machine for 8041 keyboard microcontroller
//
VOID Do_Keyboard(VOID)
{
	if (Chipset.Keyboard.shift)
		Chipset.Keyboard.ram[0x05] &= 0xFE;
	else 
		Chipset.Keyboard.ram[0x05] |= 0x01;
	if (Chipset.Keyboard.ctrl)
		Chipset.Keyboard.ram[0x05] &= 0xFD;
	else 
		Chipset.Keyboard.ram[0x05] |= 0x02;

	if (Chipset.Keyboard.ram[0x07] == 0x00)		// no key down
		Chipset.Keyboard.ram[0x21] = Chipset.Keyboard.ram[0x20];	// load key timer with auto-repeat delay
	if (Chipset.Keyboard.hi_b != Chipset.Keyboard.lo_b) {							// some key down ?
		Chipset.Keyboard.forceshift = Chipset.Keyboard.kc_buffer[Chipset.Keyboard.lo_b] >> 7;
		Chipset.Keyboard.ram[0x07] = Chipset.Keyboard.kc_buffer[Chipset.Keyboard.lo_b++] & 0x7F;
		Chipset.Keyboard.ram[0x04] |= 0x20;
	}

	switch (Chipset.Keyboard.stKeyb) {
		case 0:										// idle, wait something
			Chipset.Keyboard.status &= 0xFD;			// clear busy bit
			if (Chipset.Keyboard.status_cycles > 2457) {			// one ms elapsed after status polled ?
				if (Chipset.Keyboard.int68000 == 0)						// no interrupt already pending
					Chipset.Keyboard.stKeyb = 1;								// yes, can now do something
			}
			break;
		case 1:
			if (Chipset.Keyboard.stdatain) {										// datain strobe ?
				Chipset.Keyboard.stdatain = FALSE;										// clear strobe
				if (Chipset.Keyboard.stKeybrtn != 0) {
					Chipset.Keyboard.stKeyb = Chipset.Keyboard.stKeybrtn;														// jump back to waiting command
					Chipset.Keyboard.stKeybrtn = 0;											// forget return
				} else {
					Chipset.Keyboard.status &= 0xFD;										// clear busy in case
				}
			} else if ((Chipset.Keyboard.ram[0x04] & 0x20) &&						// some key to send
				       (!(Chipset.Keyboard.ram[0x04] & 0x01))) {						// not masked
					Chipset.Keyboard.stKeyb = 30;		
			} else if ((Chipset.Keyboard.ram[0x05] & 0x04) &&						// PSI to send
					   (!(Chipset.Keyboard.ram[0x04] & 0x08))) {						// not masked
				Chipset.Keyboard.stKeyb = 50;
			} else if ((Chipset.Keyboard.ram[0x05] & 0x08) &&						// some special timer interrupts to do
					   (!(Chipset.Keyboard.ram[0x04] & 0x04))) {						// not masked
				Chipset.Keyboard.stKeyb = 50;
			} else if ((Chipset.Keyboard.ram[0x05] & 0x10) &&						// knob data to send 
					   (!(Chipset.Keyboard.ram[0x04] & 0x01))) {						// not masked
				Chipset.Keyboard.stKeyb = 40;																// send it
			} else if ((Chipset.Keyboard.ram[0x05] & 0x80) &&						// non-maskable interrup to do 
					   (!(Chipset.Keyboard.ram[0x04] & 0x10))) {						// not masked
			    Chipset.Keyboard.stKeyb = 60;
			}
			break;
	
		case 10:									// got a command
			Chipset.Keyboard.status |= 0x02;			// set busy bit
			switch (Chipset.Keyboard.command) {
				case 0xAD:									// set time of day and date want 3 to 5 bytes
					Chipset.Keyboard.stKeyb = 0xAD0;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set time of day and date (3 or 5 bytes) \n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xAF:									// set date want 2 bytes
					Chipset.Keyboard.stKeyb = 0xAF0;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set date (2 bytes)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xB4:									// set real-time match want 3 bytes, cancel it if 0
					Chipset.Keyboard.stKeyb = 0xB40;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set real time match (3 bytes)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xB7:									// set delayed interrupt want 3 bytes, cancel it if 0
					Chipset.Keyboard.stKeyb = 0xB70;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set delayed interrupt (3 bytes)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xBA:									// set cyclic interrupt want 3 bytes, cancel if 0
					Chipset.Keyboard.stKeyb = 0xBA0;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set cyclic interrupt (3 bytes)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xB2:									// set up non-maskable timeout want 2 bytes, cancel if 0
					Chipset.Keyboard.stKeyb = 0xB20;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set non maskable timeout (2 bytes)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xA3:									// beep want 2 bytes
					Chipset.Keyboard.stKeyb = 0xA30;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Beep (2 bytes)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xA2:									// set auto-repeat rate want 1 byte
					Chipset.Keyboard.stKeyb = 0xA20;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set auto-repeat rate (1 byte)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xA0:									// set auto-repeat delay want 1 byte
					Chipset.Keyboard.stKeyb = 0xA00;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set auto-repeat delay (1 byte)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;
				case 0xA6:									// set knob pulse accumulating period want 1 byte
					Chipset.Keyboard.stKeyb = 0xA60;
					#if defined DEBUG_KEYBOARD
						k = wsprintf(buffer,_T("        : KEYBOARD : %03X Set knob pulse accumulating period (1 byte)\n"), Chipset.Keyboard.stKeyb);
						OutputDebugString(buffer); buffer[0] = 0x00;
					#endif
					break;

				default:
					if ((Chipset.Keyboard.command & 0xE0) == 0x40) {		// mask interrupts
						Chipset.Keyboard.intmask = Chipset.Keyboard.command & 0x01F;
						Chipset.Keyboard.ram[0x04] &= 0xE0;
						Chipset.Keyboard.ram[0x04] |= Chipset.Keyboard.intmask;
						Chipset.Keyboard.stKeyb = 0;
						#if defined DEBUG_KEYBOARD
							k = wsprintf(buffer,_T("        : KEYBOARD : %03X Mask interrupts : %02X\n"), Chipset.Keyboard.stKeyb, Chipset.Keyboard.intmask);
							OutputDebugString(buffer); buffer[0] = 0x00;
						#endif
					} 
					else if ((Chipset.Keyboard.command & 0xE0) == 0x00) {	// read data and send interrupt level 1 to 68000
						Chipset.Keyboard.dataout = Chipset.Keyboard.ram[Chipset.Keyboard.command & 0x01F];
						Chipset.Keyboard.stKeyb = 20;
						#if defined DEBUG_KEYBOARD
							k = wsprintf(buffer,_T("        : KEYBOARD : %03X read data (R%02X=%02X) and interrupt\n"), Chipset.Keyboard.stKeyb, Chipset.Keyboard.command & 0x01F, Chipset.Keyboard.dataout);
							OutputDebugString(buffer); buffer[0] = 0x00;
						#endif
					}
					else if ((Chipset.Keyboard.command & 0xE0) == 0x20) {	// copy data to 0x13 to 0x17
						Chipset.Keyboard.ram[0x17] = Chipset.Keyboard.ram[Chipset.Keyboard.command & 0x03F];
						Chipset.Keyboard.ram[0x16] = Chipset.Keyboard.ram[(Chipset.Keyboard.command & 0x03F)-1];
						Chipset.Keyboard.ram[0x15] = Chipset.Keyboard.ram[(Chipset.Keyboard.command & 0x03F)-2];
						Chipset.Keyboard.ram[0x14] = Chipset.Keyboard.ram[(Chipset.Keyboard.command & 0x03F)-3];
						Chipset.Keyboard.ram[0x13] = Chipset.Keyboard.ram[(Chipset.Keyboard.command & 0x03F)-4];
						Chipset.Keyboard.stKeyb = 0;
						#if defined DEBUG_KEYBOARD
							k = wsprintf(buffer,_T("        : KEYBOARD : %03X copy data from %02X-%02X to 0x13-0x17\n"), Chipset.Keyboard.stKeyb, (Chipset.Keyboard.command & 0x03F)-4, (Chipset.Keyboard.command & 0x03F));
							OutputDebugString(buffer); buffer[0] = 0x00;
						#endif
					}
					else {
						#if defined DEBUG_KEYBOARD
							k = wsprintf(buffer,_T("        : KEYBOARD : %03X unknown\n"), Chipset.Keyboard.stKeyb, Chipset.Keyboard.command);
							OutputDebugString(buffer); buffer[0] = 0x00;
						#endif
						Chipset.Keyboard.stKeyb = 0;			// unknown command
					}
					break;
			}
			break;
		case 20:									// send a level one interrupt to 68000
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : Send interrupt for data\n"));
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Keyboard.status &= 0x0E;
			Chipset.Keyboard.status |= 0x41;			// byte requested by 68000 with level one interrupt
			Chipset.Keyboard.int68000 = 1;
			Chipset.Keyboard.send_wait = 1;				// wait for read dataout before next action
			Chipset.Keyboard.stKeyb = 0;
			break;

		case 30:									// send a key
			if (Chipset.Keyboard.ram[0x07] != 0x00) {
				Chipset.Keyboard.status &= 0x0E;
				Chipset.Keyboard.status |= 0x81;			// some keycode with level one interrupt
				Chipset.Keyboard.status |= (Chipset.Keyboard.ctrl) ? 0x00 : 0x20;
				if (Chipset.Keyboard.forceshift) {			// change shift state
					Chipset.Keyboard.status |= (!Chipset.Keyboard.shift) ? 0x00 : 0x10;
				} else {
					Chipset.Keyboard.status |= (Chipset.Keyboard.shift) ? 0x00 : 0x10;
				}
				Chipset.Keyboard.dataout = Chipset.Keyboard.ram[0x07];
				Chipset.Keyboard.int68000 = 1;
				Chipset.Keyboard.send_wait = 1;				// wait for read dataout before next action
				#if defined DEBUG_KEYBOARDH
				k = wsprintf(buffer,_T("        : KEYBOARD : Send interrupt for key : %02X,%02X\n"), Chipset.Keyboard.status, Chipset.Keyboard.dataout);
					OutputDebugString(buffer); buffer[0] = 0x00;
				#endif
			}
			Chipset.Keyboard.ram[0x04] &= 0xDF;			// clear auto-repeat interrupt to do
			Chipset.Keyboard.stKeyb = 0;
			break;

		case 40:									// send a knob
			Chipset.Keyboard.status &= 0x0E;
			Chipset.Keyboard.status |= 0xC1;			// some knob with level one interrupt
			Chipset.Keyboard.status |= (Chipset.Keyboard.ctrl) ? 0x00 : 0x20;
			Chipset.Keyboard.status |= (Chipset.Keyboard.shift) ? 0x00 : 0x10;
			Chipset.Keyboard.dataout = Chipset.Keyboard.ram[0x1C];
			Chipset.Keyboard.ram[0x1C] = 0x00;			// clear knob count
			Chipset.Keyboard.int68000 = 1;
			Chipset.Keyboard.ram[0x05] &= 0xEF;			// clear knob interrupt to do 
			Chipset.Keyboard.send_wait = 1;				// wait for read dataout before next action
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDH
				k = wsprintf(buffer,_T("        : KEYBOARD : Send interrupt for knob : %02X,%02X\n"), Chipset.Keyboard.status, Chipset.Keyboard.dataout);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 50:									// send PSI and/or special timer
			Chipset.Keyboard.status &= 0x0E;			// clear PSI and special
			if ((Chipset.Keyboard.ram[0x05] & 0x04) && (!(Chipset.Keyboard.ram[0x04] & 0x08))) {
				#if defined DEBUG_KEYBOARD
					k = wsprintf(buffer,_T("        : KEYBOARD : Send interrupt for PSI\n"));
					OutputDebugString(buffer); buffer[0] = 0x00;
				#endif
				Chipset.Keyboard.ram[0x05] &= 0xFB;								// clear it
				Chipset.Keyboard.status |= 0x11;			// PSI with level one interrupt
			}
			if ((Chipset.Keyboard.ram[0x05] & 0x08) && (!(Chipset.Keyboard.ram[0x04] & 0x04))) {
				#if defined DEBUG_KEYBOARD
					k = wsprintf(buffer,_T("        : KEYBOARD : Send interrupt for special timer\n"));
					OutputDebugString(buffer); buffer[0] = 0x00;
				#endif
				Chipset.Keyboard.ram[0x05] &= 0xF7;								// clear it
				Chipset.Keyboard.status |= 0x21;			// special timer with level one interrupt
				Chipset.Keyboard.dataout = Chipset.Keyboard.ram[0x1B];
				Chipset.Keyboard.ram[0x1B] = 0x00;
			}
			Chipset.Keyboard.int68000 = 1;
			Chipset.Keyboard.ram[0x05] &= 0xEF;			// clear knob interrupt to do
			Chipset.Keyboard.send_wait = 1;				// wait for read dataout before next action
			Chipset.Keyboard.stKeyb = 0;
			break;

		case 60:									// send nmi at level seven interrupt to 68000
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : Send interrupt for nmi\n"));
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Keyboard.status &= 0x00;
			Chipset.Keyboard.status |= 0x04;			// level seven interrupt, as timeout, not reset key
			Chipset.Keyboard.int68000 = 7;
			Chipset.Keyboard.ram[0x05] &= 0x7F;			// clear it
			Chipset.Keyboard.stKeyb = 0;
			break;

		case 90:									// reset keyboard
			Chipset.Keyboard.cycles = 0;
			Chipset.Keyboard.ram[0x02] = 0x00;			// nothing counting
			Chipset.Keyboard.ram[0x04] = 0x1F;			// all masked
			Chipset.Keyboard.ram[0x11] = 0;				// US keyboard
			Chipset.Keyboard.stKeyb = 91;
			break;
		case 91:
			Chipset.Keyboard.cycles += Chipset.dcycles;
			if (Chipset.Keyboard.cycles > 40) Chipset.Keyboard.stKeyb = 92;			// 5 µs elapsed
			break;
		case 92:									// power-up (after 20 µsec)
			Chipset.Keyboard.status = 0x71;			// power-up and self test ok
			Chipset.Keyboard.dataout = 0x8E;
			Chipset.Keyboard.int68000 = 1;
			Chipset.Keyboard.stKeyb = 93;
			break;
		case 93:
			Chipset.Keyboard.cycles += Chipset.dcycles;
			if (Chipset.Keyboard.cycles > 200) Chipset.Keyboard.stKeyb = 94;			// 20 µs elapsed
			break;
		case 94:
			Chipset.Keyboard.int68000 = 0;
			Chipset.Keyboard.stKeyb = 0;
			break;

		case 0xA00:									// set auto-repeat delay want 1 byte
			Chipset.Keyboard.stKeybrtn = 0xA01;
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xA01:										// got a byte
			Chipset.Keyboard.ram[0x20] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xA20:									// set auto-repeat rate want 1 byte
			Chipset.Keyboard.stKeybrtn = 0xA21;
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xA21:										// got a byte
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Keyboard.ram[0x22] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeyb = 0;
			break;


		case 0xA30:									// beep want 2 bytes
			Chipset.Keyboard.stKeybrtn = 0xA31;
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xA31:										// got a byte
			Chipset.Keyboard.stKeybrtn = 0xA32;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			Chipset.Keyboard.ram[0x23] = Chipset.Keyboard.datain;
			break;
		case 0xA32:										// got a byte
			Chipset.Keyboard.ram[0x24] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xA60:									// set knob pulse accumulating period want 1 byte
			Chipset.Keyboard.stKeybrtn = 0xA61;
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xA61:										// got a byte
			Chipset.Keyboard.ram[0x26] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xBA0:									// set cyclic interrupt want 3 bytes, cancel if 0
			Chipset.Keyboard.stKeybrtn = 0xBA1;
			Chipset.Keyboard.ram[0x02] &= 0xEF;			// clear cyclic interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xBA1:										// got a byte
			Chipset.Keyboard.ram[0x3A] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x3D] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xBA2;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xBA2:										// got a byte
			Chipset.Keyboard.ram[0x3B] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x3E] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xBA3;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xBA3:										// got a byte
			Chipset.Keyboard.ram[0x3C] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x3F] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x02] |= 0x10;			// set cyclic interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xB70:									// set delayed interrupt want 3 bytes, cancel it if 0
			Chipset.Keyboard.stKeybrtn = 0xB71;
			Chipset.Keyboard.ram[0x02] &= 0xDF;			// clear delay interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xB71:										// got a byte
			Chipset.Keyboard.ram[0x37] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xB72;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xB72:										// got a byte
			Chipset.Keyboard.ram[0x38] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xB73;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xB73:										// got a byte
			Chipset.Keyboard.ram[0x39] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x02] |= 0x20;			// set delay interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARD
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xB40:									// set real-time match want 3 bytes, cancel it if 0
			Chipset.Keyboard.stKeybrtn = 0xB41;
			Chipset.Keyboard.ram[0x02] &= 0xBF;			// clear match interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xB41:										// got a byte
			Chipset.Keyboard.ram[0x34] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xB42;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xB42:										// got a byte
			Chipset.Keyboard.ram[0x35] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xB43;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xB43:										// got a byte
			Chipset.Keyboard.ram[0x36] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x02] |= 0x40;			// set match interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xB20:									// set up non-maskable timeout want 2 bytes, cancel if 0
			Chipset.Keyboard.stKeybrtn = 0xB21;
			Chipset.Keyboard.ram[0x02] &= 0xF7;			// clear nmi interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xB21:										// got a byte
			Chipset.Keyboard.ram[0x32] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xB22;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xB22:										// got a byte
			Chipset.Keyboard.ram[0x33] = Chipset.Keyboard.datain;
			Chipset.Keyboard.ram[0x02] |= 0x08;			// set nmi interrupt in use
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xAF0:									// set date want 2 bytes
			Chipset.Keyboard.stKeybrtn = 0xAF1;
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xAF1:										// got a byte
			Chipset.Keyboard.ram[0x30] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xAF2;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xAF2:										// got a byte
			Chipset.Keyboard.ram[0x31] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xAD0:									// set time of day and date want 3 to 5 bytes
			Chipset.Keyboard.stKeybrtn = 0xAD1;
			Chipset.Keyboard.stKeyb = 0;
			break;
		case 0xAD1:										// got a byte
			Chipset.Keyboard.ram[0x2D] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xAD2;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xAD2:										// got a byte
			Chipset.Keyboard.ram[0x2E] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xAD3;
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;
		case 0xAD3:										// got a byte
			Chipset.Keyboard.ram[0x2F] = Chipset.Keyboard.datain;
			Chipset.Keyboard.stKeybrtn = 0xAF1;								// loop to date
			Chipset.Keyboard.stKeyb = 0;
			#if defined DEBUG_KEYBOARDC
				k = wsprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), Chipset.Keyboard.stKeybrtn, Chipset.Keyboard.datain);
				OutputDebugString(buffer); buffer[0] = 0x00;
			#endif
			break;

		case 0xBB0:										// reset state
			break;

		default:
			break;
	}
}