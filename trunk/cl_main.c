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
// cl_main.c -- client main loop

#include "quakedef.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t	cl_name = {"_cl_name", "player", CVAR_ARCHIVE};
cvar_t	cl_color = {"_cl_color", "0", CVAR_ARCHIVE};

cvar_t	cl_shownet = {"cl_shownet", "0"};	// can be 0, 1, or 2
cvar_t	cl_nolerp = {"cl_nolerp", "0"};

cvar_t	cl_truelightning = {"cl_truelightning", "0"};
cvar_t	cl_sbar = {"cl_sbar", "0", CVAR_ARCHIVE};
cvar_t	cl_rocket2grenade = {"cl_r2g", "0"};
cvar_t	cl_mapname = {"mapname", "", CVAR_ROM};
cvar_t	cl_muzzleflash = {"cl_muzzleflash", "1"};

cvar_t	r_powerupglow = {"r_powerupglow", "1"};
cvar_t	r_explosiontype = {"r_explosiontype", "0"};
cvar_t	r_explosionlight = {"r_explosionlight", "1"};
cvar_t	r_rocketlight = {"r_rocketlight", "1"};
#ifdef GLQUAKE
cvar_t	r_explosionlightcolor = {"r_explosionlightcolor", "0"};
cvar_t	r_rocketlightcolor = {"r_rocketlightcolor", "0"};
#endif
cvar_t	r_rockettrail = {"r_rockettrail", "1"};
cvar_t	r_grenadetrail = {"r_grenadetrail", "1"};

static qboolean OnChange_cl_demospeed (cvar_t *var, char *string);
cvar_t	cl_demospeed = {"cl_demospeed", "1", 0, OnChange_cl_demospeed};

cvar_t	cl_demorewind = {"cl_demorewind", "0"};
cvar_t	cl_bobbing = {"cl_bobbing", "0"};
cvar_t	cl_deadbodyfilter = {"cl_deadbodyfilter", "0"};
cvar_t	cl_gibfilter = {"cl_gibfilter", "0"};
cvar_t	cl_maxfps = {"cl_maxfps", "72", CVAR_SERVER};
cvar_t	cl_advancedcompletion = {"cl_advancedcompletion", "1"};
cvar_t	cl_independentphysics = {"cl_independentphysics", "0", CVAR_INIT};

