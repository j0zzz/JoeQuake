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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"

extern	model_t	*loadmodel;

static	int	solidskytexture, alphaskytexture;
static	int	gl_filter_minmax_sky = GL_LINEAR;
static	float	speedscale, speedscale2;	// for top sky and bottom sky

static	msurface_t *warpface;

qboolean	r_skyboxloaded;
float		skyfog; // ericw 

int			gl_warpimagesize;

cvar_t r_oldwater = { "r_oldwater", "0", CVAR_ARCHIVE };
cvar_t r_waterquality = { "r_waterquality", "8" };
cvar_t r_waterwarp = { "r_waterwarp", "0" };

extern	cvar_t	gl_subdivide_size;
extern	cvar_t	r_skyfog;

extern	byte *StringToRGB (char *s);

float	turbsin_water[] =
{
#include "gl_warp_sin.h"
};

#define WARPCALC(s,t) ((s + turbsin_water[(int)((t*2)+(cl.time*(128.0/M_PI))) & 255]) * (1.0/64)) //johnfitz -- correct warp
#define WARPCALC2(s,t) ((s + turbsin_water[(int)((t*0.125+cl.time)*(128.0/M_PI)) & 255]) * (1.0/64)) //johnfitz -- old warp

static	int		skytexorder[6] = { 0, 2, 1, 3, 4, 5 };
static	char	*skybox_ext[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

typedef struct
{
	int		skybox_width;
	int		skybox_height;
} skybox_size_t;

skybox_size_t skybox_images[6];

#define	MAX_CLIP_VERTS	64

static	vec3_t	skyclip[6] = {
	{ 1, 1, 0 },
	{ 1, -1, 0 },
	{ 0, -1, 1 },
	{ 0, 1, 1 },
	{ 1, 0, 1 },
	{ -1, 0, 1 }
};

// 1 = s, 2 = t, 3 = 2048
static	int	st_to_vec[6][3] = {
	{ 3,-1, 2 },
	{ -3, 1, 2 },

	{ 1, 3, 2 },
	{ -1,-3, 2 },

	{ -2,-1, 3 },		// 0 degrees yaw, look straight up
	{ 2,-1,-3 }		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static	int	vec_to_st[6][3] = {
	{ -2, 3, 1 },
	{ 2, 3,-1 },

	{ 1, 3, 2 },
	{ -1, 3,-2 },

	{ -2,-1, 3 },
	{ -2, 1,-3 }
};

static	float	skymins[2][6], skymaxs[2][6];

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int	i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 999999999;
	maxs[0] = maxs[1] = maxs[2] = -999999999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
	{
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k, f, b;
	vec3_t		mins, maxs, front[64], back[64];
	float		m, *v, dist[64], frac, s, t, subdivide_size;
	glpoly_t	*poly;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	subdivide_size = max(1, gl_subdivide_size.value);
	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floor (m / subdivide_size + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j+1] > 0))
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = (glpoly_t *)Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys->next;
	warpface->polys->next = poly;
	poly->numverts = numverts;
	for (i = 0 ; i < numverts ; i++, verts += 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface(msurface_t *fa)
{
	vec3_t	verts[64];
	int		i;

	warpface = fa;

	//the first poly in the chain is the undivided poly for newwater rendering.
	//grab the verts from that.
	for (i = 0; i<fa->polys->numverts; i++)
		VectorCopy(fa->polys->verts[i], verts[i]);

	SubdividePolygon(fa->polys->numverts, verts[0]);
}

//=========================================================

#define	TURBSINSIZE	128
#define	TURBSCALE	((float)TURBSINSIZE / (2 * M_PI))

byte turbsin[TURBSINSIZE] =
{
	127, 133, 139, 146, 152, 158, 164, 170, 176, 182, 187, 193, 198, 203, 208, 213, 
	217, 221, 226, 229, 233, 236, 239, 242, 245, 247, 249, 251, 252, 253, 254, 254, 
	255, 254, 254, 253, 252, 251, 249, 247, 245, 242, 239, 236, 233, 229, 226, 221, 
	217, 213, 208, 203, 198, 193, 187, 182, 176, 170, 164, 158, 152, 146, 139, 133, 
	127, 121, 115, 108, 102, 96, 90, 84, 78, 72, 67, 61, 56, 51, 46, 41, 
	37, 33, 28, 25, 21, 18, 15, 12, 9, 7, 5, 3, 2, 1, 0, 0, 
	0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 28, 33, 
	37, 41, 46, 51, 56, 61, 67, 72, 78, 84, 90, 96, 102, 108, 115, 121, 
};

__inline static float SINTABLE_APPROX (float time)
{
	float	sinlerpf, lerptime, lerp;
	int	sinlerp1, sinlerp2;

	sinlerpf = time * TURBSCALE;
	sinlerp1 = floor(sinlerpf);
	sinlerp2 = sinlerp1 + 1;
	lerptime = sinlerpf - sinlerp1;

	lerp = turbsin[sinlerp1 & (TURBSINSIZE - 1)] * (1 - lerptime) + turbsin[sinlerp2 & (TURBSINSIZE - 1)] * lerptime;
	return -8 + 16 * lerp / 255.0;
}

void EmitFlatPoly (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int		i;

	for (p = fa->polys ; p ; p = p->next)
	{
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0] ; i < p->numverts ; i++, v += VERTEXSIZE)
			glVertex3fv (v);
		glEnd ();
	}
}

