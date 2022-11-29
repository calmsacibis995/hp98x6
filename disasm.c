/*
 *   Disasm.c
 * 
 *   Copyright 2010-2011 Olivier De Smet 
 */

//
//   68000 CPU disassembler
//

#include "StdAfx.h"
#include "HP98x6.h"
#include "mops.h"

#define TAB_SKIP	8

static LPCTSTR hex = _T("0123456789ABCDEF");

//
// size extension for opcodes
//
static LPCTSTR size_T[] =
{
	_T(""), _T(".B"), _T(".W"), _T(".3"), _T(".L")
};

//
// conditions codes for B opcodes
//
static LPCTSTR condcode_T[] =
{
	_T("T"), _T("F"), _T("HI"), _T("LS"), _T("CC"), _T("CS"), _T("NE"), _T("EQ"),
	_T("VC"), _T("VS"), _T("PL"), _T("MI"), _T("GE"), _T("LT"), _T("GT"), _T("LE")
};

//
// condiciton codes for DB opcodes
//
static LPCTSTR condcodeDB_T[] =
{
	_T("T"), _T("RA"), _T("HI"), _T("LS"), _T("CC"), _T("CS"), _T("NE"), _T("EQ"),
	_T("VC"), _T("VS"), _T("PL"), _T("MI"), _T("GE"), _T("LT"), _T("GT"), _T("LE")
};

//
// text of basic opcode
//
static LPCTSTR opcode_T[] =
{
	_T("ABCD"), _T("ADD"), _T("ADDA"), _T("ADDI"), _T("ADDQ"), _T("ADDX"), _T("AND"), _T("ANDI"),
	_T("ASL"), _T("ASR"), _T("B"), _T("BCHG"), _T("BCLR"), _T("BRA"), _T("BSET"), _T("BSR"),   
	_T("BTST"), _T("CHK"), _T("CLR"), _T("CMP"), _T("CMPA"), _T("CMPI"), _T("CMPM"), _T("DB"),
	_T("DIVS"), _T("DIVU"), _T("EOR"), _T("EORI"), _T("EXG"), _T("EXT"), _T("ILLEGAL"), _T("JMP"),
	_T("JSR"), _T("LEA"), _T("LINK"), _T("LSL"), _T("LSR"), _T("MOVE"), _T("MOVEA"), _T("MOVEM"),
	_T("MOVEP"), _T("MOVEQ"), _T("MULS"), _T("MULU"), _T("NBCD"), _T("NEG"), _T("NEGX"), _T("NOP"),
	_T("NOT"), _T("OR"), _T("ORI"), _T("PEA"), _T("RESET"), _T("ROL"), _T("ROR"), _T("ROXL"),
	_T("ROXR"), _T("RTE"), _T("RTR"), _T("RTS"), _T("SBCD"), _T("S"), _T("STOP"), _T("SUB"),
	_T("SUBA"), _T("SUBI"), _T("SUBQ"), _T("SUBX"), _T("SWAP"), _T("TAS"), _T("TRAP"), _T("TRAPV"),
	_T("TST"), _T("UNLK"), _T("LINEA"), _T("LINEF"),
	_T("MOVEC"), _T("MOVES")
};

//
// enum of basic opcodes
//
typedef enum {
	O_ABCD, O_ADD, O_ADDA, O_ADDI, O_ADDQ, O_ADDX, O_AND, O_ANDI,
	O_ASL, O_ASR, O_B, O_BCHG, O_BCLR, O_BRA, O_BSET, O_BSR,   
	O_BTST, O_CHK, O_CLR, O_CMP, O_CMPA, O_CMPI, O_CMPM, O_DB,
	O_DIVS, O_DIVU, O_EOR, O_EORI, O_EXG, O_EXT, O_ILLEGAL, O_JMP,
	O_JSR, O_LEA, O_LINK, O_LSL, O_LSR, O_MOVE, O_MOVEA, O_MOVEM,
	O_MOVEP, O_MOVEQ, O_MULS, O_MULU, O_NBCD, O_NEG, O_NEGX, O_NOP,
	O_NOT, O_OR, O_ORI, O_PEA, O_RESET, O_ROL, O_ROR, O_ROXL,
	O_ROXR, O_RTE, O_RTR, O_RTS, O_SBCD, O_S, O_STOP, O_SUB,
	O_SUBA, O_SUBI, O_SUBQ, O_SUBX, O_SWAP, O_TAS, O_TRAP, O_TRAPV,
	O_TST, O_UNLK, O_LINEA, O_LINEF,
	O_MOVEC, O_MOVES
} O_CODE;

//
// union to decode instructions (not used, to do later)
//
typedef union {
	struct {
		BYTE b0: 1; BYTE b1: 1; BYTE b2: 1; BYTE b3: 1;
		BYTE b4: 1;	BYTE b5: 1; BYTE b6: 1;	BYTE b7: 1;
		BYTE b8: 1;	BYTE b9: 1; BYTE b10: 1; BYTE b11: 1;
		BYTE b12: 1; BYTE b13: 1; BYTE b14: 1; BYTE b15: 1;
	};
	struct {
		BYTE b0_1: 2; BYTE b2_3: 2; BYTE b4_5: 2; BYTE b6_7: 2;
		BYTE b8_9: 2; BYTE b10_11: 2; BYTE b12_13: 2; BYTE b14_15: 2;
	};
	struct {
		BYTE b0_2: 3; BYTE b3_5: 3; BYTE b6_8: 3;
		BYTE b9_11: 3; BYTE b12_14: 3; BYTE : 1;
	};
	struct {
		BYTE b0_7: 8; BYTE b8_15: 8;
	};
	struct {
		BYTE : 7;
		BYTE b7_8: 2;
		BYTE b9_10: 2;
		BYTE : 5;
	};
	WORD b0_15;
} OPb;

//
// struct for opcode disassembly
//
typedef struct 
{
	EA eas;			// EA source
	EA ead;			// EA destination
	BOOL easd;		// EA source is also EA destination 
	BYTE vector;	// exception vector if needed
	O_CODE op;		// OP 
} OPDIS;

typedef struct
{
	DWORD	addr;
	LPCTSTR	label;
} LABEL;

