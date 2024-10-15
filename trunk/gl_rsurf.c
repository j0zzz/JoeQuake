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
// r_surf.c: surface-related refresh code

#include "quakedef.h"

#define LMBLOCK_WIDTH	256	//FIXME: make dynamic. if we have a decent card there's no real reason not to use 4k or 16k (assuming there's no lightstyles/dynamics that need uploading...)
#define LMBLOCK_HEIGHT	256 //Alternatively, use texture arrays, which would avoid the need to switch textures as often.

#define MAX_LIGHTMAP_SIZE	4096
#define MAX_LIGHTMAPS		512 //johnfitz -- was 64 

int		lightmap_bytes;

typedef struct glRect_s
{
	unsigned short	l, t, w, h;
} glRect_t;

struct lightmap_s
{
	int			texture;
	glpoly_t	*polys;
	qboolean	modified;
	glRect_t	rectchange;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte		*data;//[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
};

#define MAX_SANITY_LIGHTMAPS (1u<<20)
struct lightmap_s *lightmaps;
int			lightmap_count;
int			last_lightmap_allocated;
int			allocated[LMBLOCK_WIDTH];

unsigned	blocklights[LMBLOCK_WIDTH*LMBLOCK_HEIGHT*3]; //johnfitz -- was 18*18, added lit support (*3) and loosened surface extents maximum (LMBLOCK_WIDTH*LMBLOCK_HEIGHT)

glpoly_t	*fullbright_polys[MAX_GLTEXTURES];
glpoly_t	*luma_polys[MAX_GLTEXTURES];
qboolean	drawfullbrights = false, drawlumas = false;
glpoly_t	*caustics_polys = NULL;
glpoly_t	*detail_polys = NULL;
glpoly_t	*outline_polys = NULL;

float GL_WaterAlphaForEntitySurface(entity_t *ent, msurface_t *s);
void DrawWaterPoly(glpoly_t *p);
byte *SV_FatPVS(vec3_t org, model_t *worldmodel);

/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p)
{
	int		i;
	float	*v;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i = 0 ; i < p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

void R_RenderFullbrights (void)
{
	int			i;
	glpoly_t	*p;

	if (!drawfullbrights)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	glEnable (GL_ALPHA_TEST);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	Fog_StartAdditive();

	for (i = 1 ; i < MAX_GLTEXTURES ; i++)
	{
		if (!fullbright_polys[i])
			continue;
		GL_Bind (i);
		for (p = fullbright_polys[i] ; p ; p = p->fb_chain)
			DrawGLPoly (p);
		fullbright_polys[i] = NULL;
	}

	Fog_StopAdditive();
	glDisable (GL_ALPHA_TEST);
	glDepthMask (GL_TRUE);

	drawfullbrights = false;
}

void R_RenderLumas (void)
{
	int			i;
	glpoly_t	*p;

	if (!drawlumas)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	Fog_StartAdditive();

	for (i = 1 ; i < MAX_GLTEXTURES ; i++)
	{
		if (!luma_polys[i])
			continue;
		GL_Bind (i);
		for (p = luma_polys[i] ; p ; p = p->luma_chain)
			DrawGLPoly (p);
		luma_polys[i] = NULL;
	}

	Fog_StopAdditive();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);
	glDepthMask (GL_TRUE);

	drawlumas = false;
}

void EmitDetailPolys (void)
{
	int			i;
	float		*v;
	glpoly_t	*p;

	if (!detail_polys)
		return;

	GL_Bind (detailtexture);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

	for (p = detail_polys ; p ; p = p->detail_chain)
	{
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i = 0 ; i < p->numverts ; i++, v += VERTEXSIZE)
		{
			glTexCoord2f (v[7]*18, v[8]*18);
			glVertex3fv (v);
		}
		glEnd();
	}

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);

	detail_polys = NULL;
}

//=============================================================
// Dynamic lights

typedef struct dlightinfo_s
{
	int	local[2];
	int	rad;
	int	minlight;	// rad - minlight
	int	type;
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int	numdlights;

/*
===============
R_BuildDlightList
===============
*/
void R_BuildDlightList (msurface_t *surf)
{
	int			lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	float		dist;
	vec3_t		impact;
	mtexinfo_t	*tex;
	dlightinfo_t *light;

	numdlights = 0;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0 ; lnum < MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die < cl.time)
			continue;		// dead light

		if (!(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31))))
			continue;		// not lit by this light

		dist = PlaneDiff(cl_dlights[lnum].origin, surf->plane);
		irad = (cl_dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = cl_dlights[lnum].minlight * 256;
		if (irad < iminlight)
			continue;

		iminlight = irad - iminlight;

		for (i = 0 ; i < 3 ; i++)
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i]*dist;

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		// check if this dlight will touch the surface
		if (local[1] > 0)
		{
			tdmin = local[1] - (tmax << 4);
			if (tdmin < 0)
				tdmin = 0;
		}
		else
		{
			tdmin = -local[1];
		}

		if (local[0] > 0)
		{
			sdmin = local[0] - (smax << 4);
			if (sdmin < 0)
				sdmin = 0;
		}
		else
		{
			sdmin = -local[0];
		}

		distmin = (sdmin > tdmin) ? (sdmin << 8) + (tdmin << 7) : (tdmin << 8) + (sdmin << 7);
		if (distmin < iminlight)
		{
			// save dlight info
			light = &dlightlist[numdlights];
			light->minlight = iminlight;
			light->rad = irad;
			light->local[0] = local[0];
			light->local[1] = local[1];
			light->type = cl_dlights[lnum].type;
			numdlights++;
		}
	}
}

int dlightcolor[NUM_DLIGHTTYPES][3] = {
	{100, 90, 80},		// dimlight or brightlight
	{100, 50, 10},		// muzzleflash
	{100, 50, 10},		// explosion
	{90, 60, 7},		// rocket
	{128, 0, 0},		// red
	{0, 0, 128},		// blue
	{128, 0, 128},		// red + blue
	{0,	128, 0}			// green
};

