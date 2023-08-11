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

static byte	*mod_novis;
static int	mod_novis_capacity;

static byte	*mod_decompressed;
static int	mod_decompressed_capacity;

#define	MAX_MOD_KNOWN	2048 //johnfitz -- was 512
model_t	mod_known[MAX_MOD_KNOWN];
int	mod_numknown;

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};
cvar_t	external_ents = { "external_ents", "1", CVAR_ARCHIVE };

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
	Cvar_Register (&external_ents);
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
	byte	*out, *outend;

	row = (model->numleafs + 7) >> 3;
	if (mod_decompressed == NULL || row > mod_decompressed_capacity)
	{
		// Sphere -- we have to allocate in multiples of four bytes, because in
		// R_MarkLeaves, the result of this function will be iterated over in
		// increments of sizeof(unsigned) which is 4 on the platforms that
		// are targeted.
		mod_decompressed_capacity = NextMultipleOfFour(row);
		mod_decompressed = (byte *)Q_realloc(mod_decompressed, mod_decompressed_capacity);
		if (!mod_decompressed)
			Sys_Error("Mod_DecompressVis: realloc() failed on %d bytes", mod_decompressed_capacity);
	}
	out = mod_decompressed;
	outend = mod_decompressed + row;

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
		return mod_decompressed;
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
			if (out == outend)
				return mod_decompressed;

			*out++ = 0;
			c--;
		}
	} while (out - mod_decompressed < row);
#endif
	
	return mod_decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return Mod_NoVisPVS(model);

	return Mod_DecompressVis (leaf->compressed_vis, model);
}

byte *Mod_NoVisPVS(model_t *model)
{
	int pvsbytes;

	pvsbytes = (model->numleafs + 7) >> 3;
	if (mod_novis == NULL || pvsbytes > mod_novis_capacity)
	{
		mod_novis_capacity = NextMultipleOfFour(pvsbytes);
		mod_novis = (byte *)Q_realloc(mod_novis, mod_novis_capacity);
		if (!mod_novis)
			Sys_Error("Mod_NoVisPVS: realloc() failed on %d bytes", mod_novis_capacity);

		memset(mod_novis, 0xff, mod_novis_capacity);
	}
	return mod_novis;
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
	COM_FileBase (mod->name, loadname, sizeof(loadname));

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
#define	ISSKYTEX(name)	(!Q_strncasecmp(name, "sky", 3)) //((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')
#define ISALPHATEX(name) ((name)[0] == '{')

byte	*mod_base;

/*
=================
Mod_LoadBrushModelTexture
=================
*/
int Mod_LoadBrushModelTexture (texture_t *tx, int flags)
{
	char	*name, *mapname, *identifier, *fb_identifier, bspname[64];

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
	
	COM_FileBase(loadmodel->name, bspname, sizeof(bspname));
	identifier = va("%s:%s", bspname, name);
	fb_identifier = va("%s:@fb_%s", bspname, name);

	if (loadmodel->isworldmodel)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/%s/%s", mapname, name), identifier, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", mapname, name), fb_identifier, 0, 0, flags | TEX_LUMA);
		}
	}
	else
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/bmodels/%s", name), identifier, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s_luma", name), fb_identifier, 0, 0, flags | TEX_LUMA);
		}
	}

	if (!tx->gl_texturenum)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/%s", name), identifier, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s_luma", name), fb_identifier, 0, 0, flags | TEX_LUMA);
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
	int			i, j, pixels, num, max, altmax, texture_flag, brighten_flag, nummiptex;
	miptex_t	*mt;
	texture_t	*tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	byte		*data;
	char		bspname[64];

	//johnfitz -- don't return early if no textures; still need to create dummy texture
	if (!l->filelen)
	{
		Con_Printf("Mod_LoadTextures: no textures in bsp file\n");
		nummiptex = 0;
		m = NULL; // avoid bogus compiler warning
	}
	else
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		m->nummiptex = LittleLong(m->nummiptex);
		nummiptex = m->nummiptex;
	}
	//johnfitz

	loadmodel->numtextures = nummiptex + 2; //johnfitz -- need 2 dummy texture chains for missing textures
	loadmodel->textures = Hunk_AllocName (loadmodel->numtextures * sizeof(*loadmodel->textures), loadname);
	COM_FileBase(loadmodel->name, bspname, sizeof(bspname));

	brighten_flag = (lightmode == 2) ? TEX_BRIGHTEN : 0;
	texture_flag = TEX_MIPMAP;

	for (i = 0 ; i < nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong (m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);

		mt->width = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);
		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong(mt->offsets[j]);

		if ((mt->width & 15) || (mt->height & 15))
			Con_DPrintf("Warning: texture %s (%d x %d) is not 16 aligned\n", mt->name, mt->width, mt->height);

		pixels = mt->width * mt->height; // only copy the first mip, the rest are auto-generated
		tx = (texture_t *)Hunk_AllocName(sizeof(texture_t) + pixels, loadname);
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		if (!tx->name[0])
		{
			Q_snprintfz (tx->name, sizeof(tx->name), "unnamed%d", i);
			Con_DPrintf ("Warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		// ericw -- fence textures
		if (ISALPHATEX(tx->name))
			texture_flag |= TEX_ALPHA;

		tx->width = mt->width;
		tx->height = mt->height;

		for (j = 0; j<MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32 && 
		    CRC_Block((byte *)(mt + 1), mt->width * mt->height) == 65393)
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy (mt + 1, (byte *)(mt + 1) + 32*31, 32);
		}

		// ericw -- check for pixels extending past the end of the lump.
		// appears in the wild; e.g. jam2_tronyn.bsp (func_mapjam2),
		// kellbase1.bsp (quoth), and can lead to a segfault if we read past
		// the end of the .bsp file buffer
		if (((byte*)(mt + 1) + pixels) > (mod_base + l->fileofs + l->filelen))
		{
			Con_DPrintf("Texture %s extends past end of lump\n", mt->name);
			pixels = max(0, (mod_base + l->fileofs + l->filelen) - (byte*)(mt + 1));
		}

		tx->update_warp = false; //johnfitz
		tx->warp_texturenum = 0;

		memcpy(tx + 1, mt + 1, pixels);

		if (loadmodel->isworldmodel && ISSKYTEX(tx->name))
		{
			R_InitSky (tx);
			continue;
		}

		if (mt->offsets[0])
		{
			data = (byte *)tx + tx->offsets[0];
			tx2 = tx;
		}
		else
		{
			data = (byte *)tx2 + tx2->offsets[0];
			tx2 = r_notexture_mip;
		}

		if (!Mod_LoadBrushModelTexture(tx, texture_flag))
		{
			tx->gl_texturenum = GL_LoadTexture(va("%s:%s", bspname, tx2->name), tx2->width, tx2->height, data, texture_flag | brighten_flag, 1);
			if (!ISTURBTEX(tx->name) && Img_HasFullbrights(data, tx2->width * tx2->height))
				tx->fb_texturenum = GL_LoadTexture(va("%s:@fb_%s", bspname, tx2->name), tx2->width, tx2->height, data, texture_flag | TEX_FULLBRIGHT, 1);
		}

		if (ISTURBTEX(tx->name))
		{
			static byte	dummy[512*512];
			//now create the warpimage, using dummy data from the hunk to create the initial image
			tx->warp_texturenum = GL_LoadTexture(va("%s:@warp_%s", bspname, tx2->name), gl_warpimagesize, gl_warpimagesize, dummy, texture_flag, 1);
			tx->update_warp = true;
		}
	}

	//johnfitz -- last 2 slots in array should be filled with dummy textures
	loadmodel->textures[loadmodel->numtextures-2] = r_notexture_mip; //for lightmapped surfs
	loadmodel->textures[loadmodel->numtextures-1] = r_notexture_mip2; //for SURF_DRAWTILED surfs

