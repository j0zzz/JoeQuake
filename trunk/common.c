/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// common.c -- misc functions used in client and server

#include "quakedef.h"

#define NUM_SAFE_ARGVS  7

static	char	*largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static	char	*argvdummy = " ";

static char *safeargvs[NUM_SAFE_ARGVS] = {
	"-stdvid",
	"-nolan",
	"-nosound",
	"-nocdaudio",
	"-nojoy",
	"-nomouse",
	"-dibonly"
};

cvar_t  registered = {"registered", "0"};
cvar_t  cmdline = {"cmdline", "0", CVAR_SERVER};

qboolean	msg_suppress_1 = 0;

void COM_InitFilesystem (void);

char	com_token[1024];
int	com_argc;
char	**com_argv;

#define CMDLINE_LENGTH	256
char	com_cmdline[CMDLINE_LENGTH];

int	rogue = 0, hipnotic = 0, nehahra = 0, runequake = 0;

void COM_Path_f (void);
void COM_Gamedir_f (void);

/*

All of Quake's data access is through a hierchal file system, but the contents
of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all
game directories. The sys_* files pass this to host_init in quakeparms_t->basedir.
This can be overridden with the "-basedir" command line parm to allow code
debugging in a different directory. The base directory is only used during
filesystem initialization.

The "game directory" is the first tree on the search path and directory that all
generated files (savegames, screenshots, demos, config files) will be saved to.
This can be overridden with the "-game" command line parameter.
The game directory can never be changed while quake is executing.
This is a precacution against having a malicious server instruct clients to
write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth,
especially over ISDN / T1 lines.  If there is a cache directory specified, when
a file is found by the normal search path, it will be mirrored into the cache
directory, then opened there.


FIXME:
The file "parms.txt" will be read out of the game directory and appended to the
current command line arguments to allow different games to initialize startup
parms differently. This could be used to add a "-sspeed 22050" for the high
quality sound edition. Because they are added at the end, they will not override
an explicit setting on the original command line.
	
*/

//============================================================================

// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

			LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

void Q_strcpy (char *dest, char *src)
{
	while (*src)
		*dest++ = *src++;
	*dest++ = 0;
}

void Q_strncpy (char *dest, char *src, int count)
{
	while (*src && count--)
		*dest++ = *src++;
	if (count)
		*dest++ = 0;
}

int Q_atoi (char *str)
{
	int	val, sign, c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
	{
		sign = 1;
	}

	val = 0;

// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

// check for character
	if (str[0] == '\'')
		return sign * str[1];

// assume decimal
	while (1)
	{
		c = *str++;
		if (c < '0' || c > '9')
			return val*sign;
		val = val*10 + c - '0';
	}

	return 0;
}

float Q_atof (char *str)
{
	double	val;
	int	sign, c, decimal, total;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
	{
		sign = 1;
	}

	val = 0;

// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

// check for character
	if (str[0] == '\'')
		return sign * str[1];

// assume decimal
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val*sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}

	return val*sign;
}

void Q_strncpyz (char *dest, char *src, size_t size)
{
	strncpy (dest, src, size - 1);
	dest[size-1] = 0;
}

void Q_snprintfz (char *dest, size_t size, char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr, fmt);
	vsnprintf (dest, size, fmt, argptr);
	va_end (argptr);

	dest[size-1] = 0;
}

