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
// gl_model.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadAliasModel (model_t *mod, void *buffer);
void Mod_LoadQ3Model (model_t *mod, void *buffer);
void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int	mod_numknown;

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};

qboolean OnChange_gl_picmip (cvar_t *var, char *string)
{
	int		i;
	float	newval;
	model_t	*mod;

	newval = Q_atof (string);
	if (newval == gl_picmip.value)
		return false;

	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (mod->type == mod_alias || mod->type == mod_md3)
			mod->needload = true;
	}

	return false;
}

qboolean OnChange_gl_part_muzzleflash (cvar_t *var, char *string)
{
	int		i;
	float	newval;
	model_t	*mod;

	newval = Q_atof (string);
	if (newval == gl_part_muzzleflash.value)
		return false;

	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
	{
		if ((mod->type == mod_alias || mod->type == mod_md3) && mod->modhint == MOD_WEAPON)
			mod->needload = true;
	}

	return false;
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	Cvar_Register (&gl_subdivide_size);
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	if ((r = Cache_Check(&mod->cache)))
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");

	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	float		d;
	mnode_t		*node;
	mplane_t	*plane;

	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = PlaneDiff(p, plane);
		node = (d > 0) ? node->children[0] : node->children[1];
	}

	return NULL;	// never reached
}

/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	int		c, row;
	byte	*out;
	static byte	decompressed[MAX_MAP_LEAFS/8];

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

#if 0
	memcpy (out, in, row);
#else
	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
#endif
	
	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;

	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (mod->type != mod_alias && mod->type != mod_md3)
			mod->needload = true;
		else if (mod->needload && Cache_Check(&mod->cache))
			Cache_Free (&mod->cache);
	}
}

/*
==================
Mod_FindName
==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;

	if (!name[0])
		Sys_Error ("Mod_FindName: NULL name");

// search the currently loaded models
	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
		if (!strcmp(mod->name, name))
			break;

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
		Q_strncpyz (mod->name, name, sizeof(mod->name));
		mod->needload = true;
	}

	return mod;
}

/*
==================
Mod_TouchModel
==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	if (!mod->needload && (mod->type == mod_alias || mod->type == mod_md3))
		Cache_Check (&mod->cache);
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	unsigned	*buf;
	byte		stackbuf[1024];		// avoid dirtying the cache heap

	if (!mod->needload)
	{
		if (mod->type == mod_alias || mod->type == mod_md3)
		{
			if (Cache_Check(&mod->cache))
				return mod;
		}
		else
		{
			return mod;		// not cached at all
		}
	}

// because the world is so huge, load it one piece at a time

// load the file
	if (!(buf = (unsigned *)COM_LoadStackFile(mod->name, stackbuf, sizeof(stackbuf))))
	{
		if (crash)
			Sys_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

// allocate a new model
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

// fill it in

// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*(unsigned *)buf))
	{
		case IDPOLYHEADER:
			Mod_LoadAliasModel (mod, buf);
			break;

		case IDMD3HEADER:
			Mod_LoadQ3Model (mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel (mod, buf);
			break;

		default:
			Mod_LoadBrushModel (mod, buf);
			break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}

qboolean Img_HasFullbrights (byte *pixels, int size)
{
	int	i;

	for (i = 0 ; i < size ; i++)
		if (pixels[i] >= 224)
			return true;

	return false;
}

/*
===============================================================================

								BRUSH MODELS

===============================================================================
*/

#define	ISTURBTEX(name)	((name)[0] == '*')
#define	ISSKYTEX(name)	((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

byte	*mod_base;

/*
=================
Mod_LoadBrushModelTexture
=================
*/
int Mod_LoadBrushModelTexture (texture_t *tx, int flags)
{
	char	*name, *mapname;

	if (loadmodel->isworldmodel)
	{
		if (!gl_externaltextures_world.value)
			return 0;
	}
	else
	{
		if (!gl_externaltextures_bmodels.value)
			return 0;
	}

	name = tx->name;
	mapname = CL_MapName ();

	if (loadmodel->isworldmodel)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/%s/%s", mapname, name), name, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", mapname, name), va("@fb_%s", name), 0, 0, flags | TEX_LUMA);
		}
	}
	else
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/bmodels/%s", name), name, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s_luma", name), va("@fb_%s", name), 0, 0, flags | TEX_LUMA);
		}
	}

	if (!tx->gl_texturenum)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/%s", name), name, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s_luma", name), va("@fb_%s", name), 0, 0, flags | TEX_LUMA);
		}
	}

	if (tx->fb_texturenum)
		tx->isLumaTexture = true;

	return tx->gl_texturenum;
}

