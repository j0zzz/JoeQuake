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
// view.c -- player eye positioning

#include "quakedef.h"
#include <time.h>	// cl_clock

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

#ifndef GLQUAKE
cvar_t	lcd_x = {"lcd_x", "0"};
cvar_t	lcd_yaw = {"lcd_yaw", "0"};
#endif

cvar_t	scr_ofsx = {"scr_ofsx", "0"};
cvar_t	scr_ofsy = {"scr_ofsy", "0"};
cvar_t	scr_ofsz = {"scr_ofsz", "0"};

cvar_t	scr_clock = {"cl_clock", "0"};
cvar_t	scr_clock_x = {"cl_clock_x", "0"};
cvar_t	scr_clock_y = {"cl_clock_y", "-1"};

cvar_t	show_speed = {"show_speed", "0"};

cvar_t	show_fps = {"show_fps", "0"};
cvar_t	show_fps_x = {"show_fps_x", "-5"};
cvar_t	show_fps_y = {"show_fps_y", "-1"};

cvar_t	show_stats = {"show_stats", "1"};
cvar_t	show_stats_small = {"show_stats_small", "0"};
cvar_t	show_stats_length = {"show_stats_length", "0.5"};

cvar_t	cl_rollspeed = {"cl_rollspeed", "200"};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0"};

cvar_t	cl_bob = {"cl_bob", "0.02"};
cvar_t	cl_bobcycle = {"cl_bobcycle", "0.6"};
cvar_t	cl_bobup = {"cl_bobup", "0.5"};
cvar_t	cl_oldbob = {"cl_oldbob", "0"};
cvar_t	cl_oldgunposition = {"cl_oldgunposition", "0"};
cvar_t	cl_hand = {"cl_hand", "0"};

cvar_t	v_kicktime = {"v_kicktime", "0.5"};
cvar_t	v_kickroll = {"v_kickroll", "0.6"};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6"};
cvar_t	v_gunkick = {"v_gunkick", "0"};

cvar_t	v_iyaw_cycle = {"v_iyaw_cycle", "2"};
cvar_t	v_iroll_cycle = {"v_iroll_cycle", "0.5"};
cvar_t	v_ipitch_cycle = {"v_ipitch_cycle", "1"};
cvar_t	v_iyaw_level = {"v_iyaw_level", "0.3"};
cvar_t	v_iroll_level = {"v_iroll_level", "0.1"};
cvar_t	v_ipitch_level = {"v_ipitch_level", "0.3"};

cvar_t	v_idlescale = {"v_idlescale", "0"};

cvar_t	crosshair = {"crosshair", "0", CVAR_ARCHIVE};
cvar_t	crosshaircolor = {"crosshaircolor", "79", CVAR_ARCHIVE};
cvar_t	crosshairsize = {"crosshairsize", "1"};
cvar_t	cl_crossx = {"cl_crossx", "0"};
cvar_t	cl_crossy = {"cl_crossy", "0"};

cvar_t  v_contentblend = {"v_contentblend", "1"};
cvar_t	v_damagecshift = {"v_damagecshift", "1"};
cvar_t	v_quadcshift = {"v_quadcshift", "1"};
cvar_t	v_suitcshift = {"v_suitcshift", "1"};
cvar_t	v_ringcshift = {"v_ringcshift", "1"};
cvar_t	v_pentcshift = {"v_pentcshift", "1"};

#ifdef GLQUAKE
cvar_t	v_dlightcshift = {"v_dlightcshift", "1"};
#endif

cvar_t	v_bonusflash = {"cl_bonusflash", "1"};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;
float	xyspeed;


/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
vec3_t	right;

float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign, side;

	AngleVectors (angles, NULL, right, NULL);
	side = DotProduct(velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	side = (side < cl_rollspeed.value) ? side * cl_rollangle.value / cl_rollspeed.value : cl_rollangle.value;

	return side * sign;
}

/*
===============
V_CalcBob
===============
*/
float V_CalcBob (void)
{
	float			cycle;
	static float	bob;

	if (cl_bobcycle.value <= 0)
		return 0;

	cycle = cl.time - (int)(cl.time / cl_bobcycle.value) * cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI * (cycle - cl_bobup.value) / (1.0 - cl_bobup.value);

	bob = xyspeed * cl_bob.value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
	bob = bound(-7, bob, 4);

	return bob;
}

//=============================================================================

cvar_t	v_centermove = {"v_centermove", "0.15"};
cvar_t	v_centerspeed = {"v_centerspeed", "500"};