/*
===============
R_AddDynamicLights

NOTE: R_BuildDlightList must be called first!
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			i, j, smax, tmax, s, t, sd, td, _sd, _td, irad, idist, iminlight, color[3], tmp;
	unsigned	*dest;
	dlightinfo_t *light;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	for (i = 0, light = dlightlist ; i < numdlights ; i++, light++)
	{
		for (j = 0 ; j < 3 ; j++)
		{
			if (light->type == lt_explosion2 || light->type == lt_explosion3)
				color[j] = (int)(ExploColor[j] * 255);
			else
				color[j] = dlightcolor[light->type][j];
		}

		irad = light->rad;
		iminlight = light->minlight;

		_td = light->local[1];
		dest = blocklights;
		for (t = 0 ; t < tmax ; t++)
		{
			td = _td;
			if (td < 0)
				td = -td;
			_td -= 16;
			_sd = light->local[0];

			for (s = 0 ; s < smax ; s++)
			{
				sd = _sd < 0 ? -_sd : _sd;
				_sd -= 16;
				idist = (sd > td) ? (sd << 8) + (td << 7) : (td << 8) + (sd << 7);
				if (idist < iminlight)
				{
					tmp = (irad - idist) >> 7;
					dest[0] += tmp * color[0];
					dest[1] += tmp * color[1];
					dest[2] += tmp * color[2];
				}
				dest += 3;
			}
		}
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax, i, j, size, maps, r, g, b, blocksize;
	byte		*lightmap;
	unsigned	scale, *bl, ambient_light;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lightmap = surf->samples;

	if (cl.worldmodel->lightdata)
	{
		// clear to no light
		memset(&blocklights[0], 0, size * 3 * sizeof(unsigned int)); //johnfitz -- lit support via lordhavoc

		// clear to ambient
		bl = blocklights;
		ambient_light = (unsigned int)(r_ambient.value) << 8;
		for (i = 0; i < size; i++)
		{
			*bl++ = ambient_light;
			*bl++ = ambient_light;
			*bl++ = ambient_light;
		}

		// add all the lightmaps
		if (lightmap)
		{
			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
				maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				surf->cached_light[maps] = scale;	// 8.8 fraction
				//johnfitz -- lit support via lordhavoc
				bl = blocklights;
				for (i = 0; i < size; i++)
				{
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
				}
				//johnfitz
			}
		}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights(surf);
	}
	else
	{
		// set to full bright if no light data
		memset(&blocklights[0], 255, size * 3 * sizeof(unsigned int)); //johnfitz -- lit support via lordhavoc
	}

	// bound, invert, and shift
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= smax * 4;
		bl = blocklights;
		for (i = 0; i < tmax; i++, dest += stride)
		{
			for (j = 0; j < smax; j++)
			{
				if (gl_overbright.value)
				{
					r = *bl++ >> 8;
					g = *bl++ >> 8;
					b = *bl++ >> 8;
				}
				else
				{
					r = *bl++ >> 7;
					g = *bl++ >> 7;
					b = *bl++ >> 7;

					// artifically clamp to 255 so gl_overbright 0 renders as expected in the gl_packed_pixels case
					r = (r > 255) ? 255 : r;
					g = (g > 255) ? 255 : g;
					b = (b > 255) ? 255 : b;
				}
				if (gl_packed_pixels)
				{
					r = (r > 1023) ? 1023 : r;
					g = (g > 1023) ? 1023 : g;
					b = (b > 1023) ? 1023 : b;
					*(unsigned int*)dest = (r << 22) | (g << 12) | (b << 2) | 3;
					dest += 4;
				}
				else
				{
					*dest++ = (r > 255) ? 255 : r;
					*dest++ = (g > 255) ? 255 : g;
					*dest++ = (b > 255) ? 255 : b;
					*dest++ = 255;
				}
			}
		}
		break;
	case GL_BGRA:
		stride -= smax * 4;
		bl = blocklights;
		for (i = 0; i < tmax; i++, dest += stride)
		{
			for (j = 0; j < smax; j++)
			{
				if (gl_overbright.value)
				{
					r = *bl++ >> 8;
					g = *bl++ >> 8;
					b = *bl++ >> 8;
				}
				else
				{
					r = *bl++ >> 7;
					g = *bl++ >> 7;
					b = *bl++ >> 7;

					// artifically clamp to 255 so gl_overbright 0 renders as expected in the gl_packed_pixels case
					r = (r > 255) ? 255 : r;
					g = (g > 255) ? 255 : g;
					b = (b > 255) ? 255 : b;
				}
				if (gl_packed_pixels)
				{
					r = (r > 1023) ? 1023 : r;
					g = (g > 1023) ? 1023 : g;
					b = (b > 1023) ? 1023 : b;
					*(unsigned int*)dest = (b << 22) | (g << 12) | (r << 2) | 3;
					dest += 4;
				}
				else
				{
					*dest++ = (b > 255) ? 255 : b;
					*dest++ = (g > 255) ? 255 : g;
					*dest++ = (r > 255) ? 255 : r;
					*dest++ = 255;
				}
			}
		}
		break;
	default:
		Sys_Error("R_BuildLightMap: bad lightmap format");
	}
}

/*
===============
R_UploadLightmap -- johnfitz -- uploads the modified lightmap to opengl if necessary

assumes lightmap texture is already bound
===============
*/
static void R_UploadLightmap(int lmap)
{
	const GLenum type = gl_packed_pixels ? GL_UNSIGNED_INT_10_10_10_2 : GL_UNSIGNED_BYTE;
	struct lightmap_s *lm = &lightmaps[lmap];

	if (!lm->modified)
		return;

	lm->modified = false;

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lm->rectchange.t, LMBLOCK_WIDTH, lm->rectchange.h, gl_lightmap_format,
		type, lm->data + lm->rectchange.t * LMBLOCK_WIDTH * lightmap_bytes);
	lm->rectchange.l = LMBLOCK_WIDTH;
	lm->rectchange.t = LMBLOCK_HEIGHT;
	lm->rectchange.h = 0;
	lm->rectchange.w = 0;
}

void R_UploadLightmaps(void)
{
	int lmap;

	for (lmap = 0; lmap < lightmap_count; lmap++)
	{
		if (!lightmaps[lmap].modified)
			continue;

		GL_Bind(lightmaps[lmap].texture);
		R_UploadLightmap(lmap);
	}
}

/*
================
R_RebuildAllLightmaps -- johnfitz -- called when gl_overbright gets toggled
================
*/
void R_RebuildAllLightmaps (void)
{
	const GLenum type = gl_packed_pixels ? GL_UNSIGNED_INT_10_10_10_2 : GL_UNSIGNED_BYTE;
	int			i, j;
	model_t		*mod;
	msurface_t	*fa;
	byte		*base;

	if (!cl.worldmodel) // is this the correct test?
		return;

	//for each surface in each model, rebuild lightmap with new scale
	for (i=1; i<MAX_MODELS; i++)
	{
		if (!(mod = cl.model_precache[i]))
			continue;
		fa = &mod->surfaces[mod->firstmodelsurface];
		for (j=0; j<mod->nummodelsurfaces; j++, fa++)
		{
			if (fa->flags & SURF_DRAWTILED)
				continue;
			base = lightmaps[fa->lightmaptexturenum].data;
			base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, LMBLOCK_WIDTH*lightmap_bytes);
		}
	}

	//for each lightmap, upload it
	for (i=0; i<lightmap_count; i++)
	{
		GL_Bind (lightmaps[i].texture);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, gl_lightmap_format,
				 type, lightmaps[i].data);
	}
}

qboolean OnChange_gl_overbright(cvar_t *var, char *string)
{
	float	newval = Q_atof(string);
	
	if (!newval == !gl_overbright.value)
		return false;

	Cvar_SetValue(&gl_overbright, newval);
	R_RebuildAllLightmaps();

	return true;
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int	relative, count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
===============================================================================

								BRUSH MODELS

===============================================================================
*/

void EmitOutlinePolys(void)
{
	glpoly_t	*p;
	int			i;
	float		*v;
	float		line_width = bound(1, r_outline_surf.value, 3);

	if (!outline_polys)
		return;

	glColor4f(0, 0, 0, 1);

	glEnable(GL_LINE_SMOOTH);
	glDisable(GL_TEXTURE_2D);
	glCullFace(GL_FRONT);
	glPolygonMode(GL_BACK, GL_LINE);
	glLineWidth(line_width);

	for (p = outline_polys; p; p = p->outline_chain)
	{
		glBegin(GL_LINE_LOOP);
		v = p->verts[0];
		for (i = 0; i<p->numverts; i++, v += VERTEXSIZE)
		{
			glVertex3fv(v);
		}
		glEnd();
	}
	glCullFace(GL_BACK);
	glColor4f(1, 1, 1, 1);
	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_TEXTURE_2D);
	glPolygonMode(GL_BACK, GL_FILL);
	outline_polys = NULL;
	glEnable(GL_TEXTURE_2D);
}

/*
================
R_DrawLightmapChains -- johnfitz -- R_BlendLightmaps stripped down to almost nothing
================
*/
void R_DrawLightmapChains (void)
{
	int			i, j;
	glpoly_t	*p;
	float		*v;

	for (i = 0 ; i < lightmap_count; i++)
	{
		if (!lightmaps[i].polys)
			continue;

		GL_Bind (lightmaps[i].texture);
		for (p = lightmaps[i].polys; p ; p = p->chain)
		{
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0 ; j < p->numverts ; j++, v += VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
		}
	}
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps ()
{
	glDepthMask (GL_FALSE);		// don't bother writing Z
	if (gl_overbright.value)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
	}
	glBlendFunc (GL_ZERO, GL_SRC_COLOR);
	glEnable (GL_BLEND);

	Fog_StartAdditive();

	R_DrawLightmapChains();

	Fog_StopAdditive();
	if (gl_overbright.value)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);
	glDepthMask (GL_TRUE);		// back to normal Z buffering
}

