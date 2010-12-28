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