client_static_t	cls;
client_state_t	cl;
// FIXME: put these on hunk?
efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_entities[MAX_EDICTS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

int				cl_numvisedicts;
entity_t		*cl_visedicts[MAX_VISEDICTS];

modelindex_t		cl_modelindex[NUM_MODELINDEX];
char			*cl_modelnames[NUM_MODELINDEX];

tagentity_t		q3player_body, q3player_head;

extern qboolean physframe;

unsigned CheckModel (char *mdl);

static qboolean OnChange_cl_demospeed (cvar_t *var, char *string)
{
	float	newval = Q_atof(string);

	if (newval < 0 || newval > 20)
		return true;

	return false;
}

/*
=====================
CL_ClearState
=====================
*/
void CL_ClearState (void)
{
	int	i;

	if (!sv.active)
		Host_ClearMemory ();

	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.message);

// clear other arrays	
	memset (cl_efrags, 0, sizeof(cl_efrags));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset (cl_entities, 0, sizeof(cl_entities));
	memset (cl_temp_entities, 0, sizeof(cl_temp_entities));

// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;
	for (i=0 ; i < MAX_EFRAGS - 1 ; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
	cl.free_efrags[i].entnext = NULL;

#ifdef GLQUAKE
	if (nehahra)
		SHOWLMP_clear ();
#endif
}

#ifdef GLQUAKE
// nehahra supported
void Neh_ResetSFX (void)
{
	int	i;

	if (num_sfxorig == 0)
		num_sfxorig = num_sfx;

	num_sfx = num_sfxorig;
	Con_DPrintf ("Current SFX: %d\n", num_sfx);

	for (i = num_sfx + 1 ; i < MAX_SFX ; i++)
	{
		Q_strncpyz (known_sfx[i].name, "dfw3t23EWG#@T#@", sizeof(known_sfx[i].name));
		if (known_sfx[i].cache.data)
			Cache_Free (&known_sfx[i].cache);
	}
}
#endif

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
// stop sounds (especially looping!)
	S_StopAllSounds (true);
	
#ifdef GLQUAKE
	if (nehahra)
		FMOD_Stop_f ();
#endif

// if running a local server, shut it down
	if (cls.demoplayback)
	{
		CL_StopPlayback ();
	}
	else if (cls.state == ca_connected)
	{
		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf ("Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer (false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;

#ifdef GLQUAKE
	if (nehahra)
	        Neh_ResetSFX ();
#endif
}

void CL_Disconnect_f (void)
{
	CL_Disconnect ();
	if (sv.active)
		Host_ShutdownServer (false);
}

/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

#ifdef GLQUAKE
	if (nehahra)
	        num_sfxorig = num_sfx;
#endif

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	if (!(cls.netcon = NET_Connect(host)))
		Host_Error ("CL_Connect: connect failed");

	Con_DPrintf ("CL_EstablishConnection: connected to %s\n", host);

	if (cls.netcon->mod == MOD_JOEQUAKE && (deathmatch.value || coop.value))
		Con_DPrintf ("Connected to JoeQuake server\n");

	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing

	MSG_WriteByte (&cls.message, clc_nop);	// joe: fix for NAT from ProQuake
}

unsigned	source_key1 = 0x36117cbd;

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply (void)
{
	Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");

		if (cls.netcon && !cls.netcon->encrypt)
			cls.netcon->encrypt = 3;

		break;

	case 2:		
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %i %i\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 15));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("spawn %s", cls.spawnparms));

		if (cl_cheatfree)
		{
			char		path[MAX_QPATH];
			unsigned	crc;

			// exe checking
			Q_strncpyz (path, argv0, MAX_QPATH);
#ifdef WIN32
			if (!strstr(path, ".exe") && !strstr(path, ".EXE"))
				strcat (path, ".exe");
#endif
			if (Sys_FileTime(path) == -1)
				Host_Error ("Could not open %s", path);

			crc = Security_CRC_File (path);
			MSG_WriteLong (&cls.message, crc);
			MSG_WriteLong (&cls.message, source_key1);

			// model checking
			MSG_WriteLong (&cls.message, CheckModel("progs/player.mdl"));
			MSG_WriteLong (&cls.message, CheckModel("progs/eyes.mdl"));
		}

		break;

	case 3:	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();		// print remaining memory

		if (cls.netcon)
			cls.netcon->encrypt = 1;

		break;

	case 4:
		SCR_EndLoadingPlaque ();	// allow normal screen updates
		break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