/*
================
R_RenderDynamicLightmaps
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa, texchain_t chain)
{
	int			maps, smax, tmax;
	byte		*base;
	glRect_t	*theRect;
	qboolean	lightstyle_modified = false;
	struct lightmap_s *lm;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWTILED) //johnfitz -- not a lightmapped surface
		return;

	// add to lightmap chain
	fa->polys->chain = lightmaps[fa->lightmaptexturenum].polys;
	lightmaps[fa->lightmaptexturenum].polys = fa->polys;

	if (!r_dynamic.value)
		return;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ; maps++)
	{
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
		{
			lightstyle_modified = true;
			break;
		}
	}

	if (fa->dlightframe == r_framecount)
		R_BuildDlightList (fa);
	else
		numdlights = 0;

	if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified)
		return;

	lm = &lightmaps[fa->lightmaptexturenum];
	lm->modified = true;
	theRect = &lm->rectchange;
	if (fa->light_t < theRect->t)
	{
		if (theRect->h)
			theRect->h += theRect->t - fa->light_t;
		theRect->t = fa->light_t;
	}
	if (fa->light_s < theRect->l)
	{
		if (theRect->w)
			theRect->w += theRect->l - fa->light_s;
		theRect->l = fa->light_s;
	}
	smax = (fa->extents[0] >> 4) + 1;
	tmax = (fa->extents[1] >> 4) + 1;
	if (theRect->w + theRect->l < fa->light_s + smax)
		theRect->w = fa->light_s - theRect->l + smax;
	if (theRect->h + theRect->t < fa->light_t + tmax)
		theRect->h = fa->light_t - theRect->t + tmax;
	base = lm->data;
	base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
	R_BuildLightMap(fa, base, LMBLOCK_WIDTH * lightmap_bytes);
}

static void R_ClearTextureChains (model_t *mod, texchain_t chain)
{
	int	i;

	// set all chains to null
	for (i = 0; i < mod->numtextures; i++)
		if (mod->textures[i])
			mod->textures[i]->texturechains[chain] = NULL;

	// clear lightmap chains
	for (i = 0; i < lightmap_count; i++)
		lightmaps[i].polys = NULL;

	memset (fullbright_polys, 0, sizeof(fullbright_polys));
	memset (luma_polys, 0, sizeof(luma_polys));

	r_notexture_mip->texturechains[0] = NULL;
	r_notexture_mip->texturechains[1] = NULL;
}

/*
================
R_ChainSurface -- ericw -- adds the given surface to its texture chain
================
*/
void R_ChainSurface(msurface_t *surf, texchain_t chain)
{
	surf->texturechain = surf->texinfo->texture->texturechains[chain];
	surf->texinfo->texture->texturechains[chain] = surf;
}

/*
================
R_BackFaceCull -- johnfitz -- returns true if the surface is facing away from vieworg
================
*/
qboolean R_BackFaceCull(msurface_t *surf)
{
	double dot;

	if (surf->plane->type < 3)
		dot = r_refdef.vieworg[surf->plane->type] - surf->plane->dist;
	else
		dot = DotProduct(r_refdef.vieworg, surf->plane->normal) - surf->plane->dist;

	if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
		return true;

	return false;
}

/*
===============
R_MarkSurfaces -- johnfitz -- mark surfaces based on PVS and rebuild texture chains
===============
*/
void R_MarkSurfaces(void)
{
	byte		*vis;
	mleaf_t		*leaf;
	msurface_t	*surf, **mark;
	int			i, j;
	qboolean	nearwaterportal;
	
	// clear lightmap chains
	for (i = 0; i < lightmap_count; i++)
		lightmaps[i].polys = NULL;

	// check this leaf for water portals
	// TODO: loop through all water surfs and use distance to leaf cullbox
	nearwaterportal = false;
	for (i = 0, mark = r_viewleaf->firstmarksurface; i < r_viewleaf->nummarksurfaces; i++, mark++)
		if ((*mark)->flags & SURF_DRAWTURB)
			nearwaterportal = true;

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
		vis = Mod_NoVisPVS(cl.worldmodel);
	else if (nearwaterportal)
		vis = SV_FatPVS(r_origin, cl.worldmodel);
	else
		vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);

	r_visframecount++;

	// set all chains to null
	for (i = 0; i<cl.worldmodel->numtextures; i++)
		if (cl.worldmodel->textures[i])
			cl.worldmodel->textures[i]->texturechains[chain_world] = NULL;

	// iterate through leaves, marking surfaces
	leaf = &cl.worldmodel->leafs[1];
	for (i = 0; i<cl.worldmodel->numleafs; i++, leaf++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			if (R_CullBox(leaf->minmaxs, leaf->minmaxs + 3))
				continue;

			if (leaf->contents != CONTENTS_SKY)
				for (j = 0, mark = leaf->firstmarksurface; j<leaf->nummarksurfaces; j++, mark++)
				{
					surf = *mark;
					if (surf->visframe != r_visframecount)
					{
						surf->visframe = r_visframecount;
						if (!R_CullBox(surf->mins, surf->maxs) && !R_BackFaceCull(surf))
						{
							R_ChainSurface(surf, chain_world);
							R_RenderDynamicLightmaps(surf, chain_world);
							if (surf->texinfo->texture->warp_texturenum)
								surf->texinfo->texture->update_warp = true;
						}
					}
				}

			// add static models
			if (leaf->efrags)
				R_StoreEfrags(&leaf->efrags);
		}
	}
}

/*
=============================================================

VBO support

=============================================================
*/

GLuint gl_bmodel_vbo = 0;

void GL_DeleteBModelVertexBuffer(void)
{
	if (!(gl_vbo_able && gl_mtexable && gl_textureunits >= 4))
		return;

	qglDeleteBuffers(1, &gl_bmodel_vbo);
	gl_bmodel_vbo = 0;

	GL_ClearBufferBindings();
}

/*
==================
GL_BuildBModelVertexBuffer

Deletes gl_bmodel_vbo if it already exists, then rebuilds it with all
surfaces from world + all brush models
==================
*/
void GL_BuildBModelVertexBuffer(void)
{
	unsigned int numverts, varray_bytes, varray_index;
	int		i, j;
	model_t	*m;
	float	*varray;

	if (!(gl_vbo_able && gl_mtexable && gl_textureunits >= 4))
		return;

	// ask GL for a name for our VBO
	qglDeleteBuffers(1, &gl_bmodel_vbo);
	qglGenBuffers(1, &gl_bmodel_vbo);

	// count all verts in all models
	numverts = 0;
	for (j = 1; j<MAX_MODELS; j++)
	{
		m = cl.model_precache[j];
		if (!m || m->name[0] == '*' || m->type != mod_brush)
			continue;

		for (i = 0; i<m->numsurfaces; i++)
		{
			numverts += m->surfaces[i].numedges;
		}
	}

	// build vertex array
	varray_bytes = VERTEXSIZE * sizeof(float) * numverts;
	varray = (float *)malloc(varray_bytes);
	varray_index = 0;

	for (j = 1; j<MAX_MODELS; j++)
	{
		m = cl.model_precache[j];
		if (!m || m->name[0] == '*' || m->type != mod_brush)
			continue;

		for (i = 0; i<m->numsurfaces; i++)
		{
			msurface_t *s = &m->surfaces[i];
			s->vbo_firstvert = varray_index;
			memcpy(&varray[VERTEXSIZE * varray_index], s->polys->verts, VERTEXSIZE * sizeof(float) * s->numedges);
			varray_index += s->numedges;
		}
	}

	// upload to GPU
	qglBindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
	qglBufferData(GL_ARRAY_BUFFER, varray_bytes, varray, GL_STATIC_DRAW);
	free(varray);

	// invalidate the cached bindings
	GL_ClearBufferBindings();
}

static unsigned int R_NumTriangleIndicesForSurf(msurface_t *s)
{
	return 3 * (s->numedges - 2);
}

/*
================
R_TriangleIndicesForSurf

Writes out the triangle indices needed to draw s as a triangle list.
The number of indices it will write is given by R_NumTriangleIndicesForSurf.
================
*/
static void R_TriangleIndicesForSurf(msurface_t *s, unsigned int *dest)
{
	int i;
	for (i = 2; i<s->numedges; i++)
	{
		*dest++ = s->vbo_firstvert;
		*dest++ = s->vbo_firstvert + i - 1;
		*dest++ = s->vbo_firstvert + i;
	}
}

#define MAX_BATCH_SIZE 4096

static unsigned int vbo_indices[MAX_BATCH_SIZE];
static unsigned int num_vbo_indices;

/*
================
R_ClearBatch
================
*/
static void R_ClearBatch()
{
	num_vbo_indices = 0;
}

static GLuint useDetailTexLoc;
static GLuint useCausticsTexLoc;