void V_StartPitchDrift (void)
{
	if (cl.laststop == cl.time)
		return;		// something else is keeping it from drifting

	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when 
===============
*/
void V_DriftPitch (void)
{
	float	delta, move;

	if (noclip_anglehack || !cl.onground || cls.demoplayback)
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if (fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
			cl.driftmove = 0;
		else
			cl.driftmove += host_frametime;

		if (cl.driftmove > v_centermove.value)
			V_StartPitchDrift ();

		return;
	}

	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = host_frametime * cl.pitchvel;
	cl.pitchvel += host_frametime * v_centerspeed.value;

//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}

/*
============================================================================== 
 
				PALETTE FLASHES 
 
============================================================================== 
*/ 
  
cshift_t	cshift_empty = {{130, 80, 50}, 0};
cshift_t	cshift_water = {{130, 80, 50}, 128};
cshift_t	cshift_slime = {{0, 25, 5}, 150};
cshift_t	cshift_lava = {{255, 80, 0}, 150};

#ifdef	GLQUAKE

cvar_t		gl_cshiftpercent = {"gl_cshiftpercent", "100"};
cvar_t		gl_hwblend = {"gl_hwblend", "0"};
float		v_blend[4];		// rgba 0.0 - 1.0
cvar_t		v_gamma = {"gl_gamma", "0.7", CVAR_ARCHIVE};
cvar_t		v_contrast = {"gl_contrast", "1.7", CVAR_ARCHIVE};
unsigned short	ramps[3][256];

#else

byte		gammatable[256];	// palette is sent through this
byte		current_pal[768];	// Tonik: used for screenshots
cvar_t		v_gamma = {"gamma", "0.7", CVAR_ARCHIVE};
cvar_t		v_contrast = {"contrast", "1.7", CVAR_ARCHIVE};

#endif

#ifndef GLQUAKE
void BuildGammaTable (float g, float c)
{
	int	i, inf;

	g = bound(0.3, g, 3);
	c = bound(1, c, 3);

	if (g == 1 && c == 1)
	{
		for (i=0 ; i<256 ; i++)
			gammatable[i] = i;
		return;
	}

	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow((i + 0.5) / 255.5 * c, g) + 0.5;
		gammatable[i] = bound(0, inf, 255);
	}
}

/*
=================
V_CheckGamma
=================
*/
qboolean V_CheckGamma (void)
{
	static	float	old_gamma, old_contrast;

	if (v_gamma.value == old_gamma && v_contrast.value == old_contrast)
		return false;

	old_gamma = v_gamma.value;
	old_contrast = v_contrast.value;

	BuildGammaTable (v_gamma.value, v_contrast.value);
	vid.recalc_refdef = 1;			// force a surface cache flush

	return true;
}
#endif // GLQUAKE

double	damagetime = 0, damagecount;
vec3_t	damage_screen_pos;

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int			i, armor, blood;
	float		side, fraction;
	vec3_t		from, forward, right, up;
	entity_t	*ent;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	damagecount = blood * 0.5 + armor * 0.5;
	if (damagecount < 10)
		damagecount = 10;

	damagetime = cl.time;														// joe: for Q3 on-screen damage splash
	damage_screen_pos[0] = (vid.width >> 2) + (rand() % (vid.width >> 1));		//
	damage_screen_pos[1] = (vid.height >> 2) + (rand() % (vid.height >> 1));	//

	cl.faceanimtime = cl.time + 0.2;	// put sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3 * damagecount;
	cl.cshifts[CSHIFT_DAMAGE].percent = bound(0, cl.cshifts[CSHIFT_DAMAGE].percent, 150);

	fraction = bound(0, v_damagecshift.value, 1);
	cl.cshifts[CSHIFT_DAMAGE].percent *= fraction;

	if (armor > blood)		
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

// calculate view angle kicks
	ent = &cl_entities[cl.viewentity];
	
	VectorSubtract (from, ent->origin, from);
	VectorNormalize (from);
	
	AngleVectors (ent->angles, forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll = damagecount * side * v_kickroll.value;
	
	side = DotProduct (from, forward);
	v_dmg_pitch = damagecount * side * v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}

/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}

/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	if (!v_bonusflash.value)
		return;

	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	if (!v_contentblend.value)
	{
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
#ifdef GLQUAKE
		cl.cshifts[CSHIFT_CONTENTS].percent *= 100;
#endif
		return;
	}

	switch (contents)
	{
	case CONTENTS_EMPTY:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;

	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;

	case CONTENTS_SOLID:
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;

	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}

	if (v_contentblend.value > 0 && v_contentblend.value < 1 && contents != CONTENTS_EMPTY)
		cl.cshifts[CSHIFT_CONTENTS].percent *= v_contentblend.value;

#ifdef GLQUAKE
	if (contents != CONTENTS_EMPTY)
	{
		if (!gl_polyblend.value)
			cl.cshifts[CSHIFT_CONTENTS].percent = 0;
		else
			cl.cshifts[CSHIFT_CONTENTS].percent *= gl_cshiftpercent.value;
	}
	else
	{
		cl.cshifts[CSHIFT_CONTENTS].percent *= 100;
	}
#endif
}

