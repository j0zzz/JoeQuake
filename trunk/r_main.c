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
// r_main.c

#include "quakedef.h"
#include "r_local.h"

//define	PASSAGES

void		*colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting = {128, 192, viewlightvec};
float		r_time1;
int		r_numallocatededges;
float		r_aliasuvscale = 1.0;
int		r_outofsurfaces;
int		r_outofedges;

qboolean	r_dowarp, r_dowarpold, r_viewchanged;

mvertex_t	*r_pcurrentvertbase;

int		c_surf;
int		r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int		r_clipflags;

byte		*r_warpbuffer;

byte		*r_stack_start;

entity_t	r_worldentity;

// view origin
vec3_t		vup, base_vup;
vec3_t		vpn, base_vpn;
vec3_t		vright, base_vright;
vec3_t		r_origin;

// screen size info
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		screenwidth;

float		pixelAspect;
float		screenAspect;
float		verticalFieldOfView;
float		xOrigin, yOrigin;

mplane_t	screenedge[4];

// refresh flags
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

#define		VIEWMODNAME_LENGTH	256
char		viewmodname[VIEWMODNAME_LENGTH+1];
int		modcount;

int		*pfrustum_indexes[4];
int		r_frustum_indexes[4*6];

int		reinit_surfcache = 1;	// if 1, surface cache is currently empty and
					// must be reinitialized for current cache size

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

float		r_aliastransition, r_resfudge;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

float		dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float		se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

void R_MarkLeaves (void);

cvar_t	r_draworder = {"r_draworder", "0"};
cvar_t	r_speeds = {"r_speeds", "0"};
cvar_t	r_timegraph = {"r_timegraph", "0"};
cvar_t	r_graphheight = {"r_graphheight", "10"};
cvar_t	r_clearcolor = {"r_clearcolor", "2"};
cvar_t	r_skycolor = {"r_skycolor", "4"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_waterwarp = {"r_waterwarp", "1"};
cvar_t	r_fullbright = {"r_fullbright", "0"};
cvar_t	r_drawentities = {"r_drawentities", "1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel", "1"};
cvar_t	r_aliasstats = {"r_polymodelstats", "0"};
cvar_t	r_dspeeds = {"r_dspeeds", "0"};
cvar_t	r_drawflat = {"r_drawflat", "0"};
cvar_t	r_ambient = {"r_ambient", "0"};
cvar_t	r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t	r_maxsurfs = {"r_maxsurfs", "0"};
cvar_t	r_numsurfs = {"r_numsurfs", "0"};
cvar_t	r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t	r_maxedges = {"r_maxedges", "0"};
cvar_t	r_numedges = {"r_numedges", "0"};
cvar_t	r_aliastransbase = {"r_aliastransbase", "200"};
cvar_t	r_aliastransadj = {"r_aliastransadj", "100"};
cvar_t	r_fullbrightskins = {"r_fullbrightskins", "0"};
cvar_t	r_drawflame = {"r_drawflame", "1"};

qboolean	d_15to8table_made = false;
byte		d_15to8table[65536];

void CreatePassages (void);
void SetVisibilityByPassages (void);

/*
==================
R_InitTextures
==================
*/
void R_InitTextures (void)
{
	int	x,y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2, "notexture");

	strcpy (r_notexture_mip->name, "notexture");
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x < (16>>m) ; x++)
				*dest++ = ((y < (8>>m)) ^ (x < (8>>m))) ? 0 : 0x0e;
	}	
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	int	dummy;

// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte *)&dummy;

	R_InitTurb ();

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);	
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);	

	Cvar_Register (&r_draworder);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_timegraph);
	Cvar_Register (&r_graphheight);
	Cvar_Register (&r_drawflat);
	Cvar_Register (&r_ambient);
	Cvar_Register (&r_clearcolor);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_waterwarp);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawviewmodel);
	Cvar_Register (&r_aliasstats);
	Cvar_Register (&r_dspeeds);
	Cvar_Register (&r_reportsurfout);
	Cvar_Register (&r_maxsurfs);
	Cvar_Register (&r_numsurfs);
	Cvar_Register (&r_reportedgeout);
	Cvar_Register (&r_maxedges);
	Cvar_Register (&r_numedges);
	Cvar_Register (&r_aliastransbase);
	Cvar_Register (&r_aliastransadj);
	Cvar_Register (&r_fullbrightskins);
	Cvar_Register (&r_drawflame);

	Cvar_SetValue (&r_maxedges, (float)NUMSTACKEDGES);
	Cvar_SetValue (&r_maxsurfs, (float)NUMSTACKSURFACES);

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge = view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge = view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	R_InitTextures ();
	R_InitParticles ();