typedef enum {
	ABOVE_WATER,
	IS_WATER,
	UNDER_WATER,
} surfacetype;

/*
================
R_FlushBatch

Draw the current batch if non-empty and clears it, ready for more R_BatchSurface calls.
================
*/
static void R_FlushBatch(surfacetype surftype)
{
	if (num_vbo_indices > 0)
	{
		if (surftype == UNDER_WATER && gl_caustics.value && underwatertexture)
		{
			GL_SelectTexture(GL_TEXTURE3);
			GL_Bind(underwatertexture);
			qglUniform1i(useCausticsTexLoc, 1);
		}
		else
			qglUniform1i(useCausticsTexLoc, 0);

		if (surftype == ABOVE_WATER && gl_detail.value && detailtexture)
		{
			GL_SelectTexture(GL_TEXTURE3);
			GL_Bind(detailtexture);
			qglUniform1i(useDetailTexLoc, 1);
		}
		else
			qglUniform1i(useDetailTexLoc, 0);

		glDrawElements(GL_TRIANGLES, num_vbo_indices, GL_UNSIGNED_INT, vbo_indices);
		num_vbo_indices = 0;
	}
}

/*
================
R_BatchSurface

Add the surface to the current batch, or just draw it immediately if we're not
using VBOs.
================
*/
static void R_BatchSurface(msurface_t *s, surfacetype surftype)
{
	int num_surf_indices;

	num_surf_indices = R_NumTriangleIndicesForSurf(s);

	if (num_vbo_indices + num_surf_indices > MAX_BATCH_SIZE)
		R_FlushBatch(surftype);

	R_TriangleIndicesForSurf(s, &vbo_indices[num_vbo_indices]);
	num_vbo_indices += num_surf_indices;
}

static GLuint r_world_program;

// uniforms used in vert shader

// uniforms used in frag shader
static GLuint texLoc;
static GLuint LMTexLoc;
static GLuint fullbrightTexLoc;
static GLuint detailTexLoc;
static GLuint causticsTexLoc;
static GLuint useFullbrightTexLoc;
static GLuint useOverbrightLoc;
static GLuint useAlphaTestLoc;
static GLuint usePackedPixelsLoc;
static GLint  useLightmapOnlyLoc;
static GLuint useWaterFogLoc;
static GLuint alphaLoc;
static GLuint clTimeLoc;
static GLuint turbsinLoc;

#define vertAttrIndex 0
#define texCoordsAttrIndex 1
#define LMCoordsAttrIndex 2
#define detailCoordsAttrIndex 3

/*
=============
GLWorld_CreateShaders
=============
*/
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
void GLWorld_CreateShaders(void)
{
	const glsl_attrib_binding_t bindings[] = {
		{ "Vert", vertAttrIndex },
		{ "TexCoords", texCoordsAttrIndex },
		{ "LMCoords", LMCoordsAttrIndex },
		{ "DetailCoords", detailCoordsAttrIndex }
	};

	const GLchar *vertSource = \
		"#version 130\n"
		"\n"
		"attribute vec3 Vert;\n"
		"attribute vec2 TexCoords;\n"
		"attribute vec2 LMCoords;\n"
		"attribute vec2 DetailCoords;\n"
		"\n"
		"varying float FogFragCoord;\n"
		"\n"
		"void main()\n"
		"{\n"
		"	gl_TexCoord[0] = vec4(TexCoords, 0.0, 0.0);\n"
		"	gl_TexCoord[1] = vec4(LMCoords, 0.0, 0.0);\n"
		"	gl_TexCoord[2] = vec4(DetailCoords.xy, 0.0, 0.0);\n"
		"	gl_Position = gl_ModelViewProjectionMatrix * vec4(Vert, 1.0);\n"
		"	FogFragCoord = gl_Position.w;\n"
		"}\n";

	const GLchar *fragSource = \
		"#version 130\n"	// required for bitwise operators
		"\n"
		"#define M_PI			3.1415926535897932384626433832795\n"
		"#define TURBSINSIZE	" TOSTRING(TURBSINSIZE) "\n"
		"#define TURBSCALE		(float(TURBSINSIZE) / (2.0 * M_PI))\n"
		"\n"
		"uniform sampler2D Tex;\n"
		"uniform sampler2D LMTex;\n"
		"uniform sampler2D FullbrightTex;\n"
		"uniform sampler2D DetailTex;\n"
		"uniform sampler2D CausticsTex;\n"
		"uniform int UseFullbrightTex;\n"
		"uniform bool UseDetailTex;\n"
		"uniform bool UseCausticsTex;\n"
		"uniform bool UseOverbright;\n"
		"uniform bool UseAlphaTest;\n"
		"uniform bool UsePackedPixels;\n"
		"uniform bool UseLightmapOnly;\n"
		"uniform int UseWaterFog;\n"
		"uniform float Alpha;\n"
		"uniform float ClTime;\n"
		"uniform int turbsin[TURBSINSIZE];\n"
		"\n"
		"varying float FogFragCoord;\n"
		"\n"
		"\n"
		"float SINTABLE_APPROX(float time)\n"	// caustics effect generation
		"{\n"
		"	float sinlerpf, lerptime, lerp;\n"
		"	int	sinlerp1, sinlerp2;\n"
		"\n"
		"	sinlerpf = time * TURBSCALE;\n"
		"	sinlerp1 = int(floor(sinlerpf));\n"
		"	sinlerp2 = sinlerp1 + 1;\n"
		"	lerptime = sinlerpf - sinlerp1;\n"
		"\n"
		"	lerp = turbsin[sinlerp1 & (TURBSINSIZE - 1)] * (1 - lerptime) + turbsin[sinlerp2 & (TURBSINSIZE - 1)] * lerptime;\n"
		"	return -8.0 + 16.0 * lerp / 255.0;\n"
		"}\n"
		"void main()\n"
		"{\n"
		"	vec4 result = texture2D(Tex, gl_TexCoord[0].xy);\n"
		"	if (UseLightmapOnly)\n"
		"		result = vec4(0.5, 0.5, 0.5, 1.0);\n"
		"	if (UseAlphaTest && (result.a < 0.666))\n"
		"		discard;\n"
		"	result *= texture2D(LMTex, gl_TexCoord[1].xy);\n"
		"	if (UsePackedPixels)\n"
		"	    result.rgb *= 4.0;\n"
		"	if (UseOverbright)\n"
		"		result.rgb *= 2.0;\n"
		"	vec4 fb = texture2D(FullbrightTex, gl_TexCoord[0].xy);\n"
		"	if (UseFullbrightTex == 1)\n"
		"		result = mix(result, fb, fb.a);\n"
		"	else if (UseFullbrightTex == 2)\n"
		"		result += fb;\n"
		"	if (UseCausticsTex)\n"
		"	{\n"
		"		float s = gl_TexCoord[0].x + SINTABLE_APPROX(0.465 * (ClTime + gl_TexCoord[0].y));\n"
		"		s *= -3.0 * (0.5 / 64.0);"
		"		float t = gl_TexCoord[0].y + SINTABLE_APPROX(0.465 * (ClTime + gl_TexCoord[0].x));\n"
		"		t *= -3.0 * (0.5 / 64.0);"
		"		vec4 caustics = texture2D(CausticsTex, vec2(s, t));\n"
		"		result = caustics * result + result * caustics;\n"
		"	}\n"
		"	if (UseDetailTex)\n"
		"	{\n"
		"		vec4 detail = texture2D(DetailTex, vec2(gl_TexCoord[2].x * 18.0, gl_TexCoord[2].y * 18.0));\n"
		"		result = detail * result + result * detail;\n"
		"	}\n"
		"	result = clamp(result, 0.0, 1.0);\n"
		"	float fog = 0.0;\n"
		"	if (UseWaterFog == 1)\n"
		"		fog = (gl_Fog.end - FogFragCoord) / (gl_Fog.end - gl_Fog.start);\n"
		"	else if (UseWaterFog == 2)\n"
		"		fog = exp(-gl_Fog.density * FogFragCoord);\n"
		"	else\n"
		"		fog = exp(-gl_Fog.density * gl_Fog.density * FogFragCoord * FogFragCoord);\n"
		"	fog = clamp(fog, 0.0, 1.0);\n"
		"	result = mix(gl_Fog.color, result, fog);\n"
		"	result.a = Alpha;\n" // FIXME: This will make almost transparent things cut holes though heavy fog
		"	gl_FragColor = result;\n"
		"}\n";

	if (!gl_glsl_alias_able)
		return;

	r_world_program = GL_CreateProgram(vertSource, fragSource, sizeof(bindings) / sizeof(bindings[0]), bindings);

	if (r_world_program != 0)
	{
		// get uniform locations
		texLoc = GL_GetUniformLocation(&r_world_program, "Tex");
		LMTexLoc = GL_GetUniformLocation(&r_world_program, "LMTex");
		fullbrightTexLoc = GL_GetUniformLocation(&r_world_program, "FullbrightTex");
		detailTexLoc = GL_GetUniformLocation(&r_world_program, "DetailTex");
		causticsTexLoc = GL_GetUniformLocation(&r_world_program, "CausticsTex");
		useFullbrightTexLoc = GL_GetUniformLocation(&r_world_program, "UseFullbrightTex");
		useDetailTexLoc = GL_GetUniformLocation(&r_world_program, "UseDetailTex");
		useCausticsTexLoc = GL_GetUniformLocation(&r_world_program, "UseCausticsTex");
		useOverbrightLoc = GL_GetUniformLocation(&r_world_program, "UseOverbright");
		useAlphaTestLoc = GL_GetUniformLocation(&r_world_program, "UseAlphaTest");
		usePackedPixelsLoc = GL_GetUniformLocation(&r_world_program, "UsePackedPixels");
		useLightmapOnlyLoc = GL_GetUniformLocation (&r_world_program, "UseLightmapOnly");
		useWaterFogLoc = GL_GetUniformLocation(&r_world_program, "UseWaterFog");
		alphaLoc = GL_GetUniformLocation(&r_world_program, "Alpha");
		clTimeLoc = GL_GetUniformLocation(&r_world_program, "ClTime");
		turbsinLoc = GL_GetUniformLocation(&r_world_program, "turbsin");
	}
}