/*
=============
V_AddWaterFog

Fog in liquids, from FuhQuake
=============
*/
#ifdef GLQUAKE
void V_AddWaterfog (int contents)
{
	float	*colors;
	float	lava[4] = {1.0f, 0.314f, 0.0f, 0.5f};
	float	slime[4] = {0.039f, 0.738f, 0.333f, 0.5f};
	float	water[4] = {0.039f, 0.584f, 0.888f, 0.5f};

	if (!gl_waterfog.value || contents == CONTENTS_EMPTY || contents == CONTENTS_SOLID)
	{
		glDisable (GL_FOG);
		return;
	}

	switch (contents)
	{
	case CONTENTS_LAVA:
		colors = lava;
		break;

	case CONTENTS_SLIME:
		colors = slime;
		break;

	default:
		colors = water;
		break;
	}

	glFogfv (GL_FOG_COLOR, colors);
	if (((int)gl_waterfog.value) == 2)
	{
		glFogf (GL_FOG_DENSITY, 0.0002 + (0.0009 - 0.0002) * bound(0, gl_waterfog_density.value, 1));
		glFogi (GL_FOG_MODE, GL_EXP);
	}
	else
	{
		glFogi (GL_FOG_MODE, GL_LINEAR);
		glFogf (GL_FOG_START, 150.0f);	
		glFogf (GL_FOG_END, 4250.0f - (4250.0f - 1536.0f) * bound(0, gl_waterfog_density.value, 1));
	}
	glEnable(GL_FOG);
}
#endif

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	float	fraction;

	if (cl.items & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		fraction = bound(0, v_quadcshift.value, 1);
		cl.cshifts[CSHIFT_POWERUP].percent = 30 * fraction;
	}
	else if (cl.items & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		fraction = bound(0, v_suitcshift.value, 1);
		cl.cshifts[CSHIFT_POWERUP].percent = 20 * fraction;
	}
	else if (cl.items & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		fraction = bound(0, v_ringcshift.value, 1);
		cl.cshifts[CSHIFT_POWERUP].percent = 100 * fraction;
	}
	else if (cl.items & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		fraction = bound(0, v_pentcshift.value, 1);
		cl.cshifts[CSHIFT_POWERUP].percent = 30 * fraction;
	}
	else
	{
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
}

/*
=============
V_CalcBlend
=============
*/
#ifdef	GLQUAKE
void V_CalcBlend (void)
{
	int	j;
	float	r, g, b, a, a2;

	r = g = b = a = 0;

	if (cls.state != ca_connected)
	{
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
	else
	{
		V_CalcPowerupCshift ();
	}

	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	for (j=0 ; j<NUM_CSHIFTS ; j++)	
	{
		if ((!gl_cshiftpercent.value || !gl_polyblend.value) && j != CSHIFT_CONTENTS)
			continue;

		if (j == CSHIFT_CONTENTS)
			a2 = cl.cshifts[j].percent / 100.0 / 255.0;
		else
			a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;

		if (!a2)
			continue;

		a = a + a2 * (1 - a);
		a2 /= a;

		r = r * (1 - a2) + cl.cshifts[j].destcolor[0] * a2;
		g = g * (1 - a2) + cl.cshifts[j].destcolor[1] * a2;
		b = b * (1 - a2) + cl.cshifts[j].destcolor[2] * a2;
	}

	v_blend[0] = r / 255.0;
	v_blend[1] = g / 255.0;
	v_blend[2] = b / 255.0;
	v_blend[3] = bound(0, a, 1);
}

void V_AddLightBlend (float r, float g, float b, float a2)
{
	float	a;

	if (!gl_polyblend.value || !gl_cshiftpercent.value || !v_dlightcshift.value)
		return;

	a2 = a2 * bound(0, v_dlightcshift.value, 1) * gl_cshiftpercent.value / 100.0;

	v_blend[3] = a = v_blend[3] + a2 * (1 - v_blend[3]);

	if (!a)
		return;

	a2 /= a;

	v_blend[0] = v_blend[0] * (1 - a2) + r * a2;
	v_blend[1] = v_blend[1] * (1 - a2) + g * a2;
	v_blend[2] = v_blend[2] * (1 - a2) + b * a2;
}
#endif

/*
=============
V_UpdatePalette
=============
*/
#ifdef	GLQUAKE
void V_UpdatePalette (void)
{
	int		i, j, c;
	qboolean	new;
	float		a, rgb[3], gamma, contrast;
	static	float	prev_blend[4], old_gamma, old_contrast, old_hwblend;
	extern	float	vid_gamma;

	new = false;

	for (i=0 ; i<4 ; i++)
	{
		if (v_blend[i] != prev_blend[i])
		{
			new = true;
			prev_blend[i] = v_blend[i];
		}
	}

	gamma = bound(0.3, v_gamma.value, 3);
	if (gamma != old_gamma)
	{
		old_gamma = gamma;
		new = true;
	}

	contrast = bound(1, v_contrast.value, 3);
	if (contrast != old_contrast)
	{
		old_contrast = contrast;
		new = true;
	}

	if (gl_hwblend.value != old_hwblend)
	{
		new = true;
		old_hwblend = gl_hwblend.value;
	}

	if (!new)
		return;

	a = v_blend[3];

	if (!vid_hwgamma_enabled || !gl_hwblend.value)
		a = 0;

	rgb[0] = 255 * v_blend[0] * a;
	rgb[1] = 255 * v_blend[1] * a;
	rgb[2] = 255 * v_blend[2] * a;

	a = 1 - a;

	if (vid_gamma != 1.0)
	{
		contrast = pow (contrast, vid_gamma);
		gamma /= vid_gamma;
	}

	for (i=0 ; i<256 ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			// apply blend and contrast
			c = (i*a + rgb[j]) * contrast;
			if (c > 255)
				c = 255;
			// apply gamma
			c = 255 * pow((c + 0.5) / 255.5, gamma) + 0.5;
			c = bound(0, c, 255);
			ramps[j][i] = c << 8;
		}
	}

	VID_SetDeviceGammaRamp ((unsigned short *)ramps);
}
#else	// !GLQUAKE
void V_UpdatePalette (void)
{
	int		i, j, r, g, b;
	qboolean	new, force;
	byte		*basepal, *newpal;
	static cshift_t	prev_cshifts[NUM_CSHIFTS];

	if (cls.state != ca_connected)
	{
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
	else
	{
		V_CalcPowerupCshift ();
	}

	new = false;

	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != prev_cshifts[i].percent)
		{
			new = true;
			prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
		{
			if (cl.cshifts[i].destcolor[j] != prev_cshifts[i].destcolor[j])
			{
				new = true;
				prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
		}
	}

	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	force = V_CheckGamma ();
	if (!new && !force)
		return;

	basepal = host_basepal;
	newpal = current_pal;	// Tonik: so we can use current_pal for screenshots

	for (i=0 ; i<256 ; i++)
	{
		r = basepal[0];
		g = basepal[1];
		b = basepal[2];
		basepal += 3;

		for (j=0 ; j<NUM_CSHIFTS ; j++)	
		{
			r += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[0]-r)) >> 8;
			g += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[1]-g)) >> 8;
			b += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[2]-b)) >> 8;
		}

		newpal[0] = gammatable[r];
		newpal[1] = gammatable[g];
		newpal[2] = gammatable[b];
		newpal += 3;
	}

	VID_ShiftPalette (current_pal);
}
#endif	// !GLQUAKE