/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int			i, j, num, max, altmax, texture_flag, brighten_flag;
	miptex_t	*mt;
	texture_t	*tx, *tx2, *txblock, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	byte		*data;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures), loadname);

	txblock = Hunk_AllocName (m->nummiptex * sizeof(**loadmodel->textures), loadname);

	brighten_flag = (lightmode == 2) ? TEX_BRIGHTEN : 0;
	texture_flag = TEX_MIPMAP;

	for (i = 0 ; i < m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong (m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		loadmodel->textures[i] = tx = txblock + i;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		if (!tx->name[0])
		{
			Q_snprintfz (tx->name, sizeof(tx->name), "unnamed%d", i);
			Con_DPrintf ("Warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		tx->width = mt->width = LittleLong (mt->width);
		tx->height = mt->height = LittleLong (mt->height);
		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error ("Mod_LoadTextures: Texture %s is not 16 aligned", mt->name);

		for (j = 0 ; j < MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32 && 
		    CRC_Block((byte *)(mt + 1), mt->width * mt->height) == 65393)
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy (mt + 1, (byte *)(mt + 1) + 32*31, 32);
		}

		if (loadmodel->isworldmodel && ISSKYTEX(tx->name))
		{
			R_InitSky (mt);
			continue;
		}

		if (Mod_LoadBrushModelTexture(tx, texture_flag))
			continue;

		if (mt->offsets[0])
		{
			data = (byte *)mt + mt->offsets[0];
			tx2 = tx;
		}
		else
		{
			data = (byte *)tx2 + tx2->offsets[0];
			tx2 = r_notexture_mip;
		}

		tx->gl_texturenum = GL_LoadTexture (tx2->name, tx2->width, tx2->height, data, texture_flag | brighten_flag, 1);
		if (!ISTURBTEX(tx->name) && Img_HasFullbrights(data, tx2->width * tx2->height))
			tx->fb_texturenum = GL_LoadTexture (va("@fb_%s", tx2->name), tx2->width, tx2->height, data, texture_flag | TEX_FULLBRIGHT, 1);
	}

// sequence the animations
	for (i = 0 ; i < m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
		{
			Sys_Error ("Bad animating texture %s", tx->name);
		}

		for (j = i + 1 ; j < m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp(tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num+1;
			}
			else
			{
				Sys_Error ("Bad animating texture %s", tx->name);
			}
		}

#define	ANIM_CYCLE	2
	// link them all together
		for (j = 0 ; j < max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j = 0 ; j < altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

// joe: from FuhQuake
static byte *LoadColoredLighting (char *name, char **litfilename)
{
	byte	*data;
	char	*tmpname;

	if (!gl_loadlitfiles.value)
		return NULL;

	tmpname = CL_MapName ();

	if (strcmp(name, va("maps/%s.bsp", tmpname)))
		return NULL;

	*litfilename = va("maps/%s.lit", tmpname);
	data = COM_LoadHunkFile (*litfilename);

	if (!data)
	{
		*litfilename = va("lits/%s.lit", tmpname);
		data = COM_LoadHunkFile (*litfilename);
	}

	return data;
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	int		i, lit_ver, mark;
	byte	*in, *out, *data, d;
	char	*litfilename;

	loadmodel->lightdata = NULL;
	if (!l->filelen)
		return;

	// check for a .lit file
	mark = Hunk_LowMark ();
	data = LoadColoredLighting (loadmodel->name, &litfilename);
	if (data)
	{
		if (com_filesize < 8 || strncmp(data, "QLIT", 4))
			Con_Printf ("Corrupt .lit file (%s)...ignoring\n", COM_SkipPath(litfilename));
		else if (l->filelen * 3 + 8 != com_filesize)
			Con_Printf ("Warning: .lit file (%s) has incorrect size\n", COM_SkipPath(litfilename));
		else if ((lit_ver = LittleLong(((int *)data)[1])) != 1)
			Con_Printf ("Unknown .lit file version (v%d)\n", lit_ver);
		else
		{
			Con_DPrintf ("Static coloured lighting loaded\n");

			loadmodel->lightdata = data + 8;
			in = mod_base + l->fileofs;
			out = loadmodel->lightdata;
			for (i=0 ; i<l->filelen ; i++)
			{
				int	b = max(out[3*i], max(out[3*i+1], out[3*i+2]));

				if (!b)
					out[3*i] = out[3*i+1] = out[3*i+2] = in[i];
				else
				{	// too bright
					float	r = in[i] / (float)b;

					out[3*i+0] = (int)(r * out[3*i+0]);
					out[3*i+1] = (int)(r * out[3*i+1]);
					out[3*i+2] = (int)(r * out[3*i+2]);
				}
			}
			return;
		}
		Hunk_FreeToLowMark (mark);
	}

	// no .lit found, expand the white lighting data to color
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = Hunk_AllocName (l->filelen * 3, va("%s_@lightdata", loadmodel->name));
	// place the file at the end, so it will not be overwritten until the very last write
	in = loadmodel->lightdata + l->filelen * 2;
	out = loadmodel->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0 ; i < l->filelen ; i++, out += 3)
	{
		d = *in++;
		out[0] = out[1] = out[2] = d;
	}
}

/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName (l->filelen, loadname);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName (l->filelen, loadname);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int		i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in, *out;
	int		i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)	
		Sys_Error ("Mod_LoadSubmodels: count > MAX_MODELS");

	out = Hunk_AllocName (count * sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t	*in;
	medge_t	*out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ((count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort (in->v[0]);
		out->v[1] = (unsigned short)LittleShort (in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t	*in;
	mtexinfo_t	*out;
	int 		i, j, count, miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("Mod_LoadTexinfo: miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip;	// texture not found
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float		mins[2], maxs[2], val;
	int		i, j, e, bmins[2], bmaxs[2];
	mvertex_t	*v;
	mtexinfo_t	*tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + v->position[1] * tex->vecs[j][1] + v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = floor (mins[i] / 16);
		bmaxs[i] = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Host_Error ("CalcSurfaceExtents: Bad surface extents");
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int		i, count, surfnum, planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0 ; surfnum < count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		if ((side = LittleShort(in->side)))
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);

	// lighting info
		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		out->samples = (i == -1) ? NULL : loadmodel->lightdata + i * 3;

	// set the drawing flags flag
		if (ISSKYTEX(out->texinfo->texture->name))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}
		
		if (ISTURBTEX(out->texinfo->texture->name))	// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}
	}
}

/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int		i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		
		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort (in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);
		
		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;
		
		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}	
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t	*in, *out;
	int		i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);	

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	dclipnode_t	*out;
	int		i, j, count;
	hull_t		*hull;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName (count * sizeof(*out), loadname);	

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t	**out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int	i, count, *in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * sizeof(*out), loadname);	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j, count, bits;
	mplane_t	*out;
	dplane_t 	*in;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName (count * 2 * sizeof(*out), loadname);	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0 ; i < count ; i++, in++, out++)
	{
		bits = 0;
		for (j = 0 ; j < 3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i = 0 ; i < 3 ; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength(corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		Sys_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	loadmodel->isworldmodel = !strcmp (loadmodel->name, va("maps/%s.bsp", cl_mapname.string));

// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0 ; i < sizeof(dheader_t) / 4 ; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation

// set up the submodels (FIXME: this is confusing)
	for (i = 0 ; i < mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1 ; j < MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

								ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes. a frame may be an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

byte		aliasbboxmins[3], aliasbboxmaxs[3];

/*
=================
Mod_LoadAliasFrame
=================
*/
void *Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame)
{
	int			i;
	trivertx_t	*pinframe;
	daliasframe_t *pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i = 0 ; i < 3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min(aliasbboxmins[i], frame->bboxmin.v[i]);
		aliasbboxmaxs[i] = max(aliasbboxmaxs[i], frame->bboxmax.v[i]);
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (void *pin, maliasframedesc_t *frame)
{
	int			i, numframes;
	void		*ptemp;
	daliasgroup_t *pingroup;
	daliasinterval_t *pin_intervals;

	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i = 0 ; i < 3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min(aliasbboxmins[i], frame->bboxmin.v[i]);
		aliasbboxmaxs[i] = max(aliasbboxmaxs[i], frame->bboxmax.v[i]);
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i = 0 ; i < numframes ; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

typedef struct
{
	short	x, y;
} floodfill_t;

// must be a power of 2
#define	FLOODFILL_FIFO_SIZE	0x1000
#define	FLOODFILL_FIFO_MASK	(FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy)	\
{									\
	if (pos[off] == fillcolor)		\
	{								\
		pos[off] = 255;				\
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy);\
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;\
	}								\
	else if (pos[off] != 255) fdc = pos[off];\
}

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/
void Mod_FloodFillSkin (byte *skin, int skinwidth, int skinheight)
{
	int			i, inpt = 0, outpt = 0, filledcolor = -1;
	byte		fillcolor = *skin;	// assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0 ; i < 256 ; ++i)
		{
			if (d_8to24table[i] == (255 << 0))	// alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
		return;

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int		x = fifo[outpt].x, y = fifo[outpt].y;
		int		fdc = filledcolor;
		byte	*pos = &skin[x+skinwidth*y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
			FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)
			FLOODFILL_STEP(1, 1, 0);
		if (y > 0)
			FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)
			FLOODFILL_STEP(skinwidth, 0, 1);
		skin[x+skinwidth*y] = fdc;
	}
}

/*
=================
Mod_LoadAliasModelTexture
=================
*/
void Mod_LoadAliasModelTexture (char *identifier, int flags, int *gl_texnum, int *fb_texnum)
{
	char	loadpath[64];

	if (!gl_externaltextures_models.value)
		return;

	Q_snprintfz (loadpath, sizeof(loadpath), "textures/models/%s", identifier);
	*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
	if (*gl_texnum)
		*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);

	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "textures/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
		if (*gl_texnum)
			*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);
	}
}