// sequence the animations
	for (i = 0 ; i < nummiptex; i++)
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

		for (j = i + 1 ; j < nummiptex ; j++)
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
	char	basemapname[MAX_QPATH], entfilename[MAX_QPATH], *ents;
	int		mark;
	unsigned int crc = 0;

	if (!external_ents.value)
		goto _load_embedded;

	mark = Hunk_LowMark();
	if (l->filelen > 0) 
	{
		crc = CRC_Block(mod_base + l->fileofs, l->filelen - 1);
	}

	Q_strlcpy(basemapname, loadmodel->name, sizeof(basemapname));
	COM_StripExtension(basemapname, basemapname);

	Q_snprintfz(entfilename, sizeof(entfilename), "%s@%04x.ent", basemapname, crc);
	Con_DPrintf("trying to load %s\n", entfilename);
	ents = (char*)COM_LoadHunkFile(entfilename);

	if (!ents)
	{
		Q_snprintfz(entfilename, sizeof(entfilename), "%s.ent", basemapname);
		Con_DPrintf("trying to load %s\n", entfilename);
		ents = (char*)COM_LoadHunkFile(entfilename);
	}

	if (ents)
	{
		loadmodel->entities = ents;
		Con_DPrintf("Loaded external entity file %s\n", entfilename);
		return;
	}

