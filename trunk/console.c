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
// console.c

#ifdef NeXT
#include <libc.h>
#endif
#ifndef _MSC_VER
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

int 	con_linewidth;

float	con_cursorspeed = 6;	// joe: increased to make it blink faster

#define	CON_TEXTSIZE	32768

qboolean  con_forcedup;		// because no entities to refresh

int		con_totallines;		// total lines in console scrollback
int		con_numlines = 0;	// number of non-blank text lines, used for backscrolling
int		con_backscroll;		// lines up from bottom to display
int		con_current;		// where next message will be printed
int		con_x;				// offset in current line for next print
char	*con_text = 0;

cvar_t	con_notifytime = {"con_notifytime", "3"};	// seconds
cvar_t	_con_notifylines = {"con_notifylines", "4"};
cvar_t	con_notify_intermission = { "con_notify_intermission", "0" };	// console messages get shown in intermission

#define	NUM_CON_TIMES 16
float	con_times[NUM_CON_TIMES];	// realtime time the line was generated for transparent notify lines

int		con_vislines;
int		con_notifylines;		// scan lines to clear for notify lines

FILE	*qconsole_log;

extern	char key_lines[64][MAXCMDLINE];
extern	int	edit_line;
extern	int	key_linepos;
extern	int	key_insert;
		
qboolean con_initialized = false;

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;

	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
			key_dest = key_game;
		else
			M_Menu_Main_f ();
	}
	else
	{
		key_dest = key_console;
	}

	SCR_EndLoadingPlaque ();
	memset (con_times, 0, sizeof(con_times));
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		memset (con_text, ' ', CON_TEXTSIZE);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int	i;

	for (i = 0 ; i < NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

/*
================
Con_MessageMode_f
================
*/
extern qboolean team_message;

void Con_MessageMode_f (void)
{
	key_dest = key_message;
	team_message = false;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	team_message = true;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars, size;
	char	tbuf[CON_TEXTSIZE];

	size = Sbar_GetScaledCharacterSize();
	width = (vid.width / size) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con_text, CON_TEXTSIZE);
		memset (con_text, ' ', CON_TEXTSIZE);

		for (i = 0 ; i < numlines ; i++)
		{
			for (j = 0 ; j < numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con_current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

/*
================
Con_Init
================
*/
void Con_Init (void)
{
	if (COM_CheckParm("-condebug"))
	{
		qconsole_log = fopen (va("%s/joequake/qconsole.log", com_basedir), "a");
		fprintf (qconsole_log, "%s\n-------------------------------------------------------------------\n", LocalTime("%Y-%m-%d %H:%M:%S"));
	}

	con_text = Hunk_AllocName (CON_TEXTSIZE, "context");
	memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize ();

// register our commands
	Cvar_Register (&con_notifytime);
	Cvar_Register (&_con_notifylines);
	Cvar_Register (&con_notify_intermission);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	con_initialized = true;
	
	Con_Printf ("Console initialized\n");
}

void Con_Shutdown (void)
{
	if (qconsole_log)
	{
		fprintf (qconsole_log, "-------------------------------------------------------------------\n\n");
		fclose (qconsole_log);
	}
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con_x = 0;
	con_current++;
	if (con_numlines < con_totallines)
		con_numlines++;
	memset (&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (char *txt)
{
	int		y, c, l, mask;
	static int	cr;

	// log all messages to file
	if (qconsole_log)
	{
		fprintf (qconsole_log, "%s", txt);
		fflush (qconsole_log);
	}

	con_backscroll = 0;

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		S_LocalSound ("misc/talk.wav");	// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
	{
		mask = 0;
	}

	while ((c = *txt))
	{
	// count word length
		for (l = 0 ; l < con_linewidth ; l++)
			if (txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth))
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}

		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current%NUM_CON_TIMES] = cl.time;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
	}
}

#define	MAXPRINTMSG	4096

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
void Con_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// also echo to debugging console
	Sys_Printf ("%s", msg);

	if (!con_initialized)
		return;

	if (cls.state == ca_dedicated)
		return;		// no graphics mode

	// write it to the scrollable buffer
	Con_Print (msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}

/*
==============================================================================

DRAWING

==============================================================================
*/

/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int	i, size;
	char	*text, temp[MAXCMDLINE];

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = strcpy (temp, key_lines[edit_line]);
	
	// fill out remainder with spaces
	for (i = strlen(text) ; i < MAXCMDLINE ; i++)
		text[i] = ' ';
		
	// add the cursor frame
	if ((int)(realtime * con_cursorspeed) & 1)
		text[key_linepos] = 11 + 84 * key_insert;
	
	// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;
		
	size = Sbar_GetScaledCharacterSize();

	// draw it
#ifdef GLQUAKE
	Draw_String (size, con_vislines - (size * 2), text, true);
#else
	for (i=0 ; i<con_linewidth ; i++)
		Draw_Character ((i+1)<<3, con_vislines - 16, text[i], false);
#endif
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v, i, maxlines, size;
	char		*text;
	float		time;
	extern	char	chat_buffer[];

	maxlines = bound(0, _con_notifylines.value, NUM_CON_TIMES);

	size = Sbar_GetScaledCharacterSize();

	v = 0;
	for (i = con_current - maxlines + 1 ; i <= con_current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i%NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cl.time - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;
		scr_copytop = 1;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ((x + 1) * size, v, text[x], true);

		v += size;
	}

	if (key_dest == key_message)
	{
		clearnotify = 0;
		scr_copytop = 1;
	
		i = 0;
		
		if (team_message)
		{
			Draw_String (size, v, "(say):", true);
			x = 7;
		}
		else
		{
			Draw_String (size, v, "say:", true);
			x = 5;
		}

		while (chat_buffer[i])
		{
			Draw_Character (x * size, v, chat_buffer[i], true);
			x++;

			i++;
			if (x > con_linewidth)
			{
				x = team_message ? 7 : 5;
				v += size;
			}
		}
		Draw_Character (x * size, v, 10 + ((int)(realtime * con_cursorspeed) & 1), true);
		v += size;
	}
	
	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines, qboolean drawinput)
{
	int	i, j, x, y, rows, size;
	char	*text;
	
	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

// draw the text
	con_vislines = lines;

	size = Sbar_GetScaledCharacterSize();

	rows = (lines - (2 * size)) / size;		// rows of text to draw
	y = lines - (2 * size) - (rows * size);	// may start slightly negative

	for (i = con_current - rows + 1 ; i <= con_current ; i++, y += size)
	{
		j = i - con_backscroll;
		if (j < 0)
			j = 0;

		if (con_backscroll && i == con_current)
		{
			for (x = 0 ; x < con_linewidth ; x += (size / 2))
				Draw_Character ((x + 1) * size, y, '^', true);
			continue;
		}

		text = con_text + (j % con_totallines)*con_linewidth;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ((x + 1) * size, y, text[x], true);
	}

// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput ();
}

/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double	t1, t2;

// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf (text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do {
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2 - t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
	realtime = 0;				// put the cursor back to invisible
}