/* 
============================================================================== 
 
				VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

#define	LAND_DEFLECT_TIME	150
#define	LAND_RETURN_TIME	300

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{	
	float			yaw, pitch, move, scale, /*fracsin,*/ bobfracsin, delta, landchange;
	static float	oldyaw = 0, oldpitch = 0, prevbobfracsin1 = 2.0, prevbobfracsin2 = 2.0, landtime = 0, topheight = 0, prevtopheight = 0, bobtime = 0;
	static int		turnaround = 1;
	static qboolean oldonground = false;
#ifdef GLQUAKE
	extern qboolean player_jumped;
	extern kbutton_t in_jump;
#endif

	cl.viewent.angles[PITCH] = -r_refdef.viewangles[PITCH];
	cl.viewent.angles[YAW] = r_refdef.viewangles[YAW];
	cl.viewent.angles[ROLL] = r_refdef.viewangles[ROLL];

	cl.viewent.angles[PITCH] -= v_idlescale.value * sin(cl.time * v_ipitch_cycle.value) * v_ipitch_level.value;
	cl.viewent.angles[YAW] -= v_idlescale.value * sin(cl.time * v_iyaw_cycle.value) * v_iyaw_level.value;
	cl.viewent.angles[ROLL] -= v_idlescale.value * sin(cl.time * v_iroll_cycle.value) * v_iroll_level.value;

	//if (in_jump.state & 1)	// not working when demoplayback
	if (!cl.onground)
	{
		player_jumped = true;
	}

	if (player_jumped && cl.onground)
	{
		player_jumped = false;
	}

	if (cl_oldbob.value)
	{
		yaw = r_refdef.viewangles[YAW];
		pitch = -r_refdef.viewangles[PITCH];

		yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
		yaw = bound(-10, yaw, 10);

		pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
		pitch = bound(-10, pitch, 10);

		move = host_frametime * 20;
		if (yaw > oldyaw)
		{
			if (oldyaw + move < yaw)
				yaw = oldyaw + move;
		}
		else
		{
			if (oldyaw - move > yaw)
				yaw = oldyaw - move;
		}
	
		if (pitch > oldpitch)
		{
			if (oldpitch + move < pitch)
				pitch = oldpitch + move;
		}
		else
		{
			if (oldpitch - move > pitch)
				pitch = oldpitch - move;
		}
	
		oldyaw = yaw;
		oldpitch = pitch;

		cl.viewent.angles[PITCH] -= pitch;
		cl.viewent.angles[YAW] -= yaw;
	}
	else
	{
#ifndef GLQUAKE
		extern cvar_t r_drawviewmodel;
#endif

		if (r_drawviewmodel.value == 2)
		{
			return;
		}

		if (!player_jumped && key_dest != key_console)
		{
			if (oldonground == player_jumped)
			{
				// on odd legs, invert some angles
				bobtime += 0.01388888888888888888888888888889;
				//Con_Printf("%f\n", bobtime);
				bobfracsin = fabs(sin((((int)(bobtime * 300)) & 127) / 127.0 * M_PI));
				if (prevbobfracsin2 >= prevbobfracsin1 && prevbobfracsin1 < bobfracsin)
				{
					turnaround = -turnaround;
				}
				scale = xyspeed * turnaround;

				// gun angles from bobbing
				cl.viewent.angles[PITCH] -= xyspeed * bobfracsin * 0.005;
				cl.viewent.angles[YAW] += scale * bobfracsin * 0.01;
				cl.viewent.angles[ROLL] += scale * bobfracsin * 0.005;

				if (bobfracsin != prevbobfracsin1)
				{
					prevbobfracsin2 = prevbobfracsin1;
					prevbobfracsin1 = bobfracsin;
				}

				// add angles based on bob
				r_refdef.viewangles[PITCH] += xyspeed * bobfracsin * 0.002;
				r_refdef.viewangles[ROLL] += xyspeed * bobfracsin * 0.002 * turnaround;
			}
		}

		// drop the weapon when landing

		// we just reached ground
		if (player_jumped != oldonground)
		{
			if (!player_jumped)
			{
				landtime = cl.time;
				prevtopheight = topheight;
			}
			else
			{
				topheight = -99999;
			}
		}
	
		// search for the top height value if not on ground
		if (player_jumped && cl.viewent.origin[2] > topheight)
		{
			topheight = cl.viewent.origin[2];
		}
	
		// weapon drop amount is the difference between top and actual height
		landchange = (cl.viewent.origin[2] - prevtopheight) / 5.375;
		// never drop way too much
		landchange = max(landchange, -24);

		// count the drop
		delta = (cl.time - landtime) * 1000;
		if (delta > 0 && delta < LAND_DEFLECT_TIME)
		{
			cl.viewent.origin[2] += landchange * 0.25 * delta / LAND_DEFLECT_TIME;
		}
		else if (delta > 0 && delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME)
		{
			cl.viewent.origin[2] += landchange * 0.25 * (LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
		}
		oldonground = player_jumped;

		// idle drift
		//scale = xyspeed + 40;
		//fracsin = sin(cl.time);
		//cl.viewent.angles[PITCH] -= scale * fracsin * 0.01;
		//cl.viewent.angles[YAW] += scale * fracsin * 0.01;
		//cl.viewent.angles[ROLL] += scale * fracsin * 0.01;
	}
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
	entity_t	*ent;
	
	ent = &cl_entities[cl.viewentity];

	// absolutely bound refresh reletive to entity clipping hull
	// so the view can never be inside a solid wall
	r_refdef.vieworg[0] = max(r_refdef.vieworg[0], ent->origin[0] - 14);
	r_refdef.vieworg[0] = min(r_refdef.vieworg[0], ent->origin[0] + 14);
	r_refdef.vieworg[1] = max(r_refdef.vieworg[1], ent->origin[1] - 14);
	r_refdef.vieworg[1] = min(r_refdef.vieworg[1], ent->origin[1] + 14);
	r_refdef.vieworg[2] = max(r_refdef.vieworg[2], ent->origin[2] - 22);
	r_refdef.vieworg[2] = min(r_refdef.vieworg[2], ent->origin[2] + 30);
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.time * v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.time * v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.time * v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float	side;

	side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		r_refdef.viewangles[ROLL] += v_dmg_time / v_kicktime.value * v_dmg_roll;
		r_refdef.viewangles[PITCH] += v_dmg_time / v_kicktime.value * v_dmg_pitch;
		v_dmg_time -= host_frametime;
	}
}