/*
============================================================================

			BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean bigendien;

short (*BigShort)(short l);
short (*LittleShort)(short l);
int (*BigLong)(int l);
int (*LittleLong)(int l);
float (*BigFloat)(float l);
float (*LittleFloat)(float l);

short ShortSwap (short l)
{
	byte	b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short ShortNoSwap (short l)
{
	return l;
}

int LongSwap (int l)
{
	byte	b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float   f;
		byte    b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

// writing functions

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte	*buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte	*buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte	*buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte	*buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float	f;
		int	l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s) + 1);
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
void MSG_WriteCoord16(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, Q_rint(f * 8));
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
void MSG_WriteCoord24(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, f);
	MSG_WriteByte(sb, (int)(f * 255) % 255);
}

//johnfitz -- 32-bit float coords
void MSG_WriteCoord32f(sizebuf_t *sb, float f)
{
	MSG_WriteFloat(sb, f);
}

void MSG_WriteCoord(sizebuf_t *sb, float f, unsigned int flags)
{
	if (flags & PRFL_FLOATCOORD)
		MSG_WriteFloat(sb, f);
	else if (flags & PRFL_INT32COORD)
		MSG_WriteLong(sb, Q_rint(f * 16));
	else if (flags & PRFL_24BITCOORD)
		MSG_WriteCoord24(sb, f);
	else 
		MSG_WriteCoord16(sb, f);
}

void MSG_WriteAngle (sizebuf_t *sb, float f, unsigned int flags)
{
	if (flags & PRFL_FLOATANGLE)
		MSG_WriteFloat(sb, f);
	else if (flags & PRFL_SHORTANGLE)
		MSG_WriteShort(sb, Q_rint(f * 65536.0 / 360.0) & 65535);
	else
		MSG_WriteByte (sb, Q_rint(f * 256.0 / 360.0) & 255);
}

//johnfitz -- for PROTOCOL_FITZQUAKE
void MSG_WriteAngle16(sizebuf_t *sb, float f, unsigned int flags)
{
	if (flags & PRFL_FLOATANGLE)
		MSG_WriteFloat(sb, f);
	else 
		MSG_WriteShort(sb, Q_rint(f * 65536.0 / 360.0) & 65535);
}
//johnfitz

// reading functions
int		msg_readcount;
qboolean	msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	if (msg_readcount >= net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	return (signed char)net_message.data[msg_readcount++];
}

int MSG_ReadByte (void)
{
	if (msg_readcount >= net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	return (unsigned char)net_message.data[msg_readcount++];
}

int MSG_ReadShort (void)
{
	int	c;

	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong (void)
{
	int	c;

	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1] << 8)
	+ (net_message.data[msg_readcount+2] << 16)
	+ (net_message.data[msg_readcount+3] << 24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat (void)
{
	union
	{
		byte	b[4];
		float	f;
		int	l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount+1];
	dat.b[2] = net_message.data[msg_readcount+2];
	dat.b[3] = net_message.data[msg_readcount+3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;   
}

char *MSG_ReadString (void)
{
	static	char	string[2048];
	int		l, c;

	l = 0;
	do {
		c = MSG_ReadChar ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
float MSG_ReadCoord16(void)
{
	return MSG_ReadShort() * (1.0 / 8);
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
float MSG_ReadCoord24(void)
{
	return MSG_ReadShort() + MSG_ReadByte() * (1.0 / 255);
}

//johnfitz -- 32-bit float coords
float MSG_ReadCoord32f(void)
{
	return MSG_ReadFloat();
}

float MSG_ReadCoord(unsigned int flags)
{
	if (flags & PRFL_FLOATCOORD)
		return MSG_ReadFloat();
	else if (flags & PRFL_INT32COORD)
		return MSG_ReadLong() * (1.0 / 16.0);
	else if (flags & PRFL_24BITCOORD)
		return MSG_ReadCoord24();
	else 
		return MSG_ReadCoord16();
}

float MSG_ReadAngle(unsigned int flags)
{
	if (flags & PRFL_FLOATANGLE)
		return MSG_ReadFloat();
	else if (flags & PRFL_SHORTANGLE)
		return MSG_ReadShort() * (360.0 / 65536);
	else 
		return MSG_ReadChar() * (360.0 / 256);
}

//johnfitz -- for PROTOCOL_FITZQUAKE
float MSG_ReadAngle16(unsigned int flags)
{
	if (flags & PRFL_FLOATANGLE)
		return MSG_ReadFloat();	// make sure
	else 
		return MSG_ReadShort() * (360.0 / 65536);
}
//johnfitz

//===========================================================================

void SZ_Alloc (sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = Hunk_AllocName (startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}

void SZ_Free (sizebuf_t *buf)
{
//      Z_Free (buf->data);
//      buf->data = NULL;
//      buf->maxsize = 0;
	buf->cursize = 0;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void	*data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf ("SZ_GetSpace: overflow");
		SZ_Clear (buf); 
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write (sizebuf_t *buf, void *data, int length)
{
	memcpy (SZ_GetSpace(buf, length), data, length);
}

void SZ_Print (sizebuf_t *buf, char *data)
{
	int	len;

	len = strlen(data) + 1;

// byte * cast to keep VC++ happy
	if (buf->data[buf->cursize-1])
		memcpy ((byte *)SZ_GetSpace(buf, len), data, len);	// no trailing 0
	else
		memcpy ((byte *)SZ_GetSpace(buf, len - 1) - 1, data, len); // write over trailing 0
}

//============================================================================

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char    *last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/' || *pathname == '\\')
			last = pathname + 1;
		pathname++;
	}

	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	char	*dot;

	if (!(dot = strrchr(in, '.')))
	{
		Q_strncpyz (out, in, strlen(in) + 1);
		return;
	}

	while (*in && in != dot)
		*out++ = *in++;

	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static	char	exten[8];
	int		i;

	if (!(in = strrchr(in, '.')))
		return "";
	in++;

	for (i=0 ; i<7 && *in ; i++, in++)
		exten[i] = *in;
	exten[i] = 0;

	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char	*s, *s2;
	
	s = in + strlen(in) - 1;
	
	while (s != in && *s != '.')
		s--;
	
	for (s2 = s ; *s2 && *s2 != '/' ; s2--)
		;
	
	if (s-s2 < 2)
		strcpy (out, "?model?");
	else
	{
		s--;
		strncpy (out, s2+1, s-s2);
		out[s-s2] = 0;
	}
}


/*
==================
COM_ForceExtension

If path doesn't have an extension or has a different extension, append(!) specified extension
Extension should include the .
==================
*/
void COM_ForceExtension (char *path, char *extension)
{
	char    *src;

	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src-- == '.')
		{
			COM_StripExtension (path, path);
			strcat (path, extension);
			return;
		}
	}

	strncat (path, extension, MAX_OSPATH);
}