int	player_32bit_skins[14] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
qboolean player_32bit_skins_loaded = false;

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int			i, j, k, size, groupskins, gl_texnum, fb_texnum, texture_flag;
	char		basename[64], identifier[64];
	byte		*skin, *texels;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);

	size = pheader->skinwidth * pheader->skinheight;

	COM_StripExtension (COM_SkipPath(loadmodel->name), basename);

	texture_flag = TEX_MIPMAP;

	for (i = 0 ; i < numskins ; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

			if (Mod_IsAnyKindOfPlayerModel(loadmodel))
			{
				// save 8 bit texels for the player model to remap
				texels = Hunk_AllocName (size, loadname);
				pheader->texels[i] = texels - (byte *)pheader;
				memcpy (texels, (byte *)(pskintype + 1), size);

				// try to load all 14 colorful player suits if loading the original or view weapon player model
				if (loadmodel->modhint == MOD_PLAYER)
				{
					int c;
					qboolean failed = false;

					for (c = 0; c < 14; c++)
					{
						Mod_LoadAliasModelTexture (va("player_skin_%i", c), texture_flag, &player_32bit_skins[c], &fb_texnum);
						if (!player_32bit_skins[c])
						{
							failed = true;
							break;
						}
					}

					player_32bit_skins_loaded = !failed;
				}
			}

			Q_snprintfz (identifier, sizeof(identifier), "%s_%i", basename, i);

			gl_texnum = fb_texnum = 0;
			Mod_LoadAliasModelTexture (identifier, texture_flag, &gl_texnum, &fb_texnum);
			if (fb_texnum)
				pheader->islumaskin[i][0] = pheader->islumaskin[i][1] =
				pheader->islumaskin[i][2] = pheader->islumaskin[i][3] = true;

			if (!gl_texnum)
			{
				gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texture_flag, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1),	pheader->skinwidth * pheader->skinheight))
					fb_texnum = GL_LoadTexture (va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texture_flag | TEX_FULLBRIGHT, 1);
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] =
			pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;

			pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
			pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = fb_texnum;

			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + size);
		}
		else 
		{
			// animating skin group. yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			texels = Hunk_AllocName (size, loadname);
			pheader->texels[i] = texels - (byte *)pheader;
			memcpy (texels, (byte *)pskintype, size);

			for (j = 0 ; j < groupskins ; j++)
			{
				Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

				Q_snprintfz (identifier, sizeof(identifier), "%s_%i_%i", basename, i, j);

				gl_texnum = fb_texnum = 0;
				Mod_LoadAliasModelTexture (identifier, texture_flag, &gl_texnum, &fb_texnum);
				if (fb_texnum)
					pheader->islumaskin[i][j&3] = true;

				if (!gl_texnum)
				{
					gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth, pheader->skinheight, (byte *)pskintype, texture_flag, 1);

					if (Img_HasFullbrights((byte *)pskintype, pheader->skinwidth * pheader->skinheight))
						fb_texnum = GL_LoadTexture (va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)pskintype, texture_flag | TEX_FULLBRIGHT, 1);
				}

				pheader->gl_texturenum[i][j&3] = gl_texnum;
				pheader->fb_texturenum[i][j&3] = fb_texnum;

				pskintype = (daliasskintype_t *)((byte *)pskintype + size);
			}

			for (k = j ; j < 4 ; j++)
				pheader->gl_texturenum[i][j&3] = pheader->gl_texturenum[i][j-k];
		}
	}

	return (void *)pskintype;
}

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int			i, j, version, numframes, size, start, end, total;
	mdl_t		*pinmodel;
	stvert_t	*pinstverts;
	dtriangle_t	*pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;