int CL_NextDemo (void)
{
	char	str[128];

	if (cls.demonum == -1)
		return 0;		// don't play demos

	SCR_BeginLoadingPlaque ();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;

			return 0;
		}
	}

	Q_snprintfz (str, sizeof(str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;

	return 1;
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f (void)
{
	entity_t	*ent;
	int		i;
	
	for (i = 0, ent = cl_entities ; i < cl.num_entities ; i++, ent++)
	{
		Con_Printf ("%3i:", i);
		if (!ent->model)
		{
			Con_Printf ("EMPTY\n");
			continue;
		}
		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n", ent->model->name, ent->frame, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
	}
}

#ifdef GLQUAKE
dlighttype_t SetDlightColor (float f, dlighttype_t def, qboolean random)
{
	dlighttype_t	colors[NUM_DLIGHTTYPES-4] = {lt_red, lt_blue, lt_redblue, lt_green};

	if ((int)f == 1)
		return lt_red;
	else if ((int)f == 2)
		return lt_blue;
	else if ((int)f == 3) 
		return lt_redblue;
	else if ((int)f == 4)
		return lt_green;
	else if (((int)f == NUM_DLIGHTTYPES - 3) && random)
		return colors[rand()%(NUM_DLIGHTTYPES-4)];
	else
		return def;
}
#endif

/*
===============
CL_AllocDlight
===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int		i;
	dlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

void CL_NewDlight (int key, vec3_t origin, float radius, float time, int type)
{
	dlight_t	*dl;

	dl = CL_AllocDlight (key);
	VectorCopy (origin, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
#ifdef GLQUAKE
	dl->type = type;
#endif
}

/*
===============
CL_DecayLights
===============
*/
void CL_DecayLights (void)
{
	int		i;
	dlight_t	*dl;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= host_frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float CL_LerpPoint (void)
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp.value || cls.timedemo || sv.active)
	{
		cl.time = cl.ctime = cl.mtime[0];
		return 1;
	}

	if (f > 0.1)
	{	// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.ctime - cl.mtime[1]) / f;
	if (frac < 0)
	{
		if (frac < -0.01)
			cl.time = cl.ctime = cl.mtime[1];
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
			cl.time = cl.ctime = cl.mtime[0];
		frac = 1;
	}

	return frac;
}

qboolean Model_isDead (int modelindex, int frame)
{
	if (cl_deadbodyfilter.value == 2)
	{
		if ((modelindex == cl_modelindex[mi_fish] && frame >= 18 && frame <= 38) ||
			(modelindex == cl_modelindex[mi_dog] && frame >= 8 && frame <= 25) ||
			(modelindex == cl_modelindex[mi_soldier] && frame >= 8 && frame <= 28) ||
			(modelindex == cl_modelindex[mi_enforcer] && frame >= 41 && frame <= 65) ||
			(modelindex == cl_modelindex[mi_knight] && frame >= 76 && frame <= 96) ||
			(modelindex == cl_modelindex[mi_hknight] && frame >= 42 && frame <= 62) ||
			(modelindex == cl_modelindex[mi_scrag] && frame >= 46 && frame <= 53) ||
			(modelindex == cl_modelindex[mi_ogre] && frame >= 112 && frame <= 135) ||
			(modelindex == cl_modelindex[mi_fiend] && frame >= 45 && frame <= 53) ||
			(modelindex == cl_modelindex[mi_vore] && frame >= 16 && frame <= 22) ||
			(modelindex == cl_modelindex[mi_shambler] && frame >= 83 && frame <= 93) ||
			(modelindex == cl_modelindex[mi_player] && frame >= 41 && frame <= 102))
			return true;
	}
	else
	{
		if ((modelindex == cl_modelindex[mi_fish] && frame == 38) ||
			(modelindex == cl_modelindex[mi_dog] && (frame == 16 || frame == 25)) ||
			(modelindex == cl_modelindex[mi_soldier] && (frame == 17 || frame == 28)) ||
			(modelindex == cl_modelindex[mi_enforcer] && (frame == 54 || frame == 65)) ||
			(modelindex == cl_modelindex[mi_knight] && (frame == 85 || frame == 96)) ||
			(modelindex == cl_modelindex[mi_hknight] && (frame == 53 || frame == 62)) ||
			(modelindex == cl_modelindex[mi_scrag] && frame == 53) ||
			(modelindex == cl_modelindex[mi_ogre] && (frame == 125 || frame == 135)) ||
			(modelindex == cl_modelindex[mi_fiend] && frame == 53) ||
			(modelindex == cl_modelindex[mi_vore] && frame == 22) ||
			(modelindex == cl_modelindex[mi_shambler] && frame == 93) ||
			(modelindex == cl_modelindex[mi_player] && (frame == 49 || frame == 60 || frame == 69 || 
			 frame == 84 || frame == 93 || frame == 102)))
			return true;
	}

	return false;
}

qboolean Model_isHead (int modelindex)
{
	if (modelindex == cl_modelindex[mi_h_dog] || modelindex == cl_modelindex[mi_h_soldier] || 
		modelindex == cl_modelindex[mi_h_enforcer] || modelindex == cl_modelindex[mi_h_knight] || 
		modelindex == cl_modelindex[mi_h_hknight] || modelindex == cl_modelindex[mi_h_scrag] || 
		modelindex == cl_modelindex[mi_h_ogre] || modelindex == cl_modelindex[mi_h_fiend] || 
		modelindex == cl_modelindex[mi_h_vore] || modelindex == cl_modelindex[mi_h_shambler] || 
		modelindex == cl_modelindex[mi_h_zombie] || modelindex == cl_modelindex[mi_h_player])
		return true;

	return false;
}

/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities (void)
{
	int			i, j;
	float		frac, f, d, bobjrotate;
	vec3_t		delta, oldorg;
	entity_t	*ent;
	dlight_t	*dl;
	model_t		*model;

	// determine partial update time	
	frac = CL_LerpPoint ();

	cl_numvisedicts = 0;

	// interpolate player info
	for (i = 0 ; i < 3 ; i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback)
	{
		// interpolate the angles
		for (j = 0 ; j < 3 ; j++)
		{
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac*d;
		}
	}
	
	bobjrotate = anglemod (100 * cl.time);
	
	// start on the entity after the world
	for (i = 1, ent = cl_entities + 1 ; i < cl.num_entities ; i++, ent++)
	{
		if (!ent->model)
		{	// empty slot
			if (ent->forcelink)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}

		// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0])
		{
			ent->model = NULL;
#ifdef GLQUAKE
			ent->frame_start_time = ent->translate_start_time = ent->rotate_start_time = 0;
#endif
			continue;
		}

		VectorCopy (ent->origin, oldorg);

		if (ent->forcelink)
		{	// the entity was not updated in the last message so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		}
		else
		{	// if the delta is large, assume a teleport and don't lerp
			f = frac;

			for (j = 0 ; j < 3 ; j++)
			{
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
					f = 1;		// assume a teleportation, not a motion
			}

#ifdef GLQUAKE
			if (f >= 1)
				ent->translate_start_time = ent->rotate_start_time = 0;
#endif

		// interpolate the origin and angles
			for (j = 0 ; j < 3 ; j++)
			{
				ent->origin[j] = ent->msg_origins[1][j] + f * delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f * d;
			}
		}

		if (cl_deadbodyfilter.value && ent->model->type == mod_alias && 
			Model_isDead(ent->modelindex, ent->frame))
			continue;

		if (cl_gibfilter.value && ent->model->type == mod_alias && 
			(ent->modelindex == cl_modelindex[mi_gib1] || ent->modelindex == cl_modelindex[mi_gib2] || 
			 ent->modelindex == cl_modelindex[mi_gib3] || Model_isHead(ent->modelindex)))
			continue;

		if (ent->modelindex == cl_modelindex[mi_explo1] || ent->modelindex == cl_modelindex[mi_explo2])
		{
			// software removal of sprites
			if (r_explosiontype.value == 2 || r_explosiontype.value == 3)
				continue;
#ifdef GLQUAKE
			if (qmb_initialized && gl_part_explosions.value)
				continue;
#endif
		}

		if (!(model = cl.model_precache[ent->modelindex]))
			Host_Error ("CL_RelinkEntities: bad modelindex");

		if (ent->modelindex == cl_modelindex[mi_rocket] && cl_rocket2grenade.value && cl_modelindex[mi_grenade] != -1)
			ent->model = cl.model_precache[cl_modelindex[mi_grenade]];

		// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
		{
			ent->angles[1] = bobjrotate;
			if (cl_bobbing.value)
				ent->origin[2] += sin(bobjrotate / 90 * M_PI) * 5 + 5;
		}

		// EF_BRIGHTFIELD is not used by original progs
		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);

		// joe: force colored powerup glows for relevant models
		if (ent->model->modhint == MOD_QUAD)
		{
			CL_NewDlight(i, ent->origin, 200 + (rand() & 31), 0.1, lt_blue);
		}
		if (ent->model->modhint == MOD_PENT)
		{
			CL_NewDlight(i, ent->origin, 200 + (rand() & 31), 0.1, lt_red);
		}

		if ((ent->effects & EF_MUZZLEFLASH) && cl_muzzleflash.value)
		{
			vec3_t	fv;

			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, NULL, NULL);
			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand() & 31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
#ifdef GLQUAKE
			if ((ent->modelindex == cl_modelindex[mi_shambler] || (ent->model->modhint == MOD_PLAYER && cl.stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)) &&
				qmb_initialized && gl_part_lightning.value)
				dl->type = lt_blue;
			else if (ent->modelindex == cl_modelindex[mi_scrag] && qmb_initialized)
				dl->type = lt_green;
			else
				dl->type = lt_muzzleflash;
#endif

#ifdef GLQUAKE
			if (qmb_initialized && gl_part_muzzleflash.value)
			{
				vec3_t	rv, uv, smokeorg;

				if (i == cl.viewentity)
				{
					vec3_t	smokeorg2;

					VectorCopy (cl_entities[cl.viewentity].origin, smokeorg);
					smokeorg[2] += cl.crouch;

					smokeorg[2] += 14;	// move it up

					// adjust for current view angles
					AngleVectors (cl.viewangles, fv, rv, uv);

					// forward vector
					VectorMA (smokeorg, 18, fv, smokeorg);

					if (cl.stats[STAT_ACTIVEWEAPON] == IT_NAILGUN || cl.stats[STAT_ACTIVEWEAPON] == IT_SUPER_SHOTGUN)
					{
						VectorMA (smokeorg, -3, rv, smokeorg);
						VectorMA (smokeorg, 6, rv, smokeorg2);
						QMB_MuzzleFlash (smokeorg, true);					
						QMB_MuzzleFlash (smokeorg2, true);
					}
					else if (cl.stats[STAT_ACTIVEWEAPON] == IT_SUPER_NAILGUN || cl.stats[STAT_ACTIVEWEAPON] == IT_SHOTGUN)
					{
						QMB_MuzzleFlash (smokeorg, true);
					}
				}
				else if (ent->modelindex == cl_modelindex[mi_player])
				{
					VectorCopy (ent->origin, smokeorg);

					smokeorg[2] += 14;	// move it up

					// adjust for current view angles
					AngleVectors (ent->angles, fv, rv, uv);

					VectorScale(fv, 18, fv);
					VectorMA (fv, 7, rv, fv);
					VectorAdd(smokeorg, fv, smokeorg);

					QMB_MuzzleFlash (smokeorg, false);
				}
				else if (ent->modelindex == cl_modelindex[mi_enforcer])
				{
					VectorCopy (ent->origin, smokeorg);

					smokeorg[2] += 14;	// move it up

					// adjust for current view angles
					AngleVectors (ent->angles, fv, rv, uv);

					VectorScale(fv, 26, fv);
					VectorMA (fv, 9, rv, fv);
					VectorAdd(smokeorg, fv, smokeorg);

					QMB_MuzzleFlash (smokeorg, false);
				}
				else if (ent->modelindex == cl_modelindex[mi_soldier])
				{
					VectorCopy (ent->origin, smokeorg);

					smokeorg[2] += 14;	// move it up

					// adjust for current view angles
					AngleVectors (ent->angles, fv, rv, uv);

					VectorScale(fv, 18, fv);
					VectorMA (fv, 7, rv, fv);
					VectorAdd(smokeorg, fv, smokeorg);

					QMB_MuzzleFlash (smokeorg, false);
				}
			}
#endif
		}

		if (ent->modelindex != cl_modelindex[mi_eyes] && 
		    ((ent->modelindex != cl_modelindex[mi_player] && ent->model->modhint != MOD_PLAYER && 
		      ent->modelindex != cl_modelindex[mi_h_player]) || (r_powerupglow.value && cl.stats[STAT_HEALTH] > 0)))
		{
			if ((ent->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
				CL_NewDlight (i, ent->origin, 200 + (rand() & 31), 0.1, lt_redblue);
			else if (ent->effects & EF_BLUE)
				CL_NewDlight (i, ent->origin, 200 + (rand() & 31), 0.1, lt_blue);
			else if (ent->effects & EF_RED)
				CL_NewDlight (i, ent->origin, 200 + (rand() & 31), 0.1, lt_red);
		// EF_BRIGHTLIGHT is not used by original progs
			else if (ent->effects & EF_BRIGHTLIGHT)
			{
				vec3_t	tmp;

				VectorCopy (ent->origin, tmp);
				tmp[2] += 16;
				CL_NewDlight (i, tmp, 400 + (rand() & 31), 0.1, lt_default);
			}
		// EF_DIMLIGHT is for powerup glows and enforcer's laser
			else if (ent->effects & EF_DIMLIGHT)
			{
				dlighttype_t lighttype = lt_default;

				if (i == cl.viewentity)
				{
					if ((cl.items & IT_QUAD) && (cl.items & IT_INVULNERABILITY))
						lighttype = lt_redblue;
					else if (cl.items & IT_QUAD)
						lighttype = lt_blue;
					else if (cl.items & IT_INVULNERABILITY)
						lighttype = lt_red;
				}

				CL_NewDlight (i, ent->origin, 200 + (rand() & 31), 0.1, lighttype);
			}
		}

		if (model->flags)
		{
			if (!ent->traildrawn || !VectorL2Compare(ent->trail_origin, ent->origin, 140))
			{
				VectorCopy (ent->origin, oldorg);	//not present last frame or too far away
				ent->traildrawn = true;
			}
			else
			{
				VectorCopy (ent->trail_origin, oldorg);
			}

			if (model->flags & EF_GIB)
				R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, BIG_BLOOD_TRAIL);
			else if (model->flags & EF_ZOMGIB)
				R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, BLOOD_TRAIL);
			else if (model->flags & EF_TRACER)
				R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, TRACER1_TRAIL);
			else if (model->flags & EF_TRACER2)
				R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, TRACER2_TRAIL);
			else if (model->flags & EF_ROCKET)
			{
				if (model->modhint == MOD_LAVABALL)
				{
					R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, LAVA_TRAIL);

					dl = CL_AllocDlight (i);
					VectorCopy (ent->origin, dl->origin);
					dl->radius = 100 * (1 + bound(0, r_rocketlight.value, 1));
					dl->die = cl.time + 0.1;
			#ifdef GLQUAKE
					dl->type = lt_rocket;
			#endif
				}
				else
				{
					// R_RocketTrail _must_ run in QMB, even if rockettrail is 0
					if (r_rockettrail.value)
						R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, ROCKET_TRAIL);

					if (r_rocketlight.value)
					{
						dl = CL_AllocDlight (i);
						VectorCopy (ent->origin, dl->origin);
						dl->radius = 100 * (1 + bound(0, r_rocketlight.value, 1));
						dl->die = cl.time + 0.1;
			#ifdef GLQUAKE
						dl->type = SetDlightColor (r_rocketlightcolor.value, lt_rocket, true);
			#endif
					}

			#ifdef GLQUAKE
				/*	if (qmb_initialized)
					{
						vec3_t	back;
						float	scale;

						VectorSubtract (oldorg, ent->origin, back);
						scale = 8.0 / VectorLength(back);
						VectorMA (ent->origin, scale, back, back);
						QMB_MissileFire (back);
					}*/
			#endif
				}
			}
			else if ((model->flags & EF_GRENADE) && r_grenadetrail.value)
			{
				// Nehahra dem compatibility
#ifdef GLQUAKE
				if (ent->transparency == -1 && cl.time >= ent->smokepuff_time)
				{
					R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, NEHAHRA_SMOKE);
					ent->smokepuff_time = cl.time + 0.14;
				}
				else
