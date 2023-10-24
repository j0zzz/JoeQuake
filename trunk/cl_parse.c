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
// cl_parse.c -- parse a message received from the server

#include "quakedef.h"

char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",			// [long] server version
	"svc_setview",			// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
					// the string should be \n terminated
	"svc_setangle",			// [vec3] set the view angle to this absolute value

	"svc_serverinfo",		// [long] version
					// [string] signon string
					// [string]..[0]model cache [string]...[0]sounds cache
					// [string]..[0]item cache
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",		// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",		// [byte] [byte]
	"svc_particle",			// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",			// [string] music [string] text
	"svc_cdtrack",			// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",
// nehahra support
	"svc_showlmp",		// [string] iconlabel [string] lmpfile [byte] x [byte] y
	"svc_hidelmp",		// [string] iconlabel
	"svc_skybox",		// [string] skyname
//johnfitz -- new server messages 
	"svc_botchat", // 38 (2021 RE-RELEASE)
	"", // 39
	"svc_bf", // 40						// no data
	"svc_fog", // 41					// [byte] density [byte] red [byte] green [byte] blue [float] time
	"svc_spawnbaseline2", //42			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstatic2", // 43			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstaticsound2", //	44		// [coord3] [short] samp [byte] vol [byte] aten
//johnfitz

// 2021 RE-RELEASE:
	"svc_setviews", // 45
	"svc_updateping", // 46
	"svc_updatesocial", // 47
	"svc_updateplinfo", // 48
	"svc_rawprint", // 49
	"svc_servervars", // 50
	"svc_seq", // 51
	"svc_achievement", // 52
	"svc_chat", // 53
	"svc_levelcompleted", // 54
	"svc_backtolobby", // 55
	"svc_localsound" // 56
};

//=============================================================================