extern GLuint gl_bmodel_vbo;

/*
================
R_DrawTextureChains_GLSL
================
*/
void R_DrawTextureChains_GLSL(model_t *model, texchain_t chain)
{
	int			i, lastlightmap, fullbright, underwater;
	msurface_t	*s;
	texture_t	*t, *at;
	qboolean	bound, alphaused = false;
	entity_t	*ent = currententity;

	// enable blending / disable depth writes
	if (ISTRANSPARENT(ent))
	{
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
	}

	qglUseProgram(r_world_program);

	// Bind the buffers
	GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // indices come from client memory!

	qglEnableVertexAttribArray(vertAttrIndex);
	qglEnableVertexAttribArray(texCoordsAttrIndex);
	qglEnableVertexAttribArray(LMCoordsAttrIndex);
	qglEnableVertexAttribArray(detailCoordsAttrIndex);

	qglVertexAttribPointer(vertAttrIndex, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0));
	qglVertexAttribPointer(texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0) + 3);
	qglVertexAttribPointer(LMCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0) + 5);
	qglVertexAttribPointer(detailCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0) + 7);

	// set uniforms
	qglUniform1i(texLoc, 0);
	qglUniform1i(LMTexLoc, 1);
	qglUniform1i(fullbrightTexLoc, 2);
	qglUniform1i(detailTexLoc, 3);
	qglUniform1i(causticsTexLoc, 3);
	qglUniform1i(useFullbrightTexLoc, 0);
	qglUniform1i(useDetailTexLoc, 0);
	qglUniform1i(useCausticsTexLoc, 0);
	qglUniform1i(useOverbrightLoc, (int)gl_overbright.value);
	qglUniform1i(useAlphaTestLoc, 0);
	qglUniform1i(usePackedPixelsLoc, gl_packed_pixels);
	qglUniform1i(useLightmapOnlyLoc, 0);
	qglUniform1i(useWaterFogLoc, (r_viewleaf->contents != CONTENTS_EMPTY && r_viewleaf->contents != CONTENTS_SOLID) ? (int)gl_waterfog.value : 0);
	qglUniform1f(alphaLoc, ent->transparency == 0 ? 1 : ent->transparency);
	qglUniform1f(clTimeLoc, cl.time);
	qglUniform1iv(turbsinLoc, TURBSINSIZE, turbsin);

	for (i = 0 ; i < model->numtextures ; i++)
	{
		t = model->textures[i];

		if (!t || !t->texturechains[chain] || t->texturechains[chain]->flags & (SURF_DRAWTURB | SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;

		// Enable/disable TMU 2 (fullbrights)
		// FIXME: Move below to where we bind GL_TEXTURE0
		at = R_TextureAnimation(t);
		if (gl_fb_bmodels.value && (fullbright = at->fb_texturenum))
		{
			GL_SelectTexture(GL_TEXTURE2);
			GL_Bind(fullbright);
			qglUniform1i(useFullbrightTexLoc, at->isLumaTexture ? 2 : 1);
		}
		else
			qglUniform1i(useFullbrightTexLoc, 0);

		R_ClearBatch();

		bound = false;
		alphaused = false;
		lastlightmap = 0; // avoid compiler warning

		for (underwater = 0; underwater < 2; underwater++)
		{
			for (s = t->texturechains[chain]; s; s = s->texturechain)
			{
				if ((!underwater && !(s->flags & SURF_UNDERWATER)) || (underwater && (s->flags & SURF_UNDERWATER)))
				{
					if (!bound) //only bind once we are sure we need this texture
					{
						GL_SelectTexture(GL_TEXTURE0);
						GL_Bind(at->gl_texturenum);

						if (!alphaused && s->flags & SURF_DRAWALPHA)
						{
							qglUniform1i(useAlphaTestLoc, 1); // Flip alpha test back on
							alphaused = true;
						}

						bound = true;
						lastlightmap = s->lightmaptexturenum;
					}

					if (s->lightmaptexturenum != lastlightmap)
						R_FlushBatch(underwater ? UNDER_WATER : ABOVE_WATER);

					GL_SelectTexture(GL_TEXTURE1);
					GL_Bind(lightmaps[s->lightmaptexturenum].texture);
					lastlightmap = s->lightmaptexturenum;
					R_BatchSurface(s, underwater ? UNDER_WATER : ABOVE_WATER);
				}
			}

			// joe: we need to draw surfaces per waterline because of the caustics (only in-water) and detail (only out-water) textures
			R_FlushBatch(underwater ? UNDER_WATER : ABOVE_WATER);
		}

		if (bound && alphaused)
			qglUniform1i(useAlphaTestLoc, 0); // Flip alpha test back off
	}

	// clean up
	qglDisableVertexAttribArray(vertAttrIndex);
	qglDisableVertexAttribArray(texCoordsAttrIndex);
	qglDisableVertexAttribArray(LMCoordsAttrIndex);
	qglDisableVertexAttribArray(detailCoordsAttrIndex);

	qglUseProgram(0);
	GL_SelectTexture(GL_TEXTURE0);

	if (ISTRANSPARENT(ent))
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}
}

