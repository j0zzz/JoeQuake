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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"

int		num_temp_entities;
entity_t	cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

static	vec3_t	playerbeam_end;

float		ExploColor[3];		// joe: for color mapped explosions

model_t		*cl_bolt1_mod, *cl_bolt2_mod, *cl_bolt3_mod, *cl_beam_mod;
model_t		*cl_q3gunshot_mod, *cl_q3teleport_mod;

sfx_t		*cl_sfx_wizhit, *cl_sfx_knighthit, *cl_sfx_tink1;
sfx_t		*cl_sfx_ric1, *cl_sfx_ric2, *cl_sfx_ric3, *cl_sfx_r_exp3;

/*
=================
CL_InitTEnts
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
}

/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts (void)
{
	cl_bolt1_mod = cl_bolt2_mod = cl_bolt3_mod = cl_beam_mod = NULL;
	cl_q3gunshot_mod = cl_q3teleport_mod = NULL;

	memset (&cl_beams, 0, sizeof(cl_beams));
}

/*
=================
CL_ParseBeam
=================
*/
void CL_ParseBeam (model_t *m)
{
	int		i, ent;
	vec3_t	start, end;
	beam_t	*b;

	ent = MSG_ReadShort ();

	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();

	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

	if (ent == cl.viewentity)
		VectorCopy (end, playerbeam_end);

// override any beam with the same entity
	for (i = 0, b = cl_beams ; i < MAX_BEAMS ; i++, b++)
	{
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}

// find a free beam
	for (i = 0, b = cl_beams ; i < MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}

	Con_Printf ("beam list overflow!\n");
}

#define	SetCommonExploStuff				\
do {									\
	dl = CL_AllocDlight (0);			\
	VectorCopy (pos, dl->origin);		\
	dl->radius = 150 + 200 * bound(0, r_explosionlight.value, 1);\
	dl->die = cl.time + 0.5;			\
	dl->decay = 300;					\
} while(0)

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt (void)
{
	int			type, rnd, colorStart, colorLength;
	vec3_t		pos;
	dlight_t	*dl;
#ifdef GLQUAKE
	byte		*colorByte;
#endif

	type = MSG_ReadByte ();
	switch (type)
	{
	case TE_WIZSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (gl_part_spikes.value == 2 && !cl_q3gunshot_mod)
			cl_q3gunshot_mod = Mod_ForName ("progs/bullet.md3", true);
#endif
		R_RunParticleEffect (pos, vec3_origin, 20, 30);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (gl_part_spikes.value == 2 && !cl_q3gunshot_mod)
			cl_q3gunshot_mod = Mod_ForName ("progs/bullet.md3", true);
#endif
		R_RunParticleEffect (pos, vec3_origin, 226, 20);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;

	case TE_SPIKE:				// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (gl_part_spikes.value == 2 && !cl_q3gunshot_mod)
			cl_q3gunshot_mod = Mod_ForName ("progs/bullet.md3", true);

	// joe: they put the ventillator's wind effect to "10" in Nehahra. sigh.
		if (nehahra)
			R_RunParticleEffect (pos, vec3_origin, 0, 9);
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 0, 10);

		if (rand() % 5)
		{
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		}
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_SUPERSPIKE:			// super spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (gl_part_spikes.value == 2 && !cl_q3gunshot_mod)
			cl_q3gunshot_mod = Mod_ForName ("progs/bullet.md3", true);
#endif
		R_RunParticleEffect (pos, vec3_origin, 0, 20);
		if (rand() % 5)
		{
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		}
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (gl_part_gunshots.value == 2 && !cl_q3gunshot_mod)
			cl_q3gunshot_mod = Mod_ForName ("progs/bullet.md3", true);
#endif
		R_RunParticleEffect (pos, vec3_origin, 0, 21);
		break;

	case TE_EXPLOSION:			// rocket explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		if (r_explosiontype.value == 3)
			R_RunParticleEffect (pos, vec3_origin, 225, 50);
		else
			R_ParticleExplosion (pos);

		if (r_explosionlight.value)
		{
			SetCommonExploStuff;
#ifdef GLQUAKE
			dl->type = SetDlightColor (r_explosionlightcolor.value, lt_explosion, true);
#endif
		}

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_BlobExplosion (pos);

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_LIGHTNING1:			// lightning bolts
		if (!cl_bolt1_mod)
			//cl_bolt1_mod = Mod_ForName (gl_loadq3models.value ? "progs/bolt.md3" : "progs/bolt.mdl", true);
			cl_bolt1_mod = Mod_ForName ("progs/bolt.mdl", true);
		CL_ParseBeam (cl_bolt1_mod);
		break;

	case TE_LIGHTNING2:			// lightning bolts
		if (!cl_bolt2_mod)
			//cl_bolt2_mod = Mod_ForName (gl_loadq3models.value ? "progs/bolt2.md3" : "progs/bolt2.mdl", true);
			cl_bolt2_mod = Mod_ForName ("progs/bolt2.mdl", true);
		CL_ParseBeam (cl_bolt2_mod);
		break;

	case TE_LIGHTNING3:			// lightning bolts
		if (!cl_bolt3_mod)
			//cl_bolt3_mod = Mod_ForName (gl_loadq3models.value ? "progs/bolt3.md3" : "progs/bolt3.mdl", true);
			cl_bolt3_mod = Mod_ForName ("progs/bolt3.mdl", true);
		CL_ParseBeam (cl_bolt3_mod);
		break;

// nehahra support
	case TE_LIGHTNING4:			// lightning bolts
		CL_ParseBeam (Mod_ForName(MSG_ReadString(), true));
		break;

// PGM 01/21/97 
	case TE_BEAM:				// grappling hook beam
		if (!cl_beam_mod)
			cl_beam_mod = Mod_ForName ("progs/beam.mdl", true);
		CL_ParseBeam (cl_beam_mod);
		break;
// PGM 01/21/97

	case TE_LAVASPLASH:	
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_LavaSplash (pos);
		break;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (gl_part_telesplash.value == 2 && !cl_q3teleport_mod)
			cl_q3teleport_mod = Mod_ForName ("progs/telep.md3", true);
#endif
		R_TeleportSplash (pos);
		break;

	case TE_EXPLOSION2:			// color mapped explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		colorStart = MSG_ReadByte ();
		colorLength = MSG_ReadByte ();

		if (r_explosiontype.value == 3)
			R_RunParticleEffect (pos, vec3_origin, 225, 50);
		else
			R_ColorMappedExplosion (pos, colorStart, colorLength);

		if (r_explosionlight.value)
		{
			SetCommonExploStuff;
#ifdef GLQUAKE
			colorByte = (byte *)&d_8to24table[colorStart];
			ExploColor[0] = ((float)colorByte[0]) / (2.0 * 255.0);
			ExploColor[1] = ((float)colorByte[1]) / (2.0 * 255.0);
			ExploColor[2] = ((float)colorByte[2]) / (2.0 * 255.0);
			dl->type = lt_explosion2;
#endif
		}

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	// nehahra support
	case TE_EXPLOSION3:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		ExploColor[0] = MSG_ReadCoord () / 2;
		ExploColor[1] = MSG_ReadCoord () / 2;
		ExploColor[2] = MSG_ReadCoord () / 2;

		if (r_explosiontype.value == 3)
			R_RunParticleEffect (pos, vec3_origin, 225, 50);
		else
			R_ParticleExplosion (pos);

		if (r_explosionlight.value)
		{
			SetCommonExploStuff;
#ifdef GLQUAKE
			dl->type = lt_explosion3;
#endif
		}

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	default:
		Sys_Error ("CL_ParseTEnt: bad type");
	}
}