void CL_InitModelnames (void)
{
	int	i;

	memset (cl_modelnames, 0, sizeof(cl_modelnames));

	cl_modelnames[mi_player] = "progs/player";	// this is intended, as we have to check DME models too (playersg, playergr, etc)
	cl_modelnames[mi_q3head] = "progs/player/head.md3";
	cl_modelnames[mi_q3torso] = "progs/player/upper.md3";
	cl_modelnames[mi_q3legs] = "progs/player/lower.md3";
	cl_modelnames[mi_h_player] = "progs/h_player.mdl";
	cl_modelnames[mi_eyes] = "progs/eyes.mdl";
	cl_modelnames[mi_rocket] = "progs/missile.mdl";
	cl_modelnames[mi_grenade] = "progs/grenade.mdl";
	cl_modelnames[mi_flame0] = "progs/flame0.mdl";
	cl_modelnames[mi_flame0_md3] = "progs/flame.md3";
	cl_modelnames[mi_flame1] = "progs/flame.mdl";
	cl_modelnames[mi_flame2] = "progs/flame2.mdl";
	cl_modelnames[mi_explo1] = "progs/s_explod.spr";
	cl_modelnames[mi_explo2] = "progs/s_expl.spr";
	cl_modelnames[mi_bubble] = "progs/s_bubble.spr";
	cl_modelnames[mi_gib1] = "progs/gib1.mdl";
	cl_modelnames[mi_gib2] = "progs/gib2.mdl";
	cl_modelnames[mi_gib3] = "progs/gib3.mdl";
	cl_modelnames[mi_fish] = "progs/fish.mdl";
	cl_modelnames[mi_dog] = "progs/dog.mdl";
	cl_modelnames[mi_soldier] = "progs/soldier.mdl";
	cl_modelnames[mi_enforcer] = "progs/enforcer.mdl";
	cl_modelnames[mi_knight] = "progs/knight.mdl";
	cl_modelnames[mi_hknight] = "progs/hknight.mdl";
	cl_modelnames[mi_scrag] = "progs/wizard.mdl";
	cl_modelnames[mi_ogre] = "progs/ogre.mdl";
	cl_modelnames[mi_fiend] = "progs/demon.mdl";
	cl_modelnames[mi_vore] = "progs/shalrath.mdl";
	cl_modelnames[mi_shambler] = "progs/shambler.mdl";
	cl_modelnames[mi_zombie] = "progs/zombie.mdl";
	cl_modelnames[mi_spawn] = "progs/tarbaby.mdl";
	cl_modelnames[mi_h_dog] = "progs/h_dog.mdl";
	cl_modelnames[mi_h_soldier] = "progs/h_guard.mdl";
	cl_modelnames[mi_h_enforcer] = "progs/h_mega.mdl";
	cl_modelnames[mi_h_knight] = "progs/h_knight.mdl";
	cl_modelnames[mi_h_hknight] = "progs/h_hellkn.mdl";
	cl_modelnames[mi_h_scrag] = "progs/h_wizard.mdl";
	cl_modelnames[mi_h_ogre] = "progs/h_ogre.mdl";
	cl_modelnames[mi_h_fiend] = "progs/h_demon.mdl";
	cl_modelnames[mi_h_vore] = "progs/h_shal.mdl";
	cl_modelnames[mi_h_shambler] = "progs/h_shams.mdl";
	cl_modelnames[mi_h_zombie] = "progs/h_zombie.mdl";
	cl_modelnames[mi_vwplayer] = "progs/vwplayer.mdl";
	cl_modelnames[mi_vwplayer_md3] = "progs/vwplayer.md3";
	cl_modelnames[mi_w_shot] = "progs/w_shot.mdl";
	cl_modelnames[mi_w_shot2] = "progs/w_shot2.mdl";
	cl_modelnames[mi_w_nail] = "progs/w_nail.mdl";
	cl_modelnames[mi_w_nail2] = "progs/w_nail2.mdl";
	cl_modelnames[mi_w_rock] = "progs/w_rock.mdl";
	cl_modelnames[mi_w_rock2] = "progs/w_rock2.mdl";
	cl_modelnames[mi_w_light] = "progs/w_light.mdl";
	cl_modelnames[mi_q3w_shot] = "progs/w_shot.md3";
	cl_modelnames[mi_q3w_shot2] = "progs/w_shot2.md3";
	cl_modelnames[mi_q3w_nail] = "progs/w_nail.md3";
	cl_modelnames[mi_q3w_nail2] = "progs/w_nail2.md3";
	cl_modelnames[mi_q3w_rock] = "progs/w_rock.md3";
	cl_modelnames[mi_q3w_rock2] = "progs/w_rock2.md3";
	cl_modelnames[mi_q3w_light] = "progs/w_light.md3";

	for (i = 0 ; i < NUM_MODELINDEX ; i++)
	{
		if (!cl_modelnames[i])
			Sys_Error ("cl_modelnames[%d] not initialized", i);
	}
}

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t *CL_EntityNum (int num)
{
	//johnfitz -- check minimum number too
	if (num < 0)
		Host_Error("CL_EntityNum: %i is an invalid number", num);
	//johnfitz
	
	if (num >= cl.num_entities)
	{
		if (num >= cl_max_edicts) //johnfitz -- no more MAX_EDICTS 
			Host_Error ("CL_EntityNum: %i is an invalid number", num);

		while (cl.num_entities <= num)
		{
			cl_entities[cl.num_entities].colormap = vid.colormap;
			cl_entities[cl.num_entities].lerpflags |= LERP_RESETMOVE | LERP_RESETANIM; //johnfitz
			cl_entities[cl.num_entities].baseline.scale = ENTSCALE_DEFAULT;
			cl.num_entities++;
		}
	}

	return &cl_entities[num];
}

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket (void)
{
	vec3_t	pos;
	int	i, channel, ent, sound_num, volume, field_mask;
	float	attenuation;  

	field_mask = MSG_ReadByte ();

	volume = (field_mask & SND_VOLUME) ? MSG_ReadByte() : DEFAULT_SOUND_PACKET_VOLUME;
	attenuation = (field_mask & SND_ATTENUATION) ? MSG_ReadByte() / 64.0 : DEFAULT_SOUND_PACKET_ATTENUATION;
	
	//johnfitz -- PROTOCOL_FITZQUAKE
	if (field_mask & SND_LARGEENTITY)
	{
		ent = (unsigned short)MSG_ReadShort();
		channel = MSG_ReadByte();
	}
	else
	{
		channel = (unsigned short)MSG_ReadShort();
		ent = channel >> 3;
		channel &= 7;
	}

	if (field_mask & SND_LARGESOUND)
		sound_num = (unsigned short)MSG_ReadShort();
	else
		sound_num = MSG_ReadByte();
	//johnfitz

	//johnfitz -- check soundnum
	if (sound_num >= MAX_SOUNDS)
		Host_Error("CL_ParseStartSoundPacket: %i > MAX_SOUNDS", sound_num);
	//johnfitz

	if (ent > cl_max_edicts) //johnfitz -- no more MAX_EDICTS 
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord (cl.protocolflags);

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       

/*
==================
CL_ParseLocalSound - for 2021 rerelease
==================
*/
void CL_ParseLocalSound(void)
{
	int field_mask, sound_num;

	field_mask = MSG_ReadByte();
	sound_num = (field_mask&SND_LARGESOUND) ? MSG_ReadShort() : MSG_ReadByte();
	if (sound_num >= MAX_SOUNDS)
		Host_Error("CL_ParseLocalSound: %i > MAX_SOUNDS", sound_num);

	S_LocalSound(cl.sound_precache[sound_num]->name);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
static byte net_olddata[NET_MAXMESSAGE];
void CL_KeepaliveMessage (void)
{
	float		time;
	static float lastmsg;
	int			ret;
	sizebuf_t	old;
	byte		*olddata;
	
	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;
	if (!NET_CanSendMessage(cls.netcon))
		// Sphere -- if we cannot send another message yet, calling
		// NET_SendMessage below is not allowed and would break things. So we
		// skip sending a keepalive message for now. Maybe on the next call we
		// will be allowed to send one again, or maybe we are just loading stuff
		// so quickly that we dont need a keepalive message.
		return;

// read messages from server, should just be nops
	olddata = net_olddata;
	old = net_message;
	memcpy (olddata, net_message.data, net_message.cursize);

	do {
		ret = CL_GetMessage ();
		switch (ret)
		{
		default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");

		case 0:
			break;	// nothing waiting

		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;

		case 2:
			if (MSG_ReadByte() != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	net_message = old;
	memcpy (net_message.data, olddata, net_message.cursize);

// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo (void)
{
	int	i, nummodels, numsounds;
	char	*str, tempname[MAX_QPATH], vwplayername[MAX_QPATH];
	char	model_precache[MAX_MODELS][MAX_QPATH], sound_precache[MAX_SOUNDS][MAX_QPATH];
	extern	void R_PreMapLoad (char *);
	extern qboolean r_loadviewweapons;
#ifdef GLQUAKE
	extern qboolean r_loadq3player;
#endif

	Con_DPrintf ("\nServerinfo packet received\n");

// wipe the client_state_t struct
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();
	//johnfitz -- support multiple protocols
	if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_RMQ) 
	{
		Con_Printf("\n"); //because there's no newline after serverinfo print
		Host_Error("Server returned version %i, not %i or %i or %i", i, PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_RMQ);
	}
	cl.protocol = i;
	//johnfitz

	if (cl.protocol == PROTOCOL_RMQ)
	{
		const unsigned int supportedflags = (PRFL_SHORTANGLE | PRFL_FLOATANGLE | PRFL_24BITCOORD | PRFL_FLOATCOORD | PRFL_EDICTSCALE | PRFL_INT32COORD);

		// mh - read protocol flags from server so that we know what protocol features to expect
		cl.protocolflags = (unsigned int)MSG_ReadLong();

		if (0 != (cl.protocolflags & (~supportedflags)))
		{
			Con_DPrintf("PROTOCOL_RMQ protocolflags %i contains unsupported flags\n", cl.protocolflags);
		}
	}
	else 
		cl.protocolflags = 0;

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Host_Error("Bad maxclients (%u) from server", cl.maxclients);
	}
	cl.scores = Hunk_AllocName (cl.maxclients * sizeof(*cl.scores), "scores");

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	strncpy (cl.levelname, str, sizeof(cl.levelname)-1);

// seperate the printfs so the server message can have a color
	Con_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_Printf ("%c%s\n", 2, str);

//johnfitz -- tell user which protocol this is
	if (cl.protocol != PROTOCOL_NETQUAKE)	//joe: only print to console if not vanilla protocol
		Con_Printf("Using protocol %i\n", cl.protocol);

// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it

// precache models
	for (i=0 ; i<NUM_MODELINDEX ; i++)
		cl_modelindex[i] = -1;

#ifdef GLQUAKE
	r_loadq3player = false;
#endif
	r_loadviewweapons = false;

	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	for (nummodels = 1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels == MAX_MODELS)
			Host_Error ("Server sent too many model precaches");

		if (!strncmp(str, cl_modelnames[mi_player], 12))
		{
			if (cl_viewweapons.value &&
				(COM_FindFile(cl_modelnames[mi_vwplayer]) || COM_FindFile(cl_modelnames[mi_vwplayer_md3])) &&
				COM_FindFile(cl_modelnames[mi_w_shot]) &&
				COM_FindFile(cl_modelnames[mi_w_shot2]) &&
				COM_FindFile(cl_modelnames[mi_w_nail]) &&
				COM_FindFile(cl_modelnames[mi_w_nail2]) &&
				COM_FindFile(cl_modelnames[mi_w_rock]) &&
				COM_FindFile(cl_modelnames[mi_w_rock2]) &&
				COM_FindFile(cl_modelnames[mi_w_light]))
			{
				Q_strncpyz(vwplayername, cl_modelnames[mi_vwplayer], MAX_QPATH);
				str = vwplayername;
				r_loadviewweapons = true;
			}
			cl_modelindex[mi_player] = nummodels;
		}

#ifdef GLQUAKE
		if (gl_loadq3models.value)
		{
			if ((!strncmp(str, cl_modelnames[mi_player], 12) || !strcmp(str, cl_modelnames[mi_vwplayer])) &&
				COM_FindFile(cl_modelnames[mi_q3head]) && 
				COM_FindFile(cl_modelnames[mi_q3torso]) && 
				COM_FindFile(cl_modelnames[mi_q3legs]))
			{
				Q_strncpyz (tempname, cl_modelnames[mi_q3legs], MAX_QPATH);
				str = tempname;
				cl_modelindex[mi_player] = nummodels;
				r_loadq3player = true;
			}
			else
			{
				COM_StripExtension (str, tempname);
				strcat (tempname, ".md3");

				if (COM_FindFile(tempname))
				{
					str = tempname;
				}
			}
		}
#endif

		Q_strncpyz (model_precache[nummodels], str, sizeof(model_precache[nummodels]));

		for (i = 0 ; i < NUM_MODELINDEX ; i++)
		{
			if (!strcmp(cl_modelnames[i], model_precache[nummodels]))
			{
				cl_modelindex[i] = nummodels;
				break;
			}
		}
	}
#ifdef GLQUAKE
// load the extra "no-flamed-torch" model
	if (nummodels == MAX_MODELS)
	{
		Con_Printf ("Server sent too many model precaches -> replacing flame0.mdl with flame.mdl\n");
		cl_modelindex[mi_flame0] = cl_modelindex[mi_flame1];
	}
	else
	{
		Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_flame0], sizeof(model_precache[nummodels]));
		cl_modelindex[mi_flame0] = nummodels++;
	}
// load the view weapon models
	if (r_loadviewweapons)
	{
		if (nummodels + 6 >= MAX_MODELS)
		{
			Con_Printf ("Server sent too many model precaches -> can't load view weapon models\n");
		}
		else
		{
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_shot], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_shot] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_shot2], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_shot2] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_nail], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_nail] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_nail2], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_nail2] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_rock], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_rock] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_rock2], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_rock2] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_w_light], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_w_light] = nummodels++;
		}
	}