/*
================
R_DrawLightmapChains_GLSL -- ericw
================
*/
void R_DrawLightmapChains_GLSL(model_t *model, texchain_t chain)
{
	int			i, lastlightmap;
	msurface_t	*s;
	texture_t	*t;

	qglUseProgram(r_world_program);

	// Bind the buffers
	GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // indices come from client memory!

	qglEnableVertexAttribArray(vertAttrIndex);
	qglEnableVertexAttribArray(texCoordsAttrIndex);
	qglEnableVertexAttribArray(LMCoordsAttrIndex);

	qglVertexAttribPointer(vertAttrIndex, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float*)0));
	qglVertexAttribPointer(texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float*)0) + 3);
	qglVertexAttribPointer(LMCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float*)0) + 5);

	// set uniforms
	qglUniform1i(texLoc, 0);
	qglUniform1i(LMTexLoc, 1);
	qglUniform1i(fullbrightTexLoc, 2);
	qglUniform1i(useFullbrightTexLoc, 0);
	qglUniform1i(useOverbrightLoc, (int)gl_overbright.value);
	qglUniform1i(useDetailTexLoc, 0);
	qglUniform1i(useCausticsTexLoc, 0);
	qglUniform1i(useAlphaTestLoc, 0);
	qglUniform1i(usePackedPixelsLoc, gl_packed_pixels);
	qglUniform1f(alphaLoc, 1.0f);
	qglUniform1i(useLightmapOnlyLoc, 1);
	qglUniform1i(useWaterFogLoc, 0);
	qglUniform1f(clTimeLoc, cl.time);

	R_ClearBatch();
	lastlightmap = -1;

	for (i = 0; i < model->numtextures; i++)
	{
		t = model->textures[i];

		if (!t || !t->texturechains[chain] || t->texturechains[chain]->flags & (SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;

		if (t->texturechains[chain]->texinfo->flags & TEX_SPECIAL)
			continue; // unlit water

		for (s = t->texturechains[chain]; s; s = s->texturechain)
		{
			if (s->lightmaptexturenum < 0)
				continue;

			if (s->lightmaptexturenum != lastlightmap)
			{
				R_FlushBatch(IS_WATER);

				GL_SelectTexture(GL_TEXTURE1);
				GL_Bind(lightmaps[s->lightmaptexturenum].texture);
				lastlightmap = s->lightmaptexturenum;
			}
			R_BatchSurface(s, IS_WATER);
		}
	}

	R_FlushBatch(IS_WATER);

	// clean up
	qglDisableVertexAttribArray(vertAttrIndex);
	qglDisableVertexAttribArray(texCoordsAttrIndex);
	qglDisableVertexAttribArray(LMCoordsAttrIndex);

	qglUseProgram(0);
	GL_SelectTexture(GL_TEXTURE0);
}

/*
================
R_DrawTextureChains_Multitexture
================
*/
void R_DrawTextureChains_Multitexture (model_t *model, texchain_t chain)
{
	int			i, k, GL_LIGHTMAP_TEXTURE, GL_FB_TEXTURE;
	float		*v;
	msurface_t	*s;
	texture_t	*t, *at;
	qboolean	mtex_lightmaps, mtex_fbs, doMtex1, doMtex2, render_lightmaps = false;
	qboolean	draw_fbs, draw_mtex_fbs, can_mtex_lightmaps, can_mtex_fbs, alphaused = false;

	if (gl_fb_bmodels.value)
	{
		can_mtex_lightmaps = gl_mtexable;
		can_mtex_fbs = gl_textureunits >= 3;
	}
	else
	{
		can_mtex_lightmaps = gl_textureunits >= 3;
		can_mtex_fbs = gl_textureunits >= 3 && gl_add_ext;
	}

	GL_DisableMultitexture ();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 0 ; i < model->numtextures ; i++)
	{
		t = model->textures[i];

		if (!t || !t->texturechains[chain] || t->texturechains[chain]->flags & (SURF_DRAWTURB | SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;

		at = R_TextureAnimation(t);

		// bind the world texture
		GL_SelectTexture (GL_TEXTURE0);
		GL_Bind (at->gl_texturenum);

		draw_fbs = at->isLumaTexture || gl_fb_bmodels.value;
		draw_mtex_fbs = draw_fbs && can_mtex_fbs;

		if (gl_mtexable)
		{
			if (at->isLumaTexture && !gl_fb_bmodels.value)
			{
				if (gl_add_ext)
				{
					doMtex1 = true;
					GL_FB_TEXTURE = GL_TEXTURE1;
					GL_EnableTMU (GL_FB_TEXTURE);
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					GL_Bind (at->fb_texturenum);

					mtex_lightmaps = can_mtex_lightmaps;
					mtex_fbs = true;

					if (mtex_lightmaps)
					{
						doMtex2 = true;
						GL_LIGHTMAP_TEXTURE = GL_TEXTURE2;
						GL_EnableTMU (GL_LIGHTMAP_TEXTURE);
						if (gl_overbright.value)
						{
							glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
							glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
							glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
						}
						else
						{
							glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
						}
					}
					else
					{
						doMtex2 = false;
						render_lightmaps = true;
					}
				}
				else
				{
					GL_DisableTMU (GL_TEXTURE1);
					render_lightmaps = true;
					doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
				}
			}
			else
			{
				doMtex1 = true;
				GL_LIGHTMAP_TEXTURE = GL_TEXTURE1;
				GL_EnableTMU (GL_LIGHTMAP_TEXTURE);
				if (gl_overbright.value)
				{
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
					glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
					glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
				}
				else
				{
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				}

				mtex_lightmaps = true;
				mtex_fbs = at->fb_texturenum && draw_mtex_fbs;

				if (mtex_fbs)
				{
					doMtex2 = true;
					GL_FB_TEXTURE = GL_TEXTURE2;
					GL_EnableTMU (GL_FB_TEXTURE);
					GL_Bind (at->fb_texturenum);
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, at->isLumaTexture ? GL_ADD : GL_DECAL);
				}
				else
				{
					doMtex2 = false;
				}
			}
		}
		else
		{
			render_lightmaps = true;
			doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
		}

		for (s = t->texturechains[chain]; s; s = s->texturechain)
		{
			alphaused = false;
			if (!alphaused && s->flags & SURF_DRAWALPHA)
			{
				glEnable(GL_ALPHA_TEST);
				alphaused = true;
			}

			if (mtex_lightmaps)
			{
				// bind the lightmap texture
				GL_SelectTexture(GL_LIGHTMAP_TEXTURE);
				GL_Bind(lightmaps[s->lightmaptexturenum].texture);
			}

			glBegin(GL_POLYGON);
			v = s->polys->verts[0];
			for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE)
			{
				if (doMtex1)
				{
					qglMultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);

					if (mtex_lightmaps)
						qglMultiTexCoord2f(GL_LIGHTMAP_TEXTURE, v[5], v[6]);

					if (mtex_fbs)
						qglMultiTexCoord2f(GL_FB_TEXTURE, v[3], v[4]);
				}
				else
				{
					glTexCoord2f(v[3], v[4]);
				}
				glVertex3fv(v);
			}
			glEnd();

			if ((s->flags & SURF_UNDERWATER) && gl_caustics.value && underwatertexture)
			{
				s->polys->caustics_chain = caustics_polys;
				caustics_polys = s->polys;
			}
			if (!(s->flags & SURF_UNDERWATER) && gl_detail.value && detailtexture)
			{
				s->polys->detail_chain = detail_polys;
				detail_polys = s->polys;
			}
			if (r_outline_surf.value)
			{
				s->polys->outline_chain = outline_polys;
				outline_polys = s->polys;
			}
			if (at->fb_texturenum && draw_fbs && !mtex_fbs)
			{
				if (at->isLumaTexture)
				{
					s->polys->luma_chain = luma_polys[at->fb_texturenum];
					luma_polys[at->fb_texturenum] = s->polys;
					drawlumas = true;
				}
				else
				{
					s->polys->fb_chain = fullbright_polys[at->fb_texturenum];
					fullbright_polys[at->fb_texturenum] = s->polys;
					drawfullbrights = true;
				}
			}
		}
		if (doMtex1)
			GL_DisableTMU (GL_TEXTURE1);
		if (doMtex2)
			GL_DisableTMU (GL_TEXTURE2);
		if (alphaused)
			glDisable(GL_ALPHA_TEST);
	}

	if (gl_mtexable)
		GL_SelectTexture (GL_TEXTURE0);

	if (gl_fb_bmodels.value)
	{
		if (render_lightmaps)
			R_BlendLightmaps ();
		if (drawfullbrights)
			R_RenderFullbrights ();
		if (drawlumas)
			R_RenderLumas ();
	}
	else
	{
		if (drawlumas)
			R_RenderLumas ();
		if (render_lightmaps)
			R_BlendLightmaps ();
		if (drawfullbrights)
			R_RenderFullbrights ();
	}

	EmitOutlinePolys();//R00k
	EmitCausticsPolys();
	EmitDetailPolys();
}

