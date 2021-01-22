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
// gl_mesh.c: triangle model functions

#include "quakedef.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

model_t		*aliasmodel;
aliashdr_t	*paliashdr;

qboolean	used[8192];

// the command list holds counts and s/t values that are valid for every frame
int		commands[8192];
int		numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int		vertexorder[8192];
int		numorder;

int		allverts, alltris;

int		stripverts[128];
int		striptris[128];
int		stripcount;

/*
================
StripLength
================
*/
int StripLength (int starttri, int startv)
{
	int		m1, m2, j, k;
	mtriangle_t	*last, *check;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv+0)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+2)%3];
	m2 = last->vertindex[(startv+1)%3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri+1] ; j < pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;

		for (k = 0 ; k < 3 ; k++)
		{
			if (check->vertindex[k] != m1 || check->vertindex[(k+1)%3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[(k+2)%3];
			else
				m1 = check->vertindex[(k+2)%3];

			stripverts[stripcount+2] = check->vertindex[(k+2)%3];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}

done:
	// clear the temp used flags
	for (j = starttri + 1 ; j < pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int FanLength (int starttri, int startv)
{
	int		m1, m2, j, k;
	mtriangle_t	*last, *check;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv+0)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+0)%3];
	m2 = last->vertindex[(startv+2)%3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri+1] ; j < pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;

		for (k = 0 ; k < 3 ; k++)
		{
			if (check->vertindex[k] != m1 || check->vertindex[(k+1)%3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[(k+2)%3];

			stripverts[stripcount+2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}

done:
	// clear the temp used flags
	for (j = starttri + 1 ; j < pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
================
BuildTris

Generate a list of trifans or strips for the model, which holds for all frames
================
*/
void BuildTris (void)
{
	int		i, j, k, startv, len, bestlen, besttype, type, bestverts[1024], besttris[1024];
	float	s, t;

	// build tristrips
	numorder = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));
	for (i = 0 ; i < pheader->numtris ; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0 ; type < 2 ; type++)
//	type = 1;
		{
			for (startv = 0 ; startv < 3 ; startv++)
			{
				len = (type == 1) ? StripLength (i, startv) : FanLength (i, startv);
				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (j = 0 ; j < bestlen + 2 ; j++)
						bestverts[j] = stripverts[j];
					for (j = 0 ; j < bestlen ; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j = 0 ; j < bestlen ; j++)
			used[besttris[j]] = 1;

		commands[numcommands++] = (besttype == 1) ? (bestlen + 2) : -(bestlen + 2);

		for (j = 0 ; j < bestlen + 2 ; j++)
		{
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2;	// on back side
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			*(float *)&commands[numcommands++] = s;
			*(float *)&commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	Con_DPrintf ("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}

void ScaleVerts (byte *original, vec3_t scaled)
{
	scaled[0] = (float)original[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
	scaled[1] = (float)original[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
	scaled[2] = (float)original[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
}
/*
================
RemoveMuzzleFlash

Determines which verts belong to a muzzleflash object in a model and removes them.

It checks the distance of the verts in the source and destination animation frames,
and if the distance is longer than a given threshold, the verts of the destination
frame will be replaced with the source verts (to keep the muzzleflash hidden).
================
*/
void RemoveMuzzleFlash(char *srcframe, char *dstframe, int threshold, qboolean removeall)
{
	int		i, j;
	vec3_t	scaledcurrent, scalednext;	// scaled versions of the verts (need to prescale for comparison)
	float	vdiff;				// difference in front to back movement
	qboolean *nodraw, srcfound, dstfound;

	// get pointers to the verts
	trivertx_t	*vertscurrentframe = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	trivertx_t	*vertsnextframe = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);

	// set these pointers to the actual and next frames
	srcfound = dstfound = false;
	for (i = 0; i < paliashdr->numframes; i++)
	{
		if (!strcmp(paliashdr->frames[i].name, srcframe))
		{
			vertscurrentframe += paliashdr->frames[i].firstpose * paliashdr->poseverts;
			srcfound = true;
		}
		else if (!strcmp(paliashdr->frames[i].name, dstframe))
		{
			vertsnextframe += paliashdr->frames[i].firstpose * paliashdr->poseverts;
			dstfound = true;
		}
	}

	// if we didn't find any of the frames, return
	if (!srcfound || !dstfound)
		return;

	if (removeall)
		nodraw = Q_malloc (numorder * sizeof(qboolean));

	// now go through them and compare. we expect that (a) the animation is sensible and there's no major
	// difference between the 2 frames to be expected, and (b) any verts that do exhibit a major difference
	// can be assumed to belong to the muzzleflash
	//Con_Printf("framename: %s\n\n", srcframe);
	for (j = 0; j < numorder; j++)
	{
		ScaleVerts (vertscurrentframe->v, scaledcurrent);
		ScaleVerts (vertsnextframe->v, scalednext);

		// get difference in front to back movement
		vdiff = fabs(scalednext[0] - scaledcurrent[0]);

		// if the distance is above the given threshold, then the vertex belongs to the
		// muzzleflash object, so copy it back where it was in the previous frame
		if (removeall)
		{
			nodraw[j] = (vdiff > threshold);
		}
		else
		{
			if (vdiff > threshold)
			{
				vertsnextframe->v[0] = vertscurrentframe->v[0];
				vertsnextframe->v[1] = vertscurrentframe->v[1];
				vertsnextframe->v[2] = vertscurrentframe->v[2];
			}
		}

		//Con_Printf("distance: %f\n", vdiff);

		// next set of verts
		vertscurrentframe++;
		vertsnextframe++;
	}
	//Con_Printf("\n");

	if (removeall)
	{
		// remove the muzzleflash from all frames
		for (i = 1; i < paliashdr->numframes; i++)
		{
			// get pointers to the verts again
			vertscurrentframe = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
			vertsnextframe = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);

			// set these pointers to the 0th and i'th frames
			vertscurrentframe += paliashdr->frames[0].firstpose * paliashdr->poseverts;
			vertsnextframe += paliashdr->frames[i].firstpose * paliashdr->poseverts;

			for (j = 0; j < numorder; j++)
			{
				// copy the verts from frame 0
				if (nodraw[j])
				{
					vertsnextframe->v[0] = vertscurrentframe->v[0];
					vertsnextframe->v[1] = vertscurrentframe->v[1];
					vertsnextframe->v[2] = vertscurrentframe->v[2];
				}

				// next set of verts
				vertscurrentframe++;
				vertsnextframe++;
			}
		}

		free (nodraw);
	}
}

static void GL_MakeAliasModelDisplayLists_VBO(void);
static void GLMesh_LoadVertexBuffer(model_t *m, const aliashdr_t *hdr);

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists (model_t *mod, aliashdr_t *hdr)
{
	int			i, j, *cmds;
	trivertx_t	*verts;

	aliasmodel = mod;
	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	// Tonik: don't cache anything, because it seems just as fast
	// (if not faster) to rebuild the tris instead of loading them from disk
	BuildTris ();		// trifans or lists

	// save the data out
	paliashdr->poseverts = numorder;

	cmds = Hunk_Alloc (numcommands * 4);
	paliashdr->commands = (byte *)cmds - (byte *)paliashdr;
	memcpy (cmds, commands, numcommands * 4);

	verts = Hunk_Alloc (paliashdr->numposes * paliashdr->poseverts * sizeof(trivertx_t));
	paliashdr->posedata = (byte *)verts - (byte *)paliashdr;
	for (i = 0 ; i < paliashdr->numposes ; i++)
		for (j = 0 ; j < numorder ; j++)
			*verts++ = poseverts[i][vertexorder[j]];

	// ericw
	GL_MakeAliasModelDisplayLists_VBO();

	// there is a stupid thing in the soldier's shooting animation, namely that the muzzleflash object
	// is moved into the weapon from the body one frame earlier for no reason...
	if (mod->modhint == MOD_SOLDIER)
		RemoveMuzzleFlash("shoot3", "shoot4", 10, false);

	// code for elimination of muzzleflashes
	if (qmb_initialized && gl_part_muzzleflash.value)
	{
		if (mod->modhint == MOD_WEAPON)
			RemoveMuzzleFlash("shot1", "shot2", 10, true);
		else if (Mod_IsAnyKindOfPlayerModel(mod))
		{
			RemoveMuzzleFlash("nailatt2", "nailatt1", 10, false);
			RemoveMuzzleFlash("rockatt6", "rockatt1", 10, false);
			RemoveMuzzleFlash("shotatt3", "shotatt2", 10, false);
			RemoveMuzzleFlash("shotatt2", "shotatt1", 10, false);
		}
		else if (mod->modhint == MOD_SOLDIER)
			RemoveMuzzleFlash("shoot4", "shoot5", 10, false);
		else if (mod->modhint == MOD_ENFORCER)
			RemoveMuzzleFlash("attack5", "attack6", 10, false);
	}
}

/*
================
GL_MakeAliasModelDisplayLists_VBO

Saves data needed to build the VBO for this model on the hunk. Afterwards this
is copied to Mod_Extradata.

Original code by MH from RMQEngine
================
*/
void GL_MakeAliasModelDisplayLists_VBO(void)
{
	int i, j;
	int maxverts_vbo;
	trivertx_t *verts;
	unsigned short *indexes;
	aliasmesh_t *desc;

	if (!(gl_glsl_able && gl_vbo_able && gl_textureunits >= 3))
		return;

	// first, copy the verts onto the hunk
	verts = (trivertx_t *)Hunk_Alloc(paliashdr->numposes * paliashdr->numverts * sizeof(trivertx_t));
	paliashdr->vertexes = (byte *)verts - (byte *)paliashdr;
	for (i = 0; i<paliashdr->numposes; i++)
		for (j = 0; j<paliashdr->numverts; j++)
			verts[i*paliashdr->numverts + j] = poseverts[i][j];

	// there can never be more than this number of verts and we just put them all on the hunk
	maxverts_vbo = pheader->numtris * 3;
	desc = (aliasmesh_t *)Hunk_Alloc(sizeof(aliasmesh_t) * maxverts_vbo);

	// there will always be this number of indexes
	indexes = (unsigned short *)Hunk_Alloc(sizeof(unsigned short) * maxverts_vbo);

	pheader->indexes = (intptr_t)indexes - (intptr_t)pheader;
	pheader->meshdesc = (intptr_t)desc - (intptr_t)pheader;
	pheader->numindexes = 0;
	pheader->numverts_vbo = 0;

	for (i = 0; i < pheader->numtris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int v;

			// index into hdr->vertexes
			unsigned short vertindex = triangles[i].vertindex[j];

			// basic s/t coords
			int s = stverts[vertindex].s;
			int t = stverts[vertindex].t;

			// check for back side and adjust texcoord s
			if (!triangles[i].facesfront && stverts[vertindex].onseam) s += pheader->skinwidth / 2;

			// see does this vert already exist
			for (v = 0; v < pheader->numverts_vbo; v++)
			{
				// it could use the same xyz but have different s and t
				if (desc[v].vertindex == vertindex && (int)desc[v].st[0] == s && (int)desc[v].st[1] == t)
				{
					// exists; emit an index for it
					indexes[pheader->numindexes++] = v;

					// no need to check any more
					break;
				}
			}

			if (v == pheader->numverts_vbo)
			{
				// doesn't exist; emit a new vert and index
				indexes[pheader->numindexes++] = pheader->numverts_vbo;

				desc[pheader->numverts_vbo].vertindex = vertindex;
				desc[pheader->numverts_vbo].st[0] = s;
				desc[pheader->numverts_vbo++].st[1] = t;
			}
		}
	}

	// upload immediately
	GLMesh_LoadVertexBuffer(aliasmodel, pheader);
}

/*
================
GLMesh_LoadVertexBuffer

Upload the given alias model's mesh to a VBO

Original code by MH from RMQEngine
================
*/
static void GLMesh_LoadVertexBuffer(model_t *m, const aliashdr_t *hdr)
{
	int totalvbosize = 0;
	const aliasmesh_t *desc;
	const short *indexes;
	const trivertx_t *trivertexes;
	byte *vbodata;
	int f;

	if (!(gl_glsl_able && gl_vbo_able && gl_textureunits >= 3))
		return;

	// count the sizes we need

	// ericw -- RMQEngine stored these vbo*ofs values in aliashdr_t, but we must not
	// mutate Mod_Extradata since it might be reloaded from disk, so I moved them to qmodel_t
	// (test case: roman1.bsp from arwop, 64mb heap)
	m->vboindexofs = 0;

	m->vboxyzofs = 0;
	totalvbosize += (hdr->numposes * hdr->numverts_vbo * sizeof(meshxyz_t)); // ericw -- what RMQEngine called nummeshframes is called numposes in QuakeSpasm

	m->vbostofs = totalvbosize;
	totalvbosize += (hdr->numverts_vbo * sizeof(meshst_t));

	if (!hdr->numindexes) return;
	if (!totalvbosize) return;

	// grab the pointers to data in the extradata

	desc = (aliasmesh_t *)((byte *)hdr + hdr->meshdesc);
	indexes = (short *)((byte *)hdr + hdr->indexes);
	trivertexes = (trivertx_t *)((byte *)hdr + hdr->vertexes);

	// upload indices buffer

	qglDeleteBuffers(1, &m->meshindexesvbo);
	qglGenBuffers(1, &m->meshindexesvbo);
	qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->meshindexesvbo);
	qglBufferData(GL_ELEMENT_ARRAY_BUFFER, hdr->numindexes * sizeof(unsigned short), indexes, GL_STATIC_DRAW);

	// create the vertex buffer (empty)

	vbodata = (byte *)malloc(totalvbosize);
	memset(vbodata, 0, totalvbosize);

	// fill in the vertices at the start of the buffer
	for (f = 0; f < hdr->numposes; f++) // ericw -- what RMQEngine called nummeshframes is called numposes in QuakeSpasm
	{
		int v;
		meshxyz_t *xyz = (meshxyz_t *)(vbodata + (f * hdr->numverts_vbo * sizeof(meshxyz_t)));
		const trivertx_t *tv = trivertexes + (hdr->numverts * f);

		for (v = 0; v < hdr->numverts_vbo; v++)
		{
			trivertx_t trivert = tv[desc[v].vertindex];

			xyz[v].xyz[0] = trivert.v[0];
			xyz[v].xyz[1] = trivert.v[1];
			xyz[v].xyz[2] = trivert.v[2];
			xyz[v].xyz[3] = 1;	// need w 1 for 4 byte vertex compression

			// map the normal coordinates in [-1..1] to [-127..127] and store in an unsigned char.
			// this introduces some error (less than 0.004), but the normals were very coarse
			// to begin with
			xyz[v].normal[0] = 127 * r_avertexnormals[trivert.lightnormalindex][0];
			xyz[v].normal[1] = 127 * r_avertexnormals[trivert.lightnormalindex][1];
			xyz[v].normal[2] = 127 * r_avertexnormals[trivert.lightnormalindex][2];
			xyz[v].normal[3] = 0;	// unused; for 4-byte alignment
		}
	}

	// fill in the ST coords at the end of the buffer
	{
		meshst_t *st;
		float hscale, vscale;

		//johnfitz -- padded skins
		hscale = (float)hdr->skinwidth / (float)hdr->skinwidth;
		vscale = (float)hdr->skinheight / (float)hdr->skinheight;
		//johnfitz

		st = (meshst_t *)(vbodata + m->vbostofs);
		for (f = 0; f < hdr->numverts_vbo; f++)
		{
			st[f].st[0] = hscale * ((float)desc[f].st[0] + 0.5f) / (float)hdr->skinwidth;
			st[f].st[1] = vscale * ((float)desc[f].st[1] + 0.5f) / (float)hdr->skinheight;
		}
	}

	// upload vertexes buffer
	qglDeleteBuffers(1, &m->meshvbo);
	qglGenBuffers(1, &m->meshvbo);
	qglBindBuffer(GL_ARRAY_BUFFER, m->meshvbo);
	qglBufferData(GL_ARRAY_BUFFER, totalvbosize, vbodata, GL_STATIC_DRAW);

	free(vbodata);

	// invalidate the cached bindings
	GL_ClearBufferBindings();
}