void V_AddViewWeapon (float bob)
{
	int			i;
	float		fovOffset;
	vec3_t		forward, right, up;
	entity_t	*view;
	extern cvar_t scr_fov;

	view = &cl.viewent;

	// origin
	AngleVectors (r_refdef.viewangles, forward, right, up);

	for (i = 0; i < 3; i++)
	{
		r_refdef.vieworg[i] += scr_ofsx.value * forward[i] + scr_ofsy.value * right[i] + scr_ofsz.value * up[i];
	}

	V_BoundOffsets ();

	VectorCopy (r_refdef.vieworg, view->origin);
	VectorMA (view->origin, bob * 0.4, forward, view->origin);

	// angles
	CalcGunAngle ();

	// drop gun lower at higher fov
	if (!cl_oldgunposition.value)
	{
		fovOffset = (scr_fov.value > 90) ? -0.2 * (scr_fov.value - 90) : 0;
		VectorMA (view->origin, fovOffset, up, view->origin);
	}

	view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->frame = cl.stats[STAT_WEAPONFRAME];
	view->colormap = vid.colormap;
}

/*
==================
V_CalcIntermissionRefdef
==================
*/
void V_CalcIntermissionRefdef (void)
{
	entity_t	*ent, *view;
	float		old;

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
	// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->origin, r_refdef.vieworg);
	VectorCopy (ent->angles, r_refdef.viewangles);
	view->model = NULL;

	// always idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle ();
	v_idlescale.value = old;
}

