/*
Copyright (C) 2002, J.P. Grossman

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// security.c

#include "quakedef.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

qboolean	security_loaded;
qboolean	cl_cheatfree = false;
unsigned long	qsmackAddr;
qboolean	qsmackActive = false;		// only allow one qsmack connection
unsigned long	net_seed;

cvar_t		cl_cvar_cheatfree = {"cl_cheatfree", "0"};

Security_InitCRC_t Security_InitCRC = NULL;
Security_SetSeed_t Security_SetSeed = NULL;
Security_Decode_t Security_Decode = NULL;
Security_Encode_t Security_Encode = NULL;
Security_CRC_t Security_CRC = NULL;
Security_CRC_File_t Security_CRC_File = NULL;
Security_Verify_t Security_Verify = NULL;

#ifdef _WIN32
static	HINSTANCE hSecurity = NULL;
#else
static	void	*hSecurity = NULL;
#endif

#ifdef _WIN32
#define SECURITY_GETFUNC(f) (Security_##f = (Security_##f##_t)GetProcAddress(hSecurity, "Security_" #f))
#else
#define SECURITY_GETFUNC(f) (Security_##f = (Security_##f##_t)dlsym(hSecurity, "Security_" #f))
#endif

void Security_Init (void)
{
	security_loaded = false;

#ifdef _WIN32
	if (!(hSecurity = LoadLibrary("qsecurity.dll")))
#else
	if (!(hSecurity = dlopen("./qsecurity.so", RTLD_NOW)))
#endif
	{
		Con_Printf ("\x02" "Security module not found\n");
		goto fail;
	}

	SECURITY_GETFUNC(InitCRC);
	SECURITY_GETFUNC(SetSeed);
	SECURITY_GETFUNC(Decode);
	SECURITY_GETFUNC(Encode);
	SECURITY_GETFUNC(CRC);
	SECURITY_GETFUNC(CRC_File);
	SECURITY_GETFUNC(Verify);

	security_loaded = Security_InitCRC && Security_SetSeed && Security_Decode && 
			Security_Encode && Security_CRC && Security_CRC_File && Security_Verify;

	if (!security_loaded)
	{
		Con_Printf ("\x02" "Security module not initialized\n");
		goto fail;
	}

	Security_InitCRC (0x3915c28b);

	Cvar_Register (&cl_cvar_cheatfree);

	Con_Printf ("Security module initialized\n");
	return;

fail:
	if (hSecurity)
	{
#ifdef _WIN32
		FreeLibrary (hSecurity);
#else
		dlclose (hSecurity);
#endif
		hSecurity = NULL;
	}
}
