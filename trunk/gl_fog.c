/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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
//gl_fog.c -- global and volumetric fog

#include "quakedef.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

#define DEFAULT_DENSITY 0.0
#define DEFAULT_GRAY 0.3

float fog_density;
float fog_red;
float fog_green;
float fog_blue;

float old_density;
float old_red;
float old_green;
float old_blue;

float fade_time; //duration of fade
float fade_done; //time when fade will be done

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update (float density, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0)
	{
		//check for a fade in progress
		if (fade_done > cl.time)
		{
			float f;

			f = (fade_done - cl.time) / fade_time;
			old_density = f * old_density + (1.0 - f) * fog_density;
			old_red = f * old_red + (1.0 - f) * fog_red;
			old_green = f * old_green + (1.0 - f) * fog_green;
			old_blue = f * old_blue + (1.0 - f) * fog_blue;
		}
		else
		{
			old_density = fog_density;
			old_red = fog_red;
			old_green = fog_green;
			old_blue = fog_blue;
		}
	}

	fog_density = density;
	fog_red = red;
	fog_green = green;
	fog_blue = blue;
	fade_time = time;
	fade_done = cl.time + time;
}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{
	float density, red, green, blue, time;

	density = MSG_ReadByte() / 255.0;
	red = MSG_ReadByte() / 255.0;
	green = MSG_ReadByte() / 255.0;
	blue = MSG_ReadByte() / 255.0;
	time = max(0.0, MSG_ReadShort() / 100.0);

	Fog_Update (density, red, green, blue, time);
}

/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("usage:\n");
		Con_Printf("   fog <density>\n");
		Con_Printf("   fog <red> <green> <blue>\n");
		Con_Printf("   fog <density> <red> <green> <blue>\n");
		Con_Printf("current values:\n");
		Con_Printf("   \"density\" is \"%f\"\n", fog_density);
		Con_Printf("   \"red\" is \"%f\"\n", fog_red);
		Con_Printf("   \"green\" is \"%f\"\n", fog_green);
		Con_Printf("   \"blue\" is \"%f\"\n", fog_blue);
		break;
	case 2:
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
				   fog_red,
				   fog_green,
				   fog_blue,
				   0.0);
		break;
	case 3: //TEST
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
				   fog_red,
				   fog_green,
				   fog_blue,
				   atof(Cmd_Argv(2)));
		break;
	case 4:
		Fog_Update(fog_density,
				   bound(0.0, atof(Cmd_Argv(1)), 1.0),
				   bound(0.0, atof(Cmd_Argv(2)), 1.0),
				   bound(0.0, atof(Cmd_Argv(3)), 1.0),
				   0.0);
		break;
	case 5:
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
				   bound(0.0, atof(Cmd_Argv(2)), 1.0),
				   bound(0.0, atof(Cmd_Argv(3)), 1.0),
				   bound(0.0, atof(Cmd_Argv(4)), 1.0),
				   0.0);
		break;
	case 6: //TEST
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
				   bound(0.0, atof(Cmd_Argv(2)), 1.0),
				   bound(0.0, atof(Cmd_Argv(3)), 1.0),
				   bound(0.0, atof(Cmd_Argv(4)), 1.0),
				   atof(Cmd_Argv(5)));
		break;
	}
}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn (void)
{
	char key[128], value[4096], *data;

	//initially no fog
	fog_density = DEFAULT_DENSITY;
	fog_red = DEFAULT_GRAY;
	fog_green = DEFAULT_GRAY;
	fog_blue = DEFAULT_GRAY;

	old_density = DEFAULT_DENSITY;
	old_red = DEFAULT_GRAY;
	old_green = DEFAULT_GRAY;
	old_blue = DEFAULT_GRAY;

	fade_time = 0.0;
	fade_done = 0.0;

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
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);

		if (!strcmp("fog", key))
		{
			sscanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
			fog_density = max(0.0, fog_density);
			fog_red = bound(0.0, fog_red, 1.0);
			fog_green = bound(0.0, fog_green, 1.0);
			fog_blue = bound(0.0, fog_blue, 1.0);
		}
	}
}

/*
=============
Fog_IsWaterFog

returns true if waterfog needs to be drawn
=============
*/
qboolean Fog_IsWaterFog(void)
{
	return gl_waterfog.value && r_viewleaf->contents != CONTENTS_EMPTY && r_viewleaf->contents != CONTENTS_SOLID;
}

static float lava[4] = { 1.0f, 0.314f, 0.0f, 0.5f };
static float slime[4] = { 0.039f, 0.738f, 0.333f, 0.5f };
static float water[4] = { 0.039f, 0.584f, 0.888f, 0.5f };

