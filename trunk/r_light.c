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
// r_light.c

#include "quakedef.h"
#include "r_local.h"

int	r_dlightframecount;

/*
==================
R_AnimateLight
==================
*/
#if 0
void R_AnimateLight (void)
{
	int	i, j, k;

// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(cl.time * 10);
	for (j = 0 ; j < MAX_LIGHTSTYLES ; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}

		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}	
}
#endif

/*
==================
R_AnimateLight

Added lightstyle interpolation
==================
*/
void R_AnimateLight (void)
{
	int		j, k, flight, clight;
	float	l, lerpfrac, backlerp;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	flight = (int)floor(cl.time * 10);
	clight = (int)ceil(cl.time * 10);
	lerpfrac = (cl.time * 10) - flight;
	backlerp = 1.0f - lerpfrac;

	for (j = 0 ; j < MAX_LIGHTSTYLES ; j++)
	{
		if (!cl_lightstyle[j].length)
		{	// was 256, changed to 264 for consistency
			d_lightstylevalue[j] = 264;
			continue;
		}
		else if (cl_lightstyle[j].length == 1)
		{	// single length style so don't bother interpolating
			d_lightstylevalue[j] = 22 * (cl_lightstyle[j].map[0] - 'a');
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = flight % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		l = (float)(k * 22) * backlerp;

		// upcoming frame
		k = clight % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		l += (float)(k * 22) * lerpfrac;

		d_lightstylevalue[j] = (int)l;
	}
} 

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int lnum, mnode_t *node)
{
	int			i, j;
	float		dist;
	mplane_t	*splitplane;
	msurface_t	*surf;
	
loc0:
	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = PlaneDiff(light->origin, splitplane);
	
	if (dist > light->radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (dist < -light->radius)
	{
		node = node->children[1];
		goto loc0;
	}
		
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i = 0 ; i < node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			for (j = 0 ; j < MAX_DLIGHTS ; j += 8)
				surf->dlightbits[(j >> 3)] = 0;

			surf->dlightframe = r_dlightframecount;
		}

		surf->dlightbits[(lnum >> 3)] |= (1 << (lnum & 7));
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_MarkLights (light, lnum, node->children[0]);
			node = node->children[1];
			goto loc0;
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}
	else if (node->children[1]->contents >= 0)
	{
		node = node->children[1];
		goto loc0;
	}
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int			i;
	dlight_t	*l;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame
	l = cl_dlights;

	for (i = 0 ; i < MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;

		R_MarkLights (l, i, cl.worldmodel->nodes);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	int		r, side, s, t, ds, dt, i, maps;
	float		front, back, frac;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	byte		*lightmap;
	unsigned	scale;

	if (node->contents < 0)
		return -1;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	if (plane->type < 3)
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct(start, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;
	}
	side = front < 0;
	
	if ((back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;
	
// go down front side	
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ((back < 0) == side)
		return -1;		// didn't hit anuthing
		
// check for impact on this node

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += dt * ((surf->extents[0]>>4)+1) + ds;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1);
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

int R_LightPoint (vec3_t p)
{
	vec3_t		end;
	int		r;
	
	if (!cl.worldmodel->lightdata)
		return 255;
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	
	r = RecursiveLightPoint (cl.worldmodel->nodes, p, end);
	
	if (r == -1)
		r = 0;

	if (r < r_refdef.ambientlight)
		r = r_refdef.ambientlight;

	return r;
}