/*
================
R_DrawTextureChains_TextureOnly -- johnfitz
================
*/
void R_DrawTextureChains_TextureOnly (model_t *model, texchain_t chain)
{
	int			i;
	msurface_t	*s;
	texture_t	*t, *at;
	qboolean	bound;

	for (i = 0 ; i < model->numtextures ; i++)
	{
		t = model->textures[i];

		if (!t || !t->texturechains[chain] || t->texturechains[chain]->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;

		at = R_TextureAnimation(t);

		bound = false;

		for (s = t->texturechains[chain]; s; s = s->texturechain)
		{
			if (!bound) //only bind once we are sure we need this texture
			{
				GL_Bind (at->gl_texturenum);

				if (t->texturechains[chain]->flags & SURF_DRAWALPHA)
					glEnable (GL_ALPHA_TEST); // Flip alpha test back on

				bound = true;
			}
			DrawGLPoly (s->polys);
		}

		if (bound && t->texturechains[chain]->flags & SURF_DRAWALPHA)
			glDisable (GL_ALPHA_TEST); // Flip alpha test back off
	}
}

/*
=============
R_BeginTransparentDrawing -- ericw
=============
*/
void R_BeginTransparentDrawing(float transparency)
{
	if (transparency > 0 && transparency < 1)
	{
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1, 1, 1, transparency);
	}
}

/*
=============
R_EndTransparentDrawing -- ericw
=============
*/
void R_EndTransparentDrawing(float transparency)
{
	if (transparency > 0 && transparency < 1)
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3f(1, 1, 1);
	}
}

/*
================
R_DrawTextureChains_Water -- johnfitz
================
*/
void R_DrawTextureChains_Water(model_t *model, entity_t *ent, texchain_t chain)
{
	int			i, teleport, lastlightmap;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;
	qboolean	bound, isTeleportTexture;
	float		entalpha;
	extern cvar_t r_oldwater;

	for (teleport = 1; teleport >= 0; teleport--)
	{
		if (r_oldwater.value)
		{
			for (i = 0; i < model->numtextures; i++)
			{
				t = model->textures[i];
				if (!t || !t->texturechains[chain] || !(t->texturechains[chain]->flags & SURF_DRAWTURB))
					continue;

				bound = false;
				entalpha = 1.0f;

				for (s = t->texturechains[chain]; s; s = s->texturechain)
				{
					isTeleportTexture = !strcmp(s->texinfo->texture->name, "*teleport") || !strcmp(s->texinfo->texture->name, "*tele01");
					if ((teleport && isTeleportTexture) || (!teleport && !isTeleportTexture))
					{
						if (!bound) //only bind once we are sure we need this texture
						{
							// joe: Always render teleport as opaque
							if (!isTeleportTexture)
							{
								entalpha = GL_WaterAlphaForEntitySurface(ent, s);
								R_BeginTransparentDrawing(entalpha);
							}
							GL_Bind(t->gl_texturenum);
							bound = true;
						}
						for (p = s->polys->next; p; p = p->next)
						{
							DrawWaterPoly(p);
						}
					}
				}

				R_EndTransparentDrawing(entalpha);
			}
		}
		else if (cl.worldmodel->haslitwater && r_litwater.value && r_world_program != 0)
		{
			qglUseProgram(r_world_program);

			// Bind the buffers
			GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
			GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

			qglEnableVertexAttribArray(vertAttrIndex);
			qglEnableVertexAttribArray(texCoordsAttrIndex);
			qglEnableVertexAttribArray(LMCoordsAttrIndex);

			qglVertexAttribPointer(vertAttrIndex, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0));
			qglVertexAttribPointer(texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0) + 3);
			qglVertexAttribPointer(LMCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(float), ((float *)0) + 5);

			// Set uniforms
			qglUniform1i(texLoc, 0);
			qglUniform1i(LMTexLoc, 1);
			qglUniform1i(fullbrightTexLoc, 2);
			qglUniform1i(useFullbrightTexLoc, 0);
			qglUniform1i(useOverbrightLoc, (int)gl_overbright.value);
			qglUniform1i(useDetailTexLoc, 0);
			qglUniform1i(useCausticsTexLoc, 0);
			qglUniform1i(useAlphaTestLoc, 0);
			qglUniform1i(usePackedPixelsLoc, gl_packed_pixels);
			qglUniform1i(useLightmapOnlyLoc, 0);
			qglUniform1i(useWaterFogLoc, 0);
			qglUniform1f(clTimeLoc, cl.time);

			for (i = 0; i<model->numtextures; i++)
			{
				t = model->textures[i];
				if (!t || !t->texturechains[chain] || !(t->texturechains[chain]->flags & SURF_DRAWTURB))
					continue;

				bound = false;
				entalpha = 1.0f;
				lastlightmap = 0;
				for (s = t->texturechains[chain]; s; s = s->texturechain)
				{
					isTeleportTexture = !strcmp(s->texinfo->texture->name, "*teleport") || !strcmp(s->texinfo->texture->name, "*tele01");
					if ((teleport && isTeleportTexture) || (!teleport && !isTeleportTexture))
					{
						if (!bound) //only bind once we are sure we need this texture
						{
							// joe: Always render teleport as opaque
							if (!isTeleportTexture)
							{
								entalpha = GL_WaterAlphaForEntitySurface(ent, s);
								if (entalpha < 1.0f)
								{
									qglUniform1f(alphaLoc, entalpha);
									R_BeginTransparentDrawing(entalpha);
								}
							}

							GL_SelectTexture(GL_TEXTURE0);
							GL_Bind(t->warp_texturenum);

							if (model != cl.worldmodel)
							{
								// ericw -- this is copied from R_DrawSequentialPoly.
								// If the poly is not part of the world we have to
								// set this flag
								t->update_warp = true; // FIXME: one frame too late!
							}

							bound = true;
							lastlightmap = s->lightmaptexturenum;
						}

						if (s->lightmaptexturenum != lastlightmap)
							R_FlushBatch(IS_WATER);

						GL_SelectTexture(GL_TEXTURE1);
						GL_Bind(lightmaps[s->lightmaptexturenum].texture);
						lastlightmap = s->lightmaptexturenum;
						R_BatchSurface(s, IS_WATER);
					}
				}
				R_FlushBatch(IS_WATER);
				R_EndTransparentDrawing(entalpha);
			}

			// clean up
			qglDisableVertexAttribArray(vertAttrIndex);
			qglDisableVertexAttribArray(texCoordsAttrIndex);
			qglDisableVertexAttribArray(LMCoordsAttrIndex);

			qglUseProgram(0);
			GL_SelectTexture(GL_TEXTURE0);
		}
		else
		{
			for (i = 0; i < model->numtextures; i++)
			{
				t = model->textures[i];
				if (!t || !t->texturechains[chain] || !(t->texturechains[chain]->flags & SURF_DRAWTURB))
					continue;

				bound = false;
				entalpha = 1.0f;

				for (s = t->texturechains[chain]; s; s = s->texturechain)
				{
					isTeleportTexture = !strcmp(s->texinfo->texture->name, "*teleport") || !strcmp(s->texinfo->texture->name, "*tele01");
					if ((teleport && isTeleportTexture) || (!teleport && !isTeleportTexture))
					{
						if (!bound) //only bind once we are sure we need this texture
						{
							// joe: Always render teleport as opaque
							if (!isTeleportTexture)
							{
								entalpha = GL_WaterAlphaForEntitySurface(ent, s);
								R_BeginTransparentDrawing(entalpha);
							}
							GL_Bind(t->warp_texturenum);

							if (model != cl.worldmodel)
							{
								// ericw -- this is copied from R_DrawSequentialPoly.
								// If the poly is not part of the world we have to
								// set this flag
								t->update_warp = true; // FIXME: one frame too late!
							}

							bound = true;
						}
						DrawGLPoly(s->polys);
					}
				}

				R_EndTransparentDrawing(entalpha);
			}
		}
	}
}