_load_embedded:
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

	// johnfitz -- check world visleafs -- adapted from bjp
	out = loadmodel->submodels;

	if (out->visleafs > 8192)
		Con_DPrintf("%i visleafs exceeds standard limit of 8192.\n", out->visleafs);
	//johnfitz
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l, int bsp2)
{
	medge_t	*out;
	int 	i, count;

	if (bsp2)
	{
		dledge_t *in = (dledge_t *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (medge_t *)Hunk_AllocName((count + 1) * sizeof(*out), loadname);

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for (i = 0; i<count; i++, in++, out++)
		{
			out->v[0] = LittleLong(in->v[0]);
			out->v[1] = LittleLong(in->v[1]);
		}
	}
	else
	{
		dsedge_t *in = (dsedge_t *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Sys_Error("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = Hunk_AllocName((count + 1) * sizeof(*out), loadname);

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for (i = 0; i < count; i++, in++, out++)
		{
			out->v[0] = (unsigned short)LittleShort(in->v[0]);
			out->v[1] = (unsigned short)LittleShort(in->v[1]);
		}
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
	int 		i, j, count, miptex, missing = 0; //johnfitz

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t *)Hunk_AllocName (count * sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		//johnfitz -- rewrote this section
		if (miptex >= loadmodel->numtextures-1 || !loadmodel->textures[miptex])
		{
			if (out->flags & TEX_SPECIAL)
				out->texture = loadmodel->textures[loadmodel->numtextures-1];
			else
				out->texture = loadmodel->textures[loadmodel->numtextures-2];
			out->flags |= TEX_MISSING;
			missing++;
		}
		else
		{
			out->texture = loadmodel->textures[miptex];
		}
		//johnfitz
	}

	//johnfitz: report missing textures
	if (missing && loadmodel->numtextures > 1)
		Con_Printf("Mod_LoadTexinfo: %d texture(s) missing from BSP file\n", missing);
	//johnfitz
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
	int			i, j, e, bmins[2], bmaxs[2];
	mvertex_t	*v;
	mtexinfo_t	*tex;

	mins[0] = mins[1] = FLT_MAX;
	maxs[0] = maxs[1] = -FLT_MAX;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j = 0; j<2; j++)
		{
			/* The following calculation is sensitive to floating-point
			* precision.  It needs to produce the same result that the
			* light compiler does, because R_BuildLightMap uses surf->
			* extents to know the width/height of a surface's lightmap,
			* and incorrect rounding here manifests itself as patches
			* of "corrupted" looking lightmaps.
			* Most light compilers are win32 executables, so they use
			* x87 floating point.  This means the multiplies and adds
			* are done at 80-bit precision, and the result is rounded
			* down to 32-bits and stored in val.
			* Adding the casts to double seems to be good enough to fix
			* lighting glitches when Quakespasm is compiled as x86_64
			* and using SSE2 floating-point.  A potential trouble spot
			* is the hallway at the beginning of mfxsp17.  -- ericw
			*/
			val = ((double)v->position[0] * (double)tex->vecs[j][0]) +
				((double)v->position[1] * (double)tex->vecs[j][1]) +
				((double)v->position[2] * (double)tex->vecs[j][2]) +
				(double)tex->vecs[j][3];

			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i<2; i++)
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 2000) //johnfitz -- was 512 in glquake, 256 in winquake
			Sys_Error("Bad surface extents");
	}
}

