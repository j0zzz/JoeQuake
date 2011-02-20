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
// gl_rmain.c

#include "quakedef.h"

entity_t r_worldentity;

qboolean r_cache_thrash;		// compatibility

vec3_t	modelorg, r_entorigin;
entity_t *currententity;

int		r_visframecount;	// bumped when going to a new PVS
int		r_framecount;		// used for dlight push checking

mplane_t frustum[4];

int		c_brush_polys, c_alias_polys, c_md3_polys;

int		particletexture;	// little dot for particles
int		playertextures;		// up to 16 color translated skins
int		skyboxtextures;
int		underwatertexture, detailtexture;
int		damagetexture;

#define	INTERP_WEAP_MAXNUM	24
#define	INTERP_MINDIST		70
#define	INTERP_MAXDIST		300

typedef struct interpolated_weapon
{
	char	name[MAX_QPATH];
	int		maxDistance;
} interp_weapon_t;

static	interp_weapon_t	interpolated_weapons[INTERP_WEAP_MAXNUM];
static	int		interp_weap_num = 0;

int DoWeaponInterpolation (void);

// view origin
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

// screen size info
refdef_t r_refdef;

mleaf_t	*r_viewleaf, *r_oldviewleaf;
mleaf_t	*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

texture_t *r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_drawentities = {"r_drawentities", "1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel", "1"};
cvar_t	r_viewmodelsize = {"r_viewmodelsize", "0.9"};
cvar_t	r_speeds = {"r_speeds", "0"};
cvar_t	r_fullbright = {"r_fullbright", "0"};
cvar_t	r_lightmap = {"r_lightmap", "0"};
cvar_t	r_shadows = {"r_shadows", "2"};		// probably a bit rough...
cvar_t	r_wateralpha = {"r_wateralpha", "1"};
cvar_t	r_dynamic = {"r_dynamic", "1"};
cvar_t	r_novis = {"r_novis", "0"};
cvar_t	r_fullbrightskins = {"r_fullbrightskins", "0"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_skycolor = {"r_skycolor", "4"};
cvar_t	r_drawflame = {"r_drawflame", "1"};

cvar_t	r_farclip = {"r_farclip", "4096"};
qboolean OnChange_r_skybox (cvar_t *var, char *string);
cvar_t	r_skybox = {"r_skybox", "", 0, OnChange_r_skybox};

cvar_t	gl_clear = {"gl_clear", "0"};
cvar_t	gl_cull = {"gl_cull", "1"};
cvar_t	gl_ztrick = {"gl_ztrick", "0"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels", "1"};
cvar_t	gl_affinemodels = {"gl_affinemodels", "0"};
cvar_t	gl_polyblend = {"gl_polyblend", "1"};
cvar_t	gl_flashblend = {"gl_flashblend", "0"};
cvar_t	gl_playermip = {"gl_playermip", "0"};
cvar_t	gl_nocolors = {"gl_nocolors", "0"};
cvar_t	gl_finish = {"gl_finish", "0"};
cvar_t	gl_loadlitfiles = {"gl_loadlitfiles", "1"};
cvar_t	gl_doubleeyes = {"gl_doubleeyes", "1"};
cvar_t	gl_interdist = {"gl_interpolate_distance", "135"};
cvar_t  gl_waterfog = {"gl_waterfog", "1"};
cvar_t  gl_waterfog_density = {"gl_waterfog_density", "0.5"};
cvar_t	gl_detail = {"gl_detail", "0"};
cvar_t	gl_caustics = {"gl_caustics", "1"};
cvar_t	gl_ringalpha = {"gl_ringalpha", "0.4"};
cvar_t	gl_fb_bmodels = {"gl_fb_bmodels", "1"};
cvar_t	gl_fb_models = {"gl_fb_models", "1"};
cvar_t  gl_solidparticles = {"gl_solidparticles", "0"};
cvar_t  gl_vertexlights = {"gl_vertexlights", "1"};
cvar_t	gl_shownormals = {"gl_shownormals", "0"};
cvar_t  gl_loadq3models = {"gl_loadq3models", "0"};
cvar_t  gl_lerptextures = {"gl_lerptextures", "1"};

cvar_t	gl_part_explosions = {"gl_part_explosions", "0"};
cvar_t	gl_part_trails = {"gl_part_trails", "0"};
cvar_t	gl_part_spikes = {"gl_part_spikes", "0"};
cvar_t	gl_part_gunshots = {"gl_part_gunshots", "0"};
cvar_t	gl_part_blood = {"gl_part_blood", "0"};
cvar_t	gl_part_telesplash = {"gl_part_telesplash", "0"};
cvar_t	gl_part_blobs = {"gl_part_blobs", "0"};
cvar_t	gl_part_lavasplash = {"gl_part_lavasplash", "0"};
cvar_t	gl_part_flames = {"gl_part_flames", "0"};
cvar_t	gl_part_lightning = {"gl_part_lightning", "0"};
cvar_t	gl_part_damagesplash = {"gl_part_damagesplash", "0"};

extern qboolean OnChange_gl_part_muzzleflash (cvar_t *var, char *string);
cvar_t	gl_part_muzzleflash = {"gl_part_muzzleflash", "0", 0, OnChange_gl_part_muzzleflash};

cvar_t	gl_decal_blood = {"gl_decal_blood", "0"};
cvar_t	gl_decal_bullets = {"gl_decal_bullets", "0"};
cvar_t	gl_decal_sparks = {"gl_decal_sparks", "0"};
cvar_t	gl_decal_explosions = {"gl_decal_explosions", "0"};

int		lightmode = 2;

void R_MarkLeaves (void);
void R_InitBubble (void);

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int	i;

	for (i = 0 ; i < 4 ; i++)
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
			return true;

	return false;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere (vec3_t centre, float radius)
{
	int			i;
	mplane_t	*p;

	for (i = 0, p = frustum ; i < 4 ; i++, p++)
		if (PlaneDiff(centre, p) <= -radius)
			return true;

	return false;
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity (entity_t *ent, qboolean shadow)
{
	int		i;
	float	lerpfrac, timepassed;
	vec3_t	d, interpolated;

	// positional interpolation
	timepassed = cl.time - ent->translate_start_time;

	if (ent->translate_start_time == 0 || timepassed > 1)
	{
		ent->translate_start_time = cl.time;
		VectorCopy (ent->origin, ent->origin1);
		VectorCopy (ent->origin, ent->origin2);
	}

	if (!VectorCompare(ent->origin, ent->origin2))
	{
		ent->translate_start_time = cl.time;
		VectorCopy (ent->origin2, ent->origin1);
		VectorCopy (ent->origin,  ent->origin2);
		lerpfrac = 0;
	}
	else
	{
		lerpfrac = timepassed / 0.1;
		if (cl.paused || lerpfrac > 1)
			lerpfrac = 1;
	}

	VectorInterpolate (ent->origin1, lerpfrac, ent->origin2, interpolated);
	glTranslatef (interpolated[0], interpolated[1], interpolated[2]);

	// orientation interpolation (Euler angles, yuck!)
	timepassed = cl.time - ent->rotate_start_time; 

	if (ent->rotate_start_time == 0 || timepassed > 1)
	{
		ent->rotate_start_time = cl.time;
		VectorCopy (ent->angles, ent->angles1);
		VectorCopy (ent->angles, ent->angles2);
	}

	if (!VectorCompare(ent->angles, ent->angles2))
	{
		ent->rotate_start_time = cl.time;
		VectorCopy (ent->angles2, ent->angles1);
		VectorCopy (ent->angles,  ent->angles2);
		lerpfrac = 0;
	}
	else
	{
		lerpfrac = timepassed / 0.1;
		if (cl.paused || lerpfrac > 1)
			lerpfrac = 1;
	}

	VectorSubtract (ent->angles2, ent->angles1, d);

	// always interpolate along the shortest path
	for (i = 0 ; i < 3 ; i++)
	{
		if (d[i] > 180)
			d[i] -= 360;
		else if (d[i] < -180)
			d[i] += 360;
	}

	glRotatef (ent->angles1[1] + (lerpfrac * d[1]), 0, 0, 1);
	if (!shadow)
	{
		glRotatef (-ent->angles1[0] + (-lerpfrac * d[0]), 0, 1, 0);
		glRotatef (ent->angles1[2] + (lerpfrac * d[2]), 1, 0, 0);
	}
}

/*
=============
R_RotateForViewEntity
=============
*/
void R_RotateForViewEntity (entity_t *ent)
{
	glTranslatef (ent->origin[0], ent->origin[1], ent->origin[2]);

	glRotatef (ent->angles[1], 0, 0, 1);
	glRotatef (-ent->angles[0], 0, 1, 0);
	glRotatef (ent->angles[2], 1, 0, 0);
}

/*
===============================================================================

								SPRITE MODELS

===============================================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	int		i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;
	msprite_t	*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel (entity_t *ent)
{
	vec3_t		point, right, up;
	mspriteframe_t	*frame;
	msprite_t	*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (ent);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors (currententity->angles, NULL, right, up);
	}
	else if (psprite->type == SPR_FACING_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		right[0] = ent->origin[1] - r_origin[1];
		right[1] = -(ent->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize (right);
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	}
	else
	{	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

	GL_Bind (frame->gl_texturenum);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (ent->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (ent->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (ent->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (ent->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);
	
	glEnd ();
}

/*
===============================================================================

				ALIAS MODELS

===============================================================================
*/

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] =
#include "anorms.h"
;

vec3_t	shadevector;

qboolean full_light;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

float	apitch, ayaw;

/*
=============
R_DrawAliasFrame
=============
*/
void R_DrawAliasFrame (int frame, aliashdr_t *paliashdr, entity_t *ent, int distance)
{
	int			i, *order, count, pose, numposes;
	float		l, lerpfrac;
	vec3_t		lightvec, interpolated_verts;
	trivertx_t	*verts1, *verts2;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		ent->frame_interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / ent->frame_interval) % numposes;
	}
	else
	{
		ent->frame_interval = 0.1;
	}

	if (ent->pose2 != pose)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;
	}

	// weird things start happening if blend passes 1
	if (cl.paused || ent->framelerp > 1)
		ent->framelerp = 1;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	if (ISTRANSPARENT(ent))
		glEnable (GL_BLEND);

	while ((count = *order++))
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			if (gl_mtexable)
			{
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, ((float *)order)[0], ((float *)order)[1]);
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, ((float *)order)[0], ((float *)order)[1]);
			}
			else
			{
				glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			}

			order += 2;

			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;

			// normals and vertexes come from the frame list
			// blend the light intensity from the two frames together
			if (gl_vertexlights.value && !full_light)
			{
				l = R_LerpVertexLight (anorm_pitch[verts1->lightnormalindex], anorm_yaw[verts1->lightnormalindex],
							anorm_pitch[verts2->lightnormalindex], anorm_yaw[verts2->lightnormalindex], lerpfrac, apitch, ayaw);
				l = min(l, 1);

				for (i = 0 ; i < 3 ; i++)
					lightvec[i] = lightcolor[i] / 256 + l;
				glColor4f (lightvec[0], lightvec[1], lightvec[2], ent->transparency);
			}
			else
			{
				l = FloatInterpolate (shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]);
				l = (l * shadelight + ambientlight) / 256;
				l = min(l, 1);

				glColor4f (l, l, l, ent->transparency);
			}

			VectorInterpolate (verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv (interpolated_verts);

			verts1++;
			verts2++;
		} while (--count);

		glEnd ();
	}

	if (ISTRANSPARENT(ent))
		glDisable (GL_BLEND);
}