/*
==================
COM_DefaultExtension

If path doesn't have an extension, append extension
Extension should include the .
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;

	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
		if (*src-- == '.')
			return;			// it has an extension

	strncat (path, extension, MAX_OSPATH);
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int	c, len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;		// end of file;
		data++;
	}
	
// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		// joe, from ProQuake: removed ':' so that ip:port works
		if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' /*|| c==':'*/)
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (char *parm)
{
	int	i;

	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;	// NEXTSTEP sometimes clears appkit vars.
		if (!strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

/*
================
COM_CheckRegistered

Looks for the pop.lmp file and verifies it.
Sets the "registered" cvar.
================
*/
void COM_CheckRegistered (void)
{
	FILE	*h;

	COM_FOpenFile ("gfx/pop.lmp", &h);
	if (h)
	{
		Cvar_Set (&cmdline, com_cmdline);
		Cvar_SetValue (&registered, 1);
		fclose (h);
	}
}

int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc)
		return "";
	return com_argv[arg];
}

/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	qboolean	safe;
	int		i, j, n;

// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for (j = 0 ; j < MAX_NUM_ARGVS && j < argc ; j++)
	{
		i = 0;

		while ((n < (CMDLINE_LENGTH - 1)) && argv[j][i])
			com_cmdline[n++] = argv[j][i++];

		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}

	com_cmdline[n] = 0;

	safe = false;

	for (com_argc = 0 ; com_argc < MAX_NUM_ARGVS && com_argc < argc ; com_argc++)
	{
		largv[com_argc] = argv[com_argc];
		if (!strcmp("-safe", argv[com_argc]))
			safe = true;
	}

	if (safe)
	{
	// force all the safe-mode switches. Note that we reserved extra space in
	// case we need to add these, so we don't need an overflow check
		for (i=0 ; i<NUM_SAFE_ARGVS ; i++)
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;

	if (COM_CheckParm("-rogue"))
		rogue = 1;

	if (COM_CheckParm("-hipnotic"))
		hipnotic = 1;

#ifdef GLQUAKE
	if (COM_CheckParm("-nehahra"))
		nehahra = 1;
#endif

	if (COM_CheckParm("-runequake"))
		runequake = 1;

	if (hipnotic && rogue)
		Sys_Error ("You can't run both mission packs at the same time");
}

