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
// cl_demo.c

#include "quakedef.h"
#include "winquake.h"
#include <time.h>	// easyrecord stats

#ifdef _WIN32
#include "movie.h"
#endif

framepos_t	*dem_framepos = NULL;
qboolean	start_of_demo = false;

// .dz playback
#ifdef _WIN32
static	HANDLE	hDZipProcess = NULL;
#else
#include <errno.h>
//#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
static qboolean hDZipProcess = false;
#endif

static qboolean	dz_playback = false;
static	char	tempdem_name[256] = "";	// this file must be deleted after playback is finished
static	char	dem_basedir[256] = "";
qboolean	dz_unpacking = false;
static	void CheckDZipCompletion ();
static	void StopDZPlayback ();

// joe: support for recording demos after connecting to the server
byte	demo_head[3][MAX_MSGLEN];
int	demo_head_size[2];

void CL_FinishTimeDemo (void);

/*
==============================================================================
DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	fclose (cls.demofile);
	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;

	if (dz_playback)
		StopDZPlayback ();

	if (cls.timedemo)
		CL_FinishTimeDemo ();

#ifdef _WIN32
	Movie_StopPlayback ();
#endif

	if (streamplaying)
		FMOD_Stop_Stream_f();
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (void)
{
	int	i, len;
	float	f;

	len = LittleLong (net_message.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	for (i=0 ; i<3 ; i++)
	{
		f = LittleFloat (cl.viewangles[i]);
		fwrite (&f, 4, 1, cls.demofile);
	}
	fwrite (net_message.data, net_message.cursize, 1, cls.demofile);
	fflush (cls.demofile);
}

void PushFrameposEntry (long fbaz)
{
	framepos_t	*newf;

	newf = Q_malloc (sizeof(framepos_t));
	newf->baz = fbaz;

	if (!dem_framepos)
	{
		newf->next = NULL;
		start_of_demo = false;
	}
	else
	{
		newf->next = dem_framepos;
	}
	dem_framepos = newf;
}

void EraseTopEntry (void)
{
	framepos_t	*top;

	top = dem_framepos;
	dem_framepos = dem_framepos->next;
	free (top);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage (void)
{
	int	r, i;
	float	f;
	
	if (cl.paused & 2)
		return 0;

	CheckDZipCompletion ();
	if (dz_unpacking)
		return 0;

	if (cls.demoplayback)
	{
		if (start_of_demo && cl_demorewind.value)
			return 0;

		if (cls.signon < SIGNONS)	// clear stuffs if new demo
			while (dem_framepos)
				EraseTopEntry ();

		// decide if it is time to grab the next message		
		if (cls.signon == SIGNONS)	// always grab until fully connected
		{
			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;		// already read this frame's message
				cls.td_lastframe = host_framecount;
				// if this is the second frame, grab the real td_starttime
				// so the bogus time on the first frame doesn't count
				if (host_framecount == cls.td_startframe + 1)
					cls.td_starttime = realtime;
			}
			// modified by joe to handle rewind playing
			else if (!cl_demorewind.value && cl.ctime <= cl.mtime[0])
				return 0;		// don't need another message yet
			else if (cl_demorewind.value && cl.ctime >= cl.mtime[0])
				return 0;

			// joe: fill in the stack of frames' positions
			// enable on intermission or not...?
			// NOTE: it can't handle fixed intermission views!
			if (!cl_demorewind.value /*&& !cl.intermission*/)
				PushFrameposEntry (ftell(cls.demofile));
		}

		// get the next message
		fread (&net_message.cursize, 4, 1, cls.demofile);
		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
		for (i = 0 ; i < 3 ; i++)
		{
			r = fread (&f, 4, 1, cls.demofile);
			cl.mviewangles[0][i] = LittleFloat (f);
		}

		net_message.cursize = LittleLong (net_message.cursize);
		if (net_message.cursize > MAX_MSGLEN)
			Sys_Error ("Demo message > MAX_MSGLEN");

		r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
		if (r != 1)
		{
			CL_StopPlayback ();
			return 0;
		}

		// joe: get out framestack's top entry
		if (cl_demorewind.value /*&& !cl.intermission*/)
		{
			fseek (cls.demofile, dem_framepos->baz, SEEK_SET);
			EraseTopEntry ();
			if (!dem_framepos)
				start_of_demo = true;
		}

		return 1;
	}

	while (1)
	{
		r = NET_GetMessage (cls.netcon);
		if (r != 1 && r != 2)
			return r;

		// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Con_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();
	
	// joe: support for recording demos after connecting
	if (cls.signon < 2)
	{
		memcpy (demo_head[cls.signon], net_message.data, net_message.cursize);
		demo_head_size[cls.signon] = net_message.cursize;
	}
	
	return r;
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf ("Not recording a demo\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_disconnect);
	CL_WriteDemoMessage ();

// finish up
	fclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf ("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
	int	c, track;
	char	name[MAX_OSPATH*2], easyname[MAX_OSPATH*2] = "";

	if (cmd_source != src_command)
		return;

	if (cls.demoplayback)
	{
		Con_Printf("Can't record during demo playback\n");
		return;
	}

	c = Cmd_Argc ();

	if (c > 4)
	{
		Con_Printf ("Usage: record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (c == 1 || c == 2)
	{
		if (c == 1)
		{
			time_t	ltime;
			char	str[MAX_OSPATH];

			time (&ltime);
			strftime (str, sizeof(str)-1, "%Y%m%d_%H%M%S", localtime(&ltime));

			Q_snprintfz (easyname, sizeof(easyname), "%s_%s", CL_MapName(), str);
		}
		else if (c == 2)
		{
			if (strstr(Cmd_Argv(1), ".."))
			{
				Con_Printf ("Relative pathnames are not allowed\n");
				return;
			}
			if (cls.state == ca_connected && cls.signon < 2)
			{
				Con_Printf ("Can't record - try again when connected\n");
				return;
			}
		}
	}

	if (cls.demorecording)
		CL_Stop_f();

// write the forced cd track number, or -1
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
	{
		track = -1;
	}

	if (easyname[0])
		Q_snprintfz (name, sizeof(name), "%s/%s", com_gamedir, easyname);
	else
		Q_snprintfz (name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));

// start the map up
	if (c > 2)
	{
		Cmd_ExecuteString (va("map %s", Cmd_Argv(2)), src_command);
	// joe: if couldn't find the map, don't start recording
		if (cls.state != ca_connected)
			return;
	}

// open the demo file
	COM_ForceExtension (name, ".dem");

	Con_Printf ("recording to %s\n", name);

	if (!(cls.demofile = fopen(name, "wb")))
	{
		COM_CreatePath(name);
		if (!(cls.demofile = fopen(name, "wb")))
		{
			Con_Printf("ERROR: couldn't open %s\n", name);
			return;
		}
	}

	cls.forcetrack = track;
	fprintf (cls.demofile, "%i\n", cls.forcetrack);

	cls.demorecording = true;

	// joe: initialize the demo file if we're already connected
	if (c < 3 && cls.state == ca_connected)
	{
		byte *data = net_message.data;
		int	i, cursize = net_message.cursize;

		for (i=0 ; i<2 ; i++)
		{
			net_message.data = demo_head[i];
			net_message.cursize = demo_head_size[i];
			CL_WriteDemoMessage ();
		}

		net_message.data = demo_head[2];
		SZ_Clear (&net_message);

		// current names, colors, and frag counts
		for (i=0 ; i<cl.maxclients ; i++)
		{
			MSG_WriteByte (&net_message, svc_updatename);
			MSG_WriteByte (&net_message, i);
			MSG_WriteString (&net_message, cl.scores[i].name);
			MSG_WriteByte (&net_message, svc_updatefrags);
			MSG_WriteByte (&net_message, i);
			MSG_WriteShort (&net_message, cl.scores[i].frags);
			MSG_WriteByte (&net_message, svc_updatecolors);
			MSG_WriteByte (&net_message, i);
			MSG_WriteByte (&net_message, cl.scores[i].colors);
		}

		// send all current light styles
		for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
		{
			MSG_WriteByte (&net_message, svc_lightstyle);
			MSG_WriteByte (&net_message, i);
			MSG_WriteString (&net_message, cl_lightstyle[i].map);
		}

		// send stats
		/*
		CRASH FORT
		In coop demos on the client, "pr_global_struct" gets zerod in "Hunk_FreeToLowMark"
		which causes the stats to be garbage values. The stats in the "cl" object work in both SP and MP.
		*/
		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_TOTALSECRETS);
		MSG_WriteLong (&net_message, cl.stats[STAT_TOTALSECRETS]);

		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_TOTALMONSTERS);
		MSG_WriteLong (&net_message, cl.stats[STAT_TOTALMONSTERS]);

		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_SECRETS);
		MSG_WriteLong (&net_message, cl.stats[STAT_SECRETS]);

		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_MONSTERS);
		MSG_WriteLong (&net_message, cl.stats[STAT_MONSTERS]);

		// view entity
		MSG_WriteByte (&net_message, svc_setview);
		MSG_WriteShort (&net_message, cl.viewentity);

		// signon
		MSG_WriteByte (&net_message, svc_signonnum);
		MSG_WriteByte (&net_message, 3);

		CL_WriteDemoMessage ();

		// restore net_message
		net_message.data = data;
		net_message.cursize = cursize;
	}
}