// some models are special
	if (!strcmp(mod->name, "progs/player.mdl") || !strcmp(mod->name, "progs/vwplayer.mdl"))
		mod->modhint = MOD_PLAYER;
	// player*.mdl models are DME models
	else if (!strncmp(mod->name, "progs/player", 12))
		mod->modhint = MOD_PLAYER_DME;
	else if (!strcmp(mod->name, "progs/eyes.mdl"))
		mod->modhint = MOD_EYES;
	else if (!strcmp(mod->name, "progs/flame0.mdl") ||
		 !strcmp(mod->name, "progs/flame.mdl") ||
		 !strcmp(mod->name, "progs/flame2.mdl"))
		mod->modhint = MOD_FLAME;
	else if (!strcmp(mod->name, "progs/bolt.mdl") ||
		 !strcmp(mod->name, "progs/bolt2.mdl") ||
		 !strcmp(mod->name, "progs/bolt3.mdl"))
		mod->modhint = MOD_THUNDERBOLT;
	else if (!strcmp(mod->name, "progs/v_shot.mdl") ||
		 !strcmp(mod->name, "progs/v_shot2.mdl") ||
		 !strcmp(mod->name, "progs/v_nail.mdl") ||
		 !strcmp(mod->name, "progs/v_nail2.mdl") ||
		 !strcmp(mod->name, "progs/v_rock.mdl") ||
		 !strcmp(mod->name, "progs/v_rock2.mdl") ||
	// hipnotic weapons
		 !strcmp(mod->name, "progs/v_laserg.mdl") ||
		 !strcmp(mod->name, "progs/v_prox.mdl") ||
	// rogue weapons
		 !strcmp(mod->name, "progs/v_grpple.mdl") ||	// ?
		 !strcmp(mod->name, "progs/v_lava.mdl") ||
		 !strcmp(mod->name, "progs/v_lava2.mdl") ||
		 !strcmp(mod->name, "progs/v_multi.mdl") ||
		 !strcmp(mod->name, "progs/v_multi2.mdl") ||
		 !strcmp(mod->name, "progs/v_plasma.mdl") ||	// ?
		 !strcmp(mod->name, "progs/v_star.mdl"))		// ?
		mod->modhint = MOD_WEAPON;
	else if (!strcmp(mod->name, "progs/quaddama.mdl"))
		mod->modhint = MOD_QUAD;
	else if (!strcmp(mod->name, "progs/invulner.mdl"))
		mod->modhint = MOD_PENT;
	else if (!strcmp(mod->name, "progs/lavaball.mdl"))
		mod->modhint = MOD_LAVABALL;
	else if (!strcmp(mod->name, "progs/spike.mdl") ||
		 !strcmp(mod->name, "progs/s_spike.mdl"))
		mod->modhint = MOD_SPIKE;
	else if (!strcmp(mod->name, "progs/shambler.mdl"))
		mod->modhint = MOD_SHAMBLER;
	else if (!strcmp(mod->name, "progs/soldier.mdl"))
		mod->modhint = MOD_SOLDIER;
	else if (!strcmp(mod->name, "progs/enforcer.mdl"))
		mod->modhint = MOD_ENFORCER;
	else
		mod->modhint = MOD_NORMAL;

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);

// allocate space for a working header, plus all the data except the frames, skin and group info
	size = sizeof(aliashdr_t) + (LittleLong(pinmodel->numframes) - 1) * sizeof(pheader->frames[0]);
	pheader = Hunk_AllocName (size, loadname);

	mod->flags = LittleLong (pinmodel->flags);

// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);
	if (pheader->numverts <= 0)
		Sys_Error ("Mod_LoadAliasModel: model %s has no vertices", mod->name);
	else if (pheader->numverts > MAXALIASVERTS)
		Sys_Error ("Mod_LoadAliasModel: model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);
	if (pheader->numtris <= 0)
		Sys_Error ("Mod_LoadAliasModel: model %s has no triangles", mod->name);
	else if (pheader->numtris > MAXALIASTRIS)
		Sys_Error ("Mod_LoadAliasModel: model %s has too many triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0 ; i < 3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for (i = 0 ; i < pheader->numverts ; i++)
	{
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i = 0 ; i < pheader->numtris ; i++)
	{
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0 ; j < 3 ; j++)
			triangles[i].vertindex[j] = LittleLong (pintriangles[i].vertindex[j]);
	}

// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	aliasbboxmins[0] = aliasbboxmins[1] = aliasbboxmins[2] = 255;
	aliasbboxmaxs[0] = aliasbboxmaxs[1] = aliasbboxmaxs[2] = 0;

	for (i = 0 ; i < numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *)Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	for (i = 0 ; i < 3 ; i++)
	{
		mod->mins[i] = aliasbboxmins[i] * pheader->scale[i] + pheader->scale_origin[i];
		mod->maxs[i] = aliasbboxmaxs[i] * pheader->scale[i] + pheader->scale_origin[i];
	}

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

// build the draw lists
	GL_MakeAliasModelDisplayLists (mod, pheader);

// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;

	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

/*
===============================================================================

								Q3 MODELS

===============================================================================
*/

animdata_t	anims[NUM_ANIMTYPES];

vec3_t		md3bboxmins, md3bboxmaxs;

void Mod_GetQ3AnimData (char *buf, char *animtype, animdata_t *adata)
{
	int		i, j, data[4];
	char	*token, num[4];

	if ((token = strstr(buf, animtype)))
	{
		while (*token != '\n')
			token--;
		token++;	// so we jump back to the first char
		for (i = 0 ; i < 4 ; i++)
		{
			memset (num, 0, sizeof(num));
			for (j = 0 ; *token != '\t' ; j++)
				num[j] = *token++;
			data[i] = Q_atoi(num);
			token++;
		}
		adata->offset = data[0];
		adata->num_frames = data[1];
		adata->loop_frames = data[2];
		adata->interval = 1.0 / (float)data[3];
	}
}

void Mod_LoadQ3Animation (void)
{
	int			ofs_legs;
	char		*animdata;
	animdata_t	tmp1, tmp2;

	if (!(animdata = (char *)COM_LoadFile("progs/player/animation.cfg", 0)))
	{
		Con_Printf ("ERROR: Couldn't open animation file\n");
		return;
	}

	memset (anims, 0, sizeof(anims));

	Mod_GetQ3AnimData (animdata, "BOTH_DEATH1", &anims[both_death1]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEATH2", &anims[both_death2]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEATH3", &anims[both_death3]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEAD1", &anims[both_dead1]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEAD2", &anims[both_dead2]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEAD3", &anims[both_dead3]);

	Mod_GetQ3AnimData (animdata, "TORSO_ATTACK", &anims[torso_attack]);
	Mod_GetQ3AnimData (animdata, "TORSO_ATTACK2", &anims[torso_attack2]);
	Mod_GetQ3AnimData (animdata, "TORSO_STAND", &anims[torso_stand]);
	Mod_GetQ3AnimData (animdata, "TORSO_STAND2", &anims[torso_stand2]);

	Mod_GetQ3AnimData (animdata, "TORSO_GESTURE", &tmp1);
	Mod_GetQ3AnimData (animdata, "LEGS_WALKCR", &tmp2);
// we need to subtract the torso-only frames to get the correct indices
	ofs_legs = tmp2.offset - tmp1.offset;

	Mod_GetQ3AnimData (animdata, "LEGS_RUN", &anims[legs_run]);
	Mod_GetQ3AnimData (animdata, "LEGS_IDLE", &anims[legs_idle]);
	anims[legs_run].offset -= ofs_legs;
	anims[legs_idle].offset -= ofs_legs;

	Z_Free (animdata);
}

/*
=================
Mod_LoadQ3ModelTexture
=================
*/
void Mod_LoadQ3ModelTexture (char *identifier, int flags, int *gl_texnum, int *fb_texnum)
{
	char	loadpath[64];

	Q_snprintfz (loadpath, sizeof(loadpath), "textures/q3models/%s", identifier);
	*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
	if (*gl_texnum)
		*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);

	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "textures/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
		if (*gl_texnum)
			*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);
	}
}

