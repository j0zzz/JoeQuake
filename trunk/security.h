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
// security.h

#ifndef SECURITY_H
#define SECURITY_H

extern	qboolean	security_loaded;
extern	qboolean	cl_cheatfree;
extern	unsigned long	qsmackAddr;
extern	qboolean	qsmackActive;
extern	unsigned long	net_seed;

extern	cvar_t		cl_cvar_cheatfree;

// Functions exported by security module
typedef void (*Security_InitCRC_t)(unsigned k);
typedef void (*Security_SetSeed_t)(unsigned long seed, char *filename);
typedef void (*Security_Encode_t)(unsigned char *src, unsigned char *dst, int len, int to);
typedef void (*Security_Decode_t)(unsigned char *src, unsigned char *dst, int len, int from);
typedef unsigned long (*Security_CRC_t)(unsigned char *data, int len);
typedef unsigned (*Security_CRC_File_t)(char *filename);
typedef int (*Security_Verify_t)(unsigned a, unsigned b);

extern	Security_InitCRC_t Security_InitCRC;
extern	Security_SetSeed_t Security_SetSeed;
extern	Security_Decode_t Security_Decode;
extern	Security_Encode_t Security_Encode;
extern	Security_CRC_t Security_CRC;
extern	Security_CRC_File_t Security_CRC_File;
extern	Security_Verify_t Security_Verify;

void Security_Init (void);

#endif