/*
================
Mod_PolyForUnlitSurface -- johnfitz -- creates polys for unlightmapped surfaces (sky and water)

TODO: merge this into BuildSurfaceDisplayList?
================
*/
void Mod_PolyForUnlitSurface(msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts, i, lindex;
	float		*vec;
	glpoly_t	*poly;
	float		texscale;

	if (fa->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
		texscale = (1.0 / 128.0); //warp animation repeats every 128
	else
		texscale = (1.0 / 16.0); //to match r_notexture_mip

	// convert edges back to a normal polygon
	numverts = 0;
	for (i = 0; i<fa->numedges; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	// create the poly
	poly = (glpoly_t *)Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = NULL;
	fa->polys = poly;
	poly->numverts = numverts;
	for (i = 0, vec = (float *)verts; i<numverts; i++, vec += 3)
	{
		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = DotProduct(vec, fa->texinfo->vecs[0]) * texscale;
		poly->verts[i][4] = DotProduct(vec, fa->texinfo->vecs[1]) * texscale;
	}
}

/*
=================
Mod_CalcSurfaceBounds -- johnfitz -- calculate bounding box for per-surface frustum culling
=================
*/
void Mod_CalcSurfaceBounds(msurface_t *s)
{
	int			i, e;
	mvertex_t	*v;

	s->mins[0] = s->mins[1] = s->mins[2] = FLT_MAX;
	s->maxs[0] = s->maxs[1] = s->maxs[2] = -FLT_MAX;

	for (i = 0; i<s->numedges; i++)
	{
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		if (s->mins[0] > v->position[0])
			s->mins[0] = v->position[0];
		if (s->mins[1] > v->position[1])
			s->mins[1] = v->position[1];
		if (s->mins[2] > v->position[2])
			s->mins[2] = v->position[2];

		if (s->maxs[0] < v->position[0])
			s->maxs[0] = v->position[0];
		if (s->maxs[1] < v->position[1])
			s->maxs[1] = v->position[1];
		if (s->maxs[2] < v->position[2])
			s->maxs[2] = v->position[2];
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l, qboolean bsp2)
{
	dsface_t	*ins;
	dlface_t	*inl;
	msurface_t 	*out;
	int			i, count, surfnum, lofs, planenum, side, texinfon;

	if (bsp2)
	{
		ins = NULL;
		inl = (dlface_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
			Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (dsface_t *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
			Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*ins);
	} 
	out = (msurface_t *)Hunk_AllocName (count * sizeof(*out), loadname);

	//johnfitz -- warn mappers about exceeding old limits 
	if (count > 32767 && !bsp2)
		Con_DPrintf("%i faces exceeds standard limit of 32767.\n", count);
	//johnfitz 

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0 ; surfnum < count ; surfnum++, out++)
	{
		if (bsp2)
		{
			out->firstedge = LittleLong(inl->firstedge);
			out->numedges = LittleLong(inl->numedges);
			planenum = LittleLong(inl->planenum);
			side = LittleLong(inl->side);
			texinfon = LittleLong(inl->texinfo);
			for (i = 0; i<MAXLIGHTMAPS; i++)
				out->styles[i] = inl->styles[i];
			lofs = LittleLong(inl->lightofs);
			inl++;
		}
		else
		{
			out->firstedge = LittleLong(ins->firstedge);
			out->numedges = LittleShort(ins->numedges);
			planenum = LittleShort(ins->planenum);
			side = LittleShort(ins->side);
			texinfon = LittleShort(ins->texinfo);
			for (i = 0; i<MAXLIGHTMAPS; i++)
				out->styles[i] = ins->styles[i];
			lofs = LittleLong(ins->lightofs);
			ins++;
		} 
		out->flags = 0;

		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + texinfon;

		CalcSurfaceExtents (out);
		Mod_CalcSurfaceBounds(out); //johnfitz -- for per-surface frustum culling 

	// lighting info
		if (lofs == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + (lofs * 3); //johnfitz -- lit support via lordhavoc (was "+ i") 

	// set the drawing flags flag
		if (ISSKYTEX(out->texinfo->texture->name)) // sky surface //also note -- was Q_strncmp, changed to match qbsp
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			Mod_PolyForUnlitSurface(out); //no more subdivision 
		}
		else if (ISTURBTEX(out->texinfo->texture->name)) // warp surface
		{
			out->flags |= SURF_DRAWTURB;
			if (out->texinfo->flags & TEX_SPECIAL)
				out->flags |= SURF_DRAWTILED;
			else if (out->samples && !loadmodel->haslitwater)
			{
				Con_DPrintf("Map has lit water\n");
				loadmodel->haslitwater = true;
			}

			// detect special liquid types
			if (!strncmp(out->texinfo->texture->name, "*lava", 5))
				out->flags |= SURF_DRAWLAVA;
			else if (!strncmp(out->texinfo->texture->name, "*slime", 6))
				out->flags |= SURF_DRAWSLIME;
			else if (!strncmp(out->texinfo->texture->name, "*tele", 5))
				out->flags |= SURF_DRAWTELE;
			else out->flags |= SURF_DRAWWATER;

			// polys are only created for unlit water here.
			// lit water is handled in BuildSurfaceDisplayList
			if (out->flags & SURF_DRAWTILED)
			{
				Mod_PolyForUnlitSurface(out);
				GL_SubdivideSurface(out);
			}
		}
		else if (ISALPHATEX(out->texinfo->texture->name))
		{
			out->flags |= SURF_DRAWALPHA;
		}
		else if (out->texinfo->flags & TEX_MISSING) // texture is missing from bsp
		{
			if (out->samples) //lightmapped
			{
				out->flags |= SURF_NOTEXTURE;
			}
			else // not lightmapped
			{
				out->flags |= (SURF_NOTEXTURE | SURF_DRAWTILED);
				Mod_PolyForUnlitSurface(out);
			}
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

void Mod_LoadNodes_S(lump_t *l)
{
	int			i, j, count, p;
	dsnode_t	*in;
	mnode_t		*out;

	in = (dsnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName(count*sizeof(*out), loadname);

	//johnfitz -- warn mappers about exceeding old limits
	if (count > 32767)
		Con_DPrintf("%i nodes exceeds standard limit of 32767.\n", count);
	//johnfitz

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i<count; i++, in++, out++)
	{
		for (j = 0; j<3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = (unsigned short)LittleShort(in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = (unsigned short)LittleShort(in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j = 0; j<2; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = (unsigned short)LittleShort(in->children[j]);
			if (p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 65535 - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

void Mod_LoadNodes_L1(lump_t *l)
{
	int			i, j, count, p;
	dl1node_t	*in;
	mnode_t		*out;

	in = (dl1node_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName(count*sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i<count; i++, in++, out++)
	{
		for (j = 0; j<3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong(in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = LittleLong(in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j = 0; j<2; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = LittleLong(in->children[j]);
			if (p >= 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffffffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p >= 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

void Mod_LoadNodes_L2(lump_t *l)
{
	int			i, j, count, p;
	dl2node_t	*in;
	mnode_t		*out;

	in = (dl2node_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName(count*sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i<count; i++, in++, out++)
	{
		for (j = 0; j<3; j++)
		{
			out->minmaxs[j] = LittleFloat(in->mins[j]);
			out->minmaxs[3 + j] = LittleFloat(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong(in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = LittleLong(in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j = 0; j<2; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = LittleLong(in->children[j]);
			if (p > 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffffffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p >= 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes(lump_t *l, int bsp2)
{
	if (bsp2 == 2)
		Mod_LoadNodes_L2(l);
	else if (bsp2)
		Mod_LoadNodes_L1(l);
	else
		Mod_LoadNodes_S(l);

	Mod_SetParent(loadmodel->nodes, NULL);	// sets nodes and leafs
}

void Mod_ProcessLeafs_S(dsleaf_t *in, int filelen)
{
	mleaf_t		*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);
	count = filelen / sizeof(*in);
	out = (mleaf_t *)Hunk_AllocName(count*sizeof(*out), loadname);

	//johnfitz
	if (count > 32767)
		Host_Error("Mod_LoadLeafs: %i leafs exceeds limit of 32767.\n", count);
	//johnfitz

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i<count; i++, in++, out++)
	{
		for (j = 0; j<3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + (unsigned short)LittleShort(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces); //johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j<4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (out->contents == CONTENTS_WATER || out->contents == CONTENTS_SLIME || out->contents == CONTENTS_LAVA)
		{
			for (j = 0; j<out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

void Mod_ProcessLeafs_L1(dl1leaf_t *in, int filelen)
{
	mleaf_t		*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	out = (mleaf_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i<count; i++, in++, out++)
	{
		for (j = 0; j<3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = LittleLong(in->nummarksurfaces); //johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j<4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (out->contents == CONTENTS_WATER || out->contents == CONTENTS_SLIME || out->contents == CONTENTS_LAVA)
		{
			for (j = 0; j<out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

void Mod_ProcessLeafs_L2(dl2leaf_t *in, int filelen)
{
	mleaf_t		*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	out = (mleaf_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i<count; i++, in++, out++)
	{
		for (j = 0; j<3; j++)
		{
			out->minmaxs[j] = LittleFloat(in->mins[j]);
			out->minmaxs[3 + j] = LittleFloat(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = LittleLong(in->nummarksurfaces); //johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j<4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (out->contents == CONTENTS_WATER || out->contents == CONTENTS_SLIME || out->contents == CONTENTS_LAVA)
		{
			for (j = 0; j<out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs(lump_t *l, int bsp2)
{
	void *in = (void *)(mod_base + l->fileofs);

	if (bsp2 == 2)
		Mod_ProcessLeafs_L2((dl2leaf_t *)in, l->filelen);
	else if (bsp2)
		Mod_ProcessLeafs_L1((dl1leaf_t *)in, l->filelen);
	else
		Mod_ProcessLeafs_S((dsleaf_t *)in, l->filelen);
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l, qboolean bsp2)
{
	dsclipnode_t *ins;
	dlclipnode_t *inl;
	mclipnode_t *out; //johnfitz -- was dclipnode_t
	int		i, count;
	hull_t		*hull;

	if (bsp2)
	{
		ins = NULL;
		inl = (dlclipnode_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
			Sys_Error("Mod_LoadClipnodes: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (dsclipnode_t *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
			Sys_Error("Mod_LoadClipnodes: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*ins);
	}
	out = (mclipnode_t *)Hunk_AllocName(count*sizeof(*out), loadname);

	//johnfitz -- warn about exceeding old limits
	if (count > 32767 && !bsp2)
		Con_DPrintf("%i clipnodes exceeds standard limit of 32767.\n", count);
	//johnfitz

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

	if (bsp2)
	{
		for (i = 0; i<count; i++, out++, inl++)
		{
			out->planenum = LittleLong(inl->planenum);

			//johnfitz -- bounds check
			if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
				Host_Error("Mod_LoadClipnodes: planenum out of bounds");
			//johnfitz

			out->children[0] = LittleLong(inl->children[0]);
			out->children[1] = LittleLong(inl->children[1]);
			//Spike: FIXME: bounds check
		}
	}
	else
	{
		for (i = 0; i<count; i++, out++, ins++)
		{
			out->planenum = LittleLong(ins->planenum);

			//johnfitz -- bounds check
			if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
				Host_Error("Mod_LoadClipnodes: planenum out of bounds");
			//johnfitz

			//johnfitz -- support clipnodes > 32k
			out->children[0] = (unsigned short)LittleShort(ins->children[0]);
			out->children[1] = (unsigned short)LittleShort(ins->children[1]);

			if (out->children[0] >= count)
				out->children[0] -= 65536;
			if (out->children[1] >= count)
				out->children[1] -= 65536;
			//johnfitz
		}
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
	mclipnode_t *out; //johnfitz -- was dclipnode_t
	int			i, j, count;
	hull_t		*hull;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = (mclipnode_t *)Hunk_AllocName (count * sizeof(*out), loadname);

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
void Mod_LoadMarksurfaces (lump_t *l, int bsp2)
{	
	int			i, j, count;
	msurface_t	**out;
	
	if (bsp2)
	{
		unsigned int *in = (unsigned int *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (msurface_t **)Hunk_AllocName(count*sizeof(*out), loadname);

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		for (i = 0; i<count; i++)
		{
			j = LittleLong(in[i]);
			if (j >= loadmodel->numsurfaces)
				Host_Error("Mod_LoadMarksurfaces: bad surface number");
			out[i] = loadmodel->surfaces + j;
		}
	}
	else
	{
		short *in = (short *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (msurface_t **)Hunk_AllocName(count*sizeof(*out), loadname);

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		//johnfitz -- warn mappers about exceeding old limits
		if (count > 32767)
			Con_DPrintf("%i marksurfaces exceeds standard limit of 32767.\n", count);
		//johnfitz

		for (i = 0; i<count; i++)
		{
			j = (unsigned short)LittleShort(in[i]); //johnfitz -- explicit cast as unsigned short
			if (j >= loadmodel->numsurfaces)
				Sys_Error("Mod_LoadMarksurfaces: bad surface number");
			out[i] = loadmodel->surfaces + j;
		}
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
	int			i, j, bsp2;
	dheader_t	*header;
	dmodel_t 	*bm;
	float		radius; //johnfitz

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	switch (i)
	{
	case BSPVERSION:
		bsp2 = false;
		break;
	case BSP2VERSION_2PSB:
		bsp2 = 1;	//first iteration
		break;
	case BSP2VERSION_BSP2:
		bsp2 = 2;	//sanitised revision
		break;
	default:
		Sys_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);
		break;
	}

	loadmodel->isworldmodel = !strcmp (loadmodel->name, va("maps/%s.bsp", cl_mapname.string));

// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0 ; i < sizeof(dheader_t) / 4 ; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES], bsp2);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES], bsp2);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES], bsp2);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], bsp2);
	Mod_LoadNodes (&header->lumps[LUMP_NODES], bsp2);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES], bsp2);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation

// set up the submodels (FIXME: this is confusing)

	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
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

		//johnfitz -- calculate rotate bounds and yaw bounds
		radius = RadiusFromBounds(mod->mins, mod->maxs);
		mod->rmaxs[0] = mod->rmaxs[1] = mod->rmaxs[2] = mod->ymaxs[0] = mod->ymaxs[1] = mod->ymaxs[2] = radius;
		mod->rmins[0] = mod->rmins[1] = mod->rmins[2] = mod->ymins[0] = mod->ymins[1] = mod->ymins[2] = -radius;
		//johnfitz

		//johnfitz -- correct physics cullboxes so that outlying clip brushes on doors and stuff are handled right
		if (i > 0 || strcmp(mod->name, sv.modelname) != 0) //skip submodel 0 of sv.worldmodel, which is the actual world
		{
			// start with the hull0 bounds
			VectorCopy(mod->maxs, mod->clipmaxs);
			VectorCopy(mod->mins, mod->clipmins);

			// process hull1 (we don't need to process hull2 becuase there's
			// no such thing as a brush that appears in hull2 but not hull1)
			//Mod_BoundsFromClipNode (mod, 1, mod->hulls[1].firstclipnode); // (disabled for now becuase it fucks up on rotating models)
		}
		//johnfitz

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
	if (loadmodel->flags & MF_HOLEY)
		texture_flag |= TEX_ALPHA;

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

				// try to load all 14 colorful player suits if loading the original player model
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
Mod_CalcAliasBounds -- johnfitz -- calculate bounds of alias model for nonrotated, yawrotated, and fullrotated cases
=================
*/
static void Mod_CalcAliasBounds(aliashdr_t *a)
{
	int			i, j, k;
	float		dist, yawradius, radius;
	vec3_t		v;

	//clear out all data
	for (i = 0; i < 3; i++)
	{
		loadmodel->mins[i] = loadmodel->ymins[i] = loadmodel->rmins[i] = FLT_MAX;
		loadmodel->maxs[i] = loadmodel->ymaxs[i] = loadmodel->rmaxs[i] = -FLT_MAX;
		radius = yawradius = 0;
	}

	//process verts
	for (i = 0; i < a->numposes; i++)
		for (j = 0; j < a->numverts; j++)
		{
			for (k = 0; k < 3; k++)
				v[k] = poseverts[i][j].v[k] * pheader->scale[k] + pheader->scale_origin[k];

			for (k = 0; k < 3; k++)
			{
				loadmodel->mins[k] = min(loadmodel->mins[k], v[k]);
				loadmodel->maxs[k] = max(loadmodel->maxs[k], v[k]);
			}
			dist = v[0] * v[0] + v[1] * v[1];
			if (yawradius < dist)
				yawradius = dist;
			dist += v[2] * v[2];
			if (radius < dist)
				radius = dist;
		}

	//rbounds will be used when entity has nonzero pitch or roll
	radius = sqrt(radius);
	loadmodel->rmins[0] = loadmodel->rmins[1] = loadmodel->rmins[2] = -radius;
	loadmodel->rmaxs[0] = loadmodel->rmaxs[1] = loadmodel->rmaxs[2] = radius;

	//ybounds will be used when entity has nonzero yaw
	yawradius = sqrt(yawradius);
	loadmodel->ymins[0] = loadmodel->ymins[1] = -yawradius;
	loadmodel->ymaxs[0] = loadmodel->ymaxs[1] = yawradius;
	loadmodel->ymins[2] = loadmodel->mins[2];
	loadmodel->ymaxs[2] = loadmodel->maxs[2];
}

static qboolean nameInList(const char *list, const char *name)
{
	const char *s;
	char tmp[MAX_QPATH];
	int i;

	s = list;

	while (*s)
	{
		// make a copy until the next comma or end of string
		i = 0;
		while (*s && *s != ',')
		{
			if (i < MAX_QPATH - 1)
				tmp[i++] = *s;
			s++;
		}
		tmp[i] = '\0';
		//compare it to the model name
		if (!strcmp(name, tmp))
		{
			return true;
		}
		//search forwards to the next comma or end of string
		while (*s && *s == ',')
			s++;
	}
	return false;
}

/*
=================
Mod_SetExtraFlags -- johnfitz -- set up extra flags that aren't in the mdl
=================
*/
void Mod_SetExtraFlags(model_t *mod)
{
	extern cvar_t r_noshadow_list;

	if (!mod || mod->type != mod_alias)
		return;

	mod->flags &= (0xFF | EF_Q3TRANS | MF_HOLEY); //only preserve first byte, plus EF_Q3TRANS and MF_HOLEY

	// noshadow flag
	if (nameInList(r_noshadow_list.string, mod->name))
		mod->flags |= EF_NOSHADOW;
}

qboolean OnChange_r_noshadow_list(cvar_t *var, char *string)
{
	int i;
	
	for (i = 0; i < MAX_MODELS; i++)
		Mod_SetExtraFlags(cl.model_precache[i]);

	return false;
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
	if (!strcmp(mod->name, "progs/player.mdl") ||
		!strcmp(mod->name, "progs/vwplayer.mdl"))
		mod->modhint = MOD_PLAYER;
	// player*.mdl models are DME models
	else if (!strcmp(mod->name, "progs/playax.mdl") ||
		!strncmp(mod->name, "progs/player", 12))
		mod->modhint = MOD_PLAYER_DME;
	else if (!strcmp(mod->name, "progs/eyes.mdl"))
		mod->modhint = MOD_EYES;
	else if (!strcmp(mod->name, "progs/flame0.mdl") ||
		 !strcmp(mod->name, "progs/flame.mdl") ||
		 !strcmp(mod->name, "progs/flame2.mdl") ||
		 !strcmp(mod->name, "progs/misc_flame_big.mdl") ||	//
		 !strcmp(mod->name, "progs/misc_flame_med.mdl") ||	//
		 !strcmp(mod->name, "progs/misc_longtrch.mdl") ||	// AD flames, torches
		 !strcmp(mod->name, "progs/misc_braztall.mdl") ||	//
		 !strcmp(mod->name, "progs/misc_brazshrt.mdl"))		//
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
		 !strcmp(mod->name, "progs/v_light.mdl") ||
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
		Con_DPrintf("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

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

	Mod_SetExtraFlags(mod); //johnfitz

	Mod_CalcAliasBounds(pheader); //johnfitz

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
		if (!strcmp(animtype, "LEGS_LAND"))
			adata->interval = 1.0 / 30.0;
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
	anims[legs_run].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_BACK", &anims[legs_back]);
	anims[legs_back].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_SWIM", &anims[legs_swim]);
	anims[legs_swim].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_JUMP", &anims[legs_jump]);
	anims[legs_jump].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_LAND", &anims[legs_land]);
	anims[legs_land].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_JUMPB", &anims[legs_jumpb]);
	anims[legs_jumpb].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_LANDB", &anims[legs_landb]);
	anims[legs_landb].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_IDLE", &anims[legs_idle]);
	anims[legs_idle].offset -= ofs_legs;
	Mod_GetQ3AnimData (animdata, "LEGS_TURN", &anims[legs_turn]);
	anims[legs_turn].offset -= ofs_legs;

	Z_Free (animdata);
}

/*
=================
Mod_LoadQ3ModelTexture
=================
*/
void Mod_LoadQ3ModelTexture (char *identifier, int flags, int *gl_texnum, int *fb_texnum)
{
	char	loadpath[256];

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

qboolean Mod_IsTransparentSurface (md3surface_t *surf)
{
	if (strstr(surf->name, "energy") ||
		strstr(surf->name, "f_") ||
		strstr(surf->name, "flare") ||
		strstr(surf->name, "flash") ||
		strstr(surf->name, "Sphere02") ||		// Quake3 gunshot
		strstr(surf->name, "GeoSphere01") ||	//
		strstr(surf->name, "Box01") ||			// Quakelive teleport
		strstr(surf->name, "telep") ||			// Quake3 teleport
	   !strcmp(surf->name, "bolt"))
	{
		return true;
	}

	return false;
}
/*
=================
Mod_LoadQ3Model
=================
*/
void Mod_LoadQ3Model (model_t *mod, void *buffer)
{
	int			i, j, size, base, texture_flag, version, gl_texnum, fb_texnum, numskinsfound;
	char		basename[MAXMD3PATH], pathname[MAXMD3PATH], **skinsfound;
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
	// weapons
		 !strcmp(mod->name, "progs/g_shot.md3")	||
		 !strcmp(mod->name, "progs/g_nail.md3")	||
		 !strcmp(mod->name, "progs/g_nail2.md3") ||
		 !strcmp(mod->name, "progs/g_rock.md3")	||
		 !strcmp(mod->name, "progs/g_rock2.md3") ||
		 !strcmp(mod->name, "progs/g_light.md3") ||
	// hipnotic weapons
		 !strcmp(mod->name, "progs/g_hammer.md3") ||
		 !strcmp(mod->name, "progs/g_laserg.md3") ||
		 !strcmp(mod->name, "progs/g_prox.md3"))
		mod->flags |= EF_ROTATE;
	else if (!strcmp(mod->name, "progs/h_player.md3") ||
		 !strcmp(mod->name, "progs/gib1.md3") ||
		 !strcmp(mod->name, "progs/gib2.md3") ||
		 !strcmp(mod->name, "progs/gib3.md3") ||
		 !strcmp(mod->name, "progs/zom_gib.md3"))
		mod->flags |= EF_GIB;

// some models are special
	if (!strcmp(mod->name, "progs/player.md3") ||
		!strcmp(mod->name, "progs/vwplayer.md3") ||
		!strcmp(mod->name, "progs/player/lower.md3"))
		mod->modhint = MOD_PLAYER;
	else if (!strcmp(mod->name, "progs/flame.md3"))
		mod->modhint = MOD_FLAME;
	else if (!strcmp(mod->name, "progs/v_shot.md3")	||
		 !strcmp(mod->name, "progs/v_shot2.md3") ||
		 !strcmp(mod->name, "progs/v_nail.md3")	||
		 !strcmp(mod->name, "progs/v_nail2.md3") ||
		 !strcmp(mod->name, "progs/v_rock.md3")	||
		 !strcmp(mod->name, "progs/v_rock2.md3") ||
		 !strcmp(mod->name, "progs/v_light.md3") ||
	// hipnotic weapons
		 !strcmp(mod->name, "progs/v_laserg.md3") ||
		 !strcmp(mod->name, "progs/v_prox.md3") ||
	// rogue weapons
		 !strcmp(mod->name, "progs/v_grpple.md3") ||	// ?
		 !strcmp(mod->name, "progs/v_lava.md3") ||
		 !strcmp(mod->name, "progs/v_lava2.md3") ||
		 !strcmp(mod->name, "progs/v_multi.md3") ||
		 !strcmp(mod->name, "progs/v_multi2.md3") ||
		 !strcmp(mod->name, "progs/v_plasma.md3") ||	// ?
		 !strcmp(mod->name, "progs/v_star.md3"))		// ?
		mod->modhint = MOD_WEAPON;
	else if (!strcmp(mod->name, "progs/bolt.md3") ||
		!strcmp(mod->name, "progs/bolt2.md3") ||
		!strcmp(mod->name, "progs/bolt3.md3"))
		mod->modhint = MOD_THUNDERBOLT;
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
	else if (!strcmp(mod->name, "progs/pop.md3"))
		mod->modhint = MOD_QLTELEPORT;
	else if (!strcmp(mod->name, "progs/missile.md3"))
		mod->modhint = MOD_Q3ROCKET;	// this could be optimized: check surface names to identify if it is _really_ the Q3 missile
	else
		mod->modhint = MOD_NORMAL;

	header = (md3header_t *)buffer;

	version = LittleLong (header->version);
	if (version != MD3_VERSION)
		Sys_Error ("Mod_LoadQ3Model: %s has wrong version number (%i should be %i)", mod->name, version, MD3_VERSION);

// endian-adjust all data
	header->flags = LittleLong (header->flags);

	header->numframes = LittleLong (header->numframes);
	if (header->numframes < 1)
		Sys_Error ("Mod_LoadQ3Model: model %s has no frames", mod->name);
	else if (header->numframes > MAXMD3FRAMES)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many frames", mod->name);

	header->numtags = LittleLong (header->numtags);
	if (header->numtags > MAXMD3TAGS)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many tags", mod->name);

	header->numsurfs = LittleLong (header->numsurfs);
	if (header->numsurfs > MAXMD3SURFS)
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
		skinsfound[i] = Q_malloc (MAX_OSPATH);
		Q_snprintfz (skinsfound[i], MAX_OSPATH, "%s/%s", pathname, filelist[i].name);
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

	mod->flags |= header->flags;
	
	//joe: there's a bug with Ruohis's Shub spike model, which results the model dropping blood trail
	if (!strcmp(mod->name, "progs/teleport.md3"))
		mod->flags = 0;

// load the animation frames if loading the player model
	if (!strcmp(mod->name, cl_modelnames[mi_q3legs]))
		Mod_LoadQ3Animation ();

	texture_flag = TEX_MIPMAP;

	surf = (md3surface_t *)((byte *)header + header->ofssurfs);
	for (i = 0 ; i < header->numsurfs; i++)
	{
		// some flash models have empty surface names, so we need to assume by filename
		if (Mod_IsTransparentSurface(surf) || strstr(mod->name, "flash"))
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
				COM_StripExtension (memshader[j].name, basename);

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

qboolean Mod_IsMonsterModel(int modelindex)
{
	return modelindex == cl_modelindex[mi_fish] ||
		modelindex == cl_modelindex[mi_dog] ||
		modelindex == cl_modelindex[mi_soldier] ||
		modelindex == cl_modelindex[mi_enforcer] ||
		modelindex == cl_modelindex[mi_knight] ||
		modelindex == cl_modelindex[mi_hknight] ||
		modelindex == cl_modelindex[mi_scrag] ||
		modelindex == cl_modelindex[mi_ogre] ||
		modelindex == cl_modelindex[mi_fiend] ||
		modelindex == cl_modelindex[mi_vore] ||
		modelindex == cl_modelindex[mi_shambler] ||
		modelindex == cl_modelindex[mi_zombie] ||
		modelindex == cl_modelindex[mi_spawn];
}