/*
=================
Mod_LoadQ3Model
=================
*/
void Mod_LoadQ3Model (model_t *mod, void *buffer)
{
	int			i, j, size, base, texture_flag, version, gl_texnum, fb_texnum, numskinsfound;
	char		basename[MAX_QPATH], pathname[MAX_QPATH], **skinsfound;
	float		radiusmax;
	md3header_t	*header;
	md3frame_t	*frame;
	md3tag_t	*tag;
	md3surface_t *surf;
	md3shader_t	*shader;
	md3triangle_t *tris;
	md3tc_t		*tc;
	md3vert_t	*vert;
	searchpath_t *search;

// we need to replace missing flags. sigh.
	if (!strcmp(mod->name, "progs/missile.md3") ||
	    !strcmp(mod->name, "progs/lavaball.md3"))
		mod->flags |= EF_ROCKET;
	else if (!strcmp(mod->name, "progs/grenade.md3"))
		mod->flags |= EF_GRENADE;
	else if (!strcmp(mod->name, "progs/invulner.md3") ||
		 !strcmp(mod->name, "progs/suit.md3") ||
		 !strcmp(mod->name, "progs/invisibl.md3") ||
		 !strcmp(mod->name, "progs/quaddama.md3") ||
		 !strcmp(mod->name, "progs/armor.md3") ||
		 !strcmp(mod->name, "progs/backpack.md3") ||
		 !strcmp(mod->name, "progs/b_g_key.md3") ||
		 !strcmp(mod->name, "progs/b_s_key.md3") ||
		 !strcmp(mod->name, "progs/m_g_key.md3") ||
		 !strcmp(mod->name, "progs/m_s_key.md3") ||
		 !strcmp(mod->name, "progs/w_g_key.md3") ||
		 !strcmp(mod->name, "progs/w_s_key.md3") ||
		 !strcmp(mod->name, "progs/end1.md3") ||
		 !strcmp(mod->name, "progs/end2.md3") ||
		 !strcmp(mod->name, "progs/end3.md3") ||
		 !strcmp(mod->name, "progs/end4.md3") ||
		 !strcmp(mod->name, "progs/g_shot.md3")	||
		 !strcmp(mod->name, "progs/g_nail.md3")	||
		 !strcmp(mod->name, "progs/g_nail2.md3") ||
		 !strcmp(mod->name, "progs/g_rock.md3")	||
		 !strcmp(mod->name, "progs/g_rock2.md3") ||
		 !strcmp(mod->name, "progs/g_light.md3"))
		mod->flags |= EF_ROTATE;
	else if (!strcmp(mod->name, "progs/h_player.md3") ||
		 !strcmp(mod->name, "progs/gib1.md3") ||
		 !strcmp(mod->name, "progs/gib2.md3") ||
		 !strcmp(mod->name, "progs/gib3.md3") ||
		 !strcmp(mod->name, "progs/zom_gib.md3"))
		mod->flags |= EF_GIB;

// some models are special
	if (!strcmp(mod->name, "progs/player.md3") || !strcmp(mod->name, "progs/vwplayer.md3"))
		mod->modhint = MOD_PLAYER;
	else if (!strcmp(mod->name, "progs/flame.md3"))
		mod->modhint = MOD_FLAME;
	else if (!strcmp(mod->name, "progs/v_shot.md3")	||
		 !strcmp(mod->name, "progs/v_shot2.md3") ||
		 !strcmp(mod->name, "progs/v_nail.md3")	||
		 !strcmp(mod->name, "progs/v_nail2.md3") ||
		 !strcmp(mod->name, "progs/v_rock.md3")	||
		 !strcmp(mod->name, "progs/v_rock2.md3"))
		mod->modhint = MOD_WEAPON;
	else if (!strcmp(mod->name, "progs/quaddama.md3"))
		mod->modhint = MOD_QUAD;
	else if (!strcmp(mod->name, "progs/invulner.md3"))
		mod->modhint = MOD_PENT;
	else if (!strcmp(mod->name, "progs/lavaball.md3"))
		mod->modhint = MOD_LAVABALL;
	else if (!strcmp(mod->name, "progs/spike.md3") ||
		 !strcmp(mod->name, "progs/s_spike.md3"))
		mod->modhint = MOD_SPIKE;
	else if (!strcmp(mod->name, "progs/bullet.md3"))
		mod->modhint = MOD_Q3GUNSHOT;
	else if (!strcmp(mod->name, "progs/telep.md3"))
		mod->modhint = MOD_Q3TELEPORT;
	else
		mod->modhint = MOD_NORMAL;

	header = (md3header_t *)buffer;

	version = LittleLong (header->version);
	if (version != MD3_VERSION)
		Sys_Error ("Mod_LoadQ3Model: %s has wrong version number (%i should be %i)", mod->name, version, MD3_VERSION);

// endian-adjust all data
	header->numframes = LittleLong (header->numframes);
	if (header->numframes < 1)
		Sys_Error ("Mod_LoadQ3Model: model %s has no frames", mod->name);
	else if (header->numframes > MAXMD3FRAMES)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many frames", mod->name);

	header->numtags = LittleLong (header->numtags);
	if (header->numtags > MAXMD3TAGS)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many tags", mod->name);

	header->numsurfs = LittleLong (header->numsurfs);
	if (header->numsurfs < 1)
		Sys_Error ("Mod_LoadQ3Model: model %s has no surfaces", mod->name);
	else if (header->numsurfs > MAXMD3SURFS)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many surfaces", mod->name);

	header->numskins = LittleLong (header->numskins);
	header->ofsframes = LittleLong (header->ofsframes);
	header->ofstags = LittleLong (header->ofstags);
	header->ofssurfs = LittleLong (header->ofssurfs);
	header->ofsend = LittleLong (header->ofsend);

	// swap all the frames
	frame = (md3frame_t *)((byte *)header + header->ofsframes);
	for (i = 0  ; i < header->numframes ; i++)
	{
		frame[i].radius = LittleFloat (frame->radius);
		for (j = 0 ; j < 3 ; j++)
		{
			frame[i].mins[j] = LittleFloat (frame[i].mins[j]);
			frame[i].maxs[j] = LittleFloat (frame[i].maxs[j]);
			frame[i].pos[j] = LittleFloat (frame[i].pos[j]);
		}
	}

	// swap all the tags
	tag = (md3tag_t *)((byte *)header + header->ofstags);
	for (i = 0 ; i < header->numtags ; i++)
	{
		for (j = 0 ; j < 3 ; j++)
		{
			tag[i].pos[j] = LittleFloat (tag[i].pos[j]);
			tag[i].rot[0][j] = LittleFloat (tag[i].rot[0][j]);
			tag[i].rot[1][j] = LittleFloat (tag[i].rot[1][j]);
			tag[i].rot[2][j] = LittleFloat (tag[i].rot[2][j]);
		}
	}

	// swap all the surfaces
	surf = (md3surface_t *)((byte *)header + header->ofssurfs);
	for (i = 0 ; i < header->numsurfs ; i++)
	{
		surf->ident = LittleLong (surf->ident);
		surf->flags = LittleLong (surf->flags);
		surf->numframes = LittleLong (surf->numframes);
		if (surf->numframes != header->numframes)
			Sys_Error ("Mod_LoadQ3Model: number of frames don't match in %s", mod->name);

		surf->numshaders = LittleLong (surf->numshaders);
		if (surf->numshaders > MAXMD3SHADERS)
			Sys_Error ("Mod_LoadQ3Model: model %s has too many shaders", mod->name);

		surf->numverts = LittleLong (surf->numverts);
		if (surf->numverts <= 0)
			Sys_Error ("Mod_LoadQ3Model: model %s has no vertices", mod->name);
		else if (surf->numverts > MAXMD3VERTS)
			Sys_Error ("Mod_LoadQ3Model: model %s has too many vertices", mod->name);

		surf->numtris = LittleLong (surf->numtris);
		if (surf->numtris <= 0)
			Sys_Error ("Mod_LoadQ3Model: model %s has no triangles", mod->name);
		else if (surf->numtris > MAXMD3TRIS)
			Sys_Error ("Mod_LoadQ3Model: model %s has too many triangles", mod->name);

		surf->ofstris = LittleLong (surf->ofstris);
		surf->ofsshaders = LittleLong (surf->ofsshaders);
		surf->ofstc = LittleLong (surf->ofstc);
		surf->ofsverts = LittleLong (surf->ofsverts);
		surf->ofsend = LittleLong (surf->ofsend);

		// swap all the shaders
		shader = (md3shader_t *)((byte *)surf + surf->ofsshaders);
		for (j = 0 ; j < surf->numshaders ; j++)
			shader[j].index = LittleLong (shader[j].index);

		// swap all the triangles
		tris = (md3triangle_t *)((byte *)surf + surf->ofstris);
		for (j = 0 ; j < surf->numtris ; j++)
		{
			tris[j].indexes[0] = LittleLong (tris[j].indexes[0]);
			tris[j].indexes[1] = LittleLong (tris[j].indexes[1]);
			tris[j].indexes[2] = LittleLong (tris[j].indexes[2]);
		}

		// swap all the texture coords
		tc = (md3tc_t *)((byte *)surf + surf->ofstc);
		for (j = 0 ; j < surf->numverts ; j++)
		{
			tc[j].s = LittleFloat (tc[j].s);
			tc[j].t = LittleFloat (tc[j].t);
		}

		// swap all the vertices
		vert = (md3vert_t *)((byte *)surf + surf->ofsverts);
		for (j = 0 ; j < surf->numverts * surf->numframes ; j++)
		{
			vert[j].vec[0] = LittleShort (vert[j].vec[0]);
			vert[j].vec[1] = LittleShort (vert[j].vec[1]);
			vert[j].vec[2] = LittleShort (vert[j].vec[2]);
			vert[j].normal = LittleShort (vert[j].normal);
		}

		// find the next surface
		surf = (md3surface_t *)((byte *)surf + surf->ofsend);
	}

// load the skins
	i = strrchr (mod->name, '/') - mod->name;
	Q_strncpyz (pathname, mod->name, i+1);

	EraseDirEntries ();
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (!search->pack)
		{
			RDFlags |= RD_NOERASE;
			ReadDir (va("%s/%s", search->filename, pathname), va("%s*.skin", loadname));
		}
	}
	FindFilesInPak (va("%s/%s*.skin", pathname, loadname));

	numskinsfound = num_files;
	skinsfound = (char **)Q_malloc (numskinsfound * sizeof(char *));
	for (i = 0 ; i < numskinsfound ; i++)
	{
		skinsfound[i] = Q_malloc (MAX_QPATH);
		Q_snprintfz (skinsfound[i], MAX_QPATH, "%s/%s", pathname, filelist[i].name);
	}

