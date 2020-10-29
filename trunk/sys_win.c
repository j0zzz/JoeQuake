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
// sys_win.c -- Win32 system interface code

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include "conproc.h"
#include <limits.h>
#include <errno.h>
#include <direct.h>		// _mkdir
#include <conio.h>		// _putch

#include <fcntl.h>
#include <sys/stat.h>

#define MINIMUM_WIN_MEMORY	0x04000000	// 64 MB
#define MAXIMUM_WIN_MEMORY	0x10000000	// 256 MB

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error running
					// dedicated before exiting
#define PAUSE_SLEEP			50	// sleep time on pause or minimization

int		starttime;
qboolean	ActiveApp, Minimized;
qboolean	WinNT;

static	int	lowshift;
qboolean	isDedicated;
static qboolean	sc_return_on_enter = false;
HANDLE		hinput, houtput;

static	char	*tracking_tag = "Clams & Mooses";

static	HANDLE	tevent;
static	HANDLE	hFile;
static	HANDLE	heventParent;
static	HANDLE	heventChild;

void MaskExceptions (void);
void Sys_InitDoubleTime (void);
void Sys_PushFPCW_SetHigh (void);
void Sys_PopFPCW (void);

/*
===============================================================================

SYNCHRONIZATION

===============================================================================
*/

int	hlock;
_CRTIMP int __cdecl _open(const char *, int, ...);
_CRTIMP int __cdecl _close(int);