// TODO: collect 386-specific code in one place
#if id386
	Sys_MakeCodeWriteable ((long)R_EdgeCodeStart, (long)R_EdgeCodeEnd - (long)R_EdgeCodeStart);
#endif

	D_Init ();
}

void R_PreMapLoad (char *mapname)
{
	Cvar_ForceSet (&cl_mapname, mapname);
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int	i;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;
	R_ClearParticles ();

	r_cnumsurfs = r_maxsurfs.value;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES)
	{
		surfaces = Hunk_AllocName (r_cnumsurfs * sizeof(surf_t), "surfaces");
		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
	// surface 0 doesn't really exist; it's just a dummy because index 0
	// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}
	else
	{
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = r_maxedges.value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES)
		auxedges = NULL;
	else
		auxedges = Hunk_AllocName (r_numallocatededges * sizeof(edge_t), "edges");

	r_dowarpold = false;
	r_viewchanged = false;

#ifdef PASSAGES
	CreatePassages ();
#endif
}

/*
===============
R_SetVrect
===============
*/
void R_SetVrect (vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
	int		h;
	float		size;
	qboolean	full = false;

	if (scr_viewsize.value >= 100.0)
	{
		size = 100.0;
		full = true;
	}
	else
	{
		size = scr_viewsize.value;
	}

	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		lineadj = 0;
	}
	size /= 100.0;

	h = (!cl_sbar.value && full) ? pvrectin->height : pvrectin->height - lineadj;

	pvrect->width = full ? pvrectin->width : pvrectin->width * size;

	if (pvrect->width < 96)
	{
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;	// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;

	if (cl_sbar.value || !full)
	{
		if (pvrect->height > pvrectin->height - lineadj)
			pvrect->height = pvrectin->height - lineadj;
	}
	else if (pvrect->height > pvrectin->height)
	{
		pvrect->height = pvrectin->height;
	}

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width) / 2;
	pvrect->y = full ? 0 : (h - pvrect->height) / 2;

	if (lcd_x.value)
	{
		pvrect->y >>= 1;
		pvrect->height >>= 1;
	}
	else if (lcd_yaw.value)		// for testing split screen
	{
		pvrect->x >>= 1;
		pvrect->width >>= 1;
	}
}

/*
===============
R_ViewChanged

Called every time the vid structure or r_refdef changes.
Guaranteed to be called before the first refresh
===============
*/
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect)
{
	int		i;
	float	res_scale;

	r_viewchanged = true;

	R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

	r_refdef.horizontalFieldOfView = 2.0 * tan(r_refdef.fov_x / 360 * M_PI);
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x << 20) + (1 << 19) - 1;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright << 20) + (1 << 19) - 1;
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float)r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x + r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y + r_refdef.aliasvrect.height;

	pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;
	
	screenAspect = r_refdef.vrect.width*pixelAspect / r_refdef.vrect.height;
// 320*200 1.0 pixelAspect = 1.6 screenAspect
// 320*240 1.0 pixelAspect = 1.3333 screenAspect
// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

// values for perspective projection
// if math were exact, the values would range from 0.5 to to range+0.5
// hopefully they wll be in the 0.000001 to range+.999999 and truncate
// the polygon rasterization will never render in the first row or column
// but will definately render in the [range] row and column, so adjust the
// buffer origin to get an exact edge to edge fill
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) + r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) + r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink * pixelAspect;

// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin * r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;
	