// allocate extra size for structures different in memory
	surf = (md3surface_t *)((byte *)header + header->ofssurfs);
	for (size = 0, i = 0 ; i < header->numsurfs ; i++)
	{
		if (numskinsfound)
			surf->numshaders = numskinsfound;
		size += surf->numshaders * sizeof(md3shader_mem_t);			// shader containing texnum
		size += surf->numverts * surf->numframes * sizeof(md3vert_mem_t);	// floating point vertices
		surf = (md3surface_t *)((byte *)surf + surf->ofsend);
	}

	header = Cache_Alloc (&mod->cache, com_filesize + size, loadname);
	if (!mod->cache.data)
		return;

	memcpy (header, buffer, com_filesize);
	base = com_filesize;

	mod->type = mod_md3;
	mod->numframes = header->numframes;

	md3bboxmins[0] = md3bboxmins[1] = md3bboxmins[2] = 99999;
	md3bboxmaxs[0] = md3bboxmaxs[1] = md3bboxmaxs[2] = -99999;
	radiusmax = 0;

	frame = (md3frame_t *)((byte *)header + header->ofsframes);
	for (i = 0 ; i < header->numframes ; i++)
	{
		for (j = 0 ; j < 3 ; j++)
		{
			md3bboxmins[j] = min(md3bboxmins[j], frame[i].mins[j]);
			md3bboxmaxs[j] = max(md3bboxmaxs[j], frame[i].maxs[j]);
		}
		radiusmax = max(radiusmax, frame[i].radius);
	}
	VectorCopy (md3bboxmins, mod->mins);
	VectorCopy (md3bboxmaxs, mod->maxs);
	mod->radius = radiusmax;