#endif
					R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, GRENADE_TRAIL);
			}
			else if (model->flags & EF_TRACER3)
				R_RocketTrail (oldorg, ent->origin, &ent->trail_origin, VOOR_TRAIL);
		}

#ifdef GLQUAKE
		if (qmb_initialized)
		{
			if (ent->modelindex == cl_modelindex[mi_bubble])
			{
				QMB_StaticBubble (ent);
				continue;
			}
			else if (gl_part_lightning.value && ent->modelindex == cl_modelindex[mi_shambler] &&  ent->frame >= 65 && ent->frame <= 68)
			{
				vec3_t	liteorg;

				VectorCopy (ent->origin, liteorg);
				liteorg[2] += 32;
				QMB_ShamblerCharge (liteorg);
			}
			else if (gl_part_trails.value && ent->model->modhint == MOD_SPIKE)
			{
				if (!ent->traildrawn || !VectorL2Compare(ent->trail_origin, ent->origin, 140))
				{
					VectorCopy (ent->origin, oldorg);	//not present last frame or too far away
					ent->traildrawn = true;
				}
				else
				{
					VectorCopy (ent->trail_origin, oldorg);
				}

				QMB_RocketTrail (oldorg, ent->origin, &ent->trail_origin, BUBBLE_TRAIL);
			}
		}