float	cl_punchangle, cl_ideal_punchangle;

/*
==================
V_CalcRefdef
==================
*/
void V_CalcRefdef (void)
{
	entity_t	*ent;
	vec3_t		forward;
	float		bob;

	V_DriftPitch ();

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];

	// transform the view offset by the model's matrix to get the offset from
	// model origin for the view
	ent->angles[YAW] = cl.viewangles[YAW];		// the model should face the view dir
	ent->angles[PITCH] = -cl.viewangles[PITCH];	//
										
	xyspeed = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]);
	bob = cl_oldbob.value ? V_CalcBob() : 0.0;
	
	// set up the refresh position
	VectorCopy (ent->origin, r_refdef.vieworg);

	// never let it sit exactly on a node line, because a water plane can
	// disappear when viewed with the eye exactly on it.
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef.vieworg[0] += 1.0 / 32;
	r_refdef.vieworg[1] += 1.0 / 32;
	r_refdef.vieworg[2] += 1.0 / 32;

	// add view height
	r_refdef.vieworg[2] += cl.viewheight + bob + cl.crouch;	// smooth out stair step ups

	// set up refresh view angles
	VectorCopy (cl.viewangles, r_refdef.viewangles);
	V_CalcViewRoll ();
	V_AddIdle ();

	if (v_gunkick.value == 1)
	{
		VectorAdd (r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);
	}
	else if (v_gunkick.value == 2)
	{
		// add weapon kick offset
		AngleVectors (r_refdef.viewangles, forward, NULL, NULL);
		VectorMA (r_refdef.vieworg, cl_punchangle, forward, r_refdef.vieworg);

		// add weapon kick angle
		r_refdef.viewangles[PITCH] += cl_punchangle * 0.5;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
		r_refdef.viewangles[ROLL] = 80;	// dead view angle

	V_AddViewWeapon (bob);

	if (cl_thirdperson.value)
		Chase_Update ();
}

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

char *LocalTime (char *format)
{
	time_t	t;
	struct tm *ptm;
	static char	str[80];

	time (&t);
	if ((ptm = localtime(&t)))
		strftime (str, sizeof(str) - 1, format, ptm);
	else
		Q_strncpyz (str, "#bad date#", sizeof(str));

	return str;
}

/*
==============
SCR_DrawClock
==============
*/
void SCR_DrawClock (void)
{
	int		x, y;
	char	*str, *format;

	if (!scr_clock.value)
		return;

	format = ((int)scr_clock.value == 2) ? "%H:%M:%S" : 
			 ((int)scr_clock.value == 3) ? "%a %b %I:%M:%S %p" : 
			 ((int)scr_clock.value == 4) ? "%a %b %H:%M:%S" : "%I:%M:%S %p";

	str = LocalTime(format);

	x = ELEMENT_X_COORD(scr_clock);
	y = ELEMENT_Y_COORD(scr_clock);
	Draw_String (x, y, str);
}

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
	int		x, y;
	char	str[80];
	float	t;
	static	float	lastfps;
	static	double	lastframetime;
	extern	int		fps_count;

	if (!show_fps.value)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0)
	{
		lastfps = fps_count / (t - lastframetime);
		fps_count = 0;
		lastframetime = t;
	}

	Q_snprintfz (str, sizeof(str), "%3.1f%s", lastfps + 0.05, show_fps.value == 2 ? " FPS" : "");
	x = ELEMENT_X_COORD(show_fps);
	y = ELEMENT_Y_COORD(show_fps);
	Draw_String (x, y, str);
}