void StartPlayingOpenedDemo (void)
{
	int		c;
	qboolean	neg = false;

	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = getc(cls.demofile)) != '\n')
	{
		if (c == EOF)
		{
			Con_Printf ("ERROR: corrupted demo file\n");
			cls.demonum = -1;		// stop demo loop
			return;
		}
		else if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');
	}

	if (neg)
		cls.forcetrack = -cls.forcetrack;
}

// joe: playing demos from .dz files
static void CheckDZipCompletion (void)
{
#ifdef _WIN32
	DWORD	ExitCode;

	if (!hDZipProcess)
		return;

	if (!GetExitCodeProcess(hDZipProcess, &ExitCode))
	{
		Con_Printf ("WARNING: GetExitCodeProcess failed\n");
		hDZipProcess = NULL;
		dz_unpacking = dz_playback = cls.demoplayback = false;
		StopDZPlayback ();
		return;
	}

	if (ExitCode == STILL_ACTIVE)
		return;

	hDZipProcess = NULL;
#else
	if (!hDZipProcess)
		return;

	hDZipProcess = false;
#endif

	if (!dz_unpacking || !cls.demoplayback)
	{
		StopDZPlayback ();
		return;
	}

	dz_unpacking = false;

	if (!(cls.demofile = fopen(tempdem_name, "rb")))
	{
		Con_Printf ("ERROR: couldn't open %s\n", tempdem_name);
		dz_playback = cls.demoplayback = false;
		cls.demonum = -1;
		return;
	}

	// start playback
	StartPlayingOpenedDemo ();
}