/*
=============
R_DrawAliasShadow
=============
*/
void R_DrawAliasShadow (aliashdr_t *paliashdr, entity_t *ent, int distance, trace_t downtrace)
{
	int			*order, count;
	float		lheight, lerpfrac, s1, c1;
	vec3_t		point1, point2, interpolated;
	trivertx_t	*verts1, *verts2;

	lheight = ent->origin[2] - lightspot[2];

	s1 = sin(ent->angles[1] / 180 * M_PI);
	c1 = cos(ent->angles[1] / 180 * M_PI);

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while ((count = *order++))
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;

			point1[0] = verts1->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point1[1] = verts1->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point1[2] = verts1->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			point2[0] = verts2->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point2[1] = verts2->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point2[2] = verts2->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			VectorInterpolate (point1, lerpfrac, point2, interpolated);

			interpolated[2] = -(ent->origin[2] - downtrace.endpos[2]);

			interpolated[2] += ((interpolated[1] * (s1 * downtrace.plane.normal[0])) - 
					    (interpolated[0] * (c1 * downtrace.plane.normal[0])) - 
					    (interpolated[0] * (s1 * downtrace.plane.normal[1])) - 
					    (interpolated[1] * (c1 * downtrace.plane.normal[1]))) + 
					    ((1 - downtrace.plane.normal[2]) * 20) + 0.2;

			glVertex3fv (interpolated);

			verts1++;
			verts2++;
		} while (--count);

		glEnd ();
	}       
}