// load the animation frames if loading the player model
	if (!strcmp(mod->name, cl_modelnames[mi_q3legs]))
		Mod_LoadQ3Animation ();

	texture_flag = TEX_MIPMAP;

	surf = (md3surface_t *)((byte *)header + header->ofssurfs);
	for (i = 0 ; i < header->numsurfs; i++)
	{
		if (strstr(surf->name, "energy") ||
			strstr(surf->name, "f_") ||
			strstr(surf->name, "flare") ||
			strstr(surf->name, "flash") ||
			strstr(surf->name, "Sphere") ||
			strstr(surf->name, "telep"))
		{
			mod->flags |= EF_Q3TRANS;
		}

		shader = (md3shader_t *)((byte *)surf + surf->ofsshaders);
		surf->ofsshaders = base;
		size = !numskinsfound ? surf->numshaders : numskinsfound;
		for (j = 0 ; j < size ; j++)
		{
			md3shader_mem_t	*memshader = (md3shader_mem_t *)((byte *)header + surf->ofsshaders);

			memset (memshader[j].name, 0, sizeof(memshader[j].name));
			if (!numskinsfound)
			{
				Q_strncpyz (memshader[j].name, shader->name, sizeof(memshader[j].name));
				memshader[j].index = shader->index;
				shader++;
			}
			else
			{
				int		pos;
				char	*surfname, *skinname, *skindata;

				skindata = (char *)COM_LoadFile (skinsfound[j], 0);

				pos = 0;
				while (pos < com_filesize)
				{
					surfname = &skindata[pos];
					while (skindata[pos] != ',' && skindata[pos])
						pos++;
					skindata[pos++] = '\0';

					skinname = &skindata[pos];
					while (skindata[pos] != '\n' && skindata[pos])
						pos++;
					skindata[pos++-1] = '\0';	// becoz of \r\n

					if (strcmp(surf->name, surfname))
						continue;

					if (skinname[0])
						Q_strncpyz (memshader[j].name, skinname, sizeof(memshader[j].name));
				}

				Z_Free (skindata);
			}

			// the teleport model doesn't have any shaders, so we need to add
			if (mod->modhint == MOD_Q3TELEPORT)
				Q_strncpyz (basename, "teleportEffect2", sizeof(basename));
			else
				COM_StripExtension (COM_SkipPath(memshader[j].name), basename);

			gl_texnum = fb_texnum = 0;
			Mod_LoadQ3ModelTexture (basename, texture_flag, &gl_texnum, &fb_texnum);

			memshader[j].gl_texnum = gl_texnum;
			memshader[j].fb_texnum = fb_texnum;
		}
		base += size * sizeof(md3shader_mem_t);

		vert = (md3vert_t *)((byte *)surf + surf->ofsverts);
		surf->ofsverts = base;
		size = surf->numverts * surf->numframes;
		for (j = 0 ; j < size ; j++)
		{
			float		lat, lng;
			vec3_t		ang;
			md3vert_mem_t *vertexes = (md3vert_mem_t *)((byte *)header + surf->ofsverts);

			vertexes[j].oldnormal = vert->normal;

			vertexes[j].vec[0] = (float)vert->vec[0] * MD3_XYZ_SCALE;
			vertexes[j].vec[1] = (float)vert->vec[1] * MD3_XYZ_SCALE;
			vertexes[j].vec[2] = (float)vert->vec[2] * MD3_XYZ_SCALE;

			lat = ((vert->normal >> 8) & 0xff) * M_PI / 128.0f;
			lng = (vert->normal & 0xff) * M_PI / 128.0f;
			vertexes[j].normal[0] = cos(lat) * sin(lng);
			vertexes[j].normal[1] = sin(lat) * sin(lng);
			vertexes[j].normal[2] = cos(lng);

			vectoangles (vertexes[j].normal, ang);
			vertexes[j].anorm_pitch = ang[0] * 256 / 360;
			vertexes[j].anorm_yaw = ang[1] * 256 / 360;

			vert++;
		}
		base += size * sizeof(md3vert_mem_t);

		surf = (md3surface_t *)((byte *)surf + surf->ofsend);
	}

	for (i = 0 ; i < numskinsfound ; i++)
		free (skinsfound[i]);
	free (skinsfound);
}

/*
===============================================================================

				SPRITE MODELS

===============================================================================
*/

static	int	spr_version;

/*
=================
Mod_LoadSpriteModelTexture
=================
*/
void Mod_LoadSpriteModelTexture (char *sprite_name, dspriteframe_t *pinframe, mspriteframe_t *pspriteframe, int framenum, int width, int height, int flags)
{
	char	name[64], sprite[64], sprite2[64];

	COM_StripExtension (sprite_name, sprite);
	Q_snprintfz (name, sizeof(name), "%s_%i", sprite_name, framenum);
	Q_snprintfz (sprite2, sizeof(sprite2), "textures/sprites/%s_%i", COM_SkipPath(sprite), framenum);
	pspriteframe->gl_texturenum = GL_LoadTextureImage (sprite2, name, 0, 0, flags);

	if (pspriteframe->gl_texturenum == 0)
	{
		sprintf (sprite2, "textures/%s_%i", COM_SkipPath(sprite), framenum);
		pspriteframe->gl_texturenum = GL_LoadTextureImage (sprite2, name, 0, 0, flags);
	}

	if (pspriteframe->gl_texturenum == 0)
	{
		if (spr_version == SPRITE32_VERSION)
			pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), flags, 4);
		else
			pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), flags, 1);
	}
}

/*
=================
Mod_LoadSpriteFrame
=================
*/
void *Mod_LoadSpriteFrame (void *pin, mspriteframe_t **ppframe, int framenum)
{
	int		width, height, size, origin[2], texture_flag;
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;

	texture_flag = TEX_MIPMAP | TEX_ALPHA;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = (spr_version == SPRITE32_VERSION) ? width * height * 4 : width * height;

	pspriteframe = Hunk_AllocName (sizeof(mspriteframe_t), loadname);

	memset (pspriteframe, 0, sizeof(mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	Mod_LoadSpriteModelTexture (loadmodel->name, pinframe, pspriteframe, framenum, width, height, texture_flag);

	return (void *)((byte *)pinframe + sizeof(dspriteframe_t) + size);
}

/*
=================
Mod_LoadSpriteGroup
=================
*/
void *Mod_LoadSpriteGroup (void *pin, mspriteframe_t **ppframe, int framenum)
{
	int			i, numframes;
	float		*poutintervals;
	void		*ptemp;
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	dspriteinterval_t *pin_intervals;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof(mspritegroup_t) + (numframes - 1) * sizeof(pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof(float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0 ; i < numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval <= 0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i = 0 ; i < numframes ; i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);

	return ptemp;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int			i, numframes, size;
	dsprite_t	*pin;
	msprite_t	*psprite;
	dspriteframetype_t *pframetype;

	pin = (dsprite_t *)buffer;

	spr_version = LittleLong (pin->version);
	if (spr_version != SPRITE_VERSION && spr_version != SPRITE32_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i or %i)", mod->name, spr_version, SPRITE_VERSION, SPRITE32_VERSION);

	if (r_nospr32.value && spr_version == SPRITE32_VERSION)
		return;

	numframes = LittleLong (pin->numframes);

	size = sizeof(msprite_t) + (numframes - 1) * sizeof(psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

// load the frames
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i = 0 ; i < numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
		else
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
	}

	mod->type = (spr_version == SPRITE32_VERSION) ? mod_spr32 : mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
		Con_Printf ("%8p : %s\n", mod->cache.data, mod->name);
}

/*
================
Mod_IsPlayerModel
================
*/
qboolean Mod_IsAnyKindOfPlayerModel(model_t *mod)
{
	return mod ? mod->modhint == MOD_PLAYER || mod->modhint == MOD_PLAYER_DME : false;
}