void EmitCausticsPolys(void)
{
	glpoly_t	*p;
	int		i;
	float		*v, s, t, os, ot;
	extern glpoly_t	*caustics_polys;

	GL_Bind(underwatertexture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	for (p = caustics_polys; p; p = p->caustics_chain)
	{
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			s = os + SINTABLE_APPROX(0.465 * (cl.time + ot));
			s *= -3 * (0.5 / 64);

			t = ot + SINTABLE_APPROX(0.465 * (cl.time + os));
			t *= -3 * (0.5 / 64);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);

	caustics_polys = NULL;
}

/*
================
DrawWaterPoly -- johnfitz
================
*/
void DrawWaterPoly(glpoly_t *p)
{
	float	*v;
	int		i;

	if (gl_subdivide_size.value > 48)
	{
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i<p->numverts; i++, v += VERTEXSIZE)
		{
			glTexCoord2f(WARPCALC2(v[3], v[4]), WARPCALC2(v[4], v[3]));
			glVertex3fv(v);
		}
		glEnd();
	}
	else
	{
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i<p->numverts; i++, v += VERTEXSIZE)
		{
			glTexCoord2f(WARPCALC(v[3], v[4]), WARPCALC(v[4], v[3]));
			glVertex3fv(v);
		}
		glEnd();
	}
}

//==============================================================================
//
//  RENDER-TO-FRAMEBUFFER WATER
//
//==============================================================================

/*
=============
R_UpdateWarpTextures -- johnfitz -- each frame, update warping textures
=============
*/
void R_UpdateWarpTextures(void)
{
	int i;
	float x, y, x2, warptess;
	texture_t *tx;

	if (r_oldwater.value || cl.paused)
		return;

	warptess = 128.0 / bound(3.0, floor(r_waterquality.value), 64.0);

	for (i = 0; i<cl.worldmodel->numtextures; i++)
	{
		if (!(tx = cl.worldmodel->textures[i]))
			continue;

		if (!tx->update_warp)
			continue;

		//render warp
		GL_SetCanvas(CANVAS_WARPIMAGE);
		GL_Bind(tx->gl_texturenum);
		for (x = 0.0; x < 128.0; x = x2)
		{
			x2 = x + warptess;
			glBegin(GL_TRIANGLE_STRIP);
			for (y = 0.0; y < 128.01; y += warptess) // .01 for rounding errors
			{
				glTexCoord2f(WARPCALC(x, y), WARPCALC(y, x));
				glVertex2f(x, y);
				glTexCoord2f(WARPCALC(x2, y), WARPCALC(y, x2));
				glVertex2f(x2, y);
			}
			glEnd();
		}

		//copy to texture
		GL_Bind(tx->warp_texturenum);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glx, gly + glheight - gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);

		if (qglGenerateMipmap)
			qglGenerateMipmap(GL_TEXTURE_2D);

		tx->update_warp = false;
	}

	// ericw -- workaround for osx 10.6 driver bug when using FSAA. R_Clear only clears the warpimage part of the screen.
	GL_SetCanvas(CANVAS_DEFAULT);

	//if warp render went down into sbar territory, we need to be sure to refresh it next frame
	if (gl_warpimagesize + sb_lines > glheight)
		Sbar_Changed();
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky(texture_t *mt)
{
	int			i, j, p, r, g, b;
	byte		*src;
	unsigned	*trans, transpix, halfwidth, *rgba;

	if (mt->width != 256 || mt->height != 128)
	{
		Con_Printf("Sky texture %s is %d x %d, expected 256 x 128\n", mt->name, mt->width, mt->height);
		if (mt->width < 2 || mt->height < 1)
			return;
	}

	halfwidth = mt->width / 2;
	trans = (unsigned *)Hunk_AllocName(halfwidth * mt->height * 4, "skytex");
	src = (byte *)(mt + 1);

	// make an average value for the back to avoid a fringe on the top level
	r = g = b = 0;
	for (i=0 ; i<mt->height ; i++)
		for (j=0 ; j<halfwidth ; j++)
		{
			p = src[i*mt->width + j + halfwidth];
			rgba = &d_8to24table[p];
			trans[(i*halfwidth) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}

	((byte *)&transpix)[0] = r / (halfwidth * mt->height);
	((byte *)&transpix)[1] = g / (halfwidth * mt->height);
	((byte *)&transpix)[2] = b / (halfwidth * mt->height);
	((byte *)&transpix)[3] = 0;

	if (!solidskytexture)
		solidskytexture = texture_extension_number++;

	GL_Bind (solidskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, halfwidth, mt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_minmax_sky);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_minmax_sky);

	for (i=0 ; i<mt->height; i++)
		for (j=0 ; j<halfwidth; j++)
		{
			p = src[i*mt->width + j];
			trans[(i*halfwidth) + j] = p ? d_8to24table[p] : transpix;
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;

	GL_Bind (alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, halfwidth, mt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_minmax_sky);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_minmax_sky);
}