/*
=============
R_SetupLighting
=============
*/
void R_SetupLighting (entity_t *ent)
{
	int		i, lnum;
	float	add;
	vec3_t	dist, dlight_color;
	model_t	*clmodel = ent->model;

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT)
	{
		ambientlight = 210;
		shadelight = 0;
		full_light = ent->noshadow = true;
		return;
	}
	else if (clmodel->modhint == MOD_FLAME)
	{
		ambientlight = 255;
		shadelight = 0;
		full_light = ent->noshadow = true;
		return;
	}
	else if (clmodel->modhint == MOD_Q3GUNSHOT || clmodel->modhint == MOD_Q3TELEPORT)
	{
		ambientlight = 128;
		shadelight = 0;
		full_light = ent->noshadow = true;
		return;
	}

	// normal lighting
	ambientlight = shadelight = R_LightPoint (ent->origin);
 	full_light = false;
	ent->noshadow = false;

	for (lnum = 0 ; lnum < MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die < cl.time || !cl_dlights[lnum].radius)
			continue;

		VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
		add = cl_dlights[lnum].radius - VectorLength (dist);

		if (add > 0)
		{
		// joe: only allow colorlight affection if dynamic lights are on
			if (r_dynamic.value)
			{
				VectorCopy (bubblecolor[cl_dlights[lnum].type], dlight_color);
				for (i = 0 ; i < 3 ; i++)
				{
					lightcolor[i] = lightcolor[i] + (dlight_color[i] * add) * 2;
					if (lightcolor[i] > 256)
					{
						switch (i)
						{
						case 0:
							lightcolor[1] = lightcolor[1] - (lightcolor[1] / 3);
							lightcolor[2] = lightcolor[2] - (lightcolor[2] / 3);
							break;

						case 1:
							lightcolor[0] = lightcolor[0] - (lightcolor[0] / 3);
							lightcolor[2] = lightcolor[2] - (lightcolor[2] / 3);
							break;

						case 2:
							lightcolor[1] = lightcolor[1] - (lightcolor[1] / 3);
							lightcolor[0] = lightcolor[0] - (lightcolor[0] / 3);
							break;
						}
					}
				}
			}
			else
			{
				ambientlight += add;
			}
		}
	}

	// calculate pitch and yaw for vertex lighting
	if (gl_vertexlights.value)
	{
		apitch = ent->angles[0];
		ayaw = ent->angles[1];
	}

	// clamp lighting so it doesn't overbright as much
	ambientlight = min(128, ambientlight);
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	if (ent == &cl.viewent)
	{
		ent->noshadow = true;
		// always give the gun some light
		if (ambientlight < 24)
			ambientlight = shadelight = 24;
	}

	// never allow players to go totally black
	if (Mod_IsAnyKindOfPlayerModel(clmodel))
	{
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
		if (r_fullbrightskins.value)
		{
			ambientlight = shadelight = 128;
			full_light = true;
		}
	}

	if (r_fullbrightskins.value == 2 &&
		(ent->modelindex == cl_modelindex[mi_fish] ||
		ent->modelindex == cl_modelindex[mi_dog] ||
		ent->modelindex == cl_modelindex[mi_soldier] ||
		ent->modelindex == cl_modelindex[mi_enforcer] ||
		ent->modelindex == cl_modelindex[mi_knight] ||
		ent->modelindex == cl_modelindex[mi_hknight] ||
		ent->modelindex == cl_modelindex[mi_scrag] ||
		ent->modelindex == cl_modelindex[mi_ogre] ||
		ent->modelindex == cl_modelindex[mi_fiend] ||
		ent->modelindex == cl_modelindex[mi_vore] ||
		ent->modelindex == cl_modelindex[mi_shambler]))
	{
		ambientlight = shadelight = 128;
		full_light = true;
	}
}

/*
=============
R_SetupInterpolateDistance
=============
*/
void R_SetupInterpolateDistance (entity_t *ent, aliashdr_t *paliashdr, int *distance)
{
	*distance = INTERP_MAXDIST;

	if (ent->model->modhint == MOD_FLAME)
	{
		*distance = 0;
	}
	else
	{
		if (!gl_part_muzzleflash.value)
		{
			char *framename = paliashdr->frames[ent->frame].name;

			if (ent->model->modhint == MOD_WEAPON)
			{
				if ((*distance = DoWeaponInterpolation()) == -1)
					*distance = (int)gl_interdist.value;
			}
			else if (ent->modelindex == cl_modelindex[mi_player])
			{
				if (!strcmp(framename, "nailatt1"))
					*distance = 0;
				else if (!strcmp(framename, "nailatt2"))
					*distance = 59;
				else if (!strcmp(framename, "rockatt1"))
					*distance = 0;
				else if (!strcmp(framename, "rockatt2"))
					*distance = 115;
				else if (!strcmp(framename, "shotatt1"))
					*distance = 76;
				else if (!strcmp(framename, "shotatt3"))
					*distance = 79;
			}
			else if (ent->modelindex == cl_modelindex[mi_soldier])
			{
				if (!strcmp(framename, "shoot5"))
					*distance = 49;
				else if (!strcmp(framename, "shoot6"))
					*distance = 106;
			}
			else if (ent->modelindex == cl_modelindex[mi_enforcer])
			{
				if (!strcmp(framename, "attack6"))
					*distance = 115;
				else if (!strcmp(framename, "attack7"))
					*distance = 125;
			}
		}
	}
}

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *ent)
{
	int			i, anim, skinnum, distance, texture, fb_texture;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	model_t		*clmodel = ent->model;
	qboolean	islumaskin;

	VectorAdd (ent->origin, clmodel->mins, mins);
	VectorAdd (ent->origin, clmodel->maxs, maxs);

	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
	{
		if (R_CullSphere(ent->origin, clmodel->radius))
			return;
	}
	else
	{
		if (R_CullBox(mins, maxs))
			return;
	}

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// get lighting information
	R_SetupLighting (ent);

	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	// locate the proper data
	paliashdr = (aliashdr_t *)Mod_Extradata (clmodel);

	c_alias_polys += paliashdr->numtris;

	R_SetupInterpolateDistance (ent, paliashdr, &distance);

	// draw all the triangles

	glPushMatrix ();

	if (ent == &cl.viewent)
		R_RotateForViewEntity (ent);
	else
		R_RotateForEntity (ent, false);

	if (clmodel->modhint == MOD_EYES && gl_doubleeyes.value)
	{
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		glScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent == &cl.viewent)
	{
		float	scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;

		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else
	{
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int)(cl.time*10) & 3;
	skinnum = ent->skinnum;
	if ((skinnum >= paliashdr->numskins) || (skinnum < 0))
	{
		Con_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];
	islumaskin = paliashdr->islumaskin[skinnum][anim];

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players. Heads are just uncolored.
	if (ent->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = ent - cl_entities;

		if (i > 0 && i <= cl.maxclients)
		{
			extern int player_32bit_skins[14];
			extern qboolean player_32bit_skins_loaded;

			if (clmodel->modhint == MOD_PLAYER && player_32bit_skins_loaded && gl_externaltextures_models.value)
			{
				texture = player_32bit_skins[cl.scores[i-1].colors / 16];
			}
			else
			{
				texture = playertextures - 1 + i;
				fb_texture = player_fb_skins[i-1];
			}
		}
	}

	if (full_light || !gl_fb_models.value)
		fb_texture = 0;

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (fb_texture && gl_mtexable)
	{
		GL_DisableMultitexture ();
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_Bind (texture);

		GL_EnableMultitexture ();
		if (islumaskin)
		{
			if (gl_add_ext)
			{
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
				GL_Bind (fb_texture);
			}
		}
		else
		{
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			GL_Bind (fb_texture);
		}

		R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

		if (islumaskin && !gl_add_ext)
		{
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			GL_Bind (fb_texture);

			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glBlendFunc (GL_ONE, GL_ONE);

			R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

			glDisable (GL_BLEND);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask (GL_TRUE);
		}

		GL_DisableMultitexture ();
	}
	else
	{
		GL_DisableMultitexture ();
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_Bind (texture);

		R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

		if (fb_texture)
		{
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			GL_Bind (fb_texture);

			glDepthMask (GL_FALSE);
			if (islumaskin)
			{
				glEnable (GL_BLEND);
				glBlendFunc (GL_ONE, GL_ONE);
			}
			else
			{
				glEnable (GL_ALPHA_TEST);
			}

			R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

			if (islumaskin)
			{
				glDisable (GL_BLEND);
				glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			else
			{
				glDisable (GL_ALPHA_TEST);
			}
			glDepthMask (GL_TRUE);
		}
	}

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	if (r_shadows.value && !ent->noshadow)
	{
		int		farclip;
		float	theta;
		vec3_t	downmove;
		trace_t	downtrace;
		static float shadescale = 0;

		farclip = max((int)r_farclip.value, 4096);

		if (!shadescale)
			shadescale = 1 / sqrt(2);
		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet (shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

		glPushMatrix ();

		R_RotateForEntity (ent, true);

		VectorCopy (ent->origin, downmove);
		downmove[2] -= farclip;
		memset (&downtrace, 0, sizeof(downtrace));
		SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, ent->origin, downmove, &downtrace);

		glDepthMask (GL_FALSE);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, (ambientlight - (mins[2] - downtrace.endpos[2])) / 150);
		if (gl_have_stencil && r_shadows.value == 2)
		{
			glEnable (GL_STENCIL_TEST);
			glStencilFunc (GL_EQUAL, 1, 2);
			glStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
		}

		R_DrawAliasShadow (paliashdr, ent, distance, downtrace);

		glDepthMask (GL_TRUE);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		if (gl_have_stencil && r_shadows.value == 2)
			glDisable (GL_STENCIL_TEST);

		glPopMatrix ();
	}

	glColor3ubv (color_white);
}

void Set_Interpolated_Weapon_f (void)
{
	int		i;
	char	str[MAX_QPATH];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() == 2)
	{
		for (i=0 ; i<interp_weap_num ; i++)
		{
			if (!Q_strcasecmp(Cmd_Argv(1), interpolated_weapons[i].name))
			{
				Con_Printf ("%s`s distance is %d\n", Cmd_Argv(1), interpolated_weapons[i].maxDistance);
				return;
			}
		}
		Con_Printf ("%s`s distance is default (%d)\n", Cmd_Argv(1), (int)gl_interdist.value);
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("Usage: set_interpolated_weapon <model> <distance>\n");
		return;
	}

	Q_strcpy (str, Cmd_Argv(1));
	for (i = 0 ; i < interp_weap_num ; i++)
		if (!Q_strcasecmp(str, interpolated_weapons[i].name))
			break;
	if (i == interp_weap_num)
	{
		if (interp_weap_num == INTERP_WEAP_MAXNUM)
		{
			Con_Printf ("interp_weap_num == INTERP_WEAP_MAXNUM\n");
			return;
		}
		else
		{
			interp_weap_num++;
		}
	}

	Q_strcpy (interpolated_weapons[i].name, str);
	interpolated_weapons[i].maxDistance = (int)Q_atof(Cmd_Argv(2));
}