/*
==============
SCR_DrawSpeed
==============
*/
void SCR_DrawSpeed (void)
{
	int		x, y;
	char		st[8];
	float		speed, vspeed, speedunits;
	vec3_t		vel;
	static	float	maxspeed = 0, display_speed = -1;
	static	double	lastrealtime = 0;

	if (!show_speed.value)
		return;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_speed = -1;
		maxspeed = 0;
	}

	VectorCopy (cl.velocity, vel);
	vspeed = vel[2];
	vel[2] = 0;
	speed = VectorLength (vel);

	if (speed > maxspeed)
		maxspeed = speed;

	if (display_speed >= 0)
	{
		sprintf (st, "%3d", (int)display_speed);

		x = vid.width/2 - 80;

		if (scr_viewsize.value >= 120)
			y = vid.height - 16;

		if (scr_viewsize.value < 120)
			y = vid.height - 8*5;

		if (scr_viewsize.value < 110)
			y = vid.height - 8*8;

		if (cl.intermission)
			y = vid.height - 16;

		Draw_Fill (x, y-1, 160, 1, 10);
		Draw_Fill (x, y+9, 160, 1, 10);
		Draw_Fill (x+32, y-2, 1, 13, 10);
		Draw_Fill (x+64, y-2, 1, 13, 10);
		Draw_Fill (x+96, y-2, 1, 13, 10);
		Draw_Fill (x+128, y-2, 1, 13, 10);

		Draw_Fill (x, y, 160, 9, 52);

		speedunits = display_speed;
		if (display_speed <= 500)
		{
			Draw_Fill (x, y, (int)(display_speed / 3.125), 9, 100);
		}
		else 
		{   
			while (speedunits > 500)
				speedunits -= 500;
			Draw_Fill (x, y, (int)(speedunits / 3.125), 9, 68);
		}
		Draw_String (x + 36 - strlen(st) * 8, y, st);
	}

	if (realtime - lastrealtime >= 0.1)
	{
		lastrealtime = realtime;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

float	drawstats_limit;

/*
===============
SCR_DrawStats
===============
*/
void SCR_DrawStats (void)
{
	int		mins, secs, tens;
	extern	mpic_t	*sb_colon, *sb_nums[2][11];

	if (!show_stats.value || ((show_stats.value == 3 || show_stats.value == 4) && drawstats_limit < cl.time))
		return;

	mins = cl.ctime / 60;
	secs = cl.ctime - 60 * mins;
	tens = (int)(cl.ctime * 10) % 10;

	if (!show_stats_small.value)
	{
		Sbar_IntermissionNumber (vid.width - 140, 0, mins, 2, 0);

		Draw_TransPic (vid.width - 92, 0, sb_colon);
		Draw_TransPic (vid.width - 80, 0, sb_nums[0][secs/10]);
		Draw_TransPic (vid.width - 58, 0, sb_nums[0][secs%10]);

		Draw_TransPic (vid.width - 36, 0, sb_colon);
		Draw_TransPic (vid.width - 24, 0, sb_nums[0][tens]);

		if (show_stats.value == 2 || show_stats.value == 4)
		{
			Sbar_IntermissionNumber (vid.width - 48, 24, cl.stats[STAT_SECRETS], 2, 0);
			Sbar_IntermissionNumber (vid.width - 72, 48, cl.stats[STAT_MONSTERS], 3, 0);
		}
	}
	else
	{
		Draw_String (vid.width - 56, 0, va("%2i:%02i:%i", mins, secs, tens));
		if (show_stats.value == 2 || show_stats.value == 4)
		{
			Draw_String (vid.width - 16, 8, va("%2i", cl.stats[STAT_SECRETS]));
			Draw_String (vid.width - 24, 16, va("%3i", cl.stats[STAT_MONSTERS]));
		}
	}
}

/*
===============
SCR_DrawVolume
===============
*/
void SCR_DrawVolume (void)
{
	int		i, yofs;
	float		j;
	char		bar[11];
	static	float	volume_time = 0;
	extern qboolean	volume_changed;

	if (realtime < volume_time - 2.0)
	{
		volume_time = 0;
		return;
	}

	if (volume_changed)
	{
		volume_time = realtime + 2.0;
		volume_changed = false;
	}
	else if (realtime > volume_time)
	{
		return;
	}

	for (i = 1, j = 0.1 ; i <= 10 ; i++, j += 0.1)
		bar[i-1] = (s_volume.value >= j) ? 139 : 11;

	bar[10] = 0;

	if (show_stats.value == 1 || show_stats.value == 3)
		yofs = !show_stats_small.value ? 32 : 16;
	else if (show_stats.value == 2 || show_stats.value == 4)
		yofs = !show_stats_small.value ? 80 : 32;
	else
		yofs = 8;

	Draw_String (vid.width - 88, yofs, bar);
	Draw_String (vid.width - 88, yofs + 8, "volume");
}

/*
===============
SCR_DrawPlaybackStats
===============
*/
void SCR_DrawPlaybackStats (void)
{
#if 0
	int		yofs;
	static	float	pbstats_time = 0;

	if (!cls.demoplayback)
		return;

	if (realtime < pbstats_time - 2.0)
	{
		pbstats_time = 0;
		return;
	}

	if (pbstats_changed)
	{
		pbstats_time = realtime + 2.0;
		pbstats_changed = false;
	}
	else if (realtime > pbstats_time)
	{
		return;
	}

	if (show_stats.value == 1 || show_stats.value == 3)
		yofs = !show_stats_small.value ? 48 : 32;
	else if (show_stats.value == 2 || show_stats.value == 4)
		yofs = !show_stats_small.value ? 96 : 48;
	else
		yofs = 24;

	Draw_String (vid.width - 136, yofs, va("demo speed: %.1lfx", cl_demospeed.value));
	Draw_String (vid.width - 136, yofs + 8, va("playback mode: %s", cl_demorewind.value ? "rewind" : "forward"));
#endif
}

void V_DropPunchAngle (void)
{
	if (cl_ideal_punchangle < cl_punchangle)
	{
		if (cl_ideal_punchangle == -2)		// small kick
			cl_punchangle -= 20 * host_frametime;
		else					// big kick
			cl_punchangle -= 40 * host_frametime;

		if (cl_punchangle <= cl_ideal_punchangle)
		{
			cl_punchangle = cl_ideal_punchangle;
			cl_ideal_punchangle = 0;
		}
	}
	else
	{
		cl_punchangle += 20 * host_frametime;
		if (cl_punchangle > 0)
			cl_punchangle = 0;
	}
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView (void)
{
	if (con_forcedup)
		return;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_SetValue (&scr_ofsx, 0);
		Cvar_SetValue (&scr_ofsy, 0);
		Cvar_SetValue (&scr_ofsz, 0);
	}

	if (cls.state != ca_connected)
	{
#ifdef GLQUAKE
		V_CalcBlend ();
#endif
		return;
	}

	V_DropPunchAngle ();
	if (cl.intermission)	// intermission / finale rendering
	{
		V_CalcIntermissionRefdef ();	
	}
	else
	{
		if (!cl.paused)
			V_CalcRefdef ();
	}

	R_PushDlights ();
#ifndef GLQUAKE
	if (lcd_x.value)
	{
		// render two interleaved views
		int	i;

		vid.rowbytes <<= 1;
		vid.aspect *= 0.5;

		r_refdef.viewangles[YAW] -= lcd_yaw.value;
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] -= right[i] * lcd_x.value;
		R_RenderView ();

		vid.buffer += vid.rowbytes >> 1;

		R_PushDlights ();

		r_refdef.viewangles[YAW] += lcd_yaw.value * 2;
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] += 2 * right[i] * lcd_x.value;
		R_RenderView ();

		vid.buffer -= vid.rowbytes >> 1;

		r_refdef.vrect.height <<= 1;

		vid.rowbytes >>= 1;
		vid.aspect *= 2;
	}
	else
