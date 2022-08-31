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
// gl_rmisc.c

#include "quakedef.h"

extern qboolean r_loadq3player;

float	map_wateralpha, map_lavaalpha, map_slimealpha;

void R_InitOtherTextures (void)
{
	underwatertexture = GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  TEX_ALPHA | TEX_COMPLAIN);
	detailtexture = GL_LoadTextureImage ("textures/detail", NULL, 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN);
	damagetexture = GL_LoadTextureImage ("textures/particles/blood_screen", NULL, 0, 0, TEX_ALPHA | TEX_COMPLAIN);
}

void CreateCheckerBoardTexture(texture_t *texture)
{
	int		x, y, m;
	byte	*dest;

	for (m = 0; m < 4; m++)
	{
		dest = (byte *)texture + texture->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++)
				*dest++ = ((y < (8 >> m)) ^ (x < (8 >> m))) ? 0 : 0x0e;
	}
}

/*
==================
R_InitTextures
==================
*/
void R_InitTextures (void)
{
// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16 + 8*8 + 4*4 + 2*2, "notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;

	CreateCheckerBoardTexture(r_notexture_mip);

	r_notexture_mip2 = Hunk_AllocName (sizeof(texture_t) + 16*16 + 8*8 + 4*4 + 2*2, "notexture2");

	r_notexture_mip2->width = r_notexture_mip2->height = 16;
	r_notexture_mip2->offsets[0] = sizeof(texture_t);
	r_notexture_mip2->offsets[1] = r_notexture_mip2->offsets[0] + 16*16;
	r_notexture_mip2->offsets[2] = r_notexture_mip2->offsets[1] + 8*8;
	r_notexture_mip2->offsets[3] = r_notexture_mip2->offsets[2] + 4*4;

	CreateCheckerBoardTexture(r_notexture_mip2);
}

int	player_fb_skins[MAX_SCOREBOARD];

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int			top, bottom, i, j, size, scaled_width, scaled_height, inwidth, inheight;
	byte		translate[256], *original, *inrow;
	unsigned	translate32[256], pixels[512*256], *out, frac, fracstep;
	model_t		*model;
	aliashdr_t	*paliashdr;
	extern qboolean	Img_HasFullbrights (byte *pixels, int size);

	GL_DisableMultitexture ();

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors & 15) << 4;

	for (i = 0 ; i < 256 ; i++)
		translate[i] = i;

	for (i = 0 ; i < 16 ; i++)
	{
		// the artists made some backward ranges. sigh.
		translate[TOP_RANGE+i] = (top < 128) ? top + i : top + 15 - i;
		translate[BOTTOM_RANGE+i] = (bottom < 128) ? bottom + i : bottom + 15 - i;
	}

	// locate the original skin pixels
	currententity = &cl_entities[1+playernum];
	if (!(model = currententity->model))
		return;		// player doesn't have a model yet
	if (model->type != mod_alias)
		return;		// only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata (model);
	size = paliashdr->skinwidth * paliashdr->skinheight;
	if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins)
	{
		Con_DPrintf ("(%d): Invalid player skin #%d\n", playernum, currententity->skinnum);
		original = (byte *)paliashdr + paliashdr->texels[0];
	}
	else
	{
		original = (byte *)paliashdr + paliashdr->texels[currententity->skinnum];
	}

	if (size & 3)
		Sys_Error ("R_TranslatePlayerSkin: bad size (%d)", size);

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;

	// because this happens during gameplay, do it fast
	// instead of sending it through GL_Upload8
	GL_Bind (playertextures + playernum);

	// c'mon, it's 2006 already....
	if (gl_lerptextures.value)
	{
		byte *translated;

		// 8 bit
		translated = Q_malloc (inwidth * inheight);

		for (i = 0 ; i < size ; i++)
			translated[i] = translate[original[i]];

		GL_Upload8 (translated, inwidth, inheight, 0);

		player_fb_skins[playernum] = 0;
		if (Img_HasFullbrights(original, inwidth * inheight))
		{
			player_fb_skins[playernum] = playertextures + playernum + MAX_SCOREBOARD;

			GL_Bind (player_fb_skins[playernum]);
			GL_Upload8 (translated, inwidth, inheight, TEX_FULLBRIGHT);
		}

		free (translated);
	}
	else
	{
		scaled_width = min(gl_max_size.value, 512);
		scaled_height = min(gl_max_size.value, 256);

		// allow users to crunch sizes down even more if they want
		scaled_width >>= (int)gl_playermip.value;
		scaled_height >>= (int)gl_playermip.value;

		scaled_width = max(scaled_width, 1);
		scaled_height = max(scaled_height, 1);

		for (i = 0 ; i < 256 ; i++)
			translate32[i] = d_8to24table[translate[i]];

		out = pixels;
		memset (pixels, 0, sizeof(pixels));
		fracstep = (inwidth << 16) / scaled_width;

		for (i = 0 ; i < scaled_height ; i++, out += scaled_width)
		{
			inrow = original + inwidth * (i * inheight / scaled_height);
			frac = fracstep >> 1;
			for (j = 0 ; j < scaled_width ; j++, frac += fracstep)
				out[j] = translate32[inrow[frac>>16]];
		}

		glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		player_fb_skins[playernum] = 0;
		if (Img_HasFullbrights(original, inwidth * inheight))
		{
			player_fb_skins[playernum] = playertextures + playernum + MAX_SCOREBOARD;

			GL_Bind (player_fb_skins[playernum]);

			out = pixels;
			memset (pixels, 0, sizeof(pixels));
			fracstep = (inwidth << 16) / scaled_width;

			// make all non-fullbright colors transparent
			for (i = 0 ; i < scaled_height ; i++, out += scaled_width)
			{
				inrow = original + inwidth * (i * inheight / scaled_height);
				frac = fracstep >> 1;
				for (j = 0 ; j < scaled_width ; j++, frac += fracstep)
				{
					if (inrow[frac>>16] < 224)
						out[j] = translate32[inrow[frac>>16]] & 0x00FFFFFF; // transparent
					else
						out[j] = translate32[inrow[frac>>16]]; // fullbright
				}
			}

			glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
	}
}