/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts == MAX_VISEDICTS || num_temp_entities == MAX_TEMP_ENTITIES)
		return NULL;

	ent = &cl_temp_entities[num_temp_entities];
	memset (ent, 0, sizeof(*ent));
	num_temp_entities++;
	cl_visedicts[cl_numvisedicts++] = ent;

	ent->colormap = vid.colormap;
	return ent;
}

qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t	trace;

	memset (&trace, 0, sizeof(trace));
	if (!SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, end, &trace))
	{
		if (trace.fraction < 1)
		{
			VectorCopy (trace.endpos, impact);
			if (normal)
				VectorCopy (trace.plane.normal, normal);

			return true;
		}
	}

	return false;
}

/*
=================
CL_UpdateBeams
=================
*/
void CL_UpdateBeams (void)
{
	int		i;
	float	d;
	beam_t	*b;
	vec3_t	dist, org, beamstart, ang;
	entity_t *ent;
#ifdef GLQUAKE
	int		j;
	vec3_t	beamend;
#endif

	// update lightning
	for (i = 0, b = cl_beams ; i < MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
		{
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
			b->start[2] += cl.crouch;
			if (cl_truelightning.value)
			{
				vec3_t	forward, v, org, ang;
				float	f, delta;

				f = max(0, min(1, cl_truelightning.value));

				VectorSubtract (playerbeam_end, cl_entities[cl.viewentity].origin, v);
				v[2] -= DEFAULT_VIEWHEIGHT;	// adjust for view height
				vectoangles (v, ang);

				// lerp pitch
				ang[0] = -ang[0];
				if (ang[0] < -180)
					ang[0] += 360;
				ang[0] += (cl.viewangles[0] - ang[0]) * f;

				// lerp yaw
				delta = cl.viewangles[1] - ang[1];
				if (delta > 180)
					delta -= 360;
				if (delta < -180)
					delta += 360;
				ang[1] += delta * f;

				AngleVectors (ang, forward, NULL, NULL);
				VectorScale (forward, 600, forward);
				VectorCopy (cl_entities[cl.viewentity].origin, org);
				org[2] += 16;
				VectorAdd (org, forward, b->end);

				TraceLineN (org, b->end, b->end, NULL);
			}
		}

		// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);
		vectoangles (dist, ang);

		// add new entities for the lightning
		VectorCopy (b->start, org);
		VectorCopy (b->start, beamstart);
		d = VectorNormalize (dist);
		VectorScale (dist, 30, dist);

		for ( ; d > 0 ; d -= 30)
		{
#ifdef GLQUAKE
			if (qmb_initialized && gl_part_lightning.value)
			{
				VectorAdd (org, dist, beamend);
				for (j = 0 ; j < 3 ; j++)
					beamend[j] += (rand() % 10) - 5;
				QMB_LightningBeam (beamstart, beamend);
				VectorCopy (beamend, beamstart);
			}
			else
#endif
			{
				if (!(ent = CL_NewTempEntity()))
					return;

				VectorCopy (org, ent->origin);
				ent->model = b->model;
				ent->angles[0] = ang[0];
				ent->angles[1] = ang[1];
				ent->angles[2] = rand() % 360;
			}

			VectorAdd (org, dist, org);
		}
	}
}

/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	num_temp_entities = 0;

	CL_UpdateBeams ();
}