int DoWeaponInterpolation (void)
{
	int	i;

	if (currententity != &cl.viewent)
		return -1;

	for (i = 0 ; i < interp_weap_num ; i++)
	{
		if (!interpolated_weapons[i].name[0])
			return -1;

		if (!Q_strcasecmp(currententity->model->name, va("%s.mdl", interpolated_weapons[i].name)) || 
		    !Q_strcasecmp(currententity->model->name, va("progs/%s.mdl", interpolated_weapons[i].name)))
			return interpolated_weapons[i].maxDistance;
	}

	return -1;
}

/*
===============================================================================

								Q3 MODELS

===============================================================================
*/

void R_RotateForTagEntity (tagentity_t *tagent, md3tag_t *tag, float *m)
{
	int		i;
	float	lerpfrac, timepassed;

	// positional interpolation
	timepassed = cl.time - tagent->tag_translate_start_time;

	if (tagent->tag_translate_start_time == 0 || timepassed > 1)
	{
		tagent->tag_translate_start_time = cl.time;
		VectorCopy (tag->pos, tagent->tag_pos1);
		VectorCopy (tag->pos, tagent->tag_pos2);
	}

	if (!VectorCompare(tag->pos, tagent->tag_pos2))
	{
		tagent->tag_translate_start_time = cl.time;
		VectorCopy (tagent->tag_pos2, tagent->tag_pos1);
		VectorCopy (tag->pos, tagent->tag_pos2);
		lerpfrac = 0;
	}
	else
	{
		lerpfrac = timepassed / 0.1;
		if (cl.paused || lerpfrac > 1)
			lerpfrac = 1;
	}

	VectorInterpolate (tagent->tag_pos1, lerpfrac, tagent->tag_pos2, m + 12);
	m[15] = 1;

	for (i = 0 ; i < 3 ; i++)
	{
		// orientation interpolation (Euler angles, yuck!)
		timepassed = cl.time - tagent->tag_rotate_start_time[i];

		if (tagent->tag_rotate_start_time[i] == 0 || timepassed > 1)
		{
			tagent->tag_rotate_start_time[i] = cl.time;
			VectorCopy (tag->rot[i], tagent->tag_rot1[i]);
			VectorCopy (tag->rot[i], tagent->tag_rot2[i]);
		}

		if (!VectorCompare(tag->rot[i], tagent->tag_rot2[i]))
		{
			tagent->tag_rotate_start_time[i] = cl.time;
			VectorCopy (tagent->tag_rot2[i], tagent->tag_rot1[i]);
			VectorCopy (tag->rot[i], tagent->tag_rot2[i]);
			lerpfrac = 0;
		}
		else
		{
			lerpfrac = timepassed / 0.1;
			if (cl.paused || lerpfrac > 1)
				lerpfrac = 1;
		}

		VectorInterpolate (tagent->tag_rot1[i], lerpfrac, tagent->tag_rot2[i], m + i*4);
		m[i*4+3] = 0;
	}
}

int			bodyframe = 0, legsframe = 0;
animtype_t	bodyanim, legsanim;

void R_ReplaceQ3Frame (int frame)
{
	animdata_t		*currbodyanim, *currlegsanim;
	static animtype_t oldbodyanim, oldlegsanim;
	static float	bodyanimtime, legsanimtime;
	static qboolean	deathanim = false;

	if (deathanim)
	{
		bodyanim = oldbodyanim;
		legsanim = oldlegsanim;
	}

	if (frame < 41 || frame > 102)
		deathanim = false;

	if (frame >= 0 && frame <= 5)		// axrun
	{
		bodyanim = torso_stand2;
		legsanim = legs_run;
	}
	else if (frame >= 6 && frame <= 11)	// rockrun
	{
		bodyanim = torso_stand;
		legsanim = legs_run;
	}
	else if ((frame >= 12 && frame <= 16) || (frame >= 35 && frame <= 40))	// stand, pain
	{
		bodyanim = torso_stand;
		legsanim = legs_idle;
	}
	else if ((frame >= 17 && frame <= 28) || (frame >= 29 && frame <= 34))	// axstand, axpain
	{
		bodyanim = torso_stand2;
		legsanim = legs_idle;
	}
	else if (frame >= 41 && frame <= 102 && !deathanim)	// axdeath, deatha, b, c, d, e
	{
		bodyanim = legsanim = rand() % 3;
		deathanim = true;
	}
	else if (frame >= 103 && frame <= 118)	// gun attacks
	{
		bodyanim = torso_attack;
	}
	else if (frame >= 119)			// axe attacks
	{
		bodyanim = torso_attack2;
	}

	currbodyanim = &anims[bodyanim];
	currlegsanim = &anims[legsanim];

	if (bodyanim == oldbodyanim)
	{
		if (cl.time >= bodyanimtime + currbodyanim->interval)
		{
			if (currbodyanim->loop_frames && bodyframe + 1 == currbodyanim->offset + currbodyanim->loop_frames)
				bodyframe = currbodyanim->offset;
			else if (bodyframe + 1 < currbodyanim->offset + currbodyanim->num_frames)
				bodyframe++;
			bodyanimtime = cl.time;
		}
	}
	else
	{
		bodyframe = currbodyanim->offset;
		bodyanimtime = cl.time;
	}

	if (legsanim == oldlegsanim)
	{
		if (cl.time >= legsanimtime + currlegsanim->interval)
		{
			if (currlegsanim->loop_frames && legsframe + 1 == currlegsanim->offset + currlegsanim->loop_frames)
				legsframe = currlegsanim->offset;
			else if (legsframe + 1 < currlegsanim->offset + currlegsanim->num_frames)
				legsframe++;
			legsanimtime = cl.time;
		}
	}
	else
	{
		legsframe = currlegsanim->offset;
		legsanimtime = cl.time;
	}

	oldbodyanim = bodyanim;
	oldlegsanim = legsanim;
}

