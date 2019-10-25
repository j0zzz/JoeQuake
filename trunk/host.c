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
// host.c -- coordinates spawning and killing of local servers

#include "quakedef.h"
#ifdef _WIN32
#include "movie.h"
#endif
#ifndef GLQUAKE
#include "r_local.h"
#endif

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

quakeparms_t	host_parms;

qboolean	host_initialized;		// true if into command execution

double		host_frametime;
double		host_time;
double		realtime;			// without any filtering or bounding
double		oldrealtime;			// last frame run
double		oldphysrealtime;
int		host_framecount;

qboolean physframe;
double	physframetime;

int		host_hunklevel;

int		minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

byte		*host_basepal;
byte		*host_colormap;

int		fps_count;

cvar_t	host_framerate = {"host_framerate", "0"};	// set for slow motion
cvar_t	host_speeds = {"host_speeds", "0"};		// set for running times

cvar_t	sys_ticrate = {"sys_ticrate", "0.05"};
cvar_t	serverprofile = {"serverprofile", "0"};

cvar_t	fraglimit = {"fraglimit", "0", CVAR_SERVER};
cvar_t	timelimit = {"timelimit", "0", CVAR_SERVER};
cvar_t	teamplay = {"teamplay", "0", CVAR_SERVER};

cvar_t	samelevel = {"samelevel", "0"};
cvar_t	noexit = {"noexit", "0", CVAR_SERVER};

cvar_t	developer = {"developer", "0"};

cvar_t	skill = {"skill", "1"};			// 0 - 3
cvar_t	deathmatch = {"deathmatch", "0"};	// 0, 1, or 2
cvar_t	coop = {"coop", "0"};			// 0 or 1

cvar_t	pausable = {"pausable", "1"};

cvar_t	temp1 = {"temp1", "0"};

extern cvar_t cl_independentphysics;

void Host_WriteConfig_f (void);

/*
================
Host_EndGame
================
*/
void Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[1024];
	
	va_start (argptr, message);
	vsnprintf (string, sizeof(string), message, argptr);
	va_end (argptr);
	Con_DPrintf ("Host_EndGame: %s\n", string);
	
	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit
	
	if (cls.demonum == -1 || !CL_NextDemo())
		CL_Disconnect ();

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static qboolean inerror = false;
	
	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;
	
	SCR_EndLoadingPlaque ();		// reenable screen updates

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	Con_Printf ("\n===========================\n");
	Con_Printf ("Host_Error: %s\n",string);
	Con_Printf ("===========================\n\n");
	
	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s\n",string);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void Host_FindMaxClients (void)
{
	int	i;

	svs.maxclients = 1;
		
	if ((i = COM_CheckParm("-dedicated")))
	{
		cls.state = ca_dedicated;
		if (i != (com_argc - 1))
			svs.maxclients = Q_atoi(com_argv[i+1]);
		else
			svs.maxclients = 8;
	}
	else
	{
		cls.state = ca_disconnected;
	}

	if ((i = COM_CheckParm("-listen")))
	{
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = Q_atoi(com_argv[i+1]);
		else
			svs.maxclients = 8;
	}

	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = max(4, svs.maxclients);
	svs.clients = Hunk_AllocName (svs.maxclientslimit * sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetValue (&deathmatch, 1);
	else
		Cvar_SetValue (&deathmatch, 0);
}

/*
================
Host_InitLocal
================
*/
void Host_InitLocal (void)
{
	Host_InitCommands ();

	Cvar_Register (&host_framerate);
	Cvar_Register (&host_speeds);

	Cvar_Register (&sys_ticrate);
	Cvar_Register (&serverprofile);

	Cvar_Register (&fraglimit);
	Cvar_Register (&timelimit);
	Cvar_Register (&teamplay);
	Cvar_Register (&samelevel);
	Cvar_Register (&noexit);
	Cvar_Register (&skill);
	Cvar_Register (&developer);
	Cvar_Register (&deathmatch);
	Cvar_Register (&coop);

	Cvar_Register (&pausable);

	Cvar_Register (&temp1);

	Cmd_AddCommand ("writeconfig", Host_WriteConfig_f);

	Host_FindMaxClients ();

	host_time = 1.0;		// so a think at time 0 won't get called
}

/*
===============
Host_WriteConfig
===============
*/
void Host_WriteConfig (char *cfgname)
{
	FILE	*f;

	if (!(f = fopen(va("%s/%s", com_gamedir, cfgname), "w")))
	{
		Con_Printf ("Couldn't write %s\n", cfgname);
		return;
	}

	fprintf (f, "// Generated by JoeQuake\n");
	fprintf (f, "\n// Key bindings\n");
	Key_WriteBindings (f);
	fprintf (f, "\n// Variables\n");
	Cvar_WriteVariables (f);

	fclose (f);
}

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (void)
{
// dedicated servers initialize the host but don't parse and set the config.cfg cvars
	if (host_initialized & !isDedicated)
		Host_WriteConfig ("config.cfg");
}

/*
===============
Host_WriteConfig_f

Writes key bindings and ONLY archived cvars to a custom config file
===============
*/
void Host_WriteConfig_f (void)
{
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: writeconfig <filename>\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	COM_ForceExtension (name, ".cfg");

	Con_Printf ("Writing %s\n", name);

	Host_WriteConfig (name);
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed 
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
	va_list	argptr;
	char	string[1024];

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
	va_list	argptr;
	char	string[1024];
	int	i;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, string);
		}
	}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
	va_list	argptr;
	char	string[1024];

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (qboolean crash)
{
	int		i, saveSelf;
	client_t	*client;

	// joe, ProQuake fix: don't drop a client that's already been dropped!
	if (!host_client->active)
		return;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}
	
		if (host_client->edict && host_client->spawned)
		{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf ("Client %s removed\n", host_client->name);
	}

// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

// send notification to all clients
	for (i = 0, client = svs.clients ; i < svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer (qboolean crash)
{
	int		i, count;
	sizebuf_t	buf;
	char		message[4];
	double		start;

	if (!sv.active)
		return;

	sv.active = false;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

// flush any pending messages - like the score!!!
	start = Sys_DoubleTime ();
	do {
		count = 0;
		for (i = 0, host_client = svs.clients ; i < svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage(host_client->netconnection))
				{
					NET_SendMessage (host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime() - start) > 3.0)
			break;
	} while (count);

// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte (&buf, svc_disconnect);
	if ((count = NET_SendToAll (&buf, 5)))
		Con_Printf ("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i = 0, host_client = svs.clients ; i < svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient (crash);

// clear structures
	memset (&sv, 0, sizeof(sv));
	memset (svs.clients, 0, svs.maxclientslimit * sizeof(client_t));
}

/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrintf ("Clearing memory\n");
	D_FlushCaches ();
	Mod_ClearAll ();
	if (host_hunklevel)
		Hunk_FreeToLowMark (host_hunklevel);

	cls.signon = 0;
	memset (&sv, 0, sizeof(sv));
	memset (&cl, 0, sizeof(cl));
}

//============================================================================

/*
===================
MinPhysFrameTime
===================
*/
static double MinPhysFrameTime(void)
{
	double physfps = bound(10, cl_maxfps.value, 72);

	return 1.0 / physfps;
}

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
qboolean Host_FilterTime (double time)
{
	qboolean result;
	double	fps, minphysframetime;

	realtime += time;

	fps = bound(10, cl_maxfps.value, 999);

	result = false;

	if (cls.capturedemo || cls.timedemo || realtime - oldrealtime >= 1.0 / fps)
	{
#ifdef _WIN32
		if (Movie_IsActive())
			host_frametime = Movie_FrameTime();
		else
#endif
			host_frametime = realtime - oldrealtime;
		if (cls.demoplayback)
			host_frametime *= bound(0, cl_demospeed.value, 20);
		oldrealtime = realtime;

		if (host_framerate.value > 0)
			host_frametime = host_framerate.value;
		else
			// don't allow really long or short frames
			host_frametime = bound(0.001, host_frametime, 0.1);

		result = true;
	}
	
	minphysframetime = MinPhysFrameTime();
	if (cls.capturedemo || cls.timedemo || realtime - oldphysrealtime >= minphysframetime)
	{
		//physframe = true;
		physframetime = realtime - oldphysrealtime;
		oldphysrealtime = realtime;
	}

	return result;
}

/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void Host_GetConsoleCommands (void)
{
	char	*cmd;

	while (1)
	{
		if (!(cmd = Sys_ConsoleInput()))
			break;
		Cbuf_AddText (cmd);
	}
}

/*
==================
Host_ServerFrame
==================
*/
void Host_ServerFrame (double time)
{
	// joe, from ProQuake: stuff the port number into the server console once every minute
	static	double	port_time = 0;
	extern double sv_frametime;

	//if (!sv.paused)
	//	sv.time += time;
	sv_frametime = time;

	if (port_time > sv.time + 1 || port_time < sv.time - 60)
	{
		port_time = sv.time;
		Cmd_ExecuteString (va("port %d\n", net_hostport), src_command);
	}

	// run the world state	
	pr_global_struct->frametime = sv_frametime;

	// set the time and clear the general datagram
	SV_ClearDatagram ();

	// check for new clients
	SV_CheckForNewClients ();

	// read client messages
	SV_RunClients ();

	// move things around and think
	// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
		SV_Physics ();

	// send all messages to the clients
	SV_SendClientMessages ();
}

/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (double time)
{
	int		pass1, pass2, pass3;
	static	double	time1 = 0, time2 = 0, time3 = 0, extraphysframetime;

	if (setjmp(host_abortserver))
		return;		// something bad happened, or the server disconnected

	// keep the random time dependent
	rand ();

	// decide the simulation time
	if (!Host_FilterTime(time))
	{
		// joe, ProQuake fix: if we're not doing a frame, still check for lagged moves to send
		if (!sv.active && (cl.movemessages > 2))
			CL_SendLagMove ();
		return;			// don't run too fast, or packets will flood out
	}

	if (!cl_independentphysics.value)
	{
		physframe = true;
		physframetime = host_frametime;
		extraphysframetime = 0;
	}
	else
	{
		double	minphysframetime = MinPhysFrameTime ();

		extraphysframetime += host_frametime;
		if (extraphysframetime < minphysframetime)
		{
			physframe = false;
		}
		else
		{
			physframe = true;
			extraphysframetime = 0;
		}
	}

	if (!cl_independentphysics.value || physframe)
	{
		// get new key events
		Sys_SendKeyEvents();

		// allow mice or other external controllers to add commands
		IN_Commands();

		// process console commands
		Cbuf_Execute();

		NET_Poll();

		// if running the server locally, make intentions now
		if (sv.active)
			CL_SendCmd();

	//-------------------
	//
	// server operations
	//
	//-------------------

		// check for commands typed to the host
		Host_GetConsoleCommands();

		if (sv.active)
			Host_ServerFrame(physframetime);

	//-------------------
	//
	// client operations
	//
	//-------------------

		// if running the server remotely, send intentions now after
		// the incoming messages have been read
		if (!sv.active)
			CL_SendCmd();

		host_time += host_frametime;

		// fetch results from server
		if (cls.state == ca_connected)
			CL_ReadFromServer();

		if (cls.state == ca_disconnected) // We need to move the mouse also when disconnected
		{
			usercmd_t dummy;
			IN_Move(&dummy);
		}
	}
	else
	{
		host_time += host_frametime;

		// fetch results from server
		if (cls.state == ca_connected)
			CL_ReadFromServer();

		if (!cls.demoplayback || // not demo playback
			cls.state == ca_disconnected // We need to move the mouse also when disconnected 
			)
		{
			usercmd_t dummy;
			IN_Move(&dummy);
		}
	}

	if (host_speeds.value)
		time1 = Sys_DoubleTime ();

	// update video
	SCR_UpdateScreen ();

	if (host_speeds.value)
		time2 = Sys_DoubleTime ();

	if (cls.signon == SIGNONS)
	{
		// update audio
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	}
	else
	{
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	}

	CDAudio_Update ();

	if (host_speeds.value)
	{
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n", pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	if (!cls.demoplayback && cl_demorewind.value)
	{
		Cvar_SetValue (&cl_demorewind, 0);
		Con_Printf ("ERROR: Demorewind is only enabled during playback\n");
	}

	// don't allow cheats in multiplayer
	if (cl.gametype == GAME_DEATHMATCH)
	{
		if (cl_cheatfree)
		{
			Cvar_SetValue (&cl_thirdperson, 0);
#ifdef GLQUAKE
			Cvar_SetValue (&r_wateralpha, 1);
#endif
		}
		Cvar_SetValue (&r_fullbright, 0);
		//Cvar_SetValue (&r_fullbrightskins, 0);
#ifndef GLQUAKE
		Cvar_SetValue (&r_draworder, 0);
		Cvar_SetValue (&r_ambient, 0);
		Cvar_SetValue (&r_drawflat, 0);
#endif
	}
	else
	{
		// don't allow cheats when recording a single player demo
		if (cls.demorecording)
		{
			Cvar_SetValue (&cl_truelightning, 0);

			// don't allow higher than 72 fps during recording
			if (cl_independentphysics.value == 0 && cl_maxfps.value > 72)
			{
				Con_Printf("Capping fps at 72 for recording.\n");
				Cvar_SetValue(&cl_maxfps, 72);
			}
			else if (cl_independentphysics.value)
			{
				Host_Error("Demo recording is not yet supported with independent physics.");
			}
		}
	}

	host_framecount++;
	fps_count++;
}

void Host_Frame (double time)
{
	int		i, c, m;
	double		time1, time2;
	static	int	timecount;
	static	double	timetotal;

	if (!serverprofile.value)
	{
		_Host_Frame (time);
		return;
	}

	time1 = Sys_DoubleTime ();
	_Host_Frame (time);
	time2 = Sys_DoubleTime ();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal * 1000 / timecount;
	timecount = timetotal = 0;
	c = 0;
	for (i=0 ; i<svs.maxclients ; i++)
		if (svs.clients[i].active)
			c++;

	Con_Printf ("serverprofile: %2i clients %2i msec\n", c, m);
}

//============================================================================

int COM_FileOpenRead (char *path, FILE **hndl);

extern	FILE	*vcrFile;
#define	VCR_SIGNATURE	0x56435231

void Host_InitVCR (quakeparms_t *parms)
{
	int	i, len, n;
	char	*p;
	
	if (COM_CheckParm("-playback"))
	{
		if (com_argc != 2)
			Sys_Error("No other parameters allowed with -playback\n");

		COM_FileOpenRead ("quake.vcr", &vcrFile);
		if (!vcrFile)
			Sys_Error("playback file not found\n");

		fread (&i, 1, sizeof(int), vcrFile);
		if (i != VCR_SIGNATURE)
			Sys_Error("Invalid signature in vcr file\n");

		fread (&com_argc, 1, sizeof(int), vcrFile);
		com_argv = malloc(com_argc * sizeof(char *));
		com_argv[0] = parms->argv[0];
		for (i=0 ; i<com_argc ; i++)
		{
			fread (&len, 1, sizeof(int), vcrFile);
			p = Q_malloc (len);
			fread (p, 1, len, vcrFile);
			com_argv[i+1] = p;
		}
		com_argc++; /* add one for arg[0] */
		parms->argc = com_argc;
		parms->argv = com_argv;
	}

	if ((n = COM_CheckParm("-record")))
	{
		vcrFile = fopen ("quake.vcr", "wb");

		i = VCR_SIGNATURE;
		fwrite (&i, 1, sizeof(int), vcrFile);
		i = com_argc - 1;
		fwrite (&i, 1, sizeof(int), vcrFile);
		for (i=1 ; i<com_argc ; i++)
		{
			if (i == n)
			{
				len = 10;
				fwrite (&len, 1, sizeof(int), vcrFile);
				fwrite (&"-playback", 1, len, vcrFile);
				continue;
			}
			len = strlen (com_argv[i]) + 1;
			fwrite (&len, 1, sizeof(int), vcrFile);
			fwrite (&com_argv[i], 1, len, vcrFile);
		}
	}	
}

void GFX_Init(void)
{
	Draw_Init();
	SCR_Init();
	R_Init();
	Sbar_Init();
}

/*
====================
Host_Init
====================
*/
void Host_Init (quakeparms_t *parms)
{
	minimum_memory = (hipnotic || rogue) ? MINIMUM_MEMORY_LEVELPAK : MINIMUM_MEMORY;

	if (COM_CheckParm("-minmemory"))
		parms->memsize = minimum_memory;

	host_parms = *parms;

	if (parms->memsize < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory available, can't execute game", parms->memsize / (float)0x100000);

	com_argc = parms->argc;
	com_argv = parms->argv;

	Memory_Init (parms->membase, parms->memsize);
	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	V_Init ();
	Chase_Init ();
	Host_InitVCR (parms);
	COM_Init (parms->basedir);
	Host_InitLocal ();
	Key_Init ();

	Cbuf_AddEarlyCommands ();
	Cbuf_Execute ();

	Con_Init ();
	M_Init ();
	PR_Init ();
	Mod_Init ();
	Security_Init ();
	NET_Init ();
	SV_Init ();
	IPLog_Init ();
	SList_Init ();
	SList_Load ();

	if (cls.state != ca_dedicated)
	{
		if (!(host_basepal = (byte *)COM_LoadHunkFile("gfx/palette.lmp")))
			Sys_Error ("Couldn't load gfx/palette.lmp");
		if (!(host_colormap = (byte *)COM_LoadHunkFile("gfx/colormap.lmp")))
			Sys_Error ("Couldn't load gfx/colormap.lmp");

#ifdef __linux__
		IN_Init ();
#endif

		VID_Init (host_basepal);

#ifndef __linux__
		IN_Init ();
#endif

		Image_Init ();
		GFX_Init();
		S_Init ();
		CDAudio_Init ();
		CL_Init ();
	}

#ifdef GLQUAKE
	if (nehahra)
	        Neh_Init ();
#endif

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Con_Printf ("Hunk allocation: %4.1f MB\n", (float)parms->memsize / (1024 * 1024));

	Con_Printf ("\nJoeQuake version %s\n\n", VersionString());
	Con_Printf ("\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e JoeQuake Initialized \x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f\n");

	Cbuf_AddText ("exec quake.rc\n");
	Cbuf_AddText ("cl_warncmd 1\n");

#ifdef GLQUAKE
	if (nehahra)
	        Neh_DoBindings ();

	R_GetParticleMode ();
	R_GetDecalsState ();
#endif
}

/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error. It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown (void)
{
	int		i, j;
	FILE		*cmdhist;
	cmdhistory_t	cmdhistory;
	static qboolean isdown = false;
	extern	char	key_lines[64][MAXCMDLINE];
	extern	int	history_line, edit_line, key_linepos;

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration ();
	IPLog_WriteLog ();

	if (con_initialized && (cmdhist = fopen("joequake/cmdhist.dat", "wb")))
	{
		for (i=0 ; i<64 ; i++)
			for (j=0 ; j<MAXCMDLINE ; j++)
				cmdhistory.key_lines[i][j] = key_lines[i][j];
		cmdhistory.key_linepos = key_linepos;
		cmdhistory.history_line = history_line;
		cmdhistory.edit_line = edit_line;
		fwrite (&cmdhistory, sizeof(cmdhistory_t), 1, cmdhist);
		fclose (cmdhist);
	}

	SList_Shutdown ();
	CDAudio_Shutdown ();
	NET_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	Con_Shutdown ();

	if (cls.state != ca_dedicated)
		VID_Shutdown ();
}
