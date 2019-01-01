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
	"?",	// 38
	"?",	// 39
	"?",	// 40
	"?",	// 41
	"?",	// 42
	"?",	// 43
	"?",	// 44
	"?",	// 45
	"?",	// 46
	"?",	// 47
	"?",	// 48
	"?"	// 49
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
	if (num >= cl.num_entities)
	{
		if (num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: %i is an invalid number", num);

		while (cl.num_entities <= num)
		{
			cl_entities[cl.num_entities].colormap = vid.colormap;
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
	
	channel = MSG_ReadShort ();
	sound_num = MSG_ReadByte ();

	ent = channel >> 3;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
void CL_KeepaliveMessage (void)
{
	float		time;
	static float lastmsg;
	int			ret;
	sizebuf_t	old;
	byte		olddata[8192];
	
	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
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
	if (i != PROTOCOL_VERSION)
	{
		Con_Printf ("Server returned version %i, not %i", i, PROTOCOL_VERSION);
		return;
	}

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf ("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
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

#ifdef GLQUAKE
	R_NewMap(false);
#else
	R_NewMap();
#endif

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
int	bitcounts[16];

void CL_ParseUpdate (int bits)
{
	int		i, num;
	model_t		*model;
	qboolean	forcelink;
	entity_t	*ent;
#ifdef GLQUAKE
	int		skin;
#endif

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
		bits |= (MSG_ReadByte() << 8);

	num = (bits & U_LONGENTITY) ? MSG_ReadShort() : MSG_ReadByte();

	ent = CL_EntityNum (num);

	for (i=0 ; i<16 ; i++)
		if (bits & (1 << i))
			bitcounts[i]++;

	forcelink = (ent->msgtime != cl.mtime[1]) ? true : false;

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
			R_TranslatePlayerSkin (num - 1);
#endif
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
			R_TranslatePlayerSkin (num - 1);
	}
#else
	ent->skinnum = (bits & U_SKIN) ? MSG_ReadByte() : ent->baseline.skin;
#endif

	ent->effects = (bits & U_EFFECTS) ? MSG_ReadByte() : ent->baseline.effects;

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	ent->msg_origins[0][0] = (bits & U_ORIGIN1) ? MSG_ReadCoord() : ent->baseline.origin[0];
	ent->msg_angles[0][0] = (bits & U_ANGLE1) ? MSG_ReadAngle() : ent->baseline.angles[0];

	ent->msg_origins[0][1] = (bits & U_ORIGIN2) ? MSG_ReadCoord() : ent->baseline.origin[1];
	ent->msg_angles[0][1] = (bits & U_ANGLE2) ? MSG_ReadAngle() : ent->baseline.angles[1];

	ent->msg_origins[0][2] = (bits & U_ORIGIN3) ? MSG_ReadCoord() : ent->baseline.origin[2];
	ent->msg_angles[0][2] = (bits & U_ANGLE3) ? MSG_ReadAngle() : ent->baseline.angles[2];

#ifdef GLQUAKE
	if (bits & U_TRANS)
	{
		int	fullbright, temp;

		temp = MSG_ReadFloat ();
		ent->istransparent = true;
		ent->transparency = MSG_ReadFloat ();
		if (temp == 2)
			fullbright = MSG_ReadFloat ();
	}
	else
	{
		ent->istransparent = (ent->model && ent->model->type == mod_md3 && (ent->model->flags & EF_Q3TRANS)) ? true : false;
		ent->transparency = 1;
	}
#endif

	if (bits & U_NOLERP)
		ent->forcelink = true;

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
void CL_ParseBaseline (entity_t *ent)
{
	int	i;

	ent->baseline.modelindex = MSG_ReadByte ();
	ent->baseline.frame = MSG_ReadByte ();
	ent->baseline.colormap = MSG_ReadByte ();
	ent->baseline.skin = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord ();
		ent->baseline.angles[i] = MSG_ReadAngle ();
	}
}

extern	float	cl_ideal_punchangle;

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (int bits)
{
	int	i, j;

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
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
	int	i, j, top, bottom;
	byte	*dest, *source;

	if (slot > cl.maxclients)
		Host_Error ("CL_NewTranslation: slot > cl.maxclients");

	dest = cl.scores[slot].translations;
	source = vid.colormap;
	memcpy (dest, vid.colormap, sizeof(cl.scores[slot].translations));
	top = cl.scores[slot].colors & 0xf0;
	bottom = (cl.scores[slot].colors & 15) << 4;

#ifdef GLQUAKE
	R_TranslatePlayerSkin (slot);
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
void CL_ParseStatic (void)
{
	entity_t	*ent;

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl.num_statics];
	cl.num_statics++;
	CL_ParseBaseline (ent);

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->frame = ent->baseline.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (void)
{
	int	i, sound_num, vol, atten;
	vec3_t	org;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	sound_num = MSG_ReadByte ();
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

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int		i, cmd;
	char		*s;
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
		if (cmd & 128)
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
			Host_Error ("CL_ParseServerMessage: Illegible server message");
			break;

		case svc_nop:
			break;

		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();
			break;

		case svc_clientdata:
			i = MSG_ReadShort ();
			CL_ParseClientdata (i);
			break;

		case svc_version:
			i = MSG_ReadLong ();
			if (i != PROTOCOL_VERSION)
				Host_Error ("CL_ParseServerMessage: Server is protocol %i instead of %i", i, PROTOCOL_VERSION);
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
			SCR_CenterPrint (MSG_ReadString());
			break;

		case svc_stufftext:
			Cbuf_AddText (MSG_ReadString());
			break;

		case svc_damage:
			V_ParseDamage ();
			CHECKDRAWSTATS;
			break;

		case svc_serverinfo:
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;

		case svc_setangle:
			for (i = 0 ; i < 3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle ();
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
			CL_NewTranslation (i);
			break;

		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i));
			break;

		case svc_spawnstatic:
			CL_ParseStatic ();
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
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			cl.looptrack = MSG_ReadByte ();
			if ((cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1))
				CDAudio_Play ((byte)cls.forcetrack, true);
			else
				CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			// intermission bugfix -- by joe
			cl.completed_time = cl.mtime[0];
			vid.recalc_refdef = true;	// go to full screen
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString());
			break;

		case svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString());
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
		}
	}
}