/*
================
COM_Init
================
*/
void COM_Init (char *basedir)
{
	byte    swaptest[2] = {1, 0};

// set the byte swapping variables in a portable manner 
	if (*(short *)swaptest == 1)
	{
		bigendien = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cvar_Register (&registered);
	Cvar_Register (&cmdline);

	Cmd_AddCommand ("path", COM_Path_f);
	Cmd_AddCommand ("gamedir", COM_Gamedir_f);

	COM_InitFilesystem ();
	COM_CheckRegistered ();
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va (char *format, ...)
{
	va_list         argptr;
	static	char	string[8][2048];		
	static	int	idx = 0;

	idx++;
	if (idx == 8)
		idx = 0;

	va_start (argptr, format);
	vsnprintf (string[idx], sizeof(string[idx]), format, argptr);
	va_end (argptr);

	return string[idx];
}

char *CopyString (char *in)
{
	char	*out;
	
	out = Z_Malloc (strlen(in) + 1);
	strcpy (out, in);

	return out;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int     com_filesize;
char	com_netpath[MAX_OSPATH];

filetype_t com_filetype;

// on disk
typedef struct
{
	char	name[56];
	int	filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int	dirofs;
	int	dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK	2048

char	com_gamedir[MAX_OSPATH];
char	com_basedir[MAX_OSPATH];
char	com_gamedirname[MAX_OSPATH];

searchpath_t	*com_base_searchpaths;	// without id1 and its packs

/*
================
COM_FileLength
================
*/
int COM_FileLength (FILE *f)
{
	int	pos, end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/*
=================
COM_FileOpenRead
=================
*/
int COM_FileOpenRead (char *path, FILE **hndl)
{
	FILE	*f;

	if (!(f = fopen(path, "rb")))
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_FileLength (f);
}

/*
=================
COM_FindFile

finds files in given path including inside paks as well
=================
*/
qboolean COM_FindFile (char *filename)
{
	int		i;
	char		netpath[MAX_OSPATH];
	searchpath_t	*search;
	pack_t		*pak;

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (search->pack)
		{
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
				if (!strcmp(pak->files[i].name, filename))
					return true;
		}
		else
		{
			Q_snprintfz (netpath, sizeof(netpath), "%s/%s", search->filename, filename);
			if (Sys_FileTime(netpath) != -1)
				return true;
		}
	}

	return false;
}

/*
============
COM_Path_f
============
*/
void COM_Path_f (void)
{
	searchpath_t	*s;

	Con_Printf ("Current search path:\n");
	for (s = com_searchpaths ; s ; s = s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("------------\n");
		if (s->pack)
			Con_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Con_Printf ("%s\n", s->filename);
	}
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
qboolean COM_WriteFile (char *filename, void *data, int len)
{
	FILE	*f;
	char	name[MAX_OSPATH];

	if (!COM_IsAbsolutePath(filename))
	{
		Q_snprintfz (name, MAX_OSPATH, "%s/%s", com_basedir, filename);
	}
	else
	{
		Q_strncpyz (name, filename, MAX_OSPATH);
	}

	if (!(f = fopen(name, "wb")))
	{
		COM_CreatePath (name);
		if (!(f = fopen(name, "wb")))
		{
			Con_Printf ("ERROR: Couldn't open %s\n", filename);
			return false;
		}
	}	

	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);

	return true;
}

/*
============
COM_CreatePath

Only used for CopyFile
============
*/
void COM_CreatePath (char *path)
{
	char	*ofs;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{       // create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}

/*
============
COM_IsAbsolutePath

Checks whether the given path is absolute or relative.
============
*/
qboolean COM_IsAbsolutePath (char *path)
{
	return strstr(path, ":\\") != NULL || strstr(path, ":/") != NULL;
}

/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed. This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	*in, *out;
	int	remaining, count;
	char	buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	COM_CreatePath (cachepath);	// create directories up to the cache file
	if (!(out = fopen(cachepath, "wb")))
		Sys_Error ("Error opening %s", cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}

int SearchFileInPak(char *filename, FILE **file, searchpath_t *search)
{
	int i;
	pack_t *pak;

	// look through all the pak file elements
	pak = search->pack;
	for (i=0 ; i<pak->numfiles ; i++)
	{
		if (!strcmp(pak->files[i].name, filename))	// found it!
		{
			if (developer.value)
				Sys_Printf ("PackFile: %s : %s\n", pak->filename, filename);
			// open a new file on the pakfile
			if (!(*file = fopen(pak->filename, "rb")))
				Sys_Error ("Couldn't reopen %s", pak->filename);
			fseek (*file, pak->files[i].filepos, SEEK_SET);
			com_filesize = pak->files[i].filelen;

			Q_snprintfz (com_netpath, sizeof(com_netpath), "%s#%i", pak->filename, i);
			return com_filesize;
		}
	}

	return -1;
}

/*
=================
COM_FOpenFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
=================
*/
int COM_FOpenFile (char *filename, FILE **file)
{
	qboolean image = false;
	searchpath_t *search;

	com_filesize = -1;
	com_netpath[0] = 0;

	if (!strcmp(COM_FileExtension(filename), "tga"))
	{
		image = true;
	}

	// search through the path, one element at a time
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (image)
		{
			com_filetype = image_TGA;
			COM_ForceExtension(filename, ".tga");
		}

		// is the element a pak file?
		if (search->pack)
		{
			if (SearchFileInPak(filename, file, search) != -1)
			{
				return com_filesize;
			}

			// try to search for various types of image
			if (image)
			{
				com_filetype = image_PNG;
				COM_ForceExtension(filename, ".png");
				if (SearchFileInPak(filename, file, search) != -1)
				{
					return com_filesize;
				}

				com_filetype = image_JPG;
				COM_ForceExtension(filename, ".jpg");
				if (SearchFileInPak(filename, file, search) != -1)
				{
					return com_filesize;
				}
			}
		}
		else
		{               
			// check a file in the directory tree
			Q_snprintfz (com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

			if (!(*file = fopen(com_netpath, "rb")))
			{
				// try to search for various types of image
				if (image)
				{
					com_filetype = image_PNG;
					COM_ForceExtension(filename, ".png");
					Q_snprintfz (com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

					if (!(*file = fopen(com_netpath, "rb")))
					{
						com_filetype = image_JPG;
						COM_ForceExtension(filename, ".jpg");
						Q_snprintfz (com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

						if (!(*file = fopen(com_netpath, "rb")))
						{
							continue;
						}
					}
				}
				else
				{
					continue;
				}
			}

			if (developer.value)
				Sys_Printf ("FOpenFile: %s\n", com_netpath);

			com_filesize = COM_FileLength (*file);
			return com_filesize;
		}
		
	}

	if (developer.value)
		Sys_Printf ("COM_FOpenFile: can't find %s\n", filename);

	*file = NULL;
	com_filesize = -1;

	return -1;
}

/*
=================
COM_LoadFile

Filename are reletive to the quake directory.
Always appends a 0 byte.
=================
*/
cache_user_t 	*loadcache;
byte    	*loadbuf;
int             loadsize;

byte *COM_LoadFile (char *path, int usehunk)
{
	FILE	*h;
	byte	*buf;
	char	base[32];
	int	len;

	buf = NULL;     // quiet compiler warning

	// look for it in the filesystem or pack files
	len = COM_FOpenFile (path, &h);
	if (!h)
		return NULL;

	// extract the filename base name for hunk tag
	COM_FileBase (path, base);
	
	if (usehunk == 1)
	{
		buf = Hunk_AllocName (len + 1, base);
	}
	else if (usehunk == 2)
	{
		buf = Hunk_TempAlloc (len + 1);
	}
	else if (usehunk == 0)
	{
		buf = Z_Malloc (len + 1);
	}
	else if (usehunk == 3)
	{
		buf = Cache_Alloc (loadcache, len + 1, base);
	}
	else if (usehunk == 4)
	{
		if (len + 1 > loadsize)
			buf = Hunk_TempAlloc (len + 1);
		else
			buf = loadbuf;
	}
	else
	{
		Sys_Error ("COM_LoadFile: bad usehunk");
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);
		
	((byte *)buf)[len] = 0;

	Draw_BeginDisc ();
	fread (buf, 1, len, h);
	fclose (h);
	Draw_EndDisc ();

	return buf;
}

byte *COM_LoadHunkFile (char *path)
{
	return COM_LoadFile (path, 1);
}

byte *COM_LoadTempFile (char *path)
{
	return COM_LoadFile (path, 2);
}

void COM_LoadCacheFile (char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile (char *path, void *buffer, int bufsize)
{
	byte	*buf;
	
	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4);
	
	return buf;
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *COM_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	packfile_t	*newfiles;
	int		i, numpackfiles;
	pack_t		*pack;
	FILE		*packhandle;
	dpackfile_t	info[MAX_FILES_IN_PACK];

	if (COM_FileOpenRead(packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (memcmp(header.id, "PACK", 4))
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	newfiles = Q_malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (&info, 1, header.dirlen, packhandle);

	// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong (info[i].filepos);
		newfiles[i].filelen = LittleLong (info[i].filelen);
	}

	pack = Q_malloc (sizeof(pack_t));
	Q_strncpyz (pack->filename, packfile, sizeof(pack->filename));
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	
	Con_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);

	return pack;
}

/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void COM_AddGameDirectory (char *dir)
{
	int		i;
	searchpath_t	*search;
	pack_t		*pak;
	char		pakfile[MAX_OSPATH], *p;

	if ((p = strrchr(dir, '/')))
		Q_strncpyz (com_gamedirname, ++p, sizeof(com_gamedirname));
	else
		Q_strncpyz (com_gamedirname, p, sizeof(com_gamedirname));
	Q_strncpyz (com_gamedir, dir, sizeof(com_gamedir));

// add the directory to the search path
	search = Q_malloc (sizeof(searchpath_t));
	Q_strncpyz (search->filename, dir, sizeof(search->filename));
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

// add any pak files in the format pak0.pak pak1.pak, ...
	for (i=0 ; ; i++)
	{
		Q_snprintfz (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		if (!(pak = COM_LoadPackFile(pakfile)))
			break;
		search = Q_malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;               
	}

	// initializing demodir
	Q_snprintfz (demodir, sizeof(demodir), "/%s", com_gamedirname);
}

/*
================
COM_SetGameDir

Sets the gamedir and path to a different directory.
================
*/
void COM_SetGameDir (char *dir)
{
	int		i;
	searchpath_t	*search, *next;
	pack_t		*pak;
	char		pakfile[MAX_OSPATH];

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":"))
	{
		Con_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp(com_gamedirname, dir))
		return;		// still the same

	Q_strncpyz (com_gamedirname, dir, sizeof(com_gamedirname));

	// free up any current game dir info
	while (com_searchpaths != com_base_searchpaths)
	{
		if (com_searchpaths->pack)
		{
			fclose (com_searchpaths->pack->handle);
			free (com_searchpaths->pack->files);
			free (com_searchpaths->pack);
		}
		next = com_searchpaths->next;
		free (com_searchpaths);
		com_searchpaths = next;
	}

	// flush all data, so it will be forced to reload
	Cache_Flush ();

	Q_snprintfz (com_gamedir, sizeof(com_gamedir), "%s/%s", com_basedir, dir);

	if (!strcmp(dir, "id1") || !strcmp(dir, "joequake"))
		return;

// add the directory to the search path
	search = Q_malloc (sizeof(searchpath_t));
	Q_strncpyz (search->filename, com_gamedir, sizeof(search->filename));
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

// add any pak files in the format pak0.pak pak1.pak, ...
	for (i=0 ; ; i++)
	{
		Q_snprintfz (pakfile, sizeof(pakfile), "%s/pak%i.pak", com_gamedir, i);
		if (!(pak = COM_LoadPackFile(pakfile)))
			break;
		search = Q_malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;               
	}
}

/*
================
COM_Gamedir_f

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir_f (void)
{
	char	*dir;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Current gamedir: %s\n", com_gamedirname);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: gamedir <newdir>\n");
		return;
	}

	dir = Cmd_Argv (1);

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_Printf ("gamedir should be a single filename, not a path\n");
		return;
	}

	if (cls.state != ca_disconnected)
	{
		Con_Printf ("you must disconnect before changing gamedir\n");
		return;
	}

	COM_SetGameDir (dir);
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem (void)
{
	int	i;

// -basedir <path>
// Overrides the system supplied base directory (under GAMENAME)
	if ((i = COM_CheckParm("-basedir")) && i + 1 < com_argc)
		Q_strncpyz (com_basedir, com_argv[i+1], sizeof(com_basedir));
	else
		Q_strncpyz (com_basedir, host_parms.basedir, sizeof(com_basedir));

	for (i=0 ; i < strlen(com_basedir) ; i++)
		if (com_basedir[i] == '\\')
			com_basedir[i] = '/';

	i = strlen(com_basedir) - 1;
	if (i >= 0 && com_basedir[i] == '/')
		com_basedir[i] = 0;

	// start up with GAMENAME by default (id1)
	COM_AddGameDirectory (va("%s/"GAMENAME, com_basedir));
	COM_AddGameDirectory (va("%s/joequake", com_basedir));

	if (COM_CheckParm("-rogue"))
		COM_AddGameDirectory (va("%s/rogue", com_basedir));
	if (COM_CheckParm("-hipnotic"))
		COM_AddGameDirectory (va("%s/hipnotic", com_basedir));

#ifdef GLQUAKE
	if (COM_CheckParm("-nehahra"))
        	COM_AddGameDirectory (va("%s/nehahra", com_basedir));
#endif

	if (COM_CheckParm("-runequake"))
		COM_AddGameDirectory (va("%s/runequake", com_basedir));

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

// -game <gamedir>
// Adds basedir/gamedir as an override game
	if ((i = COM_CheckParm("-game")) && i + 1 < com_argc)
        	COM_AddGameDirectory (va("%s/%s", com_basedir, com_argv[i+1]));
}
