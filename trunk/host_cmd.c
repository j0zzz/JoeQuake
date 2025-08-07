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
// host_cmd.c

#include "quakedef.h"

extern	cvar_t	pausable;

cvar_t	cl_confirmquit = {"cl_confirmquit", "1"};
cvar_t	sv_override_spawnparams = {"sv_override_spawnparams", "0"};

int	current_skill;

void Mod_Print (void);

/*
==================
Host_Quit
==================
*/
void Host_Quit (void)
{
	CL_Disconnect ();
	Host_ShutdownServer (false);

	Sys_Quit ();
}

/*
==================
Host_Quit_f
==================
*/
void Host_Quit_f (void)
{
	if (cl_confirmquit.value)
	{
		if (key_dest != key_console && cls.state != ca_dedicated)
		{
			M_Menu_Quit_f ();
			return;
		}
	}
	Host_Quit ();
}

/*
==================
Host_Status_f
==================
*/
void Host_Status_f (void)
{
	int		seconds, minutes, hours = 0, j;
	client_t	*client;
	void		(*print)(char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			cl.console_status = true;
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
	{
		print = SV_ClientPrintf;
	}

	print ("host:    %s\n", hostname.string);
	print ("version: JoeQuake %s\n", JOEQUAKE_VERSION);
	if (tcpipAvailable)
		print ("tcp/ip:  %s\n", my_tcpip_address);
	if (ipxAvailable)
		print ("ipx:     %s\n", my_ipx_address);
	print ("map:     %s\n", sv.name);
	print ("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);
	for (j = 0, client = svs.clients ; j < svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int)(net_time - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
		{
			hours = 0;
		}
		print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
		print ("   %s\n", client->netconnection->address);
	}
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
	if (!((int)sv_player->v.flags & FL_GODMODE))
		SV_ClientPrintf ("godmode OFF\n");
	else
		SV_ClientPrintf ("godmode ON\n");
}

void Host_Notarget_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	sv_player->v.flags = (int)sv_player->v.flags ^ FL_NOTARGET;
	if (!((int)sv_player->v.flags & FL_NOTARGET))
		SV_ClientPrintf ("notarget OFF\n");
	else
		SV_ClientPrintf ("notarget ON\n");
}

qboolean noclip_anglehack;

void Host_Noclip_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		noclip_anglehack = true;
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf ("noclip ON\n");
		sv_player->v.waterlevel = 0;	// Chambers: Fixes annoying float-up bug when flying into angled geometry
	}
	else
	{
		noclip_anglehack = false;
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf ("noclip OFF\n");
	}
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	if (sv_player->v.movetype != MOVETYPE_FLY)
	{
		sv_player->v.movetype = MOVETYPE_FLY;
		SV_ClientPrintf ("flymode ON\n");
	}
	else
	{
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf ("flymode OFF\n");
	}
}

int Host_GetPing (client_t *client)
{
	int	i;
	float	total;

	if (!client->active)
		return -1;

	total = 0;
	for (i=0 ; i<NUM_PING_TIMES ; i++)
		total += client->ping_times[i];
	total /= NUM_PING_TIMES;
	return (int)(total * 1000);
}

/*
==================
Host_Ping_f
==================
*/
void Host_Ping_f (void)
{
	int		i, ping;
	char		*n;
	client_t	*client;

	if (cmd_source == src_command)
	{
		if (Cmd_Argc() == 2)
		{
			if (cls.state != ca_connected)
				return;

			n = Cmd_Argv (1);
			if (*n == '+')
			{
				Cvar_Set (&cl_lag, n+1);
				Con_Printf ("\x02" "ping +%d\n", Q_atoi(n+1));
				return;
			}
		}
		cl.console_ping = true;

		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i = 0, client = svs.clients ; i < svs.maxclients ; i++, client++)
	{
		if ((ping = Host_GetPing(client)) == -1)
			continue;

		cl.scores[i].ping = ping;
		SV_ClientPrintf ("%4i %s\n", cl.scores[i].ping, client->name);
	}
}

/*
===============================================================================

				SERVER TRANSITIONS

===============================================================================
*/

static float serverflags_override = 0.0f;