void R_PreMapLoad (char *mapname)
{
	Cvar_ForceSet (&cl_mapname, mapname);
	lightmode = 2;
}

qboolean OnChange_r_wateralpha(cvar_t *var, char *string)
{
	float	newval;

	newval = Q_atof(string);
	if (newval == r_wateralpha.value)
		return false;
	
	map_wateralpha =
	map_lavaalpha = 
	map_slimealpha = newval;

	return false;
}

/*
====================
GL_WaterAlphaForSurface -- ericw
====================
*/
float GL_WaterAlphaForSurface(msurface_t *fa)
{
	if (fa->flags & SURF_DRAWLAVA)
		return map_lavaalpha > 0 ? map_lavaalpha : map_wateralpha;
	else if (fa->flags & SURF_DRAWSLIME)
		return map_slimealpha > 0 ? map_slimealpha : map_wateralpha;
	else
		return map_wateralpha;
}

/*
================
GL_WaterAlphaForEntitySurface -- ericw

Returns the water alpha to use for the entity and surface combination.
================
*/
float GL_WaterAlphaForEntitySurface(entity_t *ent, msurface_t *s)
{
	return (ent == NULL || !ISTRANSPARENT(ent)) ? GL_WaterAlphaForSurface(s) : ent->transparency;
}

/*
=============
R_ParseWorldspawn

called at map load
=============
*/
static void R_ParseWorldspawn(void)
{
	char key[128], value[4096], *data;

	map_wateralpha = r_wateralpha.value;
	map_lavaalpha = r_wateralpha.value;
	map_slimealpha = r_wateralpha.value;

	data = COM_Parse(cl.worldmodel->entities);
	if (!data)
		return; // error
	if (com_token[0] != '{')
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

		if (!strcmp("wateralpha", key))
			map_wateralpha = atof(value);

		if (!strcmp("lavaalpha", key))
			map_lavaalpha = atof(value);

		if (!strcmp("slimealpha", key))
			map_slimealpha = atof(value);
	}
}

/*
===============
R_NewMap
===============
*/
void R_NewMap(void)
{
	int	i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset(&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();
	R_ClearDecals();

	GL_BuildLightmaps();
	GL_BuildBModelVertexBuffer();

	r_framecount = 0; //johnfitz -- paranoid?
	r_visframecount = 0; //johnfitz -- paranoid?

	Sky_NewMap(); //johnfitz -- skybox in worldspawn 
	Fog_NewMap(); //johnfitz -- global fog in worldspawn 
	R_ParseWorldspawn(); //ericw -- wateralpha, lavaalpha, telealpha, slimealpha in worldspawn 

	if (r_loadq3player)
	{
		memset(&q3player_body, 0, sizeof(tagentity_t));
		memset(&q3player_head, 0, sizeof(tagentity_t));
		memset(&q3player_weapon, 0, sizeof(tagentity_t));
		memset(&q3player_weapon_flash, 0, sizeof(tagentity_t));
	}
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int		i;
	float	start, stop, time;

	if (cls.state != ca_connected)
		return;

	glDrawBuffer (GL_FRONT);
	glFinish ();

	start = Sys_DoubleTime ();
	for (i = 0 ; i < 128 ; i++)
	{
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_DoubleTime ();
	time = stop - start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128.0 / time);

	glDrawBuffer (GL_BACK);
	GL_EndRendering ();
}

void D_FlushCaches (void)
{
}

static GLuint gl_programs[16];
static int gl_num_programs;

static qboolean GL_CheckShader(GLuint shader)
{
	GLint status;
	qglGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];

		memset(infolog, 0, sizeof(infolog));
		qglGetShaderInfoLog(shader, sizeof(infolog), NULL, infolog);

		Con_Printf("GLSL program failed to compile: %s", infolog);

		return false;
	}
	return true;
}