//
// some boot rom 3.0 labels
//
static LABEL Labels[] = 
{
	{0x00428000, _T("KB_STATCOM")},
	{0x00428002, _T("KB_DATA")},

	{0x00445000, _T("IF_XSTATCOM")},
	{0x00445400, _T("IF_CLXSTATUS")},
	{0x0044c000, _T("IF_STATCOM")},
	{0x0044c002, _T("IF_TRACK")},
	{0x0044c004, _T("IF_SECTOR")},
	{0x0044c006, _T("IF_DATA")},
	{0x0044e000, _T("IF_OBRAM")},

	{0x00510001, _T("CRT_RSEL")},
	{0x00510003, _T("CRT_RDAT")},
	{0x00512000, _T("CRTMB")},
	{0x0051fffe, _T("CRTID")},

	{0xfffffdce, _T("LOWRAM")},
	{0xfffffdd2, _T("FUBUFFER")},
	{0xfffffed2, _T("SYSFLAG")},
	{0xfffffed3, _T("DRIVE")},
	{0xfffffed4, _T("F_AREA")},
	{0xfffffed8, _T("NDRIVES")},
	{0xfffffed9, _T("RETRY")},
	{0xfffffeda, _T("SYSFLAG2")},
	{0xfffffedb, _T("DRVR_KEY")},
	{0xfffffedc, _T("DEFAULT_MSUS")},

	{0xfffffee0, _T("jmp_Pseudo_9")},
	{0xfffffee6, _T("jmp_Pseudo_8")},
	{0xfffffeec, _T("jmp_Vectored_7")},
	{0xfffffef2, _T("jmp_Vectored_6")},
	{0xfffffef8, _T("jmp_Vectored_5")},
	{0xfffffefe, _T("jmp_Vectored_4")},
	{0xffffff04, _T("jmp_Vectored_3")},
	{0xffffff0a, _T("jmp_Vectored_2")},
	{0xffffff10, _T("jmp_Vectored_1")},
	{0xffffff16, _T("jmp_Vectored_0")},
	{0xffffff1c, _T("jmp_Spurious")},

	{0xffffff22, _T("jmp_NMI_4")},
	{0xffffff28, _T("jmp_NMI_3")},
	{0xffffff2e, _T("jmp_NMI_2")},
	{0xffffff34, _T("jmp_NMI_1")},

	{0xffffff3a, _T("jmp_TRAP_15")},
	{0xffffff40, _T("jmp_TRAP_14")},
	{0xffffff46, _T("jmp_TRAP_13")},
	{0xffffff4c, _T("jmp_TRAP_12")},
	{0xffffff52, _T("jmp_TRAP_11")},
	{0xffffff58, _T("jmp_TRAP_10")},
	{0xffffff5e, _T("jmp_TRAP_9")},
	{0xffffff64, _T("jmp_TRAP_8")},
	{0xffffff6a, _T("jmp_TRAP_7")},
	{0xffffff70, _T("jmp_TRAP_6")},
	{0xffffff76, _T("jmp_TRAP_5")},
	{0xffffff7c, _T("jmp_TRAP_4")},
	{0xffffff82, _T("jmp_TRAP_3")},
	{0xffffff88, _T("jmp_TRAP_2")},
	{0xffffff8e, _T("jmp_TRAP_1")},
	{0xffffff94, _T("jmp_TRAP_0")},

	{0xffffff9a, _T("jmp_IPL_7")},
	{0xffffffa0, _T("jmp_IPL_6")},
	{0xffffffa6, _T("jmp_IPL_5")},
	{0xffffffac, _T("jmp_IPL_4")},
	{0xffffffb2, _T("jmp_IPL_3")},
	{0xffffffb8, _T("jmp_IPL_2")},
	{0xffffffbe, _T("jmp_IPL_1")},

	{0xffffffc4, _T("jmp_Line_1111")},
	{0xffffffca, _T("jmp_Line_1010")},
	{0xffffffd0, _T("jmp_Trace_trap")},
	{0xffffffd6, _T("jmp_Privilege")},
	{0xffffffdc, _T("jmp_Trapv")},
	{0xffffffe2, _T("jmp_Chk")},
	{0xffffffe8, _T("jmp_Divide0")},
	{0xffffffee, _T("jmp_Illegal")},
	{0xfffffff4, _T("jmp_AddressE")},
	{0xfffffffa, _T("jmp_BusE")}
};

static const int nLabels = sizeof(Labels)/sizeof(LABEL);

//################
//#
//#    Low level subroutines
//#
//################

//
// find a label LONG
//
static int find_labell (DWORD addr)
{
	WORD i,j;

	j = -1;
	for (i = 0; i < nLabels; i++) {
		if (Labels[i].addr == addr) j = i;
	}
	return j;
}
//
// find a label WORD
//
static int find_labelw (WORD addrw)
{
	WORD i,j;
	DWORD addr;

	addr = (DWORD) addrw;
	if (addrw & 0x8000) addr += 0xFFFF0000;
	j = -1;
	for (i = 0; i < nLabels; i++) {
		if (Labels[i].addr == addr) j = i;
	}
	return j;
}
//
// Read a word from RAM and ROM only
//
static int read_word (DWORD pwAddr, int n)
{
	WORD data;

	data = GetWORD(pwAddr);
	return data;
}

static LPTSTR append_str (LPTSTR szBuf, LPCTSTR szStr)
{
	while ((*szBuf = *szStr++))
		szBuf++;
	return szBuf;
}

static LPTSTR append_tab (LPTSTR szBuf)
{
	int n;
	LPTSTR p;
	
	n = TAB_SKIP - (lstrlen (szBuf) % TAB_SKIP);
	p = &szBuf[lstrlen (szBuf)];
	while (n--)
		*p++ = _T(' ');
	*p = 0;
	return p;
}

static LPTSTR append_word (LPTSTR szBuf, WORD data)
{
	*szBuf++ = _T('$');
	*szBuf++ = hex[data >> 12];
	*szBuf++ = hex[(data & 0x0F00) >> 8];
	*szBuf++ = hex[(data & 0x00F0) >> 4];
	*szBuf++ = hex[data & 0x000F];
	*szBuf = 0; 
	return szBuf;
}
static LPTSTR append_daword (LPTSTR szBuf, WORD data)
{
	*szBuf++ = hex[data >> 12];
	*szBuf++ = hex[(data & 0x0F00) >> 8];
	*szBuf++ = hex[(data & 0x00F0) >> 4];
	*szBuf++ = hex[data & 0x000F];
	*szBuf = 0; 
	return szBuf;
}

static LPTSTR append_sword (LPTSTR szBuf, WORD data)
{
	if (data >= 0x8000) {
		*szBuf++ = _T('-');
		data = -data;
	}
	*szBuf++ = _T('$');
	if (data > 0x0FFF) {
		*szBuf++ = hex[data >> 12];
		*szBuf++ = hex[(data & 0x0F00) >> 8];
		*szBuf++ = hex[(data & 0x00F0) >> 4];
		*szBuf++ = hex[data & 0x000F];
	} else if (data > 0x00FF) {
		*szBuf++ = hex[(data & 0x0F00) >> 8];
		*szBuf++ = hex[(data & 0x00F0) >> 4];
		*szBuf++ = hex[data & 0x000F];
 	} else if (data > 0x000F) {
		*szBuf++ = hex[(data & 0x00F0) >> 4];
		*szBuf++ = hex[data & 0x000F];
	} else *szBuf++ = hex[data & 0x000F];
	*szBuf = 0; 
	return szBuf;
}

static LPTSTR append_byte (LPTSTR szBuf, BYTE data)
{
	*szBuf++ = _T('$');
	*szBuf++ = hex[data >> 4];
	*szBuf++ = hex[data & 0x0F];
	*szBuf = 0; 
	return szBuf;
}

static LPTSTR append_sbyte (LPTSTR szBuf, BYTE data)
{
	if (data >= 0x80) {
		*szBuf++ = _T('-');
		data = -data;
	}
	*szBuf++ = _T('$');
	if (data > 0x0F) *szBuf++ = hex[data >> 4];
	*szBuf++ = hex[data & 0x0F];
	*szBuf = 0; 
	return szBuf;
}