int			multimodel_level;
qboolean	surface_transparent;

/*
=================
R_DrawQ3Frame
=================
*/
void R_DrawQ3Frame (int frame, md3header_t *pmd3hdr, md3surface_t *pmd3surf, entity_t *ent, int distance)
{
	int			i, j, numtris, pose, pose1, pose2;
	float		l, lerpfrac;
	vec3_t		lightvec, interpolated_verts;
	unsigned int *tris;
	md3tc_t		*tc;
	md3vert_mem_t *verts, *v1, *v2;
	model_t		*clmodel = ent->model;

	if ((frame >= pmd3hdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_DrawQ3Frame: no such frame %d\n", frame);
		frame = 0;
	}

	if (ent->pose1 >= pmd3hdr->numframes)
		ent->pose1 = 0;

	pose = frame;

	if (!strcmp(clmodel->name, cl_modelnames[mi_q3legs]))
		ent->frame_interval = anims[legsanim].interval;
	else if (!strcmp(clmodel->name, cl_modelnames[mi_q3torso]))
		ent->frame_interval = anims[bodyanim].interval;
	else
		ent->frame_interval = 0.1;

	if (ent->pose2 != pose)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;
	}

	// weird things start happening if blend passes 1
	if (cl.paused || ent->framelerp > 1)
		ent->framelerp = 1;

	verts = (md3vert_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsverts);
	tc = (md3tc_t *)((byte *)pmd3surf + pmd3surf->ofstc);
	tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
	numtris = pmd3surf->numtris * 3;
	pose1 = ent->pose1 * pmd3surf->numverts;
	pose2 = ent->pose2 * pmd3surf->numverts;

	if (surface_transparent)
	{
		glEnable (GL_BLEND);
		if (clmodel->modhint == MOD_Q3GUNSHOT || clmodel->modhint == MOD_Q3TELEPORT)
			glBlendFunc (GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc (GL_ONE, GL_ONE);
		glDepthMask (GL_FALSE);
		glDisable (GL_CULL_FACE);
	}
	else if (ISTRANSPARENT(ent))
	{
		glEnable (GL_BLEND);
	}

	glBegin (GL_TRIANGLES);
	for (i = 0 ; i < numtris ; i++)
	{
		float	s, t;

		v1 = verts + *tris + pose1;
		v2 = verts + *tris + pose2;

		if (clmodel->modhint == MOD_Q3TELEPORT)
			s = tc[*tris].s, t = tc[*tris].t * 4;
		else
			s = tc[*tris].s, t = tc[*tris].t;

		if (gl_mtexable)
		{
			qglMultiTexCoord2f (GL_TEXTURE0_ARB, s, t);
			qglMultiTexCoord2f (GL_TEXTURE1_ARB, s, t);
		}
		else
		{
			glTexCoord2f (s, t);
		}

		lerpfrac = VectorL2Compare(v1->vec, v2->vec, distance) ? ent->framelerp : 1;

		if (gl_vertexlights.value && !full_light)
		{
			l = R_LerpVertexLight (v1->anorm_pitch, v1->anorm_yaw, v2->anorm_pitch, v2->anorm_yaw, lerpfrac, apitch, ayaw);
			l = min(l, 1);

			for (j = 0 ; j < 3 ; j++)
				lightvec[j] = lightcolor[j] / 256 + l;
			glColor4f (lightvec[0], lightvec[1], lightvec[2], ent->transparency);
		}
		else
		{
			l = FloatInterpolate (shadedots[v1->oldnormal>>8], lerpfrac, shadedots[v2->oldnormal>>8]);
			l = (l * shadelight + ambientlight) / 256;
			l = min(l, 1);

			glColor4f (l, l, l, ent->transparency);
		}

		VectorInterpolate (v1->vec, lerpfrac, v2->vec, interpolated_verts);
		glVertex3fv (interpolated_verts);

		*tris++;
	}
	glEnd ();

	if (gl_shownormals.value)
	{
		vec3_t	temp;

		tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
		glDisable (GL_TEXTURE_2D);
		glColor3ubv (color_white);
		glBegin (GL_LINES);
		for (i=0 ; i<numtris ; i++)
		{
			glVertex3fv (verts[*tris+pose1].vec);
			VectorMA (verts[*tris+pose1].vec, 2, verts[*tris+pose1].normal, temp);
			glVertex3fv (temp);
			*tris++;
		}
		glEnd ();
		glEnable (GL_TEXTURE_2D);
	}

	if (surface_transparent)
	{
		glDisable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask (GL_TRUE);
		glEnable (GL_CULL_FACE);
	}
	else if (ISTRANSPARENT(ent))
	{
		glDisable (GL_BLEND);
	}
}

/*
=================
R_DrawQ3Shadow
=================
*/
void R_DrawQ3Shadow (entity_t *ent, float lheight, float s1, float c1, trace_t downtrace)
{
	int			i, j, numtris, pose1, pose2;
	vec3_t		point1, point2, interpolated;
	md3header_t	*pmd3hdr;
	md3surface_t *pmd3surf;
	unsigned int *tris;
	md3vert_mem_t *verts;
	model_t		*clmodel = ent->model;
#if 0
	float		m[16];
	md3tag_t	*tag;
	tagentity_t	*tagent;
#endif

	pmd3hdr = (md3header_t *)Mod_Extradata (clmodel);

	pmd3surf = (md3surface_t *)((byte *)pmd3hdr + pmd3hdr->ofssurfs);
	for (i = 0 ; i < pmd3hdr->numsurfs ; i++)
	{
		verts = (md3vert_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsverts);
		tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
		numtris = pmd3surf->numtris * 3;
		pose1 = ent->pose1 * pmd3surf->numverts;
		pose2 = ent->pose2 * pmd3surf->numverts;

		glBegin (GL_TRIANGLES);
		for (j = 0 ; j < numtris ; j++)
		{
			// normals and vertexes come from the frame list
			VectorCopy (verts[*tris+pose1].vec, point1);

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			VectorCopy (verts[*tris+pose2].vec, point2);

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			VectorInterpolate (point1, ent->framelerp, point2, interpolated);

			interpolated[2] = -(ent->origin[2] - downtrace.endpos[2]);

			interpolated[2] += ((interpolated[1] * (s1 * downtrace.plane.normal[0])) - 
								(interpolated[0] * (c1 * downtrace.plane.normal[0])) - 
								(interpolated[0] * (s1 * downtrace.plane.normal[1])) - 
								(interpolated[1] * (c1 * downtrace.plane.normal[1]))) + 
								((1 - downtrace.plane.normal[2]) * 20) + 0.2;

			glVertex3fv (interpolated);

			*tris++;
		}
		glEnd ();

		pmd3surf = (md3surface_t *)((byte *)pmd3surf + pmd3surf->ofsend);
	}

	if (!pmd3hdr->numtags)	// single model, done
		return;

// no multimodel shadow support yet
#if 0
	tag = (md3tag_t *)((byte *)pmd3hdr + pmd3hdr->ofstags);
	tag += ent->pose2 * pmd3hdr->numtags;
	for (i = 0 ; i < pmd3hdr->numtags ; i++, tag++)
	{
		if (multimodel_level == 0 && !strcmp(tag->name, "tag_torso"))
		{
			tagent = &q3player_body;
			ent = &q3player_body.ent;
			multimodel_level++;
		}
		else if (multimodel_level == 1 && !strcmp(tag->name, "tag_head"))
		{
			tagent = &q3player_head;
			ent = &q3player_head.ent;
			multimodel_level++;
		}
		else
		{
			continue;
		}

		glPushMatrix ();
		R_RotateForTagEntity (tagent, tag, m);
		glMultMatrixf (m);
		R_DrawQ3Shadow (ent, lheight, s1, c1, downtrace);
		glPopMatrix ();
	}
#endif
}

#define	ADD_EXTRA_TEXTURE(_texture, _param)					\
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, _param);\
	GL_Bind (_texture);										\
															\
	glDepthMask (GL_FALSE);									\
	glEnable (GL_BLEND);									\
	glBlendFunc (GL_ONE, GL_ONE);							\
															\
	R_DrawQ3Frame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);\
															\
	glDisable (GL_BLEND);									\
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);		\
	glDepthMask (GL_TRUE);