// load the rest of the q3 player model if possible
	if (r_loadq3player)
	{
		if (nummodels + 1 >= MAX_MODELS)
		{
			Con_Printf ("Server sent too many model precaches -> can't load Q3 player model\n");
			Q_strncpyz (model_precache[cl_modelindex[mi_player]], "progs/player.mdl", sizeof(model_precache[cl_modelindex[mi_player]]));
		}
		else
		{
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_q3torso], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_q3torso] = nummodels++;
			Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_q3head], sizeof(model_precache[nummodels]));
			cl_modelindex[mi_q3head] = nummodels++;

			// load the md3 shotgun too, if available
			if (COM_FindFile(cl_modelnames[mi_q3w_shot]))
			{
				Q_strncpyz (model_precache[nummodels], cl_modelnames[mi_q3w_shot], sizeof(model_precache[nummodels]));
				cl_modelindex[mi_q3w_shot] = nummodels++;
			}
		}
	}
#endif

// precache sounds
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	for (numsounds = 1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS)
			Host_Error ("Server sent too many sound precaches");

		Q_strncpyz (sound_precache[numsounds], str, sizeof(sound_precache[numsounds]));
		S_TouchSound (str);
	}

	COM_StripExtension (COM_SkipPath(model_precache[1]), tempname);
	R_PreMapLoad (tempname);

	Ghost_Load();