static LPTSTR append_dword (LPTSTR szBuf, DWORD data)
{
	*szBuf++ = _T('$');
	*szBuf++ = hex[data >> 28];
	*szBuf++ = hex[(data & 0x0F000000) >> 24];
	*szBuf++ = hex[(data & 0x00F00000) >> 20];
	*szBuf++ = hex[(data & 0x000F0000) >> 16];
	*szBuf++ = hex[(data & 0x0000F000) >> 12];
	*szBuf++ = hex[(data & 0x00000F00) >>  8];
	*szBuf++ = hex[(data & 0x000000F0) >>  4];
	*szBuf++ = hex[data  & 0x0000000F];
	*szBuf = 0; 
	return szBuf;
}
static LPTSTR append_pc (LPTSTR szBuf, DWORD data)
{
	*szBuf++ = _T('$');
	*szBuf++ = hex[(data & 0x00F00000) >> 20];
	*szBuf++ = hex[(data & 0x000F0000) >> 16];
	*szBuf++ = hex[(data & 0x0000F000) >> 12];
	*szBuf++ = hex[(data & 0x00000F00) >>  8];
	*szBuf++ = hex[(data & 0x000000F0) >>  4];
	*szBuf++ = hex[data  & 0x0000000F];
	*szBuf = 0; 
	return szBuf;
}
static LPTSTR append_ea (LPTSTR szCode, EA *ea, DWORD *addr, BOOL view)
{
	WORD offset;
	DWORD doffset;
	EXTW extw;
	BYTE i,j,l;
	WORD lab;

	switch (ea->imode) {
		case EA_NONE:
			break;
		case EA_CST:
			*szCode++ = _T('#');
			szCode = append_sbyte(szCode, ea->calc);
			break;
		case EA_D:												// Dn
			*szCode++ = _T('D'); *szCode++ = hex[ea->reg];
			break;
		case EA_A:												// An
			*szCode++ = _T('A'); *szCode++ = hex[ea->reg];
			break;
		case EA_IA:												// (An)
			*szCode++ = _T('(');
			*szCode++ = _T('A'); *szCode++ = hex[ea->reg];
			*szCode++ = _T(')');
			break;
		case EA_IAI:											// (An)+
			*szCode++ = _T('(');
			*szCode++ = _T('A'); *szCode++ = hex[ea->reg];
			*szCode++ = _T(')'); *szCode++ = _T('+');
			break;
		case EA_IAD:											// -(An)
			*szCode++ = _T('-'); *szCode++ = _T('(');
			*szCode++ = _T('A'); *szCode++ = hex[ea->reg];
			*szCode++ = _T(')');
			break;
		case EA_IDA:											// (d16,An)
			offset = GetWORD(*addr);
			*addr += 2;
			*szCode++ = _T('(');
			szCode = append_sword(szCode, offset);
			*szCode++ = _T(','); *szCode++ = _T('A'); 
			*szCode++ = hex[ea->reg]; *szCode++ = _T(')');
			break;
		case EA_IDAX:											// (d8,An,Xn.s)
			extw.w = GetWORD(*addr);
			*addr += 2;
			*szCode++ = _T('(');
			szCode = append_sbyte(szCode, extw.d8);
			*szCode++ = _T(','); *szCode++ = _T('A'); 
			*szCode++ = hex[ea->reg]; *szCode++ = _T(',');
			*szCode++ = (extw.da) ? _T('A') : _T('D'); 
			*szCode++ = hex[extw.reg]; *szCode++ = _T('.');
			*szCode++ = (extw.wl) ? _T('L') : _T('W');
			*szCode++ = _T(')');
			break;		
		case EA_IXW:											// xxxx.W
			offset = GetWORD(*addr);
			*addr += 2;
			lab = find_labelw(offset);
			if (lab != 0xFFFF) 
				szCode = append_str(szCode, Labels[lab].label);
			else szCode = append_word(szCode, offset);
			szCode = append_str(szCode, _T(".W"));
			break;
		case EA_IXL:											// xxxxxxxx.L
			offset = GetWORD(*addr);
			*addr += 2;
			doffset = (offset << 16) | GetWORD(*addr); 
			*addr += 2;
			lab = find_labell(doffset);
			if (lab != 0xFFFF) 
				szCode = append_str(szCode, Labels[lab].label);
			else szCode = append_pc(szCode, doffset);
			szCode = append_str(szCode, _T(".L"));
			break;
		case EA_IDP:											// (d16,PC)
			doffset = *addr;
			offset = GetWORD(*addr);
			*addr += 2;
			*szCode++ = _T('(');
			szCode = append_sword(szCode, offset);
			szCode = append_str(szCode, _T(",PC[")); 
			doffset += (offset > 0x7FFF) ? (0xFFFF0000 | offset) : offset;
			szCode = append_pc(szCode, doffset);
			*szCode++ = _T(']');*szCode++ = _T(')');
			break;
		case EA_IDPX:											// (d8,PC,Xn.s)
			doffset = *addr;
			extw.w = GetWORD(*addr);
			*addr += 2;
			*szCode++ = _T('(');
			szCode = append_sbyte(szCode, extw.d8);
			*szCode++ = _T(','); *szCode++ = _T('P'); 
			*szCode++ = _T('C'); *szCode++ = _T(',');
			*szCode++ = (extw.da) ? _T('A') : _T('D'); 
			*szCode++ = hex[extw.reg]; *szCode++ = _T('.');
			*szCode++ = (extw.wl) ? _T('L') : _T('W');
			 *szCode++ = _T(')');
			break;
		case EA_IMM:											// #yyyy / #zzzzzzzz
			*szCode++ = _T('#');
			offset = GetWORD(*addr);
			*addr += 2;
			if (ea->isize == 4) {
				doffset = (offset << 16) | GetWORD(*addr); 
				*addr += 2;
				lab = find_labell(doffset);
				if (lab != 0xFFFF) 
					szCode = append_str(szCode, Labels[lab].label);
				else szCode = append_dword(szCode, doffset);
			} else if (ea->isize == 2) {
				lab = find_labelw(offset);
				if (lab != 0xFFFF) 
					szCode = append_str(szCode, Labels[lab].label);
				else szCode = append_sword(szCode, offset);
			} else {
				szCode = append_byte(szCode, offset & 0x00FF);
			}
			break;
		case EA_CCR:
			*szCode++ = _T('C'); *szCode++ = _T('C'); *szCode++ = _T('R'); 
			break;
		case EA_SR:
			*szCode++ = _T('S'); *szCode++ = _T('R');
			break;
		case EA_USP:
			*szCode++ = _T('U'); *szCode++ = _T('S'); *szCode++ = _T('P'); 
			break;
		case EA_MOVEM:											// reg list in ea.data.w, order in ea.calc
			offset = ea->data.w[0];
			if (ea->calc) {						// reverse order D0,D1,D2,D3,D4,D5,D6,D7,A0,A1,A2,A3,A4,A5,A6,A7
				l = 0xFE;
				j = 0xFE;
				for (i=0; i < 8; i++) {
					if (offset & 0x8000) {
						if (i == (j+1)) j = i;
						else {
							if (l != 0xFE) *szCode++ =_T('/');
							l = i;
							j = i;
							*szCode++ =_T('D');
							*szCode++ = hex[i];
						}
					} else {
						if (l != 0xFE) {
							if (((l+1) != i) && (j != 0xFE)) {
								*szCode++ = _T('-');
								*szCode++ = hex[j];
							}
							j = 0xFE;
						}
					}
					offset = offset << 1;
				}
				if ((j == 7) && (l < 7)) {
					*szCode++ =_T('-');
					*szCode++ = hex[j];
				}
				if (l != 0xFE) l = 5;
				j = 0xFE;
				for (i=8; i < 16; i++) {
					if (offset & 0x8000) {
						if (i == (j+1)) j = i;
						else {
							if (l != 0xFE) *szCode++ =_T('/');
							l = i;
							j = i;
							*szCode++ =_T('A');
							*szCode++ = hex[i & 7];
						}
					} else {
						if (l != 0xFE) {
							if (((l+1) != i) && (j != 0xFE)) {
								*szCode++ = _T('-');
								*szCode++ = hex[j & 7];
							}
							j = 0xFE;
						}
					}
					offset = offset << 1;
				}
				if ((j == 15) && (l < 15)) {
					*szCode++ =_T('-');
					*szCode++ = hex[j & 7];
				}
			} else {							// normal order  A7,A6,A5,A4,A3,A2,A1,A0,D7,D6,D5,D4,D3,D2,D1,D0
				l = 0xFE;
				j = 0xFE;
				for (i=0; i < 8; i++) {
					if (offset & 0x0001) {
						if (i == (j+1)) j = i;
						else {
							if (l != 0xFE) *szCode++ =_T('/');
							l = i;
							j = i;
							*szCode++ = _T('D');
							*szCode++ = hex[i];
						}
					} else {
						if (l != 0xFE) {
							if (((l+1) != i) && (j != 0xFE)) {
								*szCode++ =_T('-');
								*szCode++ = hex[j];
							}
							j = 0xFE;
						}
					}
					offset = offset >> 1;
				}
				if ((j == 7) && (l < 7)) {
					*szCode++ =_T('-');
					*szCode++ = hex[j & 7];
				}
				if (l != 0xFE) l = 5;
				j = 0xFE;
				for (i=8; i < 16; i++) {
					if (offset & 0x0001) {
						if (i == (j+1)) j = i;
						else {
							if (l != 0xFE) *szCode++ =_T('/');
							l = i;
							j = i;
							*szCode++ = _T('A');
							*szCode++ = hex[i & 7];
						}
					} else {
						if (l != 0xFE) {
							if (((l+1) != i) && (j != 0xFE)) {
								*szCode++ =_T('-');
								*szCode++ = hex[j & 7];
							}
							j = 0xFE;
						}
					}
					offset = offset >> 1;
				}
				if ((j == 15) && (l < 15)) {
					*szCode++ =_T('-');
					*szCode++ = hex[j & 7];
				}
			}
			break;
		case EA_BCC:											// Bcc xxxx		
			offset = (ea->calc > 0x7F) ? (0xFF00 | ea->calc) : ea->calc;
			doffset = *addr;
			if (offset == 0x0000) {
				offset = GetWORD(*addr);
				*addr += 2;
			}
//			szCode = append_sword(szCode, offset);
			doffset += (offset > 0x7FFF) ? (0xFFFF0000 | offset) : offset;
//			*szCode++ = _T(' '); *szCode++ = _T('(');
			szCode = append_pc(szCode, doffset);
//			*szCode++ = _T(')');
			break;
		case EA_DBCC:											// DBcc Dn,xxxx		
			offset = GetWORD(*addr);
//			szCode = append_sword(szCode, offset);
			doffset = (offset > 0x7FFF) ? (0xFFFF0000 | offset) : offset;
//			*szCode++ = _T(' '); *szCode++ = _T('(');
			szCode = append_pc(szCode, *addr + doffset);
//			*szCode++ = _T(')');
			*addr += 2;
			break;
		case EA_RC:												// Control register
			switch (ea->data.w[0]) {
				case 0x000:		
					*szCode++ = _T('S'); *szCode++ = _T('F'); *szCode++ = _T('C');
					break;
				case 0x001:		
					*szCode++ = _T('D'); *szCode++ = _T('F'); *szCode++ = _T('C');
					break;
				case 0x800:		
					*szCode++ = _T('U'); *szCode++ = _T('S'); *szCode++ = _T('P');
					break;
				case 0x801:		
					*szCode++ = _T('V'); *szCode++ = _T('B'); *szCode++ = _T('R');
					break;
				default:		
					*szCode++ = _T('R'); *szCode++ = _T('c'); *szCode++ = _T('?');
					break;
			}
			break;
	}

	*szCode = 0;
	return szCode;
}
//################
//#
//#    Public functions
//#
//################

