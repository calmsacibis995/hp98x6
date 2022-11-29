/*
 *   Settings.c
 *
 *   
 */

//
//   do all the settings stuff (with registry)
//
//   taken as is from Emu42
//

#include "StdAfx.h"
#include "HP98x6.h"

//
// registry handling
//

#define REGISTRYKEY "Software\\HP98x6"


#define ReadString(sec, key, dv, v, sv) GetRegistryString(sec, key, dv, v, sv)
#define ReadInt(sec, key, dv)         GetRegistryInt(sec, key, dv)
#define WriteString(sec, key, v)      WriteReg(sec, key, REG_SZ, (BYTE *) v, (lstrlen(v)+1) * sizeof(*v))
#define WriteInt(sec, key, v)         WriteReg(sec, key, REG_DWORD, (BYTE *) &v, sizeof(int))

//################
//#
//#    Low level subroutines
//#
//################

static VOID ReadReg(LPCTSTR lpSubKey, LPCTSTR lpValueName, LPBYTE lpData, DWORD *pdwSize)
{
	TCHAR lpKey[256] = _T(REGISTRYKEY) _T("\\");

	DWORD retCode,dwType;
	HKEY  hKey;

	lstrcat(lpKey, lpSubKey);				// full registry key

	retCode = RegOpenKeyEx(HKEY_CURRENT_USER,
					       lpKey,
						   0,
						   KEY_QUERY_VALUE,
						   &hKey);
	if (retCode == ERROR_SUCCESS) 
	{
		retCode = RegQueryValueEx(hKey, lpValueName, NULL, &dwType, lpData, pdwSize);
		RegCloseKey(hKey);
	}

	if (retCode != ERROR_SUCCESS)			// registry entry not found
		*pdwSize = 0;						// return zero size
} 

static VOID WriteReg(LPCTSTR lpSubKey, LPCTSTR lpValueName, DWORD dwType, CONST BYTE *lpData, DWORD cbData)
{
	TCHAR lpKey[256] = _T(REGISTRYKEY) _T("\\");

	DWORD retCode;
	HKEY  hKey;
	DWORD dwDisposition;

	lstrcat(lpKey, lpSubKey);				// full registry key

	retCode = RegCreateKeyEx(HKEY_CURRENT_USER,
							 lpKey,
							 0,_T(""),
                             REG_OPTION_NON_VOLATILE,
							 KEY_WRITE,
                             NULL,
							 &hKey,
							 &dwDisposition);
	_ASSERT(retCode == ERROR_SUCCESS);

	RegSetValueEx(hKey, lpValueName, 0, dwType, lpData, cbData);
	RegCloseKey(hKey);
} 

static DWORD GetRegistryString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpDefault, LPTSTR lpData, DWORD dwSize)
{
	dwSize *= sizeof(*lpData);				// buffer size in bytes
	ReadReg(lpszSection, lpszEntry, (LPBYTE) lpData, &dwSize);
	if (dwSize == 0)
	{
		lstrcpy(lpData, lpDefault);
		dwSize = lstrlen(lpData);
	}
	else
	{
		dwSize = (dwSize / sizeof(*lpData)) - 1;
	}
	return dwSize;
}

static INT GetRegistryInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, INT nDefault)
{
	UINT  nValue;
	DWORD dwSize = sizeof(nValue);

	ReadReg(lpszSection, lpszEntry, (LPBYTE) &nValue, &dwSize);
	return dwSize ? nValue : nDefault;
}

//################
//#
//#    Public functions
//#
//################

VOID ReadSettings(VOID)
{ 
	// Files
	ReadString(_T("Files"), _T("HP98x6Directory"), szCurrentDirectory, szHP8xDirectory, ARRAYSIZEOF(szHP8xDirectory));
	bAutoSave = ReadInt(_T("Files"), _T("AutoSave"),bAutoSave);
	bAutoSaveOnExit = ReadInt(_T("Files"), _T("AutoSaveOnExit"), bAutoSaveOnExit);
	// KML
	bAlwaysDisplayLog = ReadInt(_T("KML"), _T("AlwaysDisplayLog"), bAlwaysDisplayLog);
	// Emulator
	wRealSpeed = ReadInt( _T("Emulator"), _T("RealSpeed"), wRealSpeed);
	// dwCapricornCycles = ReadInt(_T("Emulator"), _T("CapricornCycles"), dwCapricornCycles);
	// bSound = ReadInt(_T("Emulator"), _T("Sound"), bSound);
	Chipset.keeptime = ReadInt(_T("Emulator"), _T("SetDate"), Chipset.keeptime);
	SetSpeed(wRealSpeed);					// set speed
/*	CloseWaves();							// set sound
	if (bSound)
		bSound = InitWaves(); */
}

VOID WriteSettings(VOID)
{
	// Files
	WriteString(_T("Files"), _T("HP98x6Directory"), szHP8xDirectory);
	WriteInt(_T("Files"), _T("AutoSave"), bAutoSave);
	WriteInt(_T("Files"), _T("AutoSaveOnExit"), bAutoSaveOnExit);
	// KML
	WriteInt(_T("KML"), _T("AlwaysDisplayLog"), bAlwaysDisplayLog);
	// Emulator
	WriteInt(_T("Emulator"), _T("RealSpeed"), wRealSpeed);
	// WriteInt(_T("Emulator"), _T("CapricornCycles"), dwCapricornCycles);
	//WriteInt(_T("Emulator"), _T("Sound"), bSound);
	WriteInt(_T("Emulator"), _T("SetDate"), Chipset.keeptime);
}

VOID ReadLastDocument(LPTSTR szFilename, DWORD nSize)
{
	ReadString(_T("Files"),_T("LastDocument"),_T(""),szFilename,nSize);
}

VOID WriteLastDocument(LPCTSTR szFilename)
{
	WriteString(_T("Files"),_T("LastDocument"),szFilename);
}