qboolean OnChange_gl_texturemode_sky (cvar_t *var, char *string)
{
	if (!Q_strcasecmp("GL_LINEAR", string))
	{
		gl_filter_minmax_sky = GL_LINEAR;
	}
	else if (!Q_strcasecmp("GL_NEAREST", string))
	{
		gl_filter_minmax_sky = GL_NEAREST;
	}
	else
	{
		Con_Printf ("bad filter name: %s\n", string);
		return true;
	}

	GL_Bind (solidskytexture);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_minmax_sky);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_minmax_sky);
	GL_Bind (alphaskytexture);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_minmax_sky);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_minmax_sky);
	return false;
}

qboolean OnChange_r_skybox (cvar_t *var, char *string)
{
	if (!string[0])
	{
		r_skyboxloaded = false;
		return false;
	}

	if (nehahra)
	{
		Cvar_SetValue (&r_oldsky, 0);
		Q_strncpyz (prev_skybox, string, sizeof(prev_skybox));
	}

	return R_SetSky (string);
}

qboolean OnChange_r_skyfog(cvar_t *var, char *string)
{
	if (!string[0])
		return true;

	// clear any skyfog setting from worldspawn
	skyfog = Q_atof(string);

	return false;
}

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int	i, j, axis;
	float	s, t, dv, *vp;
	vec3_t	v, av;

	// decide which face it maps to
	VectorClear (v);
	for (i = 0, vp = vecs ; i < nump ; i++, vp += 3)
		VectorAdd (vp, v, v);
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j-1] : -vecs[-j-1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j-1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j-1] / dv : vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float		*norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qboolean	front, back;
	int			sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t		newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS-2)
		Sys_Error ("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");

	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs ; i < nump ; i++, v += 3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i*3)));
	newc[0] = newc[1] = 0;

	for (i=0, v=vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;

		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;

		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
================
Sky_ProcessPoly
================
*/
void Sky_ProcessPoly(glpoly_t *p)
{
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];

	// draw it
	DrawGLPoly(p);

	// update sky bounds
	if (!r_fastsky.value)
	{
		for (i = 0; i<p->numverts; i++)
			VectorSubtract(p->verts[i], r_origin, verts[i]);
		ClipSkyPolygon(p->numverts, verts[0], 0);
	}
}

/*
================
Sky_ProcessTextureChains -- handles sky polys in world model
================
*/
void Sky_ProcessTextureChains(void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechains[chain_world] || !(t->texturechains[chain_world]->flags & SURF_DRAWSKY))
			continue;

		for (s = t->texturechains[chain_world]; s; s = s->texturechain)
			Sky_ProcessPoly(s->polys);
	}
}