// right side clip
	screenedge[1].normal[0] = 1.0 / ((1.0-xOrigin) * r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;
	
// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin*verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;
	
// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0-yOrigin)*verticalFieldOfView);
	screenedge[3].normal[2] = 1;	
	screenedge[3].type = PLANE_ANYZ;
	
	for (i=0 ; i<4 ; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double)(r_refdef.vrect.width * r_refdef.vrect.height) / (320.0 * 152.0)) * (2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

// TODO: collect 386-specific code in one place
#if id386
	Sys_MakeCodeWriteable ((long)R_Surf8Start, (long)R_Surf8End - (long)R_Surf8Start);
	colormap = vid.colormap;
	R_Surf8Patch ();
#endif	// id386

	D_ViewChanged ();
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int	i;

	if (r_oldviewleaf == r_viewleaf)
		return;
	
	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
		
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1 << (i & 7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
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
	int		i, j, lnum;
	alight_t	lighting;
// FIXME: remove and do real lighting
	float		lightvec[3] = {-1, 0, 0};
	vec3_t		dist;
	float		add;

	if (!r_drawentities.value)
		return;

	for (i = 0 ; i < cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			VectorCopy (currententity->origin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);
			R_DrawSprite ();
			break;

		case mod_alias:
			VectorCopy (currententity->origin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);

		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
			if (R_AliasCheckBBox())
			{
				j = R_LightPoint (currententity->origin);
	
				lighting.ambientlight = j;
				lighting.shadelight = j;

				lighting.plightvec = lightvec;

				for (lnum = 0 ; lnum < MAX_DLIGHTS ; lnum++)
				{
					if (cl_dlights[lnum].die >= cl.time)
					{
						VectorSubtract (currententity->origin, cl_dlights[lnum].origin, dist);
						add = cl_dlights[lnum].radius - VectorLength (dist);

						if (add > 0)
							lighting.ambientlight += add;
					}
				}
	
			// clamp lighting so it doesn't overbright as much
				if (lighting.ambientlight > 128)
					lighting.ambientlight = 128;
				if (lighting.ambientlight + lighting.shadelight > 192)
					lighting.shadelight = 192 - lighting.ambientlight;

				if (r_fullbrightskins.value && currententity->model->modhint == MOD_PLAYER)
				{
					lighting.ambientlight = max(lighting.ambientlight, 100);
					lighting.shadelight = max(lighting.shadelight, 100);
				}

				R_AliasDrawModel (&lighting);
			}

			break;

		default:
			break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
// FIXME: remove and do real lighting
	int			j, lnum;
	float		add, lightvec[3] = {-1, 0, 0};
	vec3_t		dist;
	dlight_t	*dl;

	currententity = &cl.viewent;

	if (!r_drawviewmodel.value || cl.freefly_enabled || cl_thirdperson.value || !r_drawentities.value || 
	    (cl.items & IT_INVISIBILITY) || (cl.stats[STAT_HEALTH] <= 0) || !currententity->model)
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	VectorNegate (vup, viewlightvec);

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// always give some light on gun
	r_viewlighting.ambientlight = j;
	r_viewlighting.shadelight = j;

// add dynamic lights		
	for (lnum = 0 ; lnum < MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - VectorLength (dist);
		if (add > 0)
			r_viewlighting.ambientlight += add;
	}

// clamp lighting so it doesn't overbright as much
	if (r_viewlighting.ambientlight > 128)
		r_viewlighting.ambientlight = 128;
	if (r_viewlighting.ambientlight + r_viewlighting.shadelight > 192)
		r_viewlighting.shadelight = 192 - r_viewlighting.ambientlight;

	r_viewlighting.plightvec = lightvec;

	R_AliasDrawModel (&r_viewlighting);
}

/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox (model_t *clmodel, float *minmaxs)
{
	int	i, *pindex, clipflags;
	vec3_t	acceptpt, rejectpt;
	double	d;

	clipflags = 0;

	if (currententity->angles[0] || currententity->angles[1] || currententity->angles[2])
	{
		for (i=0 ; i<4 ; i++)
		{
			d = DotProduct(currententity->origin, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	}
	else
	{
		for (i=0 ; i<4 ; i++)
		{
		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];
			
			d = DotProduct(rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3+0]];
			acceptpt[1] = minmaxs[pindex[3+1]];
			acceptpt[2] = minmaxs[pindex[3+2]];

			d = DotProduct(acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}

/*
===================
R_FindTopNode
===================
*/
mnode_t *R_FindTopNode (vec3_t mins, vec3_t maxs)
{
	mplane_t	*splitplane;
	int		sides;
	mnode_t		*node;

	node = cl.worldmodel->nodes;

	while (1)
	{
		if (node->visframe != r_visframecount)
			return NULL;		// not visible at all
		
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
				return node;	// we've reached a non-solid leaf, so it's
						//  visible and not BSP clipped
			return NULL;	// in solid, so not visible
		}
		
		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE (mins, maxs, splitplane);
		
		if (sides == 3)
			return node;	// this is the splitter
		
		// not split yet; recurse down the contacted side
		node = (sides & 1) ? node->children[0] : node->children[1];
	}
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList (void)
{
	int			i, k, clipflags;
	float		minmaxs[6];
	vec3_t		oldorigin;
	model_t		*clmodel;
	mnode_t		*topnode;

	if (!r_drawentities.value)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_dlightframecount = r_framecount;

	for (i = 0 ; i < cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];
		clmodel = currententity->model;

		if (clmodel->type != mod_brush)
			continue;

	// see if the bounding box lets us trivially reject, also sets
	// trivial accept status
		VectorAdd (clmodel->mins, currententity->origin, minmaxs);
		VectorAdd (clmodel->maxs, currententity->origin, (minmaxs + 3));

		clipflags = R_BmodelCheckBBox (clmodel, minmaxs);

		if (clipflags == BMODEL_FULLY_CLIPPED)
			continue;		// off the edge of the screen

		if (!(topnode = R_FindTopNode(minmaxs, minmaxs + 3)))
			continue;	// no part in a visible leaf

		VectorCopy (currententity->origin, r_entorigin);
		VectorSubtract (r_origin, r_entorigin, modelorg);

		r_pcurrentvertbase = clmodel->vertexes;

	// FIXME: stop transforming twice
		R_RotateBmodel ();

	// calculate dynamic lighting for bmodel if it's not an instanced model
		if (clmodel->firstmodelsurface != 0)
		{
			for (k = 0 ; k < MAX_DLIGHTS ; k++)
			{
				if (cl_dlights[k].die < cl.time || !cl_dlights[k].radius)
					continue;

				R_MarkLights (&cl_dlights[k], k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
			}
		}

		currententity->topnode = topnode;

		if (topnode->contents >= 0)
		{
		// not a leaf; has to be clipped to the world BSP
			r_clipflags = clipflags;
			R_DrawSolidClippedSubmodelPolygons (clmodel);
		}
		else
		{
		// falls entirely in one leaf, so we just put all the
		// edges in the edge list and let 1/z sorting handle
		// drawing order
			R_DrawSubmodelPolygons (clmodel, clipflags);
		}

		currententity->topnode = NULL;

	// put back world rotation and frustum clipping		
	// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		VectorCopy (base_modelorg, modelorg);
		VectorCopy (oldorigin, modelorg);
		R_TransformFrustum ();
	}

	insubmodel = false;
}

/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
	edge_t	ledges[NUMSTACKEDGES + ((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
	surf_t	lsurfs[NUMSTACKSURFACES + ((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

	if (auxedges)
		r_edges = auxedges;
	else
		r_edges = (edge_t *)(((long)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));

	if (r_surfsonstack)
	{
		surfaces = (surf_t *)(((long)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
	// surface 0 doesn't really exist; it's just a dummy because index 0
	// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds.value)
		rw_time1 = Sys_DoubleTime ();

	R_RenderWorld ();

	if (r_dspeeds.value)
	{
		rw_time2 = Sys_DoubleTime ();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds.value)
	{
		db_time2 = Sys_DoubleTime ();
		se_time1 = db_time2;
	}

	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}
	
	R_ScanEdges ();
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView_ (void)
{
	byte	warpbuffer[WARP_WIDTH*WARP_HEIGHT];

	r_warpbuffer = warpbuffer;

	if (r_timegraph.value || r_speeds.value || r_dspeeds.value)
		r_time1 = Sys_DoubleTime ();

	R_SetupFrame ();

#ifdef PASSAGES
	SetVisibilityByPassages ();
#else
	R_MarkLeaves ();	// done here so we know if we're in water
#endif

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	Sys_LowFPPrecision ();

	if (!cl_entities[0].model || !r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");
		
	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}
	
	R_EdgeDrawing ();

	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}
	
	if (r_dspeeds.value)
	{
		se_time2 = Sys_DoubleTime ();
		de_time1 = se_time2;
	}

	R_DrawEntitiesOnList ();

	if (r_dspeeds.value)
	{
		de_time2 = Sys_DoubleTime ();
		dv_time1 = de_time2;
	}

	R_DrawViewModel ();

	if (r_dspeeds.value)
	{
		dv_time2 = Sys_DoubleTime ();
		dp_time1 = Sys_DoubleTime ();
	}

	R_DrawParticles ();

	if (r_dspeeds.value)
		dp_time2 = Sys_DoubleTime ();

	if (r_dowarp)
		D_WarpScreen ();

	V_SetContentsColor (r_viewleaf->contents);

	if (r_timegraph.value)
		R_TimeGraph ();

	if (r_aliasstats.value)
		R_PrintAliasStats ();
		
	if (r_speeds.value)
		R_PrintTimes ();

	if (r_dspeeds.value)
		R_PrintDSpeeds ();

	if (r_reportsurfout.value && r_outofsurfaces)
		Con_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout.value && r_outofedges)
		Con_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);

// back to high floating-point precision
	Sys_HighFPPrecision ();
}

void R_RenderView (void)
{
	int	dummy, delta;
	
	delta = (byte *)&dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if (Hunk_LowMark() & 3)
		Sys_Error ("Hunk is missaligned");

	if ((long)(&dummy) & 3)
		Sys_Error ("Stack is missaligned");

	if ((long)(&r_warpbuffer) & 3)
		Sys_Error ("Globals are missaligned");

	R_RenderView_ ();
}

/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
	int	i;
	
	for (i=0 ; i<MAXWIDTH+CYCLE ; i++)
	{
		sintable[i] = AMP + sin(i * M_PI * 2 / CYCLE) * AMP;
		intsintable[i] = AMP2 + sin(i * M_PI * 2 / CYCLE) * AMP2;	// AMP2, not 20
	}
}
