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
// common.h  -- general definitions

#ifdef _WIN32
#pragma warning (disable : 4244 4127 4201 4214 4514 4305 4115 4018)
#endif

#if !defined BYTE_DEFINED
typedef unsigned char 		byte;
#define BYTE_DEFINED 1
#endif

#undef	true
#undef	false

typedef enum {false, true}	qboolean;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define bound(a, b, c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))

//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte		*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

//============================================================================

#define Q_MAXCHAR	((char)0x7f)
#define Q_MAXSHORT	((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG	((int)0x7fffffff)
#define Q_MAXFLOAT	((int)0x7fffffff)

#define Q_MINCHAR	((char)0x80)
#define Q_MINSHORT	((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG	((int)0x80000000)
#define Q_MINFLOAT	((int)0x7fffffff)

//============================================================================

extern qboolean bigendien;

extern	short	(*BigShort)(short l);
extern	short	(*LittleShort)(short l);
extern	int	(*BigLong)(int l);
extern	int	(*LittleLong)(int l);
extern	float	(*BigFloat)(float l);
extern	float	(*LittleFloat)(float l);

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord(sizebuf_t *sb, float f, unsigned int flags);
void MSG_WriteAngle(sizebuf_t *sb, float f, unsigned int flags);
void MSG_WriteAngle16(sizebuf_t *sb, float f, unsigned int flags); //johnfitz

extern	int		msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading (void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);

float MSG_ReadCoord(unsigned int flags);
float MSG_ReadAngle(unsigned int flags);
float MSG_ReadAngle16(unsigned int flags); //johnfitz

//============================================================================

#ifdef _WIN32

#define	vsnprintf _vsnprintf

#define Q_strcasecmp(s1, s2) _stricmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))

#else

#define Q_strcasecmp(s1, s2) strcasecmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) strncasecmp((s1), (s2), (n))

#endif

void Q_strcpy (char *dest, char *src);
void Q_strncpy (char *dest, char *src, int count);
int Q_atoi (char *str);
float Q_atof (char *str);

void Q_strncpyz (char *dest, char *src, size_t size);
void Q_snprintfz (char *dest, size_t size, char *fmt, ...);
size_t Q_strlcat(char *dst, const char *src, size_t siz);
size_t Q_strlcpy(char *dst, const char *src, size_t siz);

static inline int Q_isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

static inline int Q_isblank(int c)
{
	return (c == ' ' || c == '\t');
}

static inline int Q_isspace(int c)
{
	switch (c) {
	case ' ':  case '\t':
	case '\n': case '\r':
	case '\f': case '\v': return 1;
	}
	return 0;
}

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *COM_Parse (char *data);


extern	int	com_argc;
extern	char	**com_argv;

int COM_CheckParm (char *parm);
int COM_Argc (void);
char *COM_Argv (int arg);	// range and null checked
void COM_InitArgv (int argc, char **argv);
void COM_Init (char *path);

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
char *COM_FileExtension (char *in);
void COM_FileBase(const char *in, char *out, size_t outsize);
void COM_DefaultExtension (char *path, char *extension);

char *va (char *format, ...);
// does a varargs printf into a temp buffer

char *CopyString (char *s);

unsigned COM_HashString(const char *str);

// localization support for 2021 rerelease version:
void LOC_Init(void);
void LOC_Shutdown(void);
const char* LOC_GetRawString(const char *key);
const char* LOC_GetString(const char *key);
qboolean LOC_HasPlaceholders(const char *str);
size_t LOC_Format(const char *format, const char* (*getarg_fn)(int idx, void* userdata), void* userdata, char* out, size_t len);

//============================================================================

extern	int		com_filesize;
extern	char	com_netpath[MAX_OSPATH];

typedef enum { image_TGA, image_PNG, image_JPG, other } filetype_t;

extern	filetype_t com_filetype;

struct	cache_user_s;

extern	char	com_gamedir[MAX_OSPATH];
extern	char	com_basedir[MAX_OSPATH];

typedef struct
{
	char	name[MAX_QPATH];
	int	filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int	numfiles;
	packfile_t *files;
} pack_t;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*com_searchpaths;

void COM_ForceExtension (char *path, char *extension);
int COM_FileLength (FILE *f);
void COM_CreatePath (char *path);
qboolean COM_FindFile (char *filename);
qboolean COM_WriteFile (char *filename, void *data, int len);
int COM_FOpenFile (char *filename, FILE **file);
qboolean COM_IsAbsolutePath (char *path);

byte *COM_LoadFile (char *path, int usehunk);
byte *COM_LoadStackFile (char *path, void *buffer, int bufsize);
byte *COM_LoadTempFile (char *path);
byte *COM_LoadHunkFile (char *path);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu);
byte *COM_LoadMallocFile(char *path);

extern	struct	cvar_s	registered;

extern	int	rogue, hipnotic, nehahra, runequake, machine;