static void StopDZPlayback (void)
{
	if (!hDZipProcess && tempdem_name[0])
	{
		char	temptxt_name[256];

		COM_StripExtension (tempdem_name, temptxt_name);
		strcat (temptxt_name, ".txt");
		remove (temptxt_name);
		if (remove(tempdem_name))
			Con_Printf ("Couldn't delete %s\n", tempdem_name);
		tempdem_name[0] = '\0';
	}
	dz_playback = false;
}

static void PlayDZDemo (void)
{
	char	*name, *p, dz_name[256];
#ifdef _WIN32
	char	cmdline[512];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if (hDZipProcess)
	{
		Con_Printf ("Cannot unpack -- DZip is still running!\n");
		return;
	}
#else
	char	dzipRealPath[256], tmppath[256];
	pid_t	pid;
#endif

	name = Cmd_Argv (1);

	if (strstr(name, "..") == name)
	{
		Q_snprintfz (dem_basedir, sizeof(dem_basedir), "%s%s", com_basedir, name + 2);
		p = strrchr (dem_basedir, '/');
		*p = 0;
		name = strrchr (name, '/') + 1;	// we have to cut off the path for the name
	}
	else
	{
		Q_strncpyz (dem_basedir, com_gamedir, sizeof(dem_basedir));
	}

#ifndef _WIN32
	if (!realpath(dem_basedir, tmppath))
	{
		Con_Printf ("Couldn't realpath '%s'\n", dem_basedir);
		return;
	}
	Q_strncpyz (dem_basedir, tmppath, sizeof(dem_basedir));
#endif

	Q_snprintfz (dz_name, sizeof(dz_name), "%s/%s", dem_basedir, name);

	// check if the file exists
	if (Sys_FileTime(dz_name) == -1)
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		return;
	}

	COM_StripExtension (dz_name, tempdem_name);
	strcat (tempdem_name, ".dem");

	if ((cls.demofile = fopen(tempdem_name, "rb")))
	{
		// .dem already exists, so just play it
		Con_Printf ("Playing demo from %s\n", tempdem_name);
		StartPlayingOpenedDemo ();
		return;
	}

	Con_Printf ("\x02" "\nunpacking demo. please wait...\n\n");
	key_dest = key_game;

	// start DZip to unpack the demo