/*
=================
R_SetupQ3Frame
=================
*/
void R_SetupQ3Frame (entity_t *ent)
{
	int			i, j, frame, shadernum, texture, fb_texture;
	float		m[16];
	md3header_t	*pmd3hdr;
	md3surface_t *pmd3surf;
	md3tag_t	*tag;
	model_t		*clmodel = ent->model;
	tagentity_t	*tagent;

	if (!strcmp(clmodel->name, cl_modelnames[mi_q3legs]))
		frame = legsframe;
	else if (!strcmp(clmodel->name, cl_modelnames[mi_q3torso]))
		frame = bodyframe;
	else
		frame = ent->frame;

	// locate the proper data
	pmd3hdr = (md3header_t *)Mod_Extradata (clmodel);

	// draw all the triangles

	// draw non-transparent surfaces first, then the transparent ones
	for (i = 0 ; i < 2 ; i++)
	{
		pmd3surf = (md3surface_t *)((byte *)pmd3hdr + pmd3hdr->ofssurfs);
		for (j = 0 ; j < pmd3hdr->numsurfs ; j++)
		{
			md3shader_mem_t	*shader;

			surface_transparent = ( strstr(pmd3surf->name, "energy") ||
									strstr(pmd3surf->name, "f_") ||
									strstr(pmd3surf->name, "flare") ||
									strstr(pmd3surf->name, "flash") ||
									strstr(pmd3surf->name, "Sphere") ||
									strstr(pmd3surf->name, "telep"));

			if ((!i && surface_transparent) || (i && !surface_transparent))
			{
				pmd3surf = (md3surface_t *)((byte *)pmd3surf + pmd3surf->ofsend);
				continue;
			}

			c_md3_polys += pmd3surf->numtris;

			shadernum = ent->skinnum;
			if ((shadernum >= pmd3surf->numshaders) || (shadernum < 0))
			{
				Con_DPrintf ("R_SetupQ3Frame: no such skin # %d\n", shadernum);
				shadernum = 0;
			}

			shader = (md3shader_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsshaders);

			texture = shader[shadernum].gl_texnum;
			fb_texture = shader[shadernum].fb_texnum;

			if (fb_texture && gl_mtexable)
			{
				GL_DisableMultitexture ();
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind (texture);

				GL_EnableMultitexture ();
				if (gl_add_ext)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					GL_Bind (fb_texture);
				}

				R_DrawQ3Frame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);

				if (!gl_add_ext)
				{
					ADD_EXTRA_TEXTURE(fb_texture, GL_DECAL);
				}

				GL_DisableMultitexture ();
			}
			else
			{
				GL_DisableMultitexture ();
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind (texture);

				R_DrawQ3Frame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);

				if (fb_texture)
				{
					ADD_EXTRA_TEXTURE(fb_texture, GL_REPLACE);
				}
			}

			pmd3surf = (md3surface_t *)((byte *)pmd3surf + pmd3surf->ofsend);
		}
	}

	if (!pmd3hdr->numtags)	// single model, done
		return;

	tag = (md3tag_t *)((byte *)pmd3hdr + pmd3hdr->ofstags);
	tag += frame * pmd3hdr->numtags;
	for (i = 0 ; i < pmd3hdr->numtags ; i++, tag++)
	{
		if (multimodel_level == 0 && !strcmp(tag->name, "tag_torso"))
		{
			tagent = &q3player_body;
			ent = &q3player_body.ent;
			multimodel_level++;
		}
		else if (multimodel_level == 1 && !strcmp(tag->name, "tag_head"))
		{
			tagent = &q3player_head;
			ent = &q3player_head.ent;
			multimodel_level++;
		}
		else
		{
			continue;
		}

		glPushMatrix ();
		R_RotateForTagEntity (tagent, tag, m);
		glMultMatrixf (m);
		R_SetupQ3Frame (ent);
		glPopMatrix ();
	}
}