/*
================
Sky_ProcessEntities -- handles sky polys on brush models
================
*/
void Sky_ProcessEntities(void)
{
	entity_t	*ent;
	msurface_t	*s;
	glpoly_t	*p;
	int			i, j, k, mark;
	float		dot;
	qboolean	rotated;
	vec3_t		temp, forward, right, up;

	if (!r_drawentities.value)
		return;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		ent = cl_visedicts[i];

		if (ent->model->type != mod_brush)
			continue;

		if (R_CullModelForEntity(ent))
			continue;

		if (ent->transparency == 0)
			continue;

		VectorSubtract(r_refdef.vieworg, ent->origin, modelorg);
		if (ent->angles[0] || ent->angles[1] || ent->angles[2])
		{
			rotated = true;
			AngleVectors(ent->angles, forward, right, up);
			VectorCopy(modelorg, temp);
			modelorg[0] = DotProduct(temp, forward);
			modelorg[1] = -DotProduct(temp, right);
			modelorg[2] = DotProduct(temp, up);
		}
		else
			rotated = false;

		s = &ent->model->surfaces[ent->model->firstmodelsurface];

		for (j = 0; j<ent->model->nummodelsurfaces; j++, s++)
		{
			if (s->flags & SURF_DRAWSKY)
			{
				dot = DotProduct(modelorg, s->plane->normal) - s->plane->dist;
				if (((s->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
					(!(s->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
				{
					//copy the polygon and translate manually, since Sky_ProcessPoly needs it to be in world space
					mark = Hunk_LowMark();
					p = (glpoly_t *)Hunk_Alloc(sizeof(*s->polys)); //FIXME: don't allocate for each poly
					p->numverts = s->polys->numverts;
					for (k = 0; k<p->numverts; k++)
					{
						if (rotated)
						{
							p->verts[k][0] = ent->origin[0] + s->polys->verts[k][0] * forward[0]
								- s->polys->verts[k][1] * right[0]
								+ s->polys->verts[k][2] * up[0];
							p->verts[k][1] = ent->origin[1] + s->polys->verts[k][0] * forward[1]
								- s->polys->verts[k][1] * right[1]
								+ s->polys->verts[k][2] * up[1];
							p->verts[k][2] = ent->origin[2] + s->polys->verts[k][0] * forward[2]
								- s->polys->verts[k][1] * right[2]
								+ s->polys->verts[k][2] * up[2];
						}
						else
							VectorAdd(s->polys->verts[k], ent->origin, p->verts[k]);
					}
					Sky_ProcessPoly(p);
					Hunk_FreeToLowMark(mark);
				}
			}
		}
	}
}

//==============================================================================
//
//  RENDER SKYBOX
//
//==============================================================================

/*
==================
R_SetSky
==================
*/
int R_SetSky(char *skyname)
{
	int		i, texnum;
	byte	*data[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	char	*identifier;

	if (!Q_strcasecmp(r_skybox.string, skyname))
		return 0;

	if (draw_no24bit || !skyname[0])
	{
		r_skyboxloaded = false;
		return 0;
	}

	for (i = 0; i < 6; i++)
	{
		identifier = va("skybox:%s%s", skyname, skybox_ext[i]);

		if (!(texnum = GL_LoadTextureImage(va("env/%s%s", skyname, skybox_ext[i]), identifier, 0, 0, 0)) &&
			!(texnum = GL_LoadTextureImage(va("gfx/env/%s%s", skyname, skybox_ext[i]), identifier, 0, 0, 0)) &&
			!(texnum = GL_LoadTextureImage(va("env/%s_%s", skyname, skybox_ext[i]), identifier, 0, 0, 0)) &&
			!(texnum = GL_LoadTextureImage(va("gfx/env/%s_%s", skyname, skybox_ext[i]), identifier, 0, 0, 0)))
		{
			Con_Printf("Couldn't load skybox \"%s\"\n", skyname);
			return 1;
		}

		skybox_images[i].skybox_width = image_width;
		skybox_images[i].skybox_height = image_height;

		if (i == 0)
			skyboxtextures = texnum;
	}
	r_skyboxloaded = true;

	return 0;
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int	i;

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

/*
==============
Sky_SetBoxVert
==============
*/
void Sky_SetBoxVert(float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;

	b[0] = s * r_farclip.value / sqrt(3.0);
	b[1] = t * r_farclip.value / sqrt(3.0);
	b[2] = r_farclip.value / sqrt(3.0);

	for (j = 0; j<3; j++)
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
		v[j] += r_origin[j];
	}
}

/*
==============
Sky_EmitSkyBoxVertex
==============
*/
void Sky_EmitSkyBoxVertex(float s, float t, int axis)
{
	vec3_t	v;
	float	w, h;

	Sky_SetBoxVert(s, t, axis, v);

	// convert from range [-1,1] to [0,1]
	s = (s + 1)*0.5;
	t = (t + 1)*0.5;

	// avoid bilerp seam
	w = skybox_images[skytexorder[axis]].skybox_width;
	h = skybox_images[skytexorder[axis]].skybox_height;
	s = s * (w - 1) / w + 0.5 / w;
	t = t * (h - 1) / h + 0.5 / h;

	t = 1.0 - t;
	glTexCoord2f(s, t);
	glVertex3fv(v);
}

/*
==============
R_DrawSkyBox
==============
*/
void R_DrawSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (skyboxtextures + skytexorder[i]);

		glBegin (GL_QUADS);
		Sky_EmitSkyBoxVertex(skymins[0][i], skymins[1][i], i);
		Sky_EmitSkyBoxVertex(skymins[0][i], skymaxs[1][i], i);
		Sky_EmitSkyBoxVertex(skymaxs[0][i], skymaxs[1][i], i);
		Sky_EmitSkyBoxVertex(skymaxs[0][i], skymins[1][i], i);
		glEnd ();

		if (Fog_GetDensity() > 0 && skyfog > 0)
		{
			float *c;

			c = Fog_GetColor();
			glEnable(GL_BLEND);
			glDisable(GL_TEXTURE_2D);
			glColor4f(c[0], c[1], c[2], bound(0.0, skyfog, 1.0));

			glBegin(GL_QUADS);
			Sky_EmitSkyBoxVertex(skymins[0][i], skymins[1][i], i);
			Sky_EmitSkyBoxVertex(skymins[0][i], skymaxs[1][i], i);
			Sky_EmitSkyBoxVertex(skymaxs[0][i], skymaxs[1][i], i);
			Sky_EmitSkyBoxVertex(skymaxs[0][i], skymins[1][i], i);
			glEnd();

			glColor3f(1, 1, 1);
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
		}
	}
}

//==============================================================================
//
//  RENDER CLOUDS
//
//==============================================================================

/*
=============
Sky_GetTexCoord
=============
*/
void Sky_GetTexCoord(vec3_t v, float speed, float *s, float *t)
{
	vec3_t	dir;
	float	length, scroll;

	VectorSubtract(v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
	length = sqrt(length);
	length = 6 * 63 / length;

	scroll = cl.time*speed;
	scroll -= (int)scroll & ~127;

	*s = (scroll + dir[0] * length) * (1.0 / 128);
	*t = (scroll + dir[1] * length) * (1.0 / 128);
}

/*
=================
Sky_DrawFaceQuad
=================
*/
void Sky_DrawFaceQuad(glpoly_t *p)
{
	float	s, t;
	float	*v;
	int		i;

	if (gl_mtexable)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_Bind(solidskytexture);

		GL_EnableMultitexture();
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		GL_Bind(alphaskytexture);

		glBegin(GL_QUADS);
		for (i = 0, v = p->verts[0]; i<4; i++, v += VERTEXSIZE)
		{
			Sky_GetTexCoord(v, 8, &s, &t);
			qglMultiTexCoord2f(GL_TEXTURE0, s, t);
			Sky_GetTexCoord(v, 16, &s, &t);
			qglMultiTexCoord2f(GL_TEXTURE1, s, t);
			glVertex3fv(v);
		}
		glEnd();

		GL_DisableMultitexture();
	}
	else
	{
		GL_Bind(solidskytexture);

		glBegin(GL_QUADS);
		for (i = 0, v = p->verts[0]; i<4; i++, v += VERTEXSIZE)
		{
			Sky_GetTexCoord(v, 8, &s, &t);
			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();

		glEnable(GL_BLEND);
		GL_Bind(alphaskytexture);

		glBegin(GL_QUADS);
		for (i = 0, v = p->verts[0]; i<4; i++, v += VERTEXSIZE)
		{
			Sky_GetTexCoord(v, 16, &s, &t);
			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();

		glDisable(GL_BLEND);
	}

	if (Fog_GetDensity() > 0 && skyfog > 0)
	{
		float	*c;

		c = Fog_GetColor();
		glEnable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		glColor4f(c[0], c[1], c[2], bound(0.0, skyfog, 1.0));

		glBegin(GL_QUADS);
		for (i = 0, v = p->verts[0]; i<4; i++, v += VERTEXSIZE)
			glVertex3fv(v);
		glEnd();

		glColor3f(1, 1, 1);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}
}

/*
=================
Sky_DrawFace
=================
*/
void Sky_DrawFace(int axis)
{
	glpoly_t	*p;
	vec3_t		verts[4];
	int			i, j, start;
	float		di, qi, dj, qj;
	vec3_t		vup, vright, temp, temp2;

	Sky_SetBoxVert(-1.0, -1.0, axis, verts[0]);
	Sky_SetBoxVert(-1.0, 1.0, axis, verts[1]);
	Sky_SetBoxVert(1.0, 1.0, axis, verts[2]);
	Sky_SetBoxVert(1.0, -1.0, axis, verts[3]);

	start = Hunk_LowMark();
	p = (glpoly_t *)Hunk_Alloc(sizeof(glpoly_t));

	VectorSubtract(verts[2], verts[3], vup);
	VectorSubtract(verts[2], verts[1], vright);

	di = 12;
	qi = 1.0 / di;
	dj = (axis < 4) ? di * 2 : di; //subdivide vertically more than horizontally on skybox sides
	qj = 1.0 / dj;

	for (i = 0; i<di; i++)
	{
		for (j = 0; j<dj; j++)
		{
			if (i*qi < skymins[0][axis] / 2 + 0.5 - qi || i*qi > skymaxs[0][axis] / 2 + 0.5 ||
				j*qj < skymins[1][axis] / 2 + 0.5 - qj || j*qj > skymaxs[1][axis] / 2 + 0.5)
				continue;

			//if (i&1 ^ j&1) continue; //checkerboard test
			VectorScale(vright, qi*i, temp);
			VectorScale(vup, qj*j, temp2);
			VectorAdd(temp, temp2, temp);
			VectorAdd(verts[0], temp, p->verts[0]);

			VectorScale(vup, qj, temp);
			VectorAdd(p->verts[0], temp, p->verts[1]);

			VectorScale(vright, qi, temp);
			VectorAdd(p->verts[1], temp, p->verts[2]);

			VectorAdd(p->verts[0], temp, p->verts[3]);

			Sky_DrawFaceQuad(p);
		}
	}
	Hunk_FreeToLowMark(start);
}

/*
==============
R_DrawSkyLayers

draws the old-style scrolling cloud layers
==============
*/
void R_DrawSkyLayers(void)
{
	int i;

	for (i = 0; i<6; i++)
		if (skymins[0][i] < skymaxs[0][i] && skymins[1][i] < skymaxs[1][i])
			Sky_DrawFace(i);
}

/*
==============
Sky_NewMap
==============
*/
void Sky_NewMap(void)
{
	char	key[128], value[4096], *data;

	// initially no sky
	Cvar_Set(&r_skybox, "");
	skyfog = r_skyfog.value;

	// read worldspawn (this is so ugly, and shouldn't it be done on the server?)
	data = cl.worldmodel->entities;
	if (!data)
		return; //FIXME: how could this possibly ever happen? -- if there's no
				// worldspawn then the sever wouldn't send the loadmap message to the client

	data = COM_Parse(data);
	if (!data) //should never happen
		return; // error
	if (com_token[0] != '{') //should never happen
		return; // error
	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key) - 1] == ' ') // remove trailing spaces
			key[strlen(key) - 1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);

		if (!strcmp("sky", key))
			R_SetSky(value);

		if (!strcmp("skyfog", key))
			skyfog = atof(value);
	}
}

/*
==============
R_DrawSky

called once per frame before drawing anything else
==============
*/
void R_DrawSky(void)
{
	byte	*col;
	
	// reset sky bounds
	R_ClearSkyBox();

	// process world and bmodels: draw flat-shaded sky surfs, and update skybounds
	Fog_DisableGFog();

	// r_fastsky drawing - this is drawn by default
	glDisable(GL_TEXTURE_2D);
	if (Fog_GetDensity() > 0)
		glColor3fv(Fog_GetColor());
	else
	{
		col = StringToRGB(r_skycolor.string);
		glColor3ubv(col);
	}

	Sky_ProcessTextureChains();
	Sky_ProcessEntities();

	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);

	// render slow sky: cloud layers or skybox
	if (!r_fastsky.value && !(Fog_GetDensity() > 0 && skyfog >= 1))
	{
		glDepthFunc(GL_GEQUAL);
		glDepthMask(GL_FALSE);

		if (r_skyboxloaded)
			R_DrawSkyBox();
		else
			R_DrawSkyLayers();

		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LEQUAL);
	}

	Fog_EnableGFog();
}