#ifdef _WIN32
	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	Q_snprintfz (cmdline, sizeof(cmdline), "%s/dzip.exe -x -f \"%s\"", com_basedir, name);
	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, dem_basedir, &si, &pi))
	{
		Con_Printf ("Couldn't execute %s/dzip.exe\n", com_basedir);
		return;
	}

	hDZipProcess = pi.hProcess;
#else
	if (!realpath(com_basedir, dzipRealPath))
	{
		Con_Printf ("Couldn't realpath '%s'\n", com_basedir);
		return;
	}

	if (chdir(dem_basedir) == -1)
	{
		Con_Printf ("Couldn't chdir to '%s'\n", dem_basedir);
		return;
	}

	switch (pid = fork())
	{
	case -1:
		Con_Printf ("Couldn't create subprocess\n");
		return;

	case 0:
		if (execlp(va("%s/dzip-linux", dzipRealPath), "dzip-linux", "-x", "-f", dz_name, NULL) == -1)
		{
			Con_Printf ("Couldn't execute %s/dzip-linux\n", com_basedir);
			exit (-1);
		}

	default:
		if (waitpid(pid, NULL, 0) == -1)
		{
			Con_Printf ("waitpid failed\n");
			return;
		}
		break;
	}

	if (chdir(dzipRealPath) == -1)
	{
		Con_Printf ("Couldn't chdir to '%s'\n", dzipRealPath);
		return;
	}

	hDZipProcess = true;
#endif

	dz_unpacking = dz_playback = true;

	// demo playback doesn't actually start yet, we just set cls.demoplayback
	// so that CL_StopPlayback() will be called if CL_Disconnect() is issued
	cls.demoplayback = true;
	cls.demofile = NULL;
	cls.state = ca_connected;
}

/*
====================
CL_PlayDemo_f

playdemo [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
	char	name[256];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() == 1)
	{
		Cbuf_AddText ("menu_demos\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

// disconnect from server
	CL_Disconnect ();
	
	// added by joe, but FIXME!
	if (cl_demorewind.value)
	{
		Con_Printf ("ERROR: rewind is on\n");
		cls.demonum = -1;
		return;
	}

// open the demo file
	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));

	if (strlen(name) > 3 && !Q_strcasecmp(name + strlen(name) - 3, ".dz"))
	{
		PlayDZDemo ();
		return;
	}

	COM_DefaultExtension (name, ".dem");

	if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3))
		cls.demofile = fopen (va("%s/%s", com_basedir, name + 3), "rb");
	else
		COM_FOpenFile (name, &cls.demofile);

	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		cls.demonum = -1;		// stop demo loop
		return;
	}

	Con_Printf ("Playing demo from %s\n", COM_SkipPath(name));

	StartPlayingOpenedDemo ();
}

/*
====================
CL_FinishTimeDemo
====================
*/
void CL_FinishTimeDemo (void)
{
	int	frames;
	float	time;
	
	cls.timedemo = false;
	
// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();
	
// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted
	cls.timedemo = true;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;		// get a new message this frame
}