//
// disassemble one opcode
//

BYTE dis_stage1 (WORD op, OPDIS *ope)
{
//	OPb opb;		// not used, to do later

// bits to decode isntructions
	BYTE op4_3 = (op & 0x0018) >> 3;
	BOOL op5 = (op & 0x0020);
	BOOL op6 = (op & 0x0040);
	BOOL op7 = (op & 0x0080);
	BOOL op8 = (op & 0x0100);
	BOOL op11 = (op & 0x0800);
	BYTE op11_8 = (op & 0x0F00) >> 8;
	BYTE op11_9 = (op & 007000) >> 9;
	BOOL op14 = (op & 0x4000);
	
	BYTE ea_reg = (op & 000007);
	BYTE ea_mode = (op & 000070) >> 3;
	BYTE ea_size = (op & 000300) >> 6;
	BOOL ea_size11 = (ea_size == 0x3);
	
	BOOL ea_d = (ea_mode == 0);
	BOOL ea_a = (ea_mode == 1);
	BOOL ea_ia = (ea_mode == 2);
	BOOL ea_iai = (ea_mode == 3);
	BOOL ea_iad = (ea_mode == 4);
	BOOL ea_ida = (ea_mode == 5);
	BOOL ea_idax = (ea_mode == 6);
	BOOL ea_ixw = (ea_mode == 7) && (ea_reg == 0);
	BOOL ea_ixl = (ea_mode == 7) && (ea_reg == 1);
	BOOL ea_idp = (ea_mode == 7) && (ea_reg == 2);
	BOOL ea_idpx = (ea_mode == 7) && (ea_reg == 3);
	BOOL ea_imm = (ea_mode == 7) && (ea_reg == 4);
	
	BYTE ea_imode = (ea_d) ? EA_D : (ea_a) ? EA_A : (ea_ia) ? EA_IA : (ea_iai) ? EA_IAI :
					 (ea_iad) ? EA_IAD : (ea_ida) ? EA_IDA : (ea_idax) ? EA_IDAX : (ea_ixw) ? EA_IXW :
					 (ea_ixl) ? EA_IXL : (ea_idp) ? EA_IDP : (ea_idpx) ? EA_IDPX : (ea_imm) ? EA_IMM : EA_NONE;
	
	BOOL ea_valid = (ea_imode != 0);
	BOOL ea_data = ea_valid && !(ea_a);
	BOOL ea_memory = ea_valid && !(ea_d || ea_a);
	BOOL ea_control = ea_valid && !(ea_d || ea_a || ea_iai || ea_iad || ea_imm);
	BOOL ea_alterable = ea_valid && !(ea_idp || ea_idpx || ea_imm);
	
	BYTE ea2_reg = (op & 007000) >> 9;
	BYTE ea2_mode = (op & 00700) >> 6;
	BOOL ea2_d = (ea2_mode == 0);
	BOOL ea2_a = (ea2_mode == 1);
	BOOL ea2_ia = (ea2_mode == 2);
	BOOL ea2_iai = (ea2_mode == 3);
	BOOL ea2_iad = (ea2_mode == 4);
	BOOL ea2_ida = (ea2_mode == 5);
	BOOL ea2_idax = (ea2_mode == 6);
	BOOL ea2_ixw = (ea2_mode == 7) && (ea2_reg == 0);
	BOOL ea2_ixl = (ea2_mode == 7) && (ea2_reg == 1);
	BOOL ea2_idp = (ea2_mode == 7) && (ea2_reg == 2);
	BOOL ea2_idpx = (ea2_mode == 7) && (ea2_reg == 3);
	BOOL ea2_imm = (ea2_mode == 7) && (ea2_reg == 4);
	
	BYTE ea2_imode = (ea2_d) ? EA_D : (ea2_a) ? EA_A : (ea2_ia) ? EA_IA : (ea2_iai) ? EA_IAI :
					  (ea2_iad) ? EA_IAD : (ea2_ida) ? EA_IDA : (ea2_idax) ? EA_IDAX : (ea2_ixw) ? EA_IXW :
					  (ea2_ixl) ? EA_IXL : (ea2_idp) ? EA_IDP : (ea2_idpx) ? EA_IDPX : (ea2_imm) ? EA_IMM : EA_NONE;
	
	BOOL ea2_valid = (ea2_imode != 0);
	BOOL ea2_data = ea2_valid && !(ea2_a);
	BOOL ea2_memory = ea2_valid && !(ea2_d || ea2_a);
	BOOL ea2_control = ea2_valid && !(ea2_d || ea2_a || ea2_iai || ea2_iad || ea2_imm);
	BOOL ea2_alterable = ea2_valid && !(ea2_idp || ea2_idpx || ea2_imm);
	
	BOOL ill_op = FALSE;
	
	EA ea1, ea2;

	ea1.isize = (ea_size == 0) ? 1 : (ea_size == 1) ? 2 : 4;
	ea1.imode = ea_imode;
	ea1.reg = ea_reg;

	ea2.isize = ea1.isize;
	ea2.imode = ea2_imode;
	ea2.reg = ea2_reg;

	ope->eas.imode = EA_NONE;
	ope->eas.isize = 0;
	ope->ead.imode = EA_NONE;
	ope->vector = 0x00;		// no exception

	switch ((op & 0xF000) >> 12) {
		case 0x0:	// IMM and other
			if (op8) {										// other ?
				if (ea_a) {										// MOVEP ?
					if (op7) {										// MOVEP Dx, (d,Ay)
						ope->eas.imode = EA_D; ope->eas.reg = ea2.reg;
						ope->ead.imode = EA_IDA; ope->ead.reg = ea1.reg;
						ope->eas.isize = (op6) ? 4: 2;
						ope->op = O_MOVEP;
					} else {										// MOVEP (d,Ay),Dx
						ope->eas.imode = EA_IDA; ope->eas.reg = ea1.reg;
						ope->ead.imode = EA_D; ope->ead.reg = ea2.reg;
						ope->eas.isize = (op6) ? 4: 2;
						ope->op = O_MOVEP;
					}
				} else if (ea_size == 0) {					// BTST Dn, Ea ?
					if (ea_data && !ea_imm) {					// BTST Dn,Ea
						ope->eas = ea2; ope->eas.imode = EA_D;
						ope->eas.isize = 1;
						ope->ead = ea1;
						ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
						ope->op = O_BTST;
					} else ill_op = TRUE;
				} else if (ea_data && ea_alterable) {		// BCHG BCLR BSET Dn,Ea
					ope->eas = ea2; ope->eas.imode = EA_D;
					ope->eas.isize = 1;
					ope->ead = ea1;
					ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
					ope->op = (ea_size == 1) ? O_BCHG : (ea_size == 2) ? O_BCLR : O_BSET;
				} else ill_op = TRUE;
			}
			else {
				ope->eas.imode = EA_IMM;					// imm
				ope->eas.isize = ea1.isize;
				ope->ead = ea1;
				switch (op11_9) {
					case 0x0: 								// ORI
					case 0x1:								// ANDI
					case 0x5:								// EORI
						ope->op = (op11_9 == 0) ? O_ORI : (op11_9 == 1) ? O_ANDI : O_EORI;
						if (ea_imm) {
							if (ea_size == 0) {					// #,CCR
								ope->ead.imode = EA_CCR;		
							} else if (ea_size == 1) {			// #,SR
								ope->ead.imode = EA_SR;
							} else ill_op = TRUE;
						} else if (!ea_data || !ea_alterable) ill_op = TRUE;
						break;
					case 0x2:								// SUBI
					case 0x3:								// ADDI
					case 0x6:								// CMPI
						ope->op = (op11_9 == 2) ? O_SUBI : (op11_9 == 3) ? O_ADDI : O_CMPI;
						if (ea_size11) ill_op = TRUE;
						else if (!ea_data || !ea_alterable) ill_op = TRUE;
						break;
					case 0x4:								// BTST BCHG BCLR BSET #, Ea?
						if (ea_size == 0) {						// BTST #,Ea ?
							if (ea_data && !ea_imm) {				// BTST #,Ea
								ope->eas.isize = 1;
								ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
								ope->op = O_BTST;
							} else ill_op = TRUE;
						} else if (ea_data && ea_alterable) {		// BCHG BCLR BSET #, Ea?
							ope->eas.isize = 1;
							ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
							ope->op = (ea_size == 1) ? O_BCHG : (ea_size == 2) ? O_BCLR : O_BSET;
						} else ill_op = TRUE;
						break;
#ifdef M68000
					default: ill_op = TRUE;
#else
					case 0x07:								// MOVES for 68010 and up
						if (ea_memory && ea_alterable) {
							ope->eas = ea1;
							ope->op = O_MOVES; 
						} else ill_op = TRUE;
						break;
#endif
				}
			}
			break;
		case 0x1:	// MOVE.B
			if (ea_valid && ea2_data && ea2_alterable && !ea_a) {
				ope->eas = ea1; ope->ead = ea2;
				ope->eas.isize = 1;
				ope->op = (ea2_imode == EA_A) ? O_MOVEA : O_MOVE;
			} else ill_op = TRUE;
			break;
		case 0x2:	// MOVE.L MOVEA.L
			if (ea_valid && ((ea2_data && ea2_alterable) || ea2_a)) {
				ope->eas = ea1; ope->ead = ea2;
				ope->eas.isize = 4;
				ope->op = (ea2_imode == EA_A) ? O_MOVEA : O_MOVE;
			} else ill_op = TRUE;
			break;
		case 0x3:	// MOVE.W MOVEA.W
			if (ea_valid && ((ea2_data && ea2_alterable) || ea2_a)) {
				ope->eas = ea1; ope->ead = ea2;
				ope->eas.isize = 2;
				ope->op = (ea2_imode == EA_A) ? O_MOVEA : O_MOVE;
			} else ill_op = TRUE;
			break;	
		case 0x4:	// MISC
			if (op8) {										// CHK LEA ?
				if (op6) {										// LEA ?
					if (op7) {										// LEA ?
						if (ea_control) {								// LEA Ea,An
							ope->eas = ea1;	ope->ead = ea2;
							ope->eas.isize = 0;								// no .BWL
							ope->ead.imode = EA_A;
							ope->op = O_LEA;
						} else ill_op = TRUE;
					} else ill_op = TRUE;
				} else if (op7) {
					if (ea_data) {									// CHK Ea,Dn
						ope->eas = ea1; ope->ead = ea2;
						ope->eas.isize = 2;									// no .BWL
						ope->ead.imode = EA_D;
						ope->op = O_CHK;
					} else ill_op = TRUE;
				} else ill_op = TRUE;
			}
			else {											// MISC ?
				switch (op11_9) {
					case 0x0:									// NEGX MOVE SR ?
						if (ea_data && ea_alterable) {
							if (ea_size11) {						// MOVE SR,Ea
								ope->eas.imode = EA_SR;
								ope->eas.isize = 0;						// no .BWL
								ope->ead = ea1;
								ope->op = O_MOVE;
							} else {								// NEGX Ea
								ope->eas = ea1;
								ope->op = O_NEGX;
							}
						} else ill_op = TRUE;
						break;
					case 0x1:									// CLR MOVE CCR ?
						if (ea_data && ea_alterable) {
							if (ea_size11) {						// MOVE CCR,Ea
								ope->eas.imode = EA_CCR;
								ope->eas.isize = 2;						// .W
								ope->ead = ea1;
								ope->op = O_MOVE;	
							} else {								// CLR Ea
								ope->eas = ea1;
								ope->op = O_CLR;		
							}
						} else ill_op = TRUE;
						break;
					case 0x2:									// NEG MOVE CCR ?
						if (ea_size11) {							// MOVE Ea,CCR ?
							if (ea_data) {								// MOVE Ea,CCR
								#if defined M68000
									ill_op = TRUE;
								#else
									ope->eas = ea1;
									ope->eas.isize = 2;						// .W
									ope->ead.imode = EA_CCR;
									ope->op = O_MOVE;
								#endif
							} else ill_op = TRUE;
						}
						else if (ea_data && ea_alterable) {			// NEG Ea
							ope->eas = ea1;
							ope->op = O_NEG;
						} else ill_op = TRUE;
						break;
					case 0x3:									// NOT MOVE SR ?
						if (ea_size11) {							// MOVE Ea,SR ?
							if (ea_data) {								// MOVE Ea,SR
								ope->eas = ea1;
								ope->eas.isize = 2;						// .W
								ope->ead.imode = EA_SR;
								ope->op = O_MOVE;
							} else ill_op = TRUE;
						}
						else if (ea_data && ea_alterable) {			// NOT Ea
							ope->eas = ea1;
							ope->op = O_NOT;
						} else ill_op = TRUE;
						break;
					case 0x4:									// NBCD SWAP BKP PEA EXT MOVEM ?
						switch (ea_size) {		
							case 0: 
								if (ea_data && ea_alterable) {		// NBCD
									ope->eas = ea1;
									ope->eas.isize = 0;					// no .BWL
									ope->op = O_NBCD;
								} else ill_op = TRUE;
								break;
							case 1:
								if (ea_d) {							// SWAP Dn
									ope->eas = ea1;
									ope->eas.isize = 0;					// no .BWL
									ope->op = O_SWAP;
								} else if (ea_a) {					// BKP (68010)
									ill_op = TRUE;
								} else if (ea_control) {			// PEA Ea
									ope->eas = ea1;
									ope->eas.isize = 0;					// no .BWL
									ope->op = O_PEA;
								} else ill_op = TRUE;
								break;
							default:
								if (ea_d) {							// EXT Dn
									ope->eas = ea1;
									ope->eas.isize = (ea_size == 2) ? 2 : 4;
									ope->op = O_EXT;
								} else if (ea_control && ea_alterable || ea_iad) {	// MOVEM L,Ea
									ope->eas.imode = EA_MOVEM;
									ope->eas.isize = (op6) ? 4 : 2;
									ope->ead = ea1;
									ope->op = O_MOVEM;
								} else ill_op = TRUE;
								break;
						}
						break;
					case 0x5:									// TST TAS ILLEGAL ?
						if (ea_size11) {
							if (ea_data && ea_alterable) {			// TAS Ea
								ope->eas = ea1;
								ope->eas.isize = 0;					// no .BWL
								ope->op = O_TAS;
							} else ill_op = TRUE;					// ILLEGAL and more
						} else if (ea_data && ea_alterable) {		// TST Ea
								ope->eas = ea1;
								ope->op = O_TST;
						} else ill_op = TRUE;
						break;
					case 0x6:									// MOVEM ?
						if (op7) {
							if (ea_control || ea_iai) {
								ope->eas = ea1;
								ope->eas.isize = (op6) ? 4 : 2;
								ope->ead.imode = EA_MOVEM;
								ope->op = O_MOVEM;
							} else ill_op = TRUE;
						} else ill_op = TRUE;
						break;
					default:									// TRAP LINK UNLK MOVE USP RESET NOP STOP RTE RTD RTS TRAPV RTR JSR JMP ?
						if (op7) {									// JSR JMP ?
							if (op6) {									// JMP ?
								if (ea_control) {							// JMP Ea
									ope->eas = ea1;
									ope->eas.isize = 0;							// no .BWL
									ope->op = O_JMP;
								} else ill_op = TRUE;
							}
							else if (ea_control) {							// JSR Ea
									ope->eas = ea1;
									ope->eas.isize = 0;							// no .BWL
									ope->op = O_JSR;
							} else ill_op = TRUE;
						}
						else if (op6) {
							if (ea_a || ea_d) {										// TRAP
								ope->eas.imode = EA_CST;
								ope->eas.isize = 0;										// no .BWL
								ope->eas.calc = (op & 0x000F);
								ope->op = O_TRAP;
							}
							else if (ea_ia) {										// LINK
								ope->eas = ea1; ope->eas.imode = EA_A;
								ope->eas.isize = 0;										// no .BWL
								ope->ead.imode = EA_IMM;
								ope->ead.isize = 2;
								ope->op = O_LINK;
							}
							else if (ea_iai) {										// UNLK
								ope->eas = ea1; ope->eas.imode = EA_A;
								ope->eas.isize = 0;										// no .BWL
								ope->op = O_UNLK;
							}
							else if (ea_iad) {										// MOVE An,USP
								ope->eas = ea1; ope->eas.imode = EA_A;
								ope->eas.isize = 0;										// no .BWL
								ope->ead.imode = EA_USP;
								ope->op = O_MOVE;
							}
							else if (ea_ida) {										// MOVE USP,An
								ope->ead = ea1; ope->ead.imode = EA_A;
								ope->eas.isize = 0;										// no .BWL
								ope->eas.imode = EA_USP;
								ope->op = O_MOVE;
							}
							else if (ea_idax) {										// MISC ?
								switch (ea_reg) {
									case 0:												// RESET
										ope->op = O_RESET;
										break;
									case 1:												// NOP
										ope->op = O_NOP;
										break;
									case 2:												// STOP
										ope->eas.imode = EA_IMM;
										ope->eas.isize = 2;
										ope->op = O_STOP;
										break;
									case 3:												// RTE
										ope->op = O_RTE;
										break;
									case 5:												// RTS
										ope->op = O_RTS;
										break;
									case 6:												// TRAPV
										ope->op = O_TRAPV;
										break;
									case 7:												// RTR
										ope->op = O_RTR;
										break;
									default: ill_op = TRUE;								// RTD (68010)
								}
							} 
#ifdef M68000
							else ill_op = TRUE;
#else
							else if (ea_idp || ea_idpx) {								// MOVEC
//								ope->eas = ea1;
								ope->eas.isize = 0;											// no .BWL
								ope->op = O_MOVEC;
							}
							else ill_op = TRUE;
#endif
						} else ill_op = TRUE;
				}
			}
			break;
		case 0x5:	// ADDQ SUBQ DBcc Scc 
			if (ea_size11) {								// DBcc Scc ?
				if (ea_a) {										// DBcc
					ope->eas = ea1; ope->eas.imode = EA_D;
					ope->eas.isize = 0;								// no .BWL
					ope->ead.imode = EA_DBCC;
					ope->op = O_DB;
				} else if (ea_data && ea_alterable) {			// Scc Ea
					ope->eas = ea1;
					ope->eas.isize = 0;								// no .BWL
					ope->op = O_S;
				} else ill_op = TRUE;
			}
			else {											// ADDQ SUBQ ?
				if (ea_alterable) {								// ADDQ SUBQ
					if ((ea_imode == EA_A) && (ea_size == 0)) ill_op = TRUE;	// .B forbidden for An
					else {
						ope->eas = ea1; ope->eas.imode = EA_CST;
						ope->eas.calc = (op11_9 == 0) ? 8 : op11_9;
						ope->ead = ea1;
						ope->op = (op8) ? O_SUBQ : O_ADDQ;
					}
				} else ill_op = TRUE;
			}
			break;
		case 0x6:	// BRA BSR Bcc
			switch (op11_8) {
				case 0:											// BRA
					ope->eas.calc = (op & 0x00FF);
					ope->eas.imode = EA_BCC;
					ope->op = O_BRA;
					break;
				case 1:											// BSR
					ope->eas.calc = (op & 0x00FF);
					ope->eas.imode = EA_BCC;
					ope->op = O_BSR;
					break;
				default:										// Bcc
					ope->eas.calc = (op & 0x00FF);
					ope->eas.imode = EA_BCC;
					ope->op = O_B;
					break;
			}
			break;
		case 0x7:	// MOVEQ
			if (op8) ill_op = TRUE;
			else {												// MOVEQ #,Dn
				ope->eas.imode = EA_CST;
				ope->eas.calc = (op & 0x00FF);
				ope->eas.isize = 0;									// no .BWL
				ope->ead = ea2; ope->ead.imode = EA_D;
				ope->op = O_MOVEQ;
			}
			break;
		case 0x8:	// SBCD OR DIVx
			if (ea_size11) {								// DIVx ?
				if (ea_data) {									// DIVx
					ope->eas = ea1;
					ope->eas.isize = 0;								// no .BWL
					ope->ead = ea2; ope->ead.imode = EA_D;
					ope->op = (op8) ? O_DIVS : O_DIVU;
				} else ill_op = TRUE;
			} else if (op8) {									// SBCD OR ?
				if (ea_d) {										// SBCD ?
					if (ea_size == 0) {								// SBCD Dy,Dx
						ope->eas = ea1; ope->eas.imode = EA_D;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_D;
						ope->op = O_SBCD;
					} else ill_op = TRUE;
				} else if (ea_a) {								// SBCD ?
					if (ea_size == 0) {								// SBCD -(Ay),-(Ax)
						ope->eas = ea1; ope->eas.imode = EA_IAD;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_IAD;
						ope->op = O_SBCD;
					}
				} else if (ea_memory && ea_alterable) {			// OR Dn,Ea
					ope->eas = ea2; ope->eas.imode = EA_D; 
					ope->ead = ea1;
					ope->op = O_OR;
				} else ill_op = TRUE;
			} else if (ea_data) {							// OR Ea,Dn
				ope->eas = ea1;
				ope->ead = ea2; ope->ead.imode = EA_D;
				ope->op = O_OR;
			} else ill_op = TRUE;
			break;
		case 0x9:	// SUB
		case 0xD:	// ADD
			if (ea_size11) {								// ADDA ?
				if (ea_valid) {									// ADDA Ea,An
					ope->eas = ea1;
					ope->eas.isize = (op8) ? 4 : 2;
					ope->ead = ea2; ope->ead.imode = EA_A;
					ope->op = (op14) ? O_ADDA : O_SUBA;
				} else ill_op = TRUE;
			} else {										// ADD ADDX ?
				if (op8) {										// ADD ADDX ?
					if (ea_a) {										// ADDX -(Ay),-(Ax)
						ope->eas = ea1; ope->eas.imode = EA_IAD;
						ope->ead = ea2; ope->ead.imode = EA_IAD;
						ope->op = (op14) ? O_ADDX : O_SUBX;
					} else if (ea_d) {								// ADDX Dy,Dx
						ope->eas = ea1; ope->eas.imode = EA_D;
						ope->ead = ea2; ope->ead.imode = EA_D;
						ope->op = (op14) ? O_ADDX : O_SUBX;
					} else if (ea_memory && ea_alterable) {			// ADD Dn,Ea
						ope->eas = ea2; ope->eas.imode = EA_D;
						ope->ead = ea1;
						ope->op = (op14) ? O_ADD : O_SUB;
					} else ill_op = TRUE;
				} else if (ea_valid) {						// ADD Ea, Dn
					if ((ea_a) && (ea_size == 0)) ill_op = TRUE;	// ADD.B An,Dn forbidden
					else {
						ope->eas = ea1;
						ope->ead = ea2; ope->ead.imode = EA_D;
						ope->op = (op14) ? O_ADD : O_SUB;
					}
				} else ill_op = TRUE;
			}
			break;
		case 0xA:	// LINEA
			ope->op = O_LINEA;
			break;
		case 0xB:	// CMP EOR CMPA CMPM
			if (ea_size11) {								// CMPA ?
				if (ea_valid) {									// CMPA Ea,An
					ope->eas = ea1;
					ope->eas.isize = (op8) ? 4 : 2;
					ope->ead = ea2; ope->ead.imode = EA_A;
					ope->op = O_CMPA;
				} else ill_op = TRUE;
			} else if (op8) {								// EOR CMPM ?
				if (ea_a) {										// CMPM (Ay)+,(Ax)+
					ope->eas = ea1; ope->eas.imode = EA_IAI;
					ope->ead = ea2; ope->ead.imode = EA_IAI;
					ope->op = O_CMPM;
				} else if (ea_data && ea_alterable) {			// EOR Dn,Ea
					ope->eas = ea2; ope->eas.imode = EA_D;
					ope->ead = ea1;
					ope->op = O_EOR;
				} else ill_op = TRUE;
			} else if (ea_valid) {							// CMP Ea,Dn
				if ((ea_a) && (ea_size == 0)) ill_op = TRUE;	// CMP.B An,Dn forbidden
				else {
					ope->eas = ea1;
					ope->ead = ea2; ope->ead.imode = EA_D;
					ope->op = O_CMP;
				}
			} else ill_op = TRUE;
			break;
		case 0xC:	// AND MUL ABCD EXG
			if (ea_size11) {								// MULx ?
				if (ea_data) {									// MUL Ea,Dn
					ope->eas = ea1;
					ope->eas.isize = 0;								// no .BWL
					ope->ead = ea2; ope->ead.imode = EA_D;
					ope->op = (op8) ? O_MULS : O_MULU;
				} else ill_op = TRUE;
			} else if (op8) {								// AND ABCD EXG ?
				if (ea_a) {										// ABCD EXG ?
					if (ea_size == 0) {								// ABCD -(Ay),-(Ax)
						ope->eas = ea1; ope->eas.imode = EA_IAD;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_IAD;
						ope->op = O_ABCD;
					} else if (ea_size == 1) {						// EXG Ax,Ay
						ope->eas = ea1; ope->eas.imode = EA_A;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_A;
						ope->op = O_EXG;
					} else {										// EXG Dx,Ay
						ope->eas = ea1; ope->eas.imode = EA_A;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_D;
						ope->op = O_EXG;
					}
				} else if (ea_d) {								// ABCD EXG ?
					if (ea_size == 0) {								// ABCD Dy,Dx
						ope->eas = ea1; ope->eas.imode = EA_D;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_D;
						ope->op = O_ABCD;
					} else if (ea_size == 1) {						// EXG Dy,Dx
						ope->eas = ea1; ope->eas.imode = EA_D;
						ope->eas.isize = 0;								// no .BWL
						ope->ead = ea2; ope->ead.imode = EA_D;
						ope->op = O_EXG;
					} else ill_op = TRUE;
				}
				else if (ea_memory && ea_alterable) {			// AND Dn,Ea
					ope->ead = ea1;
					ope->eas = ea2; ope->eas.imode = EA_D;
					ope->op = O_AND;
				} else ill_op = TRUE;
			}
			else {											// AND ?
				if (ea_data) {									// AND Ea,Dn
					ope->eas = ea1;
					ope->ead = ea2; ope->ead.imode = EA_D;
					ope->op = O_AND;
				} else ill_op = TRUE;
			}
			break;
		case 0xE:	// ASx LSx ROXs ROx
			if (ea_size11) {								// xxx Ea ?
				if (op11) ill_op = TRUE;
				else if (ea_memory && ea_alterable) {			// xxx Ea
					switch (op11_9) {
						case 0: ope->op = (op8) ? O_ASL : O_ASR; break;
						case 1: ope->op = (op8) ? O_LSL : O_LSR; break;
						case 2: ope->op = (op8) ? O_ROXL : O_ROXR; break;
						case 3: ope->op = (op8) ? O_ROL : O_ROR; break;
					}
					ope->eas = ea1;
					ope->eas.isize = 2;								
				} else ill_op = TRUE;
			} else {										// xxx ,Dn ?
				switch (op4_3) {
					case 0: ope->op = (op8) ? O_ASL : O_ASR; break;
					case 1: ope->op = (op8) ? O_LSL : O_LSR; break;
					case 2: ope->op = (op8) ? O_ROXL : O_ROXR; break;
					case 3: ope->op = (op8) ? O_ROL : O_ROR; break;
				}
				if (op5) {										// xxx Dx,Dn
					ope->eas = ea2; ope->eas.imode = EA_D;
					ope->ead = ea1; ope->ead.imode = EA_D;
				} else {										// xxx #,Dn
					ope->eas = ea1; ope->eas.imode = EA_CST;
					ope->eas.calc = (op11_9 == 0) ? 8 : op11_9;
					ope->ead = ea1; ope->ead.imode = EA_D;
				}
			}
			break;
		case 0xF:	// LINEF
			ope->op = O_LINEF;
			break;
	}
	if (ill_op) {
		ope->op = O_ILLEGAL;
		ope->eas.isize = 0;
	}
	return ill_op;
}