#endif
	R_RenderView ();
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);

#ifndef GLQUAKE
	Cvar_Register (&lcd_x);
	Cvar_Register (&lcd_yaw);
#endif

	Cvar_Register (&v_centermove);
	Cvar_Register (&v_centerspeed);

	Cvar_Register (&v_iyaw_cycle);
	Cvar_Register (&v_iroll_cycle);
	Cvar_Register (&v_ipitch_cycle);
	Cvar_Register (&v_iyaw_level);
	Cvar_Register (&v_iroll_level);
	Cvar_Register (&v_ipitch_level);
	Cvar_Register (&v_idlescale);

	Cvar_Register (&crosshair);
	Cvar_Register (&crosshaircolor);
	Cvar_Register (&crosshairsize);
	Cvar_Register (&cl_crossx);
	Cvar_Register (&cl_crossy);

	Cvar_Register (&scr_ofsx);
	Cvar_Register (&scr_ofsy);
	Cvar_Register (&scr_ofsz);

	Cvar_Register (&scr_clock);
	Cvar_Register (&scr_clock_x);
	Cvar_Register (&scr_clock_y);

	Cvar_Register (&show_speed);

	Cvar_Register (&show_fps);
	Cvar_Register (&show_fps_x);
	Cvar_Register (&show_fps_y);

	Cvar_Register (&show_stats);
	Cvar_Register (&show_stats_small);
	Cvar_Register (&show_stats_length);

	Cvar_Register (&cl_rollspeed);
	Cvar_Register (&cl_rollangle);

	Cvar_Register (&cl_bob);
	Cvar_Register (&cl_bobcycle);
	Cvar_Register (&cl_bobup);
	Cvar_Register (&cl_oldbob);
	Cvar_Register (&cl_oldgunposition);
	Cvar_Register (&cl_hand);

	Cvar_Register (&v_kicktime);
	Cvar_Register (&v_kickroll);
	Cvar_Register (&v_kickpitch);	
	Cvar_Register (&v_gunkick);

	Cvar_Register (&v_bonusflash);
	Cvar_Register (&v_contentblend);
	Cvar_Register (&v_damagecshift);
	Cvar_Register (&v_quadcshift);
	Cvar_Register (&v_suitcshift);
	Cvar_Register (&v_ringcshift);
	Cvar_Register (&v_pentcshift);
#ifdef GLQUAKE
	Cvar_Register (&v_dlightcshift);
	Cvar_Register (&gl_cshiftpercent);
	Cvar_Register (&gl_hwblend);
#endif

	Cvar_Register (&v_gamma);
	Cvar_Register (&v_contrast);

#ifdef GLQUAKE
	Cmd_AddLegacyCommand ("gamma", v_gamma.name);
	Cmd_AddLegacyCommand ("contrast", v_contrast.name);
#endif

#ifndef GLQUAKE
	BuildGammaTable (v_gamma.value, v_contrast.value);
#endif
}