// now we try to load everything else until a cache allocation fails
	for (i = 1 ; i < nummodels ; i++)
	{
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (!cl.model_precache[i])
		{
			qboolean model_failed = true;

			// this is a common issue when trying to play back such QdQ demo that is referring to a DME model
			if (!strcmp(model_precache[i], "progs/playax.mdl") ||
				!strncmp(model_precache[i], "progs/player", 12))
			{
				cl.model_precache[i] = Mod_ForName ("progs/player.mdl", false);
				if (cl.model_precache[i])
				{
					model_failed = false;
				}
			}

			if (model_failed)
			{
				Con_Printf ("\nThe required model file '%s' could not be found\n\n", model_precache[i]);
				Host_EndGame ("Server disconnected\n");
				return;
			}
		}
		CL_KeepaliveMessage ();
	}

	S_BeginPrecaching ();
	for (i = 1 ; i < numsounds ; i++)
	{
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}
	S_EndPrecaching ();

// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];

	R_NewMap();

	Hunk_Check ();			// make sure nothing is hurt

	noclip_anglehack = false;	// noclip is turned off at start
}

/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked. Other attributes can change without relinking.
==================
*/
void CL_ParseUpdate (int bits)
{
	int			i, num;
	model_t		*model;
	qboolean	forcelink;
	entity_t	*ent;
#ifdef GLQUAKE
	int		skin;
#endif
	extern cvar_t cl_independentphysics;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
		bits |= (MSG_ReadByte() << 8);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol != PROTOCOL_NETQUAKE)
	{
		if (bits & U_EXTEND1)
			bits |= MSG_ReadByte() << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte() << 24;
	}
	//johnfitz

	num = (bits & U_LONGENTITY) ? MSG_ReadShort() : MSG_ReadByte();

	ent = CL_EntityNum (num);

	if (cl_independentphysics.value)
		ent->forcelink = false;

	forcelink = (ent->msgtime != cl.mtime[1]) ? true : false;

	//johnfitz -- lerping
	if (ent->msgtime + 0.2 < cl.mtime[0]) //more than 0.2 seconds since the last message (most entities think every 0.1 sec)
		ent->lerpflags |= LERP_RESETANIM; //if we missed a think, we'd be lerping from the wrong frame
	//johnfitz

	ent->msgtime = cl.mtime[0];

	if (bits & U_MODEL)
	{
		ent->modelindex = MSG_ReadByte ();
		if (ent->modelindex >= MAX_MODELS)
			Host_Error ("CL_ParseUpdate: bad modelindex");
	}
	else
	{
		ent->modelindex = ent->baseline.modelindex;
	}

	ent->frame = (bits & U_FRAME) ? MSG_ReadByte() : ent->baseline.frame;

	i = (bits & U_COLORMAP) ? MSG_ReadByte() : ent->baseline.colormap;
	if (i && i <= cl.maxclients && Mod_IsAnyKindOfPlayerModel(ent->model))
		ent->colormap = cl.scores[i-1].translations;
	else
		ent->colormap = vid.colormap;

#ifdef GLQUAKE
	skin = (bits & U_SKIN) ? MSG_ReadByte() : ent->baseline.skin;
	if (skin != ent->skinnum)
	{
		ent->skinnum = skin;
		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin (num - 1, false);
	}
#else
	ent->skinnum = (bits & U_SKIN) ? MSG_ReadByte() : ent->baseline.skin;