/*
=================
R_DrawQ3Model
=================
*/
void R_DrawQ3Model (entity_t *ent)
{
	vec3_t		mins, maxs, md3_scale_origin = {0, 0, 0};
	model_t		*clmodel = ent->model;

	if (clmodel->modhint == MOD_Q3TELEPORT)
		ent->origin[2] -= 30;

	VectorAdd (ent->origin, clmodel->mins, mins);
	VectorAdd (ent->origin, clmodel->maxs, maxs);

	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
	{
		if (R_CullSphere(ent->origin, clmodel->radius))
			return;
	}
	else
	{
		if (R_CullBox(mins, maxs))
			return;
	}

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// get lighting information
	R_SetupLighting (ent);

	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	glPushMatrix ();

	if (ent == &cl.viewent)
		R_RotateForViewEntity (ent);
	else
		R_RotateForEntity (ent, false);

	if (ent == &cl.viewent)
	{
		float	scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;

		glTranslatef (md3_scale_origin[0], md3_scale_origin[1], md3_scale_origin[2]);
		glScalef (scale, 1, 1);
	}
	else
	{
		glTranslatef (md3_scale_origin[0], md3_scale_origin[1], md3_scale_origin[2]);
	}

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (!strcmp(clmodel->name, cl_modelnames[mi_q3legs]))
	{
		R_ReplaceQ3Frame (ent->frame);
		ent->noshadow = true;
	}

	multimodel_level = 0;
	R_SetupQ3Frame (ent);

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	if (r_shadows.value && !ent->noshadow)
	{
		int			farclip;
		float		theta, lheight, s1, c1;
		vec3_t		downmove;
		trace_t		downtrace;
		static float shadescale = 0;

		farclip = max((int)r_farclip.value, 4096);

		if (!shadescale)
			shadescale = 1 / sqrt(2);
		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet (shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

		glPushMatrix ();

		R_RotateForEntity (ent, true);

		VectorCopy (ent->origin, downmove);
		downmove[2] -= farclip;
		memset (&downtrace, 0, sizeof(downtrace));
		SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, ent->origin, downmove, &downtrace);

		lheight = ent->origin[2] - lightspot[2];

		s1 = sin(ent->angles[1] / 180 * M_PI);
		c1 = cos(ent->angles[1] / 180 * M_PI);

		glDepthMask (GL_FALSE);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, (ambientlight - (mins[2] - downtrace.endpos[2])) / 150);
		if (gl_have_stencil && r_shadows.value == 2) 
		{
			glEnable (GL_STENCIL_TEST);
			glStencilFunc (GL_EQUAL, 1, 2);
			glStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
		}

		multimodel_level = 0;
		R_DrawQ3Shadow (ent, lheight, s1, c1, downtrace);

		glDepthMask (GL_TRUE);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		if (gl_have_stencil && r_shadows.value == 2)
			glDisable (GL_STENCIL_TEST);

		glPopMatrix ();
	}

	glColor3ubv (color_white);
}

//==================================================================================

void R_SetSpritesState (qboolean state)
{
	static qboolean	r_state = false;

	if (r_state == state)
		return;

	r_state = state;

	if (state)
	{
		GL_DisableMultitexture ();
		glEnable (GL_ALPHA_TEST);
	}
	else
	{
		glDisable (GL_ALPHA_TEST);
	}
}

void R_SetSpr32State (qboolean state)
{
	static qboolean	r_state = false;

	if (r_state == state)
		return;

	r_state = state;

	if (state)
	{
		GL_DisableMultitexture ();
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask (GL_FALSE);
	}
	else
	{
		glDisable (GL_BLEND);
		glDepthMask (GL_TRUE);
	}
}

int SetFlameModelState (void)
{
	if (!gl_part_flames.value && !strcmp(currententity->model->name, "progs/flame0.mdl"))
	{
		currententity->model = cl.model_precache[cl_modelindex[mi_flame1]];
	}
	else if (gl_part_flames.value)
	{
		vec3_t	liteorg;

		VectorCopy (currententity->origin, liteorg);
		if (currententity->baseline.modelindex == cl_modelindex[mi_flame0])
		{
			if (gl_part_flames.value == 2)
			{
				liteorg[2] += 14;
				QMB_Q3TorchFlame (liteorg, 15);
			}
			else
			{
				liteorg[2] += 5.5;
				QMB_TorchFlame (liteorg, 7, 0.8);
			}
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame0_md3])
		{
			if (gl_part_flames.value == 2)
			{
				liteorg[2] += 12;
				QMB_Q3TorchFlame (liteorg, 15);
			}
			else
			{
				liteorg[2] += 3.5;
				QMB_TorchFlame (liteorg, 7, 0.8);
			}
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame1])
		{
			if (gl_part_flames.value == 2)
			{
				liteorg[2] += 14;
				QMB_Q3TorchFlame (liteorg, 15);
			}
			else
			{
				liteorg[2] += 5.5;
				QMB_TorchFlame (liteorg, 7, 0.8);
			}
			currententity->model = cl.model_precache[cl_modelindex[mi_flame0]];
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame2])
		{
			if (gl_part_flames.value == 2)
			{
				liteorg[2] += 14;
				QMB_Q3TorchFlame (liteorg, 32);
			}
			else
			{
				liteorg[2] -= 1;
				QMB_TorchFlame (liteorg, 12, 1);
			}
			return -1;	//continue;
		}
	}

	return 0;
}

void SortEntitiesByTransparency (void)
{
	int		i, j;
	entity_t	*tmp;

	for (i = 0 ; i < cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i]->istransparent)
		{
			for (j = cl_numvisedicts - 1 ; j > i ; j--)
			{
				// if not transparent, exchange with transparent
				if (!(cl_visedicts[j]->istransparent))
				{
					tmp = cl_visedicts[i];
					cl_visedicts[i] = cl_visedicts[j];
					cl_visedicts[j] = tmp;
					break;
				}
			}
			if (j == i)
				return;
		}
	}
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int	i;

	if (!r_drawentities.value)
		return;

	SortEntitiesByTransparency ();

	// draw sprites seperately, because of alpha blending
	for (i = 0 ; i < cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if (qmb_initialized && SetFlameModelState() == -1)
			continue;

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;

		case mod_md3:
			R_DrawQ3Model (currententity);
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	for (i = 0 ; i < cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_SetSpritesState (true);
			R_DrawSpriteModel (currententity);
			break;

		case mod_spr32:
			R_SetSpr32State (true);
			R_DrawSpriteModel (currententity);
			break;

		default:
			break;
		}
	}

	R_SetSpritesState (false);
	R_SetSpr32State (false);
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	currententity = &cl.viewent;

	if (!r_drawviewmodel.value || chase_active.value || !r_drawentities.value || 
	    (cl.stats[STAT_HEALTH] <= 0) || !currententity->model)
		return;

	currententity->transparency = (cl.items & IT_INVISIBILITY) ? gl_ringalpha.value : bound(0, r_drawviewmodel.value, 1);
	currententity->istransparent = true;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	switch (currententity->model->type)
	{
	case mod_alias:
		R_DrawAliasModel (currententity);
		break;

	case mod_md3:
		R_DrawQ3Model (currententity);
		break;
	}

	glDepthRange (gldepthmin, gldepthmax);
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if ((vid_hwgamma_enabled && gl_hwblend.value) || !v_blend[3])
		return;

	glDisable (GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);

	glColor4fv (v_blend);

	glBegin (GL_QUADS);
	glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y);
	glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
	glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
	glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);

	glColor3ubv (color_white);
}

/*
================
R_BrightenScreen
================
*/
void R_BrightenScreen (void)
{
	float		f;
	extern float vid_gamma;

	if (vid_hwgamma_enabled || v_contrast.value <= 1.0)
		return;

	f = min(v_contrast.value, 3);
	f = pow(f, vid_gamma);

	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ONE);

	glBegin (GL_QUADS);
	while (f > 1)
	{
		if (f >= 2)
			glColor3ubv (color_white);
		else
			glColor3f (f - 1, f - 1, f - 1);
		glVertex2f (0, 0);
		glVertex2f (vid.width, 0);
		glVertex2f (vid.width, vid.height);
		glVertex2f (0, vid.height);
		f *= 0.5;
	}
	glEnd ();

	glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);

	glColor3ubv (color_white);
}

