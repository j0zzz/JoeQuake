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
// sys_linux.c

#if defined(SDL2)
#include <SDL.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#include "quakedef.h"

qboolean isDedicated = false;

int	nostdout = 0;

char	*basedir = ".";

cvar_t  sys_linerefresh = {"sys_linerefresh", "0"};	// set for entity display

/*
===============================================================================

SYNCHRONIZATION

===============================================================================
*/

int	hlock;

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
		hlock = open (va("%s/lock.dat", com_gamedir), O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (hlock != -1)
			return;
		sleep (1);
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
		close (hlock);
	unlink (va("%s/lock.dat", com_gamedir));
}

// =======================================================================
// General routines
// =======================================================================

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[2048];
	unsigned char	*p;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error ("memory overwrite in Sys_Printf");

	if (nostdout)
		return;

	for (p=(unsigned char *)text ; *p ; p++)
	{
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf ("[%02x]", *p);
		else
			putc (*p, stdout);
	}

	// joe, from ProQuake: rcon (64 doesn't mean anything special,
	// but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - strlen(text) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString (&rcon_message, text);
	}
}

void Sys_Quit (void)
{
	Host_Shutdown ();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	fflush (stdout);

	exit (0);
}

void Sys_Error (char *error, ...)
{ 
	va_list	argptr;
	char	string[1024];

// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);
	fprintf (stderr, "Error: %s\n", string);

	Host_Shutdown ();
	exit (1);
} 

void Sys_Warn (char *warning, ...)
{ 
	va_list	argptr;
	char	string[1024];

	va_start (argptr, warning);
	vsnprintf (string, sizeof(string), warning, argptr);
	va_end (argptr);
	fprintf (stderr, "Warning: %s", string);
} 

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime (char *path)
{
	struct	stat	buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

void Sys_mkdir (char *path)
{
	mkdir (path, 0777);
}

double Sys_DoubleTime (void)
{
	struct	timeval		tp;
	struct	timezone	tzp;
	static	int		secbase;

	gettimeofday (&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

// =======================================================================
// Sleeps for microseconds
// =======================================================================

static volatile int	oktogo;

void alarm_handler (int x)
{
	oktogo = 1;
}

void Sys_LineRefresh (void)
{
}

void floating_point_exception_handler (int whatever)
{
//	Sys_Warn ("floating point exception\n");
	signal (SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput (void)
{
	static	char	text[256];
	int     	len;
	fd_set		fdset;
	struct timeval	timeout;

	if (cls.state == ca_dedicated)
	{
		FD_ZERO(&fdset);
		FD_SET(0, &fdset); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select(1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
			return NULL;

		len = read (0, text, sizeof(text));
		if (len < 1)
			return NULL;
		text[len-1] = 0;    // rip off the /n and terminate

		return text;
	}

	return NULL;
}

#if !id386
void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}
#endif

char	*argv0;

int main (int argc, char **argv)
{
	int		j;
	double		time, oldtime, newtime;
	quakeparms_t	parms;
	extern	FILE	*vcrFile;
	extern	int	recording;

//	static char cwd[1024];

//	signal (SIGFPE, floating_point_exception_handler);
	signal (SIGFPE, SIG_IGN);

	memset (&parms, 0, sizeof(parms));

	COM_InitArgv (argc, argv);
	parms.argc = com_argc;
	parms.argv = com_argv;
	argv0 = argv[0];

	parms.memsize = 16 * 1024 * 1024;

	if ((j = COM_CheckParm("-mem")) && j + 1 < com_argc)
		parms.memsize = Q_atoi(com_argv[j+1]) * 1024 * 1024;

	parms.membase = Q_malloc (parms.memsize);
	parms.basedir = basedir;

	fcntl (0, F_SETFL, fcntl(0, F_GETFL, 0) | FNDELAY);

	Host_Init (&parms);
#if id386
	Sys_SetFPCW ();
#endif

	if (COM_CheckParm("-nostdout"))
		nostdout = 1;

	oldtime = Sys_DoubleTime () - 0.1;
	while (1)
	{
// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated)
		{	// play vcrfiles at max speed
			if (time < sys_ticrate.value && (!vcrFile || recording))
			{
				usleep (1);
				continue;       // not time to run a server only tic yet
			}
			time = sys_ticrate.value;
		}

		if (time > sys_ticrate.value * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame (time);

// graphic debugging aids
		if (sys_linerefresh.value)
			Sys_LineRefresh ();
	}
}

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	int		r;
	unsigned long	addr;
	int		psize = getpagesize ();

	addr = (startaddr & ~(psize-1)) - psize;
	r = mprotect ((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
    		Sys_Error ("Protection change failed\n");
}

#define SYS_CLIPBOARD_SIZE	256
static	char	clipboard_buffer[SYS_CLIPBOARD_SIZE] = {0};

#if !defined(SDL2)
char *Sys_GetClipboardData (void)
{
	return clipboard_buffer;
}
#else
char *Sys_GetClipboardData (void)
{
	return SDL_GetClipboardText();
}

qboolean Sys_SetClipboardData (const char *text)
{
	qboolean success;
	success = (SDL_SetClipboardText(text) == 0);
	if (!success)
		Con_Printf("Error setting clipboard: %s\n", SDL_GetError());
	return success;
}
#endif