/*
================
R_DrawTextureChains
================
*/
void R_DrawTextureChains(model_t *model, entity_t *ent, texchain_t chain)
{
	float entalpha;

	entalpha = (ent != NULL) ? ent->transparency : 1.0f;
	
	R_UploadLightmaps();

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		R_BeginTransparentDrawing (entalpha);
		R_DrawTextureChains_TextureOnly (model, chain);
		R_EndTransparentDrawing (entalpha);
		return;
	}

	if (r_lightmap.value)
	{
		if (r_world_program != 0)
		{
			R_DrawLightmapChains_GLSL(model, chain);
			return;
		}

		if (!gl_overbright.value)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor3f(0.5, 0.5, 0.5);
		}
		R_DrawLightmapChains ();
		if (!gl_overbright.value)
		{
			glColor3f(1, 1, 1);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		return;
	}

	if (r_world_program != 0)
	{
		R_DrawTextureChains_GLSL(model, chain);
		return;
	}

	R_BeginTransparentDrawing(entalpha);

	R_DrawTextureChains_Multitexture(model, chain);

	R_EndTransparentDrawing(entalpha);
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *ent)
{
	int			i, k;
	float		dot, scalefactor = 1.0f;
	msurface_t	*psurf;
	mplane_t	*pplane;
	model_t		*clmodel = ent->model;
	extern cvar_t gl_zfix; // QuakeSpasm z-fighting fix

	currenttexture = -1;

	if (R_CullModelForEntity(ent))
		return;

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	VectorSubtract (r_refdef.vieworg, ent->origin, modelorg);
	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
	{
		vec3_t	temp, forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (ent->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		for (k = 0 ; k < MAX_DLIGHTS ; k++)
		{
			if (cl_dlights[k].die < cl.time || !cl_dlights[k].radius)
				continue;

			R_MarkLights (&cl_dlights[k], k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	glPushMatrix ();
	if (gl_zfix.value)
	{
		ent->origin[0] -= DIST_EPSILON;
		ent->origin[1] -= DIST_EPSILON;
		ent->origin[2] -= DIST_EPSILON;
	}
	glTranslatef(ent->origin[0], ent->origin[1], ent->origin[2]);
	glRotatef(ent->angles[1], 0, 0, 1);
	glRotatef(ent->angles[0], 0, 1, 0);
	glRotatef(ent->angles[2], 1, 0, 0);
	scalefactor = ENTSCALE_DECODE(ent->scale);
	if (scalefactor != 1.0f)
	{
		glScalef(scalefactor, scalefactor, scalefactor);
	}
	if (gl_zfix.value)
	{
		ent->origin[0] += DIST_EPSILON;
		ent->origin[1] += DIST_EPSILON;
		ent->origin[2] += DIST_EPSILON;
	}

	R_ClearTextureChains (clmodel, chain_model);

	// draw texture
	for (i = 0 ; i < clmodel->nummodelsurfaces ; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = PlaneDiff(modelorg, pplane);

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_ChainSurface(psurf, chain_model);
			R_RenderDynamicLightmaps(psurf, chain_model);
		}
	}

	R_DrawTextureChains (clmodel, ent, chain_model);
	R_DrawTextureChains_Water(clmodel, ent, chain_model);

	glPopMatrix();
}

/*
===============================================================================

								WORLD MODEL

===============================================================================
*/

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	R_DrawSky();

	// draw the world
	R_DrawTextureChains (cl.worldmodel, NULL, chain_world);
}

/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces(void)
{
	R_DrawTextureChains_Water(cl.worldmodel, NULL, chain_world);
}
/*
===============================================================================

						LIGHTMAP ALLOCATION

===============================================================================
*/

/*
========================
AllocBlock -- returns a texture number and the position inside it
========================
*/
int AllocBlock(int w, int h, int *x, int *y)
{
	int	i, j, best, best2, texnum;

	// ericw -- rather than searching starting at lightmap 0 every time,
	// start at the last lightmap we allocated a surface in.
	// This makes AllocBlock much faster on large levels (can shave off 3+ seconds
	// of load time on a level with 180 lightmaps), at a cost of not quite packing
	// lightmaps as tightly vs. not doing this (uses ~5% more lightmaps)
	for (texnum = last_lightmap_allocated; texnum < MAX_SANITY_LIGHTMAPS; texnum++)
	{
		if (texnum == lightmap_count)
		{
			lightmap_count++;
			lightmaps = (struct lightmap_s *)realloc(lightmaps, sizeof(*lightmaps) * lightmap_count);
			memset(&lightmaps[texnum], 0, sizeof(lightmaps[texnum]));
			lightmaps[texnum].data = (byte *)calloc(1, 4 * LMBLOCK_WIDTH * LMBLOCK_HEIGHT);
			//as we're only tracking one texture, we don't need multiple copies of allocated any more.
			memset(allocated, 0, sizeof(allocated));
		}
		best = LMBLOCK_HEIGHT;

		for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j = 0 ; j < w ; j++)
			{
				if (allocated[i+j] >= best)
					break;
				if (allocated[i+j] > best2)
					best2 = allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LMBLOCK_HEIGHT)
			continue;

		for (i = 0 ; i < w ; i++)
			allocated[*x+i] = best + h;

		last_lightmap_allocated = texnum;
		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0; //johnfitz -- shut up compiler
}

mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts, vertpage;
	float		*vec, s, t, s0, t0, sdiv, tdiv;
	medge_t		*pedges, *r_pedge;
	glpoly_t	*poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// draw texture
	poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	if (fa->flags & SURF_DRAWTURB)
	{
		// match Mod_PolyForUnlitSurface
		s0 = t0 = 0.f;
		sdiv = tdiv = 128.f;
	}
	else
	{
		s0 = fa->texinfo->vecs[0][3];
		t0 = fa->texinfo->vecs[1][3];
		sdiv = fa->texinfo->texture->width;
		tdiv = fa->texinfo->texture->height;
	}

	for (i = 0 ; i < lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}

		s = DotProduct(vec, fa->texinfo->vecs[0]) + s0;
		s /= sdiv;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + t0;
		t /= tdiv;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= LMBLOCK_WIDTH * 16;	//fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= LMBLOCK_HEIGHT * 16;	//fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;

		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= 128;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= 128;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][7] = s;
		poly->verts[i][8] = t;
	}

	poly->numverts = lnumverts;

	// support r_oldwater 1 on lit water
	if (fa->flags & SURF_DRAWTURB)
		GL_SubdivideSurface(fa);
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

	if (surf->flags & SURF_DRAWTILED)
	{
		surf->lightmaptexturenum = -1;
		return;
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps[surf->lightmaptexturenum].data;
	base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap(surf, base, LMBLOCK_WIDTH * lightmap_bytes);
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j, lightmap_textures;
	struct lightmap_s *lm;
	model_t	*m;

	r_framecount = 1;		// no dlightcache

	//Spike -- wipe out all the lightmap data (johnfitz -- the gltexture objects were already freed by Mod_ClearAll)
	for (i = 0; i < lightmap_count; i++)
		free(lightmaps[i].data);
	free(lightmaps);
	lightmaps = NULL;
	last_lightmap_allocated = 0;
	lightmap_count = 0;

	gl_lightmap_format = GL_RGBA;//FIXME: hardcoded for now!

	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		break;
	case GL_BGRA:
		lightmap_bytes = 4;
		break;
	default:
		Sys_Error("GL_BuildLightmaps: bad lightmap format");
	}

	for (j = 1 ; j < MAX_MODELS ; j++)
	{
		if (!(m = cl.model_precache[j]))
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0 ; i < m->numsurfaces ; i++)
		{
			//johnfitz -- rewritten to use SURF_DRAWTILED instead of the sky/water flags
			if (m->surfaces[i].flags & SURF_DRAWTILED)
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
			//johnfitz
		}
	}

	if (lightmap_count > 0)
	{
		lightmap_textures = texture_extension_number;
		texture_extension_number += lightmap_count;
	}

 	if (gl_mtexable)
 		GL_EnableMultitexture ();

	// upload all lightmaps that were filled
	for (i = 0 ; i < lightmap_count; i++)
	{
		const GLint internalfmt = gl_packed_pixels ? GL_RGB10_A2 : lightmap_bytes;
		const GLenum type = gl_packed_pixels ? GL_UNSIGNED_INT_10_10_10_2 : GL_UNSIGNED_BYTE;

		lm = &lightmaps[i];
		lm->modified = false;
		lm->rectchange.l = LMBLOCK_WIDTH;
		lm->rectchange.t = LMBLOCK_HEIGHT;
		lm->rectchange.w = 0;
		lm->rectchange.h = 0;
		lm->texture = lightmap_textures + i;

		GL_Bind (lm->texture);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, internalfmt, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0,
			gl_lightmap_format, type, lm->data);
	}

	if (gl_mtexable)
 		GL_DisableMultitexture ();
}