/*
================
R_Q3DamageDraw
================
*/
void R_Q3DamageDraw (void)
{
	float		scale, halfwidth, halfheight;
	vec3_t		center;
	extern float damagetime, damagecount;

	if (!qmb_initialized || !gl_part_damagesplash.value || damagetime + 0.5 < cl.time)
	{
		return;
	}
	else if (damagetime > cl.time)
	{
		damagetime = 0;
		return;
	}

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	GL_Bind (damagetexture);

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix ();

	center[0] = vid.width / 2;
	center[1] = vid.height / 2;
	scale = 1 + damagecount / 100;
	halfwidth = vid.width * scale / 2;
	halfheight = vid.height * scale / 2;
	glColor4f (1, 1, 1, 0.8 * (damagetime + 0.5 - cl.time));

	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (center[0] - halfwidth, center[1] - halfheight);
	glTexCoord2f (0, 1);
	glVertex2f (center[0] + halfwidth, center[1] - halfheight);
	glTexCoord2f (1, 1);
	glVertex2f (center[0] + halfwidth, center[1] + halfheight);
	glTexCoord2f (1, 0);
	glVertex2f (center[0] - halfwidth, center[1] + halfheight);
	glEnd ();

	glPopMatrix ();

	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);

	glColor3ubv (color_white);
}

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0 ; j < 3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}

	return bits;
}

void R_SetFrustum (void)
{
	int	i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector (frustum[0].normal, vup, vpn, -(90 - r_refdef.fov_x / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector (frustum[1].normal, vup, vpn, 90 - r_refdef.fov_x / 2);
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector (frustum[2].normal, vright, vpn, 90 - r_refdef.fov_y / 2);
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector (frustum[3].normal, vright, vpn, -(90 - r_refdef.fov_y / 2));

	for (i = 0 ; i < 4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	vec3_t		testorigin;
	mleaf_t		*leaf;

	if (nehahra)
	{
		if (r_oldsky.value && r_skybox.string[0])
			Cvar_Set (&r_skybox, "");
		if (!r_oldsky.value && !r_skybox.string[0])
			Cvar_Set (&r_skybox, prev_skybox);
	}

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA)
	{
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	}
	else if (r_viewleaf->contents == CONTENTS_EMPTY)
	{
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);
	V_AddWaterfog (r_viewleaf->contents);	
	if (nehahra)
		Neh_SetupFrame ();
	V_CalcBlend ();

	r_cache_thrash = false;
}

__inline void MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble	xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum (xmin, xmax, ymin, ymax, zNear, zFar);
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	int	x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height - r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	w = x2 - x;
	h = y - y2;

	glViewport (glx + x, gly + y2, w, h);

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	farclip = max((int)r_farclip.value, 4096);
	MYgluPerspective (r_refdef.fov_y, screenaspect, 4, farclip);

	glCullFace (GL_FRONT);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90, 1, 0, 0);	    // put Z going up
	glRotatef (90, 0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	// set drawing parms
	if (gl_cull.value)
		glEnable (GL_CULL_FACE);
	else
		glDisable (GL_CULL_FACE);

	glDisable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glEnable (GL_DEPTH_TEST);
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{	
	void R_ToggleParticles_f (void);
	void R_ToggleDecals_f (void);

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
	Cmd_AddCommand ("toggleparticles", R_ToggleParticles_f);
	Cmd_AddCommand ("toggledecals", R_ToggleDecals_f);
	Cmd_AddCommand ("set_interpolated_weapon", Set_Interpolated_Weapon_f);

	Cvar_Register (&r_lightmap);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawviewmodel);
	Cvar_Register (&r_viewmodelsize);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&r_novis);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_fullbrightskins);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&r_skybox);
	Cvar_Register (&r_farclip);

	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_cull);
	Cvar_Register (&gl_ztrick);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_affinemodels);
	Cvar_Register (&gl_polyblend);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&gl_playermip);
	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_loadlitfiles);
	Cvar_Register (&gl_doubleeyes);
	Cvar_Register (&gl_interdist);
	Cvar_Register (&gl_waterfog);
	Cvar_Register (&gl_waterfog_density);
	Cvar_Register (&gl_detail);
	Cvar_Register (&gl_caustics);
	Cvar_Register (&gl_ringalpha);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_solidparticles);
	Cvar_Register (&gl_vertexlights);
	Cvar_Register (&gl_shownormals);
	Cvar_Register (&gl_loadq3models);
	Cvar_Register (&gl_lerptextures);

	Cvar_Register (&gl_part_explosions);
	Cvar_Register (&gl_part_trails);
	Cvar_Register (&gl_part_spikes);
	Cvar_Register (&gl_part_gunshots);
	Cvar_Register (&gl_part_blood);
	Cvar_Register (&gl_part_telesplash);
	Cvar_Register (&gl_part_blobs);
	Cvar_Register (&gl_part_lavasplash);
	Cvar_Register (&gl_part_flames);
	Cvar_Register (&gl_part_lightning);
	Cvar_Register (&gl_part_damagesplash);
	Cvar_Register (&gl_part_muzzleflash);

	Cvar_Register (&gl_decal_blood);
	Cvar_Register (&gl_decal_bullets);
	Cvar_Register (&gl_decal_sparks);
	Cvar_Register (&gl_decal_explosions);

	Cmd_AddLegacyCommand ("loadsky", "r_skybox");

	// this minigl driver seems to slow us down if the particles are drawn WITHOUT Z buffer bits
	if (!strcmp(gl_vendor, "METABYTE/WICKED3D"))
		Cvar_SetDefault (&gl_solidparticles, 1);

	if (!gl_allow_ztrick)
		Cvar_SetDefault (&gl_ztrick, 0);

	R_InitTextures ();
	R_InitBubble ();
	R_InitParticles ();
	R_InitVertexLights ();
	R_InitDecals ();

	R_InitOtherTextures ();

	playertextures = texture_extension_number;
	texture_extension_number += 16;

	// fullbright skins
	texture_extension_number += 16;

	skyboxtextures = texture_extension_number;
	texture_extension_number += 6;
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawTeleport ();

	R_DrawDecals ();

	R_DrawEntitiesOnList ();

	R_DrawWaterSurfaces ();

	GL_DisableMultitexture ();
}

int	gl_ztrickframe = 0;

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	int	clearbits = 0;

	if (gl_clear.value || (!vid_hwgamma_enabled && v_contrast.value > 1))
		clearbits |= GL_COLOR_BUFFER_BIT;

	if (gl_ztrick.value)
	{
		if (clearbits)
			glClear (clearbits);

		gl_ztrickframe = !gl_ztrickframe;
		if (gl_ztrickframe)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		clearbits |= GL_DEPTH_BUFFER_BIT;
		glClear (clearbits);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax);

	if (r_shadows.value == 2)
	{
		glClearStencil (GL_TRUE);
		glClear (GL_STENCIL_BUFFER_BIT);
	}
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1 = 0, time2;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = c_alias_polys = c_md3_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	R_Clear ();

	// render normal view

	R_RenderScene ();
	R_RenderDlights ();
	R_DrawParticles ();
	R_DrawViewModel ();

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly %4i md3poly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys, c_md3_polys);
	}
}