static char quakechars[8*16] = {
	'<', '=', '>', '§', 'o', '.', 'o', 'o', 'o', 'o', ' ', 'o', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&','\'', '(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'´', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '<'
};

void CL_ClearPlayerName(char *cleared_playername)
{
	int		i, j, playername_len, quakechar_idx;
	
	playername_len = strlen(cl_name.string);
	for (i = 0, j = 0; i < playername_len; i++)
	{
		quakechar_idx = (cl_name.string[i] < 0) ? (cl_name.string[i] + 0x80) : cl_name.string[i];
		if (strchr("<>:\"/\\|?*", quakechars[quakechar_idx]))
			continue;
		cleared_playername[j++] = quakechars[quakechar_idx];
	}
	cleared_playername[j] = 0;
}

/*
====================
CL_KeepDemo_f
====================
*/
void CL_KeepDemo_f(void)
{
	int		mins, secs, millisecs, fname_counter = 1;
	double	finishtime;
	char	oldname[MAX_OSPATH*2], newname[MAX_OSPATH*2], *demoname_prefix, cleared_playername[16];
	extern cvar_t cl_autodemo, cl_autodemo_name;

	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf("Not recording a demo\n");
		return;
	}

	if (!cl.intermission)
	{
		Con_Printf("Keepdemo is only allowed after you finished the level\n");
		return;
	}

	if (cl_autodemo.value && !cl_autodemo_name.string[0])
	{
		Con_Printf("Keepdemo is only allowed if cl_autodemo is enabled and cl_autodemo_name is not empty\n");
		return;
	}

	CL_Stop_f();

	CL_ClearPlayerName(cleared_playername);

	Q_snprintfz(oldname, sizeof(oldname), "%s/%s.%s", com_gamedir, cl_autodemo_name.string, "dem");
	demoname_prefix = cl_autodemo.value == 2 ? CL_MapName() : cl_autodemo_name.string;
	finishtime = cls.marathon_level > 1 ? cls.marathon_time : cl.completed_time;
	mins = finishtime / 60;
	secs = finishtime - (mins * 60);
	millisecs = (finishtime - secs) * 1000;
	Q_snprintfz(newname, sizeof(newname), "%s/%s_%i%02i%03i_%i_%s", com_gamedir, demoname_prefix, mins, secs, millisecs, (int)skill.value, cleared_playername);

	// try with a different name if this file already exists
	while (Sys_FileTime(va("%s.%s", newname, "dem")) == 1)
		Q_snprintfz(newname, sizeof(newname), "%s_%03i", newname, fname_counter++);

	if (rename(oldname, va("%s.%s", newname, "dem")))
		Con_Printf("Renaming of demo failed! %i\n", errno);
	else
		Con_Printf("Renamed demo to %s\n", newname);
}


#define DEMO_INTERMISSION_BUFFER_SIZE 8
struct {
	int index;
	int data[DEMO_INTERMISSION_BUFFER_SIZE];
} static dem_intermission_buf = {0};

static int DemoIntermissionStatePop (void) {
	dem_intermission_buf.index -= 1;
	if (dem_intermission_buf.index < 0)
		dem_intermission_buf.index = DEMO_INTERMISSION_BUFFER_SIZE - 1;
	return dem_intermission_buf.data[dem_intermission_buf.index];
}

static void DemoIntermissionStatePush (int value) {
	dem_intermission_buf.data[dem_intermission_buf.index] = value;
	dem_intermission_buf.index += 1;
	if (dem_intermission_buf.index >= DEMO_INTERMISSION_BUFFER_SIZE)
		dem_intermission_buf.index = 0;
}

int CL_DemoIntermissionState (int old_state, int new_state)
{
	if (cl_demorewind.value)
		return DemoIntermissionStatePop();

	DemoIntermissionStatePush(old_state);
	return new_state;
}