/*
=============
Fog_GetColor

calculates fog color for this frame, taking into account fade times
=============
*/
float *Fog_GetColor (void)
{
	static float c[4];
	float	f;
	int		i;

	if (Fog_IsWaterFog())
	{
		switch (r_viewleaf->contents)
		{
		case CONTENTS_LAVA:
			return lava;

		case CONTENTS_SLIME:
			return slime;

		default:
			return water;
		}
	}
	else
	{
		if (fade_done > cl.time)
		{
			f = (fade_done - cl.time) / fade_time;
			c[0] = f * old_red + (1.0 - f) * fog_red;
			c[1] = f * old_green + (1.0 - f) * fog_green;
			c[2] = f * old_blue + (1.0 - f) * fog_blue;
			c[3] = 1.0;
		}
		else
		{
			c[0] = fog_red;
			c[1] = fog_green;
			c[2] = fog_blue;
			c[3] = 1.0;
		}

		//find closest 24-bit RGB value, so solid-colored sky can match the fog perfectly
		for (i = 0; i < 3; i++)
			c[i] = (float)(Q_rint(c[i] * 255)) / 255.0f;

		return c;
	}
}

/*
=============
Fog_GetDensity

returns current density of fog
=============
*/
float Fog_GetDensity (void)
{
	float f;

	if (Fog_IsWaterFog())
		return 1.0;		// joe: this is a hack to force all the Fog_GetDensity > 0 conditions to be true
	else if (fade_done > cl.time)
	{
		f = (fade_done - cl.time) / fade_time;
		return f * old_density + (1.0 - f) * fog_density;
	}
	else
		return fog_density;
}

/*
=============
Fog_SetupFrame

called at the beginning of each frame

joe: moved waterfog handling code here. Fog in liquids, from FuhQuake
=============
*/
void Fog_SetupFrame (void)
{
	fog_data.useWaterFog = Fog_IsWaterFog() ? (int)gl_waterfog.value : 0;
	fog_data.density = (Fog_IsWaterFog() && gl_waterfog.value == 2) ? 0.0002 + (0.0009 - 0.0002) * bound(0, gl_waterfog_density.value, 1) : Fog_GetDensity() / 64.0;
	fog_data.start = 150.0f;
	fog_data.end = 4250.0f - (4250.0f - 1536.0f) * bound(0, gl_waterfog_density.value, 1);
	memcpy(fog_data.color, Fog_GetColor(), sizeof(fog_data.color));

	glFogfv(GL_FOG_COLOR, fog_data.color);
	if (fog_data.useWaterFog == 2)
	{
		glFogi(GL_FOG_MODE, GL_EXP);
		glFogf(GL_FOG_DENSITY, fog_data.density);
	}
	else if (fog_data.useWaterFog == 1)
	{
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogf(GL_FOG_START, fog_data.start);
		glFogf(GL_FOG_END, fog_data.end);
	}
	else
	{
		glFogi(GL_FOG_MODE, GL_EXP2);
		glFogf(GL_FOG_DENSITY, fog_data.density);
	}
}

/*
=============
Fog_EnableGFog

called before drawing stuff that should be fogged
=============
*/
void Fog_EnableGFog ()
{
	if (Fog_GetDensity() > 0)
		glEnable(GL_FOG);
}

/*
=============
Fog_DisableGFog

called after drawing stuff that should be fogged
=============
*/
void Fog_DisableGFog ()
{
	if (Fog_GetDensity() > 0)
		glDisable(GL_FOG);
}

/*
=============
Fog_StartAdditive

called before drawing stuff that is additive blended -- sets fog color to black
=============
*/
void Fog_StartAdditive (void)
{
	vec3_t color = { 0, 0, 0 };

	if (Fog_GetDensity() > 0)
		glFogfv(GL_FOG_COLOR, color);
}

/*
=============
Fog_StopAdditive

called after drawing stuff that is additive blended -- restores fog color
=============
*/
void Fog_StopAdditive (void)
{
	if (Fog_GetDensity() > 0)
		glFogfv(GL_FOG_COLOR, Fog_GetColor());
}

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
=============
Fog_NewMap

called whenever a map is loaded
=============
*/
void Fog_NewMap (void)
{
	Fog_ParseWorldspawn (); //for global fog
}

/*
=============
Fog_Init

called when quake initializes
=============
*/
void Fog_Init (void)
{
	Cmd_AddCommand ("fog", Fog_FogCommand_f);

	//set up global fog
	fog_density = DEFAULT_DENSITY;
	fog_red = DEFAULT_GRAY;
	fog_green = DEFAULT_GRAY;
	fog_blue = DEFAULT_GRAY;

	Fog_SetupState ();
}

/*
=============
Fog_SetupState
 
ericw -- moved from Fog_Init, state that needs to be setup when a new context is created
=============
*/
void Fog_SetupState (void)
{
	glFogi(GL_FOG_MODE, GL_EXP2);
}