#endif

	ent->effects = (bits & U_EFFECTS) ? MSG_ReadByte() : ent->baseline.effects;

	// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	ent->msg_origins[0][0] = (bits & U_ORIGIN1) ? MSG_ReadCoord(cl.protocolflags) : ent->baseline.origin[0];
	ent->msg_angles[0][0] = (bits & U_ANGLE1) ? MSG_ReadAngle(cl.protocolflags) : ent->baseline.angles[0];

	ent->msg_origins[0][1] = (bits & U_ORIGIN2) ? MSG_ReadCoord(cl.protocolflags) : ent->baseline.origin[1];
	ent->msg_angles[0][1] = (bits & U_ANGLE2) ? MSG_ReadAngle(cl.protocolflags) : ent->baseline.angles[1];

	ent->msg_origins[0][2] = (bits & U_ORIGIN3) ? MSG_ReadCoord(cl.protocolflags) : ent->baseline.origin[2];
	ent->msg_angles[0][2] = (bits & U_ANGLE3) ? MSG_ReadAngle(cl.protocolflags) : ent->baseline.angles[2];

	if (cl.protocol == PROTOCOL_NETQUAKE)
	{
#ifdef GLQUAKE
		if (bits & U_TRANS)
		{
			int	fullbright, temp;

			temp = MSG_ReadFloat();
			ent->transparency = MSG_ReadFloat();
			if (temp == 2)
				fullbright = MSG_ReadFloat();
		}
		else
		{
			ent->transparency = 1;
		}
		ent->scale = ent->baseline.scale;
#endif
	}

	//johnfitz -- lerping for movetype_step entities
	if (bits & U_NOLERP)
	{
		ent->lerpflags |= LERP_MOVESTEP;
		ent->forcelink = true;
	}
	else
		ent->lerpflags &= ~LERP_MOVESTEP;
	//johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol != PROTOCOL_NETQUAKE)
	{
		if (bits & U_ALPHA)
		{
			byte alpha = MSG_ReadByte();
			ent->transparency = ENTALPHA_DECODE(alpha);
		}
		else
			ent->transparency = ENTALPHA_DECODE(ent->baseline.alpha);
		if (bits & U_SCALE)
			ent->scale = MSG_ReadByte();
		else
			ent->scale = ent->baseline.scale;
		if (bits & U_FRAME2)
			ent->frame = (ent->frame & 0x00FF) | (MSG_ReadByte() << 8);
		if (bits & U_MODEL2)
			ent->modelindex = (ent->modelindex & 0x00FF) | (MSG_ReadByte() << 8);
		if (bits & U_LERPFINISH)
		{
			ent->frame_finish_time = ent->msgtime + ((float)(MSG_ReadByte()) / 255);
			ent->lerpflags |= LERP_FINISH;
		}
		else
			ent->lerpflags &= ~LERP_FINISH;
	}
	//johnfitz

	model = cl.model_precache[ent->modelindex];
	if (model != ent->model)
	{
		ent->model = model;
		// automatic animation (torches, etc) can be either all together or randomized
		if (model)
			ent->syncbase = (model->synctype == ST_RAND) ? (float)(rand() & 0x7fff) / 0x7fff : 0.0;
		else
			forcelink = true;	// hack to make null model players work
#ifdef GLQUAKE
		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin(num - 1, false);
#endif
		ent->lerpflags |= LERP_RESETANIM; //johnfitz -- don't lerp animation across model changes
	}

	if (forcelink)
	{	// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent, int version) //johnfitz -- added argument 
{
	int	i, bits; //johnfitz 

	//johnfitz -- PROTOCOL_FITZQUAKE
	bits = (version == 2) ? MSG_ReadByte() : 0;
	ent->baseline.modelindex = (bits & B_LARGEMODEL) ? MSG_ReadShort() : MSG_ReadByte();
	ent->baseline.frame = (bits & B_LARGEFRAME) ? MSG_ReadShort() : MSG_ReadByte();
	//johnfitz

	ent->baseline.colormap = MSG_ReadByte ();
	ent->baseline.skin = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord (cl.protocolflags);
		ent->baseline.angles[i] = MSG_ReadAngle (cl.protocolflags);
	}

	ent->baseline.alpha = (bits & B_ALPHA) ? MSG_ReadByte() : ENTALPHA_DEFAULT; //johnfitz -- PROTOCOL_FITZQUAKE
	ent->baseline.scale = (bits & B_SCALE) ? MSG_ReadByte() : ENTSCALE_DEFAULT;
}