static qboolean GL_CheckProgram(GLuint program)
{
	GLint status;
	qglGetProgramiv(program, GL_LINK_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];

		memset(infolog, 0, sizeof(infolog));
		qglGetProgramInfoLog(program, sizeof(infolog), NULL, infolog);

		Con_Printf("GLSL program failed to link: %s", infolog);

		return false;
	}
	return true;
}

/*
=============
GL_GetUniformLocation
=============
*/
GLint GL_GetUniformLocation(GLuint *programPtr, const char *name)
{
	GLint location;

	if (!programPtr)
		return -1;

	location = qglGetUniformLocation(*programPtr, name);
	if (location == -1)
	{
		Con_Printf("GL_GetUniformLocationFunc %s failed\n", name);
		*programPtr = 0;
	}
	return location;
}

/*
====================
GL_CreateProgram

Compiles and returns GLSL program.
====================
*/
GLuint GL_CreateProgram(const GLchar *vertSource, const GLchar *fragSource, int numbindings, const glsl_attrib_binding_t *bindings)
{
	int i;
	GLuint program, vertShader, fragShader;

	if (!gl_glsl_able)
		return 0;

	vertShader = qglCreateShader(GL_VERTEX_SHADER);
	qglShaderSource(vertShader, 1, &vertSource, NULL);
	qglCompileShader(vertShader);
	if (!GL_CheckShader(vertShader))
	{
		qglDeleteShader(vertShader);
		return 0;
	}

	fragShader = qglCreateShader(GL_FRAGMENT_SHADER);
	qglShaderSource(fragShader, 1, &fragSource, NULL);
	qglCompileShader(fragShader);
	if (!GL_CheckShader(fragShader))
	{
		qglDeleteShader(vertShader);
		qglDeleteShader(fragShader);
		return 0;
	}

	program = qglCreateProgram();
	qglAttachShader(program, vertShader);
	qglDeleteShader(vertShader);
	qglAttachShader(program, fragShader);
	qglDeleteShader(fragShader);

	for (i = 0; i < numbindings; i++)
	{
		qglBindAttribLocation(program, bindings[i].attrib, bindings[i].name);
	}

	qglLinkProgram(program);

	if (!GL_CheckProgram(program))
	{
		qglDeleteProgram(program);
		return 0;
	}
	else
	{
		if (gl_num_programs == (sizeof(gl_programs) / sizeof(GLuint)))
			Host_Error("gl_programs overflow");

		gl_programs[gl_num_programs] = program;
		gl_num_programs++;

		return program;
	}
}

/*
====================
R_DeleteShaders

Deletes any GLSL programs that have been created.
====================
*/
void R_DeleteShaders(void)
{
	int i;

	if (!gl_glsl_able)
		return;

	for (i = 0; i < gl_num_programs; i++)
	{
		qglDeleteProgram(gl_programs[i]);
		gl_programs[i] = 0;
	}
	gl_num_programs = 0;
}

GLuint current_array_buffer;
GLuint current_element_array_buffer;

/*
====================
GL_BindBuffer

glBindBuffer wrapper
====================
*/
void GL_BindBuffer(GLenum target, GLuint buffer)
{
	GLuint *cache;

	if (!gl_vbo_able)
		return;

	switch (target)
	{
	case GL_ARRAY_BUFFER:
		cache = &current_array_buffer;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		cache = &current_element_array_buffer;
		break;
	default:
		Host_Error("GL_BindBuffer: unsupported target %d", (int)target);
		return;
	}

	if (*cache != buffer)
	{
		*cache = buffer;
		qglBindBuffer(target, *cache);
	}
}

/*
====================
GL_ClearBufferBindings

This must be called if you do anything that could make the cached bindings
invalid (e.g. manually binding, destroying the context).
====================
*/
void GL_ClearBufferBindings()
{
	if (!gl_vbo_able)
		return;

	current_array_buffer = 0;
	current_element_array_buffer = 0;
	qglBindBuffer(GL_ARRAY_BUFFER, 0);
	qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