#endif

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active.value)
			continue;

		// nehahra support
		if (ent->effects & EF_NODRAW)
			continue;

		if (cl_numvisedicts < MAX_VISEDICTS)
			cl_visedicts[cl_numvisedicts++] = ent;
	}
}

/*
===============
CL_CalcCrouch

Smooth out stair step ups
===============
*/
void CL_CalcCrouch (void)
{
	qboolean	teleported;
	entity_t	*ent;
	static	vec3_t	oldorigin = {0, 0, 0};
	static	float	oldz = 0, extracrouch = 0, crouchspeed = 100;
	
	ent = &cl_entities[cl.viewentity];

	teleported = !VectorL2Compare(ent->origin, oldorigin, 48);
	VectorCopy (ent->origin, oldorigin);

	if (teleported)
	{
		// possibly teleported or respawned
		oldz = ent->origin[2];
		extracrouch = 0;
		crouchspeed = 100;
		cl.crouch = 0;
		return;
	}

	if (cl.onground && ent->origin[2] - oldz > 0)
	{
		if (ent->origin[2] - oldz > 20)
		{
			// if on steep stairs, increase speed
			if (crouchspeed < 160)
			{
				extracrouch = ent->origin[2] - oldz - host_frametime * 200 - 15;
				extracrouch = min(extracrouch, 5);
			}
			crouchspeed = 160;
		}

		oldz += host_frametime * crouchspeed;
		if (oldz > ent->origin[2])
			oldz = ent->origin[2];

		if (ent->origin[2] - oldz > 15 + extracrouch)
			oldz = ent->origin[2] - 15 - extracrouch;
		extracrouch -= host_frametime * 200;
		extracrouch = max(extracrouch, 0);

		cl.crouch = oldz - ent->origin[2];
	}
	else
	{
		// in air or moving down
		oldz = ent->origin[2];
		cl.crouch += host_frametime * 150;
		if (cl.crouch > 0)
			cl.crouch = 0;
		crouchspeed = 100;
		extracrouch = 0;
	}
}