extern	float	cl_ideal_punchangle;

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata ()
{
	int	i, j, bits; //johnfitz 

	bits = (unsigned short)MSG_ReadShort(); //johnfitz -- read bits here instead of in CL_ParseServerMessage() 

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_EXTEND1)
		bits |= (MSG_ReadByte() << 16);
	if (bits & SU_EXTEND2)
		bits |= (MSG_ReadByte() << 24);
	//johnfit

	cl.viewheight = (bits & SU_VIEWHEIGHT) ? MSG_ReadChar() : DEFAULT_VIEWHEIGHT;
	cl.idealpitch = (bits & SU_IDEALPITCH) ? MSG_ReadChar() : 0;

	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		cl.punchangle[i] = (bits & (SU_PUNCH1 << i)) ? MSG_ReadChar() : 0;
		cl.mvelocity[0][i] = (bits & (SU_VELOCITY1 << i)) ? MSG_ReadChar()*16 : 0;
	}

	// hack for smooth punchangle
	if (cl.punchangle[0] < 0)
	{
		if (cl.punchangle[0] >= -2)
			cl_ideal_punchangle = -2;
		else if (cl.punchangle[0] >= -4)
			cl_ideal_punchangle = -4;
	}

	i = MSG_ReadLong ();
	if (cl.items != i)
	{	// set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ((i & (1 << j)) && !(cl.items & (1 << j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ? MSG_ReadByte() : 0;

	i = (bits & SU_ARMOR) ? MSG_ReadByte() : 0;
	if (cl.stats[STAT_ARMOR] != i)
	{
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	i = (bits & SU_WEAPON) ? MSG_ReadByte() : 0;
	if (cl.stats[STAT_WEAPON] != i)
	{
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();
	}
	
	i = MSG_ReadShort ();
	if (cl.stats[STAT_HEALTH] != i)
	{
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte ();
	if (cl.stats[STAT_AMMO] != i)
	{
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i=0 ; i<4 ; i++)
	{
		j = MSG_ReadByte ();
		if (cl.stats[STAT_SHELLS+i] != j)
		{
			cl.stats[STAT_SHELLS+i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte ();

	if (hipnotic || rogue)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1 << i))
		{
			cl.stats[STAT_ACTIVEWEAPON] = (1 << i);
			Sbar_Changed ();
		}
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
		{
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_WEAPON2)
		cl.stats[STAT_WEAPON] |= (MSG_ReadByte() << 8);
	if (bits & SU_ARMOR2)
		cl.stats[STAT_ARMOR] |= (MSG_ReadByte() << 8);
	if (bits & SU_AMMO2)
		cl.stats[STAT_AMMO] |= (MSG_ReadByte() << 8);
	if (bits & SU_SHELLS2)
		cl.stats[STAT_SHELLS] |= (MSG_ReadByte() << 8);
	if (bits & SU_NAILS2)
		cl.stats[STAT_NAILS] |= (MSG_ReadByte() << 8);
	if (bits & SU_ROCKETS2)
		cl.stats[STAT_ROCKETS] |= (MSG_ReadByte() << 8);
	if (bits & SU_CELLS2)
		cl.stats[STAT_CELLS] |= (MSG_ReadByte() << 8);
	if (bits & SU_WEAPONFRAME2)
		cl.stats[STAT_WEAPONFRAME] |= (MSG_ReadByte() << 8);
	if (bits & SU_WEAPONALPHA)
	{
		byte weaponalpha = MSG_ReadByte();
		cl.viewent.transparency = ENTALPHA_DECODE(weaponalpha);
	}
	else
		cl.viewent.transparency = 1;
	//johnfitz

	//johnfitz -- lerping
	//ericw -- this was done before the upper 8 bits of cl.stats[STAT_WEAPON] were filled in, breaking on large maps like zendar.bsp
	if (cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]])
	{
		cl.viewent.lerpflags |= LERP_RESETANIM; //don't lerp animation across model changes
	}
	//johnfitz
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot, qboolean ghost)
{
	int	i, j, top, bottom;
	byte	*dest, *source;

	if (slot > cl.maxclients)
		Host_Error ("CL_NewTranslation: slot > cl.maxclients");

	source = vid.colormap;
	if (ghost)
	{
		dest = ghost_color_info[slot].translations;
		memcpy(dest, vid.colormap, sizeof(ghost_color_info[slot].translations));
		top = ghost_color_info[slot].colors & 0xf0;
		bottom = (ghost_color_info[slot].colors & 15) << 4;
	}
	else
	{
		dest = cl.scores[slot].translations;
		memcpy(dest, vid.colormap, sizeof(cl.scores[slot].translations));
		top = cl.scores[slot].colors & 0xf0;
		bottom = (cl.scores[slot].colors & 15) << 4;
	}

#ifdef GLQUAKE
	R_TranslatePlayerSkin (slot, ghost);
#endif

	for (i = 0 ; i < VID_GRADES ; i++, dest += 256, source += 256)
	{
		if (top < 128)	// the artists made some backward ranges. sigh.
			memcpy (dest + TOP_RANGE, source + top, 16);
		else
			for (j=0 ; j<16 ; j++)
				dest[TOP_RANGE+j] = source[top+15-j];

		if (bottom < 128)
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		else
			for (j=0 ; j<16 ; j++)
				dest[BOTTOM_RANGE+j] = source[bottom+15-j];
	}
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (int version) //johnfitz -- added a parameter 
{
	entity_t	*ent;

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");

	ent = &cl_static_entities[cl.num_statics];
	cl.num_statics++;
	CL_ParseBaseline (ent, version); //johnfitz -- added second parameter 

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->lerpflags |= LERP_RESETANIM; //johnfitz -- lerping
	ent->frame = ent->baseline.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;
	ent->transparency = ENTALPHA_DECODE(ent->baseline.alpha); //johnfitz -- alpha 
	ent->scale = ent->baseline.scale;

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (int version) //johnfitz -- added argument 
{
	int		i, sound_num, vol, atten;
	vec3_t	org;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord (cl.protocolflags);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (version == 2)
		sound_num = MSG_ReadShort();
	else
		sound_num = MSG_ReadByte();
	//johnfitz

	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}

/*
======================
CL_ParseString
======================
*/
void CL_ParseString (char *string)
{
	int		ping;
	char		*s, *s2, *s3;
	static	int	checkping = -1;
	static	int	checkip = -1;	// player whose IP address we're expecting
	static	int	remove_status = 0, begin_status = 0, playercount = 0;	// for ip logging

	s = string;
	if (!strcmp(string, "Client ping times:\n"))
	{
		cl.last_ping_time = cl.time;
		checkping = 0;
		if (!cl.console_ping)
			*string = 0;
	}
	else if (checkping >= 0)
	{
		while (*s == ' ')
			s++;
		ping = 0;
		if (*s >= '0' && *s <= '9')
		{
			while (*s >= '0' && *s <= '9')
				ping = 10 * ping + *s++ - '0';
			if ((*s++ == ' ') && *s && (s2 = strchr(s, '\n')))
			{
				s3 = cl.scores[checkping].name;
				while ((s3 = strchr(s3, '\n')) && s2)
				{
					s3++;
					s2 = strchr(s2+1, '\n');
				}
				if (s2)
				{
					*s2 = 0;
					if (!strncmp(cl.scores[checkping].name, s, 15))
					{
						cl.scores[checkping].ping = ping > 9999 ? 9999 : ping;
						checkping++;
						while (!*cl.scores[checkping].name && checkping < cl.maxclients)
							checkping++;
					}
					*s2 = '\n';
				}
				if (!cl.console_ping)
					*string = 0;
				if (checkping == cl.maxclients)
					checkping = -1;
			}
			else
				checkping = -1;
		}
		else
			checkping = -1;
		cl.console_ping = cl.console_ping && (checkping >= 0);
	}

	// check for IP information
	if (iplog_size)
	{
		if (!strncmp(string, "host:    ", 9))
		{
			begin_status = 1;
			if (!cl.console_status)
				remove_status = 1;
		}
		if (begin_status && !strncmp(string, "players: ", 9))
		{
			begin_status = 0;
			remove_status = 0;
			if (sscanf(string + 9, "%d", &playercount))
			{
				if (!cl.console_status)
					*string = 0;
			}
			else
				playercount = 0;
		}
		else if (playercount && string[0] == '#')
		{
			if (!sscanf(string, "#%d", &checkip) || --checkip < 0 || checkip >= cl.maxclients)
				checkip = -1;
			if (!cl.console_status)
				*string = 0;
			remove_status = 0;
		}
		else if (checkip != -1)
		{
			int a, b, c;
			if (sscanf(string, "   %d.%d.%d", &a, &b, &c) == 3)
			{
				cl.scores[checkip].addr = (a << 16) | (b << 8) | c;
				IPLog_Add (cl.scores[checkip].addr, cl.scores[checkip].name);
			}
			checkip = -1;
			if (!cl.console_status)
				*string = 0;
			remove_status = 0;

			if (!--playercount)
				cl.console_status = 0;
		}
		else 
		{
			playercount = 0;
			if (remove_status)
				*string = 0;
		}
	}
}

#define SHOWNET(x)						\
	if (cl_shownet.value == 2)				\
		Con_Printf ("%3i:%s\n", msg_readcount - 1, x)

#define CHECKDRAWSTATS						\
	if (show_stats.value == 3 || show_stats.value == 4)	\
		(drawstats_limit = cl.time + show_stats_length.value)

char *GetPrintedTime(double time)
{
	int			mins;
	double		secs;
	static char timestring[16];
	
	mins = time / 60;
	secs = time - (mins * 60);
	if (mins > 0)
		Q_snprintfz(timestring, sizeof(timestring), "%i:%08.5lf", mins, secs);
	else
		Q_snprintfz(timestring, sizeof(timestring), "%.5lf", secs);

	return timestring;
}

void PrintFinishTime()
{
	if (cl_demorewind.value)
		return;

	cls.marathon_time += cl.completed_time;
	cls.marathon_level++;

	if (!pr_qdqstats && !cls.demoplayback && sv.active)
	{
		char *timestring;
		
		// Sphere --- calling SV_ from CL_ here is probably not the best, but at
		// least we check for sv.active. We need the broadcast so that the time
		// messages appear in recorded demos and also get sent to clients
		// during coop play.
		timestring = GetPrintedTime(cl.completed_time);
		SV_BroadcastPrintf("\nexact time was %s\n", timestring);

		if (cls.marathon_level > 1)
		{
			SV_BroadcastPrintf("level %i in the sequence\n", cls.marathon_level);
			timestring = GetPrintedTime(cls.marathon_time);
			SV_BroadcastPrintf("total time is %s\n", timestring);
		}

		SV_BroadcastPrintf("\n");
	}

	Ghost_Finish ();
}


static void HandleFinish (int intermission)
{
	int old_intermission;

	if (!cl.intermission)	//joe: only save cl.completed_time if there was no intermission overlay shown already
	{
		cl.completed_time = cl.mtime[0];
		PrintFinishTime();
	}

	if (cls.demoplayback)
	{
		old_intermission = cl.intermission;
		cl.intermission = CL_DemoIntermissionState(cl.intermission, intermission);

		// Remove the last level time if we're reversing and intermission just
		// disappeared.
		if (old_intermission && !cl.intermission)
		{
			cls.marathon_time -= cl.completed_time;
			cls.marathon_level--;
		}
	}
	else
	{
		cl.intermission = intermission;
	}
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int		i, cmd, lastcmd;
	char	*s, *str;
	extern	float	drawstats_limit;
	extern	cvar_t	show_stats, show_stats_length;

// if recording demos, copy the message out
	if (cl_shownet.value == 1)
		Con_Printf ("%i ", net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");

	cl.onground = false;	// unless the server says otherwise

// parse the message
	MSG_BeginReading ();

	lastcmd = 0;
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();
		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL) //johnfitz -- was 128, changed for clarity 
		{
			SHOWNET("fast update");
			CL_ParseUpdate (cmd & 127);
			continue;
		}

		SHOWNET(svc_strings[cmd]);

	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("Illegible server message, previous was %s", svc_strings[lastcmd]); //johnfitz -- added svc_strings[lastcmd]
			break;

		case svc_nop:
			break;

		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();
			break;

		case svc_clientdata:
			CL_ParseClientdata(); //johnfitz -- removed bits parameter, we will read this inside CL_ParseClientdata() 
			break;

		case svc_version:
			i = MSG_ReadLong ();
			//johnfitz -- support multiple protocols
			if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_RMQ)
				Host_Error("Server returned version %i, not %i or %i or %i", i, PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_RMQ);
			cl.protocol = i;
			//johnfitz
			break;

		case svc_disconnect:
			Host_EndGame ("Server disconnected\n");

		case svc_print:
			s = MSG_ReadString ();
			CL_ParseString (s);
			Con_Printf ("%s", s);
			CHECKDRAWSTATS;
			break;

		case svc_centerprint:
			//johnfitz -- log centerprints to console
			str = MSG_ReadString();
			SCR_CenterPrint(str);
			Con_LogCenterPrint(str);
			//johnfitz
			break;

		case svc_stufftext:
			Cbuf_AddText (MSG_ReadString());
			break;

		case svc_damage:
			V_ParseDamage ();
			CHECKDRAWSTATS;
			break;

		case svc_serverinfo:
			if (cls.demoplayback)
				S_StopAllSounds (true);
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;

		case svc_setangle:
			for (i = 0 ; i < 3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle (cl.protocolflags);
			break;

		case svc_setview:
			cl.viewentity = MSG_ReadShort ();
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			Q_strncpyz (cl_lightstyle[i].map, MSG_ReadString(), sizeof(cl_lightstyle[i].map));
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;

		case svc_sound:
			CL_ParseStartSoundPacket ();
			break;

		case svc_stopsound:
			i = MSG_ReadShort ();
			S_StopSound (i>>3, i&7);
			break;

		case svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename > MAX_SCOREBOARD");
			Q_strncpyz (cl.scores[i].name, MSG_ReadString(), sizeof(cl.scores[i].name));
			break;

		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
			cl.scores[i].frags = MSG_ReadShort ();
			break;			

		case svc_updatecolors:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
			cl.scores[i].colors = MSG_ReadByte ();
			CL_NewTranslation (i, false);
			break;

		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 1); // johnfitz -- added second parameter 
			break;

		case svc_spawnstatic:
			CL_ParseStatic (1); //johnfitz -- added parameter 
			break;			

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_setpause:
			if ((cl.paused = MSG_ReadByte()))
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

		case svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= cls.signon)
				Host_Error ("Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			//johnfitz -- if signonnum==2, signon packet has been fully parsed, so check for excessive static ents and efrags
			if (i == 2)
			{
				if (cl.num_statics > 128)
					Con_DPrintf("%i static entities exceeds standard limit of 128 (max = %d).\n", cl.num_statics, MAX_STATIC_ENTITIES);
				R_CheckEfrags();
			}
			//johnfitz
			CL_SignonReply ();
			break;

		case svc_killedmonster:
			if (cls.demoplayback && cl_demorewind.value)
				cl.stats[STAT_MONSTERS]--;
			else
				cl.stats[STAT_MONSTERS]++;
			CHECKDRAWSTATS;
			break;

		case svc_foundsecret:
			if (cls.demoplayback && cl_demorewind.value)
				cl.stats[STAT_SECRETS]--;
			else
				cl.stats[STAT_SECRETS]++;
			CHECKDRAWSTATS;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				Sys_Error ("svc_updatestat: %i is invalid", i);
			cl.stats[i] = MSG_ReadLong ();
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound (1); //johnfitz -- added parameter 
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			cl.looptrack = MSG_ReadByte ();
			if (fmod_loaded)
				FMOD_PlayTrack(cl.cdtrack);
			else
			{
				if ((cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1))
					CDAudio_Play((byte)cls.forcetrack, true);
				else
					CDAudio_Play((byte)cl.cdtrack, true);
			}
			break;

		case svc_intermission:
			HandleFinish(1);
			vid.recalc_refdef = true;	// go to full screen
			V_RestoreAngles ();
			break;

		case svc_finale:
			HandleFinish(2);
			vid.recalc_refdef = true;	// go to full screen
			//johnfitz -- log centerprints to console
			str = MSG_ReadString();
			SCR_CenterPrint(str);
			Con_LogCenterPrint(str);
			//johnfitz
			V_RestoreAngles ();
			break;

		case svc_cutscene:
			HandleFinish(3);
			vid.recalc_refdef = true;	// go to full screen
			//johnfitz -- log centerprints to console
			str = MSG_ReadString();
			SCR_CenterPrint(str);
			Con_LogCenterPrint(str);
			//johnfitz
			V_RestoreAngles ();
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;

#ifdef GLQUAKE
		// nehahra support
		case svc_hidelmp:
			SHOWLMP_decodehide ();
			break;

		case svc_showlmp:
			SHOWLMP_decodeshow ();
			break;

		case svc_skybox:
			Cvar_Set (&r_skybox, MSG_ReadString());
			break;
#endif

		//johnfitz -- new svc types
		case svc_bf:
			Cmd_ExecuteString("bf", src_command);
			break;

		case svc_fog:
			Fog_ParseServerMessage();
			break;

		case svc_spawnbaseline2: //PROTOCOL_FITZQUAKE
			i = MSG_ReadShort();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline(CL_EntityNum(i), 2);
			break;

		case svc_spawnstatic2: //PROTOCOL_FITZQUAKE
			CL_ParseStatic(2);
			break;

		case svc_spawnstaticsound2: //PROTOCOL_FITZQUAKE
			CL_ParseStaticSound(2);
			break;
			//johnfitz

		case svc_achievement:	//used by the 2021 rerelease
			s = MSG_ReadString();
			Con_DPrintf("Ignoring svc_achievement (%s)\n", s);
			break;

		case svc_localsound:
			CL_ParseLocalSound();
			break;
		}

		lastcmd = cmd; //johnfitz 
	}
}