/*
================
Sys_GetLock
================
*/
void Sys_GetLock (void)
{
	int	i;

	for (i=0 ; i<10 ; i++)
	{
		hlock = _open (va("%s/lock.dat", com_gamedir), _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
		if (hlock != -1)
			return;
		Sleep (1000);
	}

	Sys_Printf ("Warning: could not open lock; using crowbar\n");
}

/*
================
Sys_ReleaseLock
================
*/
void Sys_ReleaseLock (void)
{
	if (hlock != -1)
		_close (hlock);
	_unlink (va("%s/lock.dat", com_gamedir));
}

/*
===============================================================================

FILE IO

===============================================================================
*/

int Sys_FileTime (char *path)
{
	FILE	*f;
	int	retval;
#ifndef GLQUAKE
	int	t;

	t = VID_ForceUnlockedAndReturnState ();
#endif

	if ((f = fopen(path, "rb")))
	{
		fclose (f);
		retval = 1;
	}
	else
	{
		retval = -1;
	}

#ifndef GLQUAKE
	VID_ForceLockState (t);
#endif
	return retval;
}

void Sys_mkdir (char *path)
{
	_mkdir (path);
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	DWORD  flOldProtect;

	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect))
		Sys_Error ("Protection change failed\n");
}

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFOEXA vinfo;

	MaskExceptions ();
	Sys_SetFPCW ();

	Sys_InitDoubleTime ();

	// check for proper windows version
	ZeroMemory(&vinfo, sizeof(OSVERSIONINFOEXA));
	vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
	vinfo.dwMajorVersion = 4;
	vinfo.dwPlatformId = VER_PLATFORM_WIN32s;

	DWORDLONG dwlConditionMask = 0;
	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(dwlConditionMask, VER_PLATFORMID, VER_GREATER_EQUAL);

	if (!VerifyVersionInfo(&vinfo, VER_MAJORVERSION | VER_PLATFORMID, dwlConditionMask))
		Sys_Error("Quake requires at least Win95 or NT 4.0");

	// check if NT
	ZeroMemory(&vinfo, sizeof(OSVERSIONINFOEXA));
	vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
	vinfo.dwPlatformId = VER_PLATFORM_WIN32_NT;

	dwlConditionMask = 0;
	VER_SET_CONDITION(dwlConditionMask, VER_PLATFORMID, VER_EQUAL);

	WinNT = VerifyVersionInfo(&vinfo, VER_PLATFORMID, dwlConditionMask);
}

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024], text2[1024];
	char		*text3 = "Press Enter to exit\n";
	char		*text4 = "***********************************\n";
	char		*text5 = "\n";
	DWORD		dummy;
	double		starttime;
	static	int	in_sys_error0 = 0, in_sys_error1 = 0, in_sys_error2 = 0, in_sys_error3 = 0;

	if (!in_sys_error3)
	{
		in_sys_error3 = 1;
#ifndef GLQUAKE
		VID_ForceUnlockedAndReturnState ();
#endif
	}

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	if (isDedicated)
	{
		va_start (argptr, error);
		vsnprintf (text, sizeof(text), error, argptr);
		va_end (argptr);

		sprintf (text2, "ERROR: %s\n", text);
		WriteFile (houtput, text5, strlen(text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen(text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen(text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen(text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen(text4), &dummy, NULL);

		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;	// so Enter will get us out of here
	}
	else
	{
	// switch to windowed so the message box is visible, unless we already
	// tried that and failed
		if (!in_sys_error0)
		{
			in_sys_error0 = 1;
			VID_SetDefaultMode ();
			MessageBox (NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
		else
		{
			MessageBox (NULL, text, "Double Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
	}

	if (!in_sys_error1)
	{
		in_sys_error1 = 1;
		Host_Shutdown ();
	}

	// shut down QHOST hooks if necessary
	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
		DeinitConProc ();
	}

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list	argptr;
	char	text[2048];
	DWORD	dummy;
	
	if (isDedicated)
	{
		va_start (argptr, fmt);
		vsnprintf (text, sizeof(text), fmt, argptr);
		va_end (argptr);

		WriteFile (houtput, text, strlen(text), &dummy, NULL);

		// joe, from ProQuake: rcon (64 doesn't mean anything special,
		// but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
		if (rcon_active  && (rcon_message.cursize < rcon_message.maxsize - strlen(text) - 64))
		{
			rcon_message.cursize--;
			MSG_WriteString (&rcon_message, text);
		}
	}
}

void Sys_Quit (void)
{
#ifndef GLQUAKE
	VID_ForceUnlockedAndReturnState ();
#endif

	Host_Shutdown ();

	if (tevent)
		CloseHandle (tevent);

	if (isDedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}

// joe: not using just float for timing any more,
// this is added from ZQuake source to fix overspeeding.

static	double	pfreq;
static qboolean	hwtimer = false;

/*
================
Sys_InitDoubleTime
================
*/
void Sys_InitDoubleTime (void)
{
	__int64	freq;

	if (!COM_CheckParm("-nohwtimer") && QueryPerformanceFrequency ((LARGE_INTEGER *)&freq) && freq > 0)
	{
		// hardware timer available
		pfreq = (double)freq;
		hwtimer = true;
	}
	else
	{
		// make sure the timer is high precision, otherwise
		// NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	__int64		pcount;
	static	__int64	startcount;
	static	DWORD	starttime;
	static qboolean	first = true;
	DWORD	now;

	if (hwtimer)
	{
		QueryPerformanceCounter ((LARGE_INTEGER *)&pcount);
		if (first)
		{
			first = false;
			startcount = pcount;
			return 0.0;
		}
		// TODO: check for wrapping
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime ();

	if (first)
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}

char *Sys_ConsoleInput (void)
{
	int		dummy, ch, numread, numevents;
	static char	text[256];
	static int	len;
	INPUT_RECORD	recs[1024];

	if (!isDedicated)
		return NULL;

	for ( ; ; )
	{
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
				case '\r':
					WriteFile (houtput, "\r\n", 2, &dummy, NULL);

					if (len)
					{
						text[len] = 0;
						len = 0;
						return text;
					}
					else if (sc_return_on_enter)
					{
					// special case to allow exiting from the error handler on Enter
						text[0] = '\r';
						len = 0;
						return text;
					}
					break;

				case '\b':
					WriteFile (houtput, "\b \b", 3, &dummy, NULL);
					if (len)
						len--;
					break;

				default:
					if (ch >= ' ')
					{
						WriteFile (houtput, &ch, 1, &dummy, NULL);
						text[len] = ch;
						len = (len + 1) & 0xff;
					}
					break;
				}
			}
		}
	}

	return NULL;
}

void Sys_Sleep (void)
{
	Sleep (1);
}

void Sys_SendKeyEvents (void)
{
	MSG	msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
	// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		if (!GetMessage(&msg, NULL, 0, 0))
			Sys_Quit ();

		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
}

#define	SYS_CLIPBOARD_SIZE	256

char *Sys_GetClipboardData (void)
{
	HANDLE		th;
	char		*cliptext, *s, *t;
	static	char	clipboard[SYS_CLIPBOARD_SIZE];

	if (!OpenClipboard(NULL))
		return NULL;

	if (!(th = GetClipboardData(CF_TEXT)))
	{
		CloseClipboard ();
		return NULL;
	}

	if (!(cliptext = GlobalLock(th)))
	{
		CloseClipboard ();
		return NULL;
	}

	s = cliptext;
	t = clipboard;
	while (*s && t - clipboard < SYS_CLIPBOARD_SIZE - 1 && *s != '\n' && *s != '\r' && *s != '\b')
		*t++ = *s++;
	*t = 0;

	GlobalUnlock (th);
	CloseClipboard ();

	return clipboard;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

void SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE	global_hInstance;
int		global_nCmdShow;
char		*argv[MAX_NUM_ARGVS], *argv0;
HWND		hwnd_dialog;
static	char	*empty_string = "";

/*
==================
WinMain
==================
*/
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int		t;
	char		*ch;
	static	char	cwd[1024];
	double		time, oldtime, newtime;
	quakeparms_t	parms;
	MEMORYSTATUS	lpBuffer;
	RECT		rect;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	if (!GetCurrentDirectory(sizeof(cwd), cwd))
		Sys_Error ("Couldn't determine current directory");

	if (cwd[strlen(cwd)-1] == '/')
		cwd[strlen(cwd)-1] = 0;

	parms.basedir = cwd;
	parms.argc = 1;
	argv[0] = empty_string;

	// joe, from ProQuake: isolate the exe name, eliminate quotes
	argv0 = GetCommandLine ();
	lpCmdLine[-1] = 0;
	if (argv0[0] == '\"')
		argv0++;
	if ((ch = strchr(argv0, '\"')))
		*ch = 0;

	while (*lpCmdLine && (parms.argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[parms.argc] = lpCmdLine;
			parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	parms.argv = argv;

	COM_InitArgv (parms.argc, parms.argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	if (!(isDedicated = COM_CheckParm("-dedicated")))
	{
		hwnd_dialog = CreateDialog (hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);

		if (hwnd_dialog)
		{
			if (GetWindowRect(hwnd_dialog, &rect))
			{
				if (rect.left > (rect.top * 2))
				{
					SetWindowPos (hwnd_dialog, 0,
						(rect.left / 2) - ((rect.right - rect.left) / 2),
						rect.top, 0, 0,
						SWP_NOZORDER | SWP_NOSIZE);
				}
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	}

// take the greater of all the available memory or half the total memory,
// but at least 8 Mb and no more than 16 Mb, unless they explicitly request otherwise
	parms.memsize = lpBuffer.dwAvailPhys;

	if (parms.memsize < MINIMUM_WIN_MEMORY)
		parms.memsize = MINIMUM_WIN_MEMORY;

	if (parms.memsize < (lpBuffer.dwTotalPhys >> 1))
		parms.memsize = lpBuffer.dwTotalPhys >> 1;

	if (parms.memsize > MAXIMUM_WIN_MEMORY)
		parms.memsize = MAXIMUM_WIN_MEMORY;

	if ((t = COM_CheckParm("-heapsize")) && t + 1 < com_argc)
		parms.memsize = Q_atoi(com_argv[t+1]) * 1024;

	if ((t = COM_CheckParm("-mem")) && t + 1 < com_argc)
		parms.memsize = Q_atoi(com_argv[t+1]) * 1024 * 1024;

	parms.membase = Q_malloc (parms.memsize);

	if (!(tevent = CreateEvent(NULL, FALSE, FALSE, NULL)))
		Sys_Error ("Couldn't create event");

	if (isDedicated)
	{
		if (!AllocConsole())
			Sys_Error ("Couldn't create dedicated server console");

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm("-HFILE")) > 0)
		{
			if (t < com_argc)
				hFile = (HANDLE)Q_atoi(com_argv[t+1]);
		}
			
		if ((t = COM_CheckParm("-HPARENT")) > 0)
		{
			if (t < com_argc)
				heventParent = (HANDLE)Q_atoi(com_argv[t+1]);
		}
			
		if ((t = COM_CheckParm("-HCHILD")) > 0)
		{
			if (t < com_argc)
				heventChild = (HANDLE)Q_atoi(com_argv[t+1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}

	Sys_Init ();

// because sound is off until we become active
	S_BlockSound ();

	Sys_Printf ("Host_Init\n");
	Host_Init (&parms);

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1)
	{
		if (isDedicated)
		{
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate.value)
			{
				Sys_Sleep ();
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}
		}
		else
		{
		// yield the CPU for a little while when paused, minimized, or not the focus
			if ((cl.paused && (!ActiveApp && !DDActive)) || Minimized || block_drawing)
			{
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			}
			else if (!ActiveApp && !DDActive)
			{
				SleepUntilInput (sys_inactivesleep.value);
			}

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
		}

		Host_Frame (time);
		oldtime = newtime;
	}

	// return success of application
	return TRUE;
}