DWORD disassemble (DWORD start, LPTSTR out, BOOL view)
{
	LPTSTR p = out;
	DWORD ea_addr;
	DWORD addr = start;

	TCHAR szCode[64];
	LPTSTR c = szCode;

	OPDIS ope;
	BYTE status;
	WORD op;
	
	op = GetWORD(addr);							// read opcode
	if (view) {
		p = append_daword(p, op);
		*p++ = _T(' ');
	}
	addr += 2;
	status = dis_stage1(op, &ope);

	c = append_str(c, opcode_T[ope.op]);
	if ((ope.op == O_B) || (ope.op == O_S))
		c = append_str(c, condcode_T[(op & 0x0F00) >> 8]);
	else if (ope.op == O_DB)
		c = append_str(c, condcodeDB_T[(op & 0x0F00) >> 8]);
	else if (ope.op == O_MOVEM) {
		if (ope.eas.imode == EA_MOVEM) {
			ope.eas.data.w[0] = GetWORD(addr);
			ope.eas.calc = (ope.ead.imode == EA_IAD);	// reverse order of mask for -(An) mode
		} else {
			ope.ead.data.w[0] = GetWORD(addr);
			ope.ead.calc = 0;
		}
		if (view) { 
			p = append_daword(p, GetWORD(addr));
			*p++ = _T(' ');
		}
		addr += 2;
	} 
#ifdef M68000
#else
	else if (ope.op == O_MOVEC) {
		extw.w = GetWORD(addr);
		if (view) { 
			p = append_daword(p, extw.w);
			*p++ = _T(' ');
		}
		addr += 2;
		if (op & 0x0001) {								// movec Rn,Rc
			ope.eas.imode = (extw.da) ? EA_A : EA_D;
			ope.eas.reg = extw.reg;
			ope.ead.imode = EA_RC;
			ope.ead.data.w[0] = (extw.w ) & 0x0FFF;
		} else {										// movec Rc,Rn
			ope.ead.imode = (extw.da) ? EA_A : EA_D;
			ope.ead.reg = extw.reg;
			ope.eas.imode = EA_RC;
			ope.eas.data.w[0] = (extw.w ) & 0x0FFF;
		}
	}
	else if (ope.op == O_MOVES) {
		extw.w = GetWORD(addr);
		if (view) { 
			p = append_daword(p, extw.w);
			*p++ = _T(' ');
		}
		addr += 2;
		if (extw.wl) {									// moves Rn,Ea
			ope.ead = ope.eas;
			ope.eas.imode = (extw.da) ? EA_A : EA_D;
			ope.eas.reg = extw.reg;
		} else {										// moves Ea,Rn
			ope.ead.imode = (extw.da) ? EA_A : EA_D;
			ope.ead.reg = extw.reg;
		}
	}
#endif
	if (ope.op != O_ILLEGAL) {
		if ((ope.op == O_BCHG) || (ope.op == O_BCLR) || (ope.op == O_BSET) || (ope.op == O_BTST))
			c = append_str(c, size_T[ope.ead.isize]);
		else c = append_str(c, size_T[ope.eas.isize]);
		if (ope.eas.imode != EA_NONE) {
			*c++ = _T(' ');
			ea_addr = addr;
			c = append_ea(c, &(ope.eas), &addr, view);
			if (view) {
				while (ea_addr != addr) {
					p = append_daword(p, GetWORD(ea_addr));
					*p++ = _T(' ');
					ea_addr += 2;
				}
			}
		}
		if (ope.ead.imode != EA_NONE) {
			*c++ = _T(',');
			ea_addr = addr;
			c = append_ea(c, &(ope.ead), &addr, view);
			if (view) {
				while (ea_addr != addr) {
					p = append_daword(p, GetWORD(ea_addr));
					*p++ = _T(' ');
					ea_addr += 2;
				}
			}
		}
	}
	if (view) {
		while ((p - out) < 27) *p++ = _T(' ');
	}
	p = append_str(p, szCode);
	return addr;
}