#ifdef GLQUAKE
void CL_CopyPlayerInfo (entity_t *ent, entity_t *player)
{
	memcpy (&ent->baseline, &player->baseline, sizeof(entity_state_t));

	ent->msgtime = player->msgtime;
	memcpy (ent->msg_origins, player->msg_origins, sizeof(ent->msg_origins));
	VectorCopy (player->origin, ent->origin);
	memcpy (ent->msg_angles, player->msg_angles, sizeof(ent->msg_angles));
	VectorCopy (player->angles, ent->angles);

	ent->model = (ent == &q3player_body.ent) ? cl.model_precache[cl_modelindex[mi_q3torso]] : cl.model_precache[cl_modelindex[mi_q3head]];
	ent->efrag = player->efrag;

	ent->frame = player->frame;
	ent->syncbase = player->syncbase;
	ent->colormap = player->colormap;
	ent->effects = player->effects;
	ent->skinnum = player->skinnum;
	ent->visframe = player->visframe;
	ent->dlightframe = player->dlightframe;
	ent->dlightbits = player->dlightbits;

	ent->trivial_accept = player->trivial_accept;
	ent->topnode = player->topnode;

	ent->modelindex = (ent == &q3player_body.ent) ? cl_modelindex[mi_q3torso] : cl_modelindex[mi_q3head];
	VectorCopy (player->trail_origin, ent->trail_origin);
	ent->traildrawn = player->traildrawn;
	ent->noshadow = player->noshadow;

	ent->istransparent = player->istransparent;
	ent->transparency = player->transparency;
	ent->smokepuff_time = player->smokepuff_time;
}
#endif