/*
======================
Host_Map_f

handle a 
map <servername>
command from the console. Active clients are kicked off.
======================
*/
void Host_Map_f (void)
{
	int	i;
	char	name[MAX_QPATH];

	if (cmd_source != src_command)
		return;

#ifdef GLQUAKE
	if (nehahra)
	{
		if (NehGameType == TYPE_DEMO)
		{
			M_Menu_Main_f ();
			return;
		}
	}
#endif

	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer (false);

	key_dest = key_game;			// remove console or menu
	SCR_BeginLoadingPlaque ();

	cls.mapstring[0] = 0;
	for (i=0 ; i<Cmd_Argc() ; i++)
	{
		strcat (cls.mapstring, Cmd_Argv(i));
		strcat (cls.mapstring, " ");
	}
	strcat (cls.mapstring, "\n");

	svs.serverflags = 0;			// haven't completed an episode yet
	if (sv_override_spawnparams.value) {
		svs.serverflags = serverflags_override;
	}
	strcpy (name, Cmd_Argv(1));
	SV_SpawnServer (name);
	if (!sv.active)
		return;

	// joe: cheat free from ProQuake
	if ((cl_cheatfree = (cl_cvar_cheatfree.value && security_loaded)))
		Con_DPrintf ("Spawning cheat-free server\n");

	if (cls.state != ca_dedicated)
	{
		strcpy (cls.spawnparms, "");

		for (i=2 ; i<Cmd_Argc() ; i++)
		{
			strcat (cls.spawnparms, Cmd_Argv(i));
			strcat (cls.spawnparms, " ");
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f (void)
{
	char	level[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf ("Only the server may changelevel\n");
		return;
	}
	SV_SaveSpawnparms ();
	strcpy (level, Cmd_Argv(1));
	SV_SpawnServer (level);
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f (void)
{
	char	mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;

	// must copy out, because it gets cleared in sv_spawnserver
	Q_strncpyz (mapname, sv.name, MAX_QPATH);
	SV_SpawnServer (mapname);
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
void Host_Reconnect_f (void)
{
	if (cls.demoplayback)//R00k: Don't. This causes all muckymuck that you really don't want to deal with.
	{
		SCR_EndLoadingPlaque(); //Baker: reconnect happens before signon reply #4
		return;
	}

	SCR_BeginLoadingPlaque ();
	cls.signon = 0;		// need new connection messages
	CL_CountReconnects ();
}

extern	char	server_name[MAX_QPATH];

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
void Host_Connect_f (void)
{
	char	name[MAX_QPATH];
	
	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback ();
		CL_Disconnect ();
	}
	Q_strncpyz (name, Cmd_Argv(1), MAX_QPATH);
	CL_EstablishConnection (name);
	Host_Reconnect_f ();

	Q_strncpyz (server_name, name, MAX_QPATH);	// added from ProQuake
}

/*
===============================================================================

				LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current 
===============
*/
static void Host_SavegameComment (char text[SAVEGAME_COMMENT_LENGTH + 1])
{
	int		i;
	char	kills[20];
	char	*p;

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		text[i] = ' ';
	text[SAVEGAME_COMMENT_LENGTH] = '\0';

	i = (int) strlen(cl.levelname);
	if (i > 22) i = 22;
	memcpy (text, cl.levelname, (size_t)i);

	// Remove CR/LFs from level name to avoid broken saves, e.g. with autumn_sp map:
	// https://celephais.net/board/view_thread.php?id=60452&start=3666
	while ((p = strchr(text, '\n')) != NULL)
		*p = ' ';
	while ((p = strchr(text, '\r')) != NULL)
		*p = ' ';

	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, strlen(kills));

	// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
	{
		if (text[i] == ' ')
			text[i] = '_';
	}
}

/*
===============
Host_Savegame_f
===============
*/
void Host_Savegame_f (void)
{
	int		i;
	char	name[MAX_OSPATH], comment[SAVEGAME_COMMENT_LENGTH+1];
	FILE	*f;

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}
		
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0) )
		{
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	Q_snprintfz (name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_ForceExtension (name, ".sav");		// joe: force to ".sav"
	
	Con_Printf ("Saving game to %s...\n", name);
	if (!(f = fopen(name, "w")))
	{
		Con_Printf ("ERROR: couldn't open\n");
		return;
	}
	
	fprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		fprintf (f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf (f, "%d\n", current_skill);
	fprintf (f, "%s\n", sv.name);
	fprintf (f, "%f\n", sv.time);

// write the light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}

	ED_WriteGlobals (f);
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ED_Write (f, EDICT_NUM(i));
		fflush (f);
	}
	fclose (f);
	Con_Printf ("done.\n");
}

/*
===============
Host_Loadgame_f
===============
*/
void Host_Loadgame_f (void)
{
	char	name[MAX_OSPATH], mapname[MAX_QPATH], str[32768], *start;
	float	time, tfloat, spawn_parms[NUM_SPAWN_PARMS];
	int	i, r, entnum, version;
	edict_t	*ent;
	FILE	*f;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("load <savename> : load a game\n");
		return;
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav");
	
// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used. The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	Con_Printf ("Loading game from %s...\n", name);
	if (!(f = fopen(name, "r")))
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fscanf (f, "%i\n", &version);
	if (version != SAVEGAME_VERSION)
	{
		fclose (f);
		Con_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	fscanf (f, "%s\n", str);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		fscanf (f, "%f\n", &spawn_parms[i]);

// this silliness is so we can load 1.06 save files, which have float skill values
	fscanf (f, "%f\n", &tfloat);
	current_skill = (int)(tfloat + 0.1);
	Cvar_SetValue (&skill, (float)current_skill);

	fscanf (f, "%s\n", mapname);
	fscanf (f, "%f\n", &time);

	CL_Disconnect_f ();

	SV_SpawnServer (mapname);
	if (!sv.active)
	{
		Con_Printf ("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

// load the light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		fscanf (f, "%s\n", str);
		sv.lightstyles[i] = Hunk_Alloc (strlen(str)+1);
		strcpy (sv.lightstyles[i], str);
	}

// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f))
	{
		for (i=0 ; i<sizeof(str)-1 ; i++)
		{
			r = fgetc (f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}')
			{
				i++;
				break;
			}
		}
		if (i == sizeof(str)-1)
			Sys_Error ("Loadgame buffer overflow");
		str[i] = 0;
		start = str;
		start = COM_Parse (str);
		if (!com_token[0])
			break;		// end of file
		if (strcmp(com_token, "{"))
			Sys_Error ("First token isn't a brace");

		if (entnum == -1)
		{	// parse the global vars
			ED_ParseGlobals (start);
		}
		else
		{	// parse an edict
			ent = EDICT_NUM(entnum);
			if (entnum < sv.num_edicts) {
				memset (&ent->v, 0, progs->entityfields * 4);
			}
			else if (entnum < sv.max_edicts) {
				memset (ent, 0, pr_edict_size);
				ent->baseline.scale = ENTSCALE_DEFAULT;
			}
			else {
				Host_Error ("Loadgame: no free edicts (max_edicts is %i)", sv.max_edicts);
			}
			ent->free = false;
			ED_ParseEdict (start, ent);
	
			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	// Free edicts allocated during map loading but no longer used after restoring saved game state
	for (i = entnum; i < sv.num_edicts; i++)
		ED_Free(EDICT_NUM(i));

	sv.num_edicts = entnum;
	sv.time = time;

	fclose (f);

	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
void Host_Name_f (void)
{
	int	a, b, c;
	char	*newName;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}

	newName = (Cmd_Argc() == 2) ? Cmd_Argv (1) : Cmd_Args ();
	newName[15] = 0;

	if (cmd_source == src_command)
	{
		if (!strcmp(cl_name.string, newName))
			return;
		Cvar_Set (&cl_name, newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->name[0] && strcmp(host_client->name, "unconnected"))
		if (strcmp(host_client->name, newName))
			Con_Printf ("%s renamed to %s\n", host_client->name, newName);

	Q_strcpy (host_client->name, newName);
	host_client->edict->v.netname = PR_SetEngineString(host_client->name);
	
	// joe, from ProQuake: log the IP address
	if (sscanf(host_client->netconnection->address, "%d.%d.%d", &a, &b, &c) == 3)
		IPLog_Add ((a << 16) | (b << 8) | c, newName);

	// joe, from ProQuake: prevent messages right after a colour/name change
	host_client->change_time = sv.time;

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

void Host_Say (qboolean teamonly)
{
	int		j;
	char		*p;
	unsigned char	text[64];
	qboolean	fromServer = false;
	client_t	*client, *save;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
		{
			fromServer = true;
			teamonly = false;
		}
		else
		{
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc() < 2)
		return;

	save = host_client;
	p = Cmd_Args ();

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// turn on color set 1
	if (!fromServer)
	{
		// joe, from ProQuake: don't allow messages right after a colour/name change
	//	if (sv.time - host_client->change_time < 1 && host_client->netconnection->mod != MOD_QSMACK)
	//		return;

		if (teamplay.value && teamonly)
			sprintf (text, "%c(%s): ", 1, save->name);
		else
			sprintf (text, "%c%s: ", 1, save->name);
	}
	else
	{
		sprintf (text, "%c<%s> ", 1, hostname.string);
	}

	j = sizeof(text) - 2 - strlen (text);  // -2 for /n and null terminator
	if (strlen(p) > j)
		p[j] = 0;

	strcat (text, p);
	strcat (text, "\n");

	for (j = 0, client = svs.clients ; j < svs.maxclients ; j++, client++)
	{
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay.value && teamonly && client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientPrintf ("%s", text);
	}
	host_client = save;

	Sys_Printf ("%s", &text[1]);
}

void Host_Say_f (void)
{
	Host_Say (false);
}

void Host_Say_Team_f (void)
{
	Host_Say (true);
}

void Host_Tell_f (void)
{
	int		j;
	char		*p, text[64];
	client_t	*client, *save;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (Cmd_Argc () < 3)
		return;

	Q_strncpyz (text, host_client->name, sizeof(text));
	strcat (text, ": ");

	p = Cmd_Args ();

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// check length & truncate if necessary
	j = sizeof(text) - 2 - strlen (text);  // -2 for /n and null terminator
	if (strlen(p) > j)
		p[j] = 0;

	strcat (text, p);
	strcat (text, "\n");

	save = host_client;
	for (j = 0, client = svs.clients ; j < svs.maxclients ; j++, client++)
	{
		if (!client->active || !client->spawned)
			continue;
		if (Q_strcasecmp(client->name, Cmd_Argv(1)))
			continue;
		host_client = client;
		SV_ClientPrintf ("%s", text);
		break;
	}
	host_client = save;
}

/*
==================
Host_Color_f
==================
*/
void Host_Color_f (void)
{
	int	top, bottom, playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 0x0f);
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
	{
		top = bottom = atoi(Cmd_Argv(1));
	}
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValue (&cl_color, playercolor);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

	// joe, from ProQuake: prevent messages right after a colour/name change
	if (bottom)
		host_client->change_time = sv.time;

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (sv_player->v.health <= 0)
	{
		SV_ClientPrintf ("Can't suicide -- allready dead!\n");
		return;
	}
	
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}

/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f (void)
{
	cl.paused ^= 2;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!pausable.value)
	{
		SV_ClientPrintf ("Pause not allowed.\n");
	}
	else
	{
		sv.paused ^= 1;

		if (sv.paused)
			SV_BroadcastPrintf ("%s paused the game\n", PR_GetString(sv_player->v.netname));
		else
			SV_BroadcastPrintf ("%s unpaused the game\n", PR_GetString(sv_player->v.netname));

	// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================

/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}
	
	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	host_client->sendsignon = true;

	host_client->netconnection->encrypt = 2;
}

static float spawnparams_override[NUM_SPAWN_PARMS];

/*
==================
Host_Spawn_f
==================
*/
void Host_Spawn_f (void)
{
	int		i;
	client_t	*client;
	edict_t		*ent;
	func_t		RestoreGame;
    dfunction_t	*f;
	float	*sendangle; //MH
	extern	dfunction_t *ED_FindFunction (char *name);

	if (cmd_source == src_command)
	{
		Con_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}

	host_client->nomap = false;
	if (cl_cheatfree && host_client->netconnection->mod != MOD_QSMACK)
	{
		unsigned	a, b;

		a = MSG_ReadLong ();
		b = MSG_ReadLong ();

		if (!Security_Verify(a, b))
		{
			MSG_WriteByte (&host_client->message, svc_print);
			MSG_WriteString (&host_client->message, "Invalid executable\n");
			SV_BroadcastPrintf ("%s (%s) WARNING: connected with an invalid executable\n", host_client->name, host_client->netconnection->address);
		//	SV_DropClient (false);
		//	return;
		}

		a = MSG_ReadLong ();
		b = MSG_ReadLong ();

		if (sv.player_model_crc != a || sv.eyes_model_crc != b)
			SV_BroadcastPrintf ("%s (%s) WARNING: non standard player/eyes model detected\n", host_client->name, host_client->netconnection->address);
	}

// run the entrance script
	if (sv.loadgame)
	{	// loaded games are fully inited already
		// if this is the last client to be connected, unpause
		sv.paused = false;

		// nehahra stuff
	        if ((f = ED_FindFunction("RestoreGame")))
		{
			if ((RestoreGame = (func_t)(f - pr_functions)))
			{
				Con_DPrintf ("Calling RestoreGame\n");
				pr_global_struct->time = sv.time;
				pr_global_struct->self = EDICT_TO_PROG(sv_player);
				PR_ExecuteProgram (RestoreGame);
			}
		}
	}
	else
	{	// set up the edict
		ent = host_client->edict;

		memset (&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = PR_SetEngineString(host_client->name);

		if (sv_override_spawnparams.value) {
			memcpy (host_client->spawn_parms, spawnparams_override, sizeof(spawnparams_override));
		}
		// copy spawn parms out of the client_t
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect);

		if ((Sys_DoubleTime() - host_client->netconnection->connecttime) <= sv.time)
			Sys_Printf ("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer);	
	}

// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i = 0, client = svs.clients ; i < svs.maxclients ; i++, client++)
	{
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

// send some stats
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);

// send a fixangle
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
	ent = EDICT_NUM(1 + (host_client - svs.clients));
	MSG_WriteByte (&host_client->message, svc_setangle);
	sendangle = sv.loadgame ? ent->v.v_angle : ent->v.angles; //MH Correct viewangles on load game(06-05-2012)
	for (i=0 ; i<2 ; i++)
		MSG_WriteAngle (&host_client->message, sendangle[i], sv.protocolflags); //MH updated for correct angle.
	MSG_WriteAngle (&host_client->message, 0, sv.protocolflags);

	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
}

/*
==================
Host_Begin_f
==================
*/
void Host_Begin_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
	host_client->netconnection->encrypt = 0;
}

//===========================================================================

/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f (void)
{
	int		i;
	char		*who, *message = NULL;
	client_t	*save;
	qboolean	byNumber = false;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
	}
	else if (pr_global_struct->deathmatch)
	{
		return;
	}

	save = host_client;

	if (Cmd_Argc() > 2 && !strcmp(Cmd_Argv(1), "#"))
	{
		i = Q_atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (!Q_strcasecmp(host_client->name, Cmd_Argv(1)))
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
		{
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		}
		else
		{
			who = save->name;
		}

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = COM_Parse(Cmd_Args());
			if (byNumber)
			{
				message++;				// skip the #
				while (*message == ' ')			// skip white space
					message++;
				message += strlen (Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

/*
===============================================================================

				DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
void Host_Give_f (void)
{
	char	*t;
	int	v;
	eval_t	*val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));
	
	switch (t[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// MED 01/04/97 added hipnotic give stuff
		if (hipnotic)
		{
			if (t[0] == '6')
			{
				if (t[1] == 'a')
					sv_player->v.items = (int)sv_player->v.items | HIT_PROXIMITY_GUN;
				else
					sv_player->v.items = (int)sv_player->v.items | IT_GRENADE_LAUNCHER;
			}
			else if (t[0] == '9')
				sv_player->v.items = (int)sv_player->v.items | HIT_LASER_CANNON;
			else if (t[0] == '0')
				sv_player->v.items = (int)sv_player->v.items | HIT_MJOLNIR;
			else if (t[0] >= '2')
				sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
			if (t[0] >= '2')
				sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;
	
	case 's':
		if (rogue)
		{
		    if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_shells1)))
			    val->_float = v;
		}
		sv_player->v.ammo_shells = v;
		break;

	case 'n':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_nails1)))
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		else
		{
			sv_player->v.ammo_nails = v;
		}
		break;

	case 'l':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_lava_nails)))
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		break;

	case 'r':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_rockets1)))
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		else
		{
			sv_player->v.ammo_rockets = v;
		}
		break;

	case 'm':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_multi_rockets)))
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		break;

	case 'h':
		sv_player->v.health = v;
		break;

	case 'c':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_cells1)))
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		else
		{
			sv_player->v.ammo_cells = v;
		}
		break;

	case 'p':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_plasma)))
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		break;
    }
}

edict_t	*FindViewthing (void)
{
	int	i;
	edict_t	*e;
	
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if (!strcmp (PR_GetString(e->v.classname), "viewthing"))
			return e;
	}
	Con_Printf ("No viewthing on map\n");
	return NULL;
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f (void)
{
	edict_t	*e;
	model_t	*m;

	if (!(e = FindViewthing ()))
		return;

	if (!(m = Mod_ForName(Cmd_Argv(1), false)))
	{
		Con_Printf ("Can't load %s\n", Cmd_Argv(1));
		return;
	}
	
	e->v.frame = 0;
	cl.model_precache[(int)e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f (void)
{
	int	f;
	edict_t	*e;
	model_t	*m;

	if (!(e = FindViewthing ()))
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes-1;

	e->v.frame = f;
}

void PrintFrameName (model_t *m, int frame)
{
	aliashdr_t 		*hdr;
	maliasframedesc_t	*pframedesc;

	if (!(hdr = (aliashdr_t *)Mod_Extradata(m)))
		return;

	pframedesc = &hdr->frames[frame];

	Con_Printf ("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f (void)
{
	edict_t	*e;
	model_t	*m;

	if (!(e = FindViewthing()))
		return;

	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName (m, e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	edict_t	*e;
	model_t	*m;

	if (!(e = FindViewthing()))
		return;

	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName (m, e->v.frame);
}

/*
===============================================================================

				DEMO LOOP CONTROL

===============================================================================
*/

/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f (void)
{
	int	i, c;

	if (cls.state == ca_dedicated)
	{
		if (!sv.active)
			Cbuf_AddText ("map start\n");
		return;
	}

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_Printf ("%i demo(s) in loop\n", c);

	for (i=1 ; i<c+1 ; i++)
		strncpy (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0])-1);

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		CL_NextDemo ();
	}
	else
	{
		cls.demonum = -1;
	}
}

/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f (void)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demonum == -1)
		cls.demonum = 1;

	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f (void)
{
	if (cls.state == ca_dedicated || !cls.demoplayback)
		return;

	CL_StopPlayback ();
	CL_Disconnect ();
}

void Host_GetCoords_f (void)
{
	entity_t	*ent;
	char		name[MAX_OSPATH], cmdname[MAX_QPATH] = "";
	FILE		*f;

	if (cmd_source != src_command)
		return;

	if (cls.state != ca_connected)
	{
		Con_Printf ("ERROR: You must be connected\n");
		return;
	}

	if (Cmd_Argc() == 3)
	{
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
		Q_strncpyz (cmdname, Cmd_Argv(2), sizeof(cmdname));
	}
	else if (Cmd_Argc() == 2)
	{
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	}
	else
	{
		Con_Printf ("Usage: %s [filename] [command]\n", Cmd_Argv(0));
		return;
	}

	COM_DefaultExtension (name, ".cam");

	if (!(f = fopen(va("%s/%s", com_gamedir, name), "a")))
	{
		Con_Printf ("Couldn't write %s\n", name);
		return;
	}

	ent = &cl_entities[cl.viewentity];
	fprintf (f, " %s %3.1f %3.1f %3.1f\n", cmdname, ent->origin[0], ent->origin[1], ent->origin[2]);
	fclose (f);
}

/*
===============================================================================

				SPAWN PARAMS CONTROL

===============================================================================
*/

void Host_SetServerFlags_f (void)
{
	if (cmd_source != src_command || !sv.active)
		return;

	const int argc = Cmd_Argc();
	if (argc != 2)
	{
		Con_Printf ("Usage: setserverflags <value>\n");
		return;
	}

	svs.serverflags = atof(Cmd_Argv(1));
	serverflags_override = svs.serverflags;
}

static char* spawnparam_names[NUM_SPAWN_PARMS] = {
	"items",
	"health",
	"armorvalue",
	"shells",
	"nails",
	"rockets",
	"cells",
	"weapon",
	"armortype",
	"[unknown]",
	"[unknown]",
	"[unknown]",
	"[unknown]",
	"[unknown]",
	"[unknown]",
	"[unknown]"
};

static void PrintSpawnParam (const client_t* client, const int param_num)
{
	Con_Printf("%2i  %10s: %f\n", param_num, spawnparam_names[param_num], client->spawn_parms[param_num]);
}

static void PrintSpawnParams (const client_t* client)
{
	Con_Printf("spawn params for %s:\n", client->name);
	for (int i = 0; i < NUM_SPAWN_PARMS; ++i)
		PrintSpawnParam(client, i);
	Con_Printf("\n");
}

void Host_PrintSpawnParams_f (void)
{
	if (cmd_source != src_command || !sv.active)
		return;

	const int argc = Cmd_Argc();
	if (argc > 2)
	{
		Con_Printf ("Usage: printspawnparams [clientnum]\n");
	}

	if (argc > 1)
	{
		const int client_num = atoi(Cmd_Argv(1));
		if (client_num < 0 || client_num >= svs.maxclients)
		{
			Con_Printf("Invalid client number.\n");
			return;
		}
		PrintSpawnParams(svs.clients + client_num);
		return;
	}

	for (int client_num = 0; client_num < svs.maxclients; ++client_num)
	{
		if (!svs.clients[client_num].active)
			continue;
		PrintSpawnParams(svs.clients + client_num);
	}
}

void Host_SetSpawnParam_f (void)
{
	if (cmd_source != src_command || !sv.active)
		return;

	const int argc = Cmd_Argc();
	if (argc < 3 || argc > 4)
	{
		Con_Printf ("Usage: setspawnparam <paramnum> <value> [clientnum]\n");
		return;
	}
	const int param_num = atoi(Cmd_Argv(1));
	const float value = atof(Cmd_Argv(2));

	int client_num = 0;
	if (Cmd_Argc() > 3)
	{
		client_num = atoi(Cmd_Argv(3));
	}

	if (client_num < 0 || client_num >= svs.maxclients)
	{
		Con_Printf("Invalid client number.\n");
		return;
	}
	if (param_num < 0 || param_num > NUM_SPAWN_PARMS)
	{
		Con_Printf("Invalid spawn param number.\n");
		return;
	}

	svs.clients[client_num].spawn_parms[param_num] = value;
	if (client_num == 0) {
		spawnparams_override[param_num] = value;
	}
}

struct spawn_params_to_write
{
	int items;
	float health;
	float armorvalue;
	float ammo_shells;
	float ammo_nails;
	float ammo_rockets;
	float ammo_cells;
	float active_weapon;
	float armortype;
};

static void WriteNextSpawnParams (FILE *f, const struct spawn_params_to_write params, const int client_num)
{
	// Remove items that would be removed with level change.
	// Also remove sigils, as stuff doesn't fit into float precision with them,
	// so they are handled through svs.serverflags instead.
	const int items = params.items - (params.items & (
		IT_SUPERHEALTH|IT_KEY1|IT_KEY2|IT_INVISIBILITY|IT_INVULNERABILITY|IT_SUIT|IT_QUAD|
		IT_SIGIL1|IT_SIGIL2|IT_SIGIL3|IT_SIGIL4)
	);
	fprintf(f, "setserverflags %f\n", (float)(params.items >> 28));
	fprintf(f, "setspawnparam 0 %f %i\n", (float)items, client_num);
	fprintf(f, "setspawnparam 1 %f %i\n", min(max(params.health, 50.0f), 100.0f), client_num);
	fprintf(f, "setspawnparam 2 %f %i\n", params.armorvalue, client_num);
	fprintf(f, "setspawnparam 3 %f %i\n", max(params.ammo_shells, 25.0f), client_num);
	fprintf(f, "setspawnparam 4 %f %i\n", params.ammo_nails, client_num);
	fprintf(f, "setspawnparam 5 %f %i\n", params.ammo_rockets, client_num);
	fprintf(f, "setspawnparam 6 %f %i\n", params.ammo_cells, client_num);
	fprintf(f, "setspawnparam 7 %f %i\n", params.active_weapon, client_num);
	fprintf(f, "setspawnparam 8 %f %i\n", params.armortype, client_num);
}

static float GetActiveWeaponForSpawnParams (const int active_weapon)
{
	if (!pr_qdqstats)
	{
		return active_weapon;
	}

	// qdqstats progs handle the spawn param for the active weapon in a special
	// way. We mirror that in this function to be able to figure out the next
	// spawn params.
	switch (active_weapon)
	{
		case IT_AXE:
			return 0.0f;
		case IT_SHOTGUN:
			return 1.0f;
		case IT_SUPER_SHOTGUN:
			return 2.0f;
		case IT_NAILGUN:
			return 3.0f;
		case IT_SUPER_NAILGUN:
			return 4.0f;
		case IT_GRENADE_LAUNCHER:
			return 5.0f;
		case IT_ROCKET_LAUNCHER:
			return 6.0f;
		case IT_LIGHTNING:
			return 7.0f;
		default:
			// Set default to axe. It's possible that active_weapon is 0 if we
			// get it from cl.stats[STAT_ACTIVEWEAPON], because IT_AXE does not
			// fit in the byte that is sent over to populate it. In that case
			// returning the value 0.0f for axe is indeed correct. All other
			// cases would be invalid and should not happen, but defaulting to
			// axe for those seems like a decent idea too.
			return 0.0f;
	}
}

static void WriteNextSpawnParamsForServerClient (FILE *f, entvars_t *client_vars, int client_num)
{
	struct spawn_params_to_write params;
	params.items = (int)client_vars->items;
	params.health = client_vars->health;
	params.armorvalue = client_vars->armorvalue;
	params.ammo_shells = client_vars->ammo_shells;
	params.ammo_nails = client_vars->ammo_nails;
	params.ammo_rockets = client_vars->ammo_rockets;
	params.ammo_cells = client_vars->ammo_cells;
	params.active_weapon = GetActiveWeaponForSpawnParams((int)client_vars->weapon);
	params.armortype = roundf(client_vars->armortype * 100.0f);
	WriteNextSpawnParams(f, params, client_num);
}

static void WriteNextSpawnParamsForThisClient (FILE *f, int client_num)
{
	struct spawn_params_to_write params;
	params.items = cl.items;
	params.health = (float)cl.stats[STAT_HEALTH];
	params.armorvalue = (float)cl.stats[STAT_ARMOR];
	params.ammo_shells = (float)cl.stats[STAT_SHELLS];
	params.ammo_nails = (float)cl.stats[STAT_NAILS];
	params.ammo_rockets = (float)cl.stats[STAT_ROCKETS];
	params.ammo_cells = (float)cl.stats[STAT_CELLS];
	params.active_weapon = GetActiveWeaponForSpawnParams(cl.stats[STAT_ACTIVEWEAPON]);

	if (cl.items & IT_ARMOR1)
		params.armortype = 30.0f;
	else if (cl.items & IT_ARMOR2)
		params.armortype = 60.0f;
	else if (cl.items & IT_ARMOR3)
		params.armortype = 80.0f;
	else
		params.armortype = 0.0f;

	WriteNextSpawnParams(f, params, client_num);
}

void Host_WriteNextSpawnParams_f(void)
{
	char	name[MAX_OSPATH*2], path[MAX_OSPATH*2];

	if (cmd_source != src_command)
		return;
	if (!sv.active && cls.state == ca_disconnected)
		return;

	const int argc = Cmd_Argc();
	int client_num = 0;
	if (argc > 1)
	{
		client_num = atoi(Cmd_Argv(1));
		if (sv.active && (client_num < 0 || client_num >= svs.maxclients))
		{
			Con_Printf("Invalid client number.\n");
			return;
		}
	}
	if (argc > 3)
	{
		Con_Printf ("Usage: writenextspawnparams [clientnum] [filename]\n");
		return;
	}

	if (argc < 3)
	{
		time_t	ltime;
		char	str[MAX_OSPATH];

		time (&ltime);
		strftime (str, sizeof(str)-1, "%Y%m%d_%H%M%S", localtime(&ltime));

		Q_snprintfz (name, sizeof(name), "%s_%s", CL_MapName(), str);
	}
	else if (argc == 3)
	{
		if (strstr(Cmd_Argv(2), ".."))
		{
			Con_Printf ("Relative pathnames are not allowed\n");
			return;
		}
		Q_strncpyz (name, Cmd_Argv(2), sizeof(name));
	}

	COM_ForceExtension (name, ".cfg");
	Q_snprintfz (path, sizeof(path), "%s/%s", com_gamedir, name);

	FILE* f = NULL;
	if (!(f = fopen(path, "w")))
	{
		COM_CreatePath(path);
		if (!(f = fopen(path, "w")))
		{
			Con_Printf("ERROR: couldn't open %s\n", name);
			return;
		}
	}

	if (!sv.active)
	{
		WriteNextSpawnParamsForThisClient(f, client_num);
	}
	else
	{
		if (argc > 1)
		{
			WriteNextSpawnParamsForServerClient(f, &svs.clients[client_num].edict->v, client_num);
		}
		else
		{
			for (client_num = 0; client_num < svs.maxclients; ++client_num)
			{
				if (!svs.clients[client_num].active)
					continue;
				WriteNextSpawnParamsForServerClient(f, &svs.clients[client_num].edict->v, client_num);
			}
		}
	}
	Con_Printf ("Wrote %s\n", name);
	fclose(f);
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	Cmd_AddCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	Cmd_AddCommand ("god", Host_God_f);
	Cmd_AddCommand ("notarget", Host_Notarget_f);
	Cmd_AddCommand ("fly", Host_Fly_f);
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand ("name", Host_Name_f);
	Cmd_AddCommand ("noclip", Host_Noclip_f);
	Cmd_AddCommand ("version", Host_Version_f);
#ifdef IDGODS
	Cmd_AddCommand ("please", Host_Please_f);
#endif
	Cmd_AddCommand ("say", Host_Say_f);
	Cmd_AddCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand ("tell", Host_Tell_f);
	Cmd_AddCommand ("color", Host_Color_f);
	Cmd_AddCommand ("kill", Host_Kill_f);
	Cmd_AddCommand ("pause", Host_Pause_f);
	Cmd_AddCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand ("begin", Host_Begin_f);
	Cmd_AddCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand ("kick", Host_Kick_f);
	Cmd_AddCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);
	Cmd_AddCommand ("give", Host_Give_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);
	Cmd_AddCommand ("demos", Host_Demos_f);
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand ("viewframe", Host_Viewframe_f);
	Cmd_AddCommand ("viewnext", Host_Viewnext_f);
	Cmd_AddCommand ("viewprev", Host_Viewprev_f);

	Cmd_AddCommand ("getcoords", Host_GetCoords_f);

	Cmd_AddCommand ("setserverflags", Host_SetServerFlags_f);
	Cmd_AddCommand ("printspawnparams", Host_PrintSpawnParams_f);
	Cmd_AddCommand ("setspawnparam", Host_SetSpawnParam_f);
	Cmd_AddCommand ("writenextspawnparams", Host_WriteNextSpawnParams_f);

	Cmd_AddCommand ("mcache", Mod_Print);

	Cvar_Register (&cl_confirmquit);
	Cvar_Register (&sv_override_spawnparams);
}