/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
void CL_ReadFromServer (void)
{
	int	ret;

	cl.oldtime = cl.ctime;
	cl.time += host_frametime;
	if (!cl_demorewind.value || !cls.demoplayback)
		cl.ctime += host_frametime;
	else
		cl.ctime -= host_frametime;

	do {
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet.value)
		Con_Printf ("\n");

	CL_RelinkEntities ();
	CL_CalcCrouch ();
	CL_UpdateTEnts ();
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	usercmd_t	cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
	// get basic movement from keyboard
		CL_BaseMove (&cmd);

	// allow mice or other external controllers to add to the move
		if (!cl_independentphysics.value || (physframe && cl_independentphysics.value))
			IN_Move (&cmd);

	// send the unreliable message
		CL_SendMove (&cmd);
	}

	if (cls.demoplayback)
	{
		SZ_Clear (&cls.message);
		return;
	}

// send the reliable message
	if (!cls.message.cursize)
		return;		// no message at all

	if (!NET_CanSendMessage(cls.netcon))
	{
		Con_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage(cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

char *CL_MapName (void)
{
	return cl_mapname.string;
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{	
	SZ_Alloc (&cls.message, 1024);

	CL_InitInput ();
	CL_InitModelnames ();
	CL_InitTEnts ();

// register our commands
	Cvar_Register (&cl_name);
	Cvar_Register (&cl_color);

	Cvar_Register (&cl_shownet);
	Cvar_Register (&cl_nolerp);

	Cvar_Register (&cl_truelightning);
	Cvar_Register (&cl_sbar);
	Cvar_Register (&cl_rocket2grenade);
	Cvar_Register (&cl_mapname);
	Cvar_Register (&cl_muzzleflash);
	Cvar_Register (&cl_warncmd);

	Cvar_Register (&r_powerupglow);
	Cvar_Register (&r_explosiontype);
	Cvar_Register (&r_explosionlight);
	Cvar_Register (&r_rocketlight);
#ifdef GLQUAKE
	Cvar_Register (&r_explosionlightcolor);
	Cvar_Register (&r_rocketlightcolor);
#endif
	Cvar_Register (&r_rockettrail);
	Cvar_Register (&r_grenadetrail);

	Cvar_Register (&cl_demospeed);
	Cvar_Register (&cl_demorewind);
	Cvar_Register (&cl_bobbing);
	Cvar_Register (&cl_deadbodyfilter);
	Cvar_Register (&cl_gibfilter);
	Cvar_Register (&cl_maxfps);
	Cvar_Register (&cl_advancedcompletion);
	Cvar_Register (&cl_independentphysics);

	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
}
