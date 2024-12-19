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
// view.h

extern	cvar_t	v_gamma;
extern	cvar_t	v_contrast;
extern	cvar_t	crosshair;
extern	cvar_t	crosshairsize;
extern	cvar_t	crosshaircolor;
extern	cvar_t	scr_clock;
extern	cvar_t	show_speed;
extern	cvar_t	show_fps;
extern	cvar_t	cl_hand;
extern	cvar_t	v_gunkick;
extern	cvar_t	show_stats;
extern	cvar_t	show_stats_small;
extern	cvar_t	show_movekeys;

extern	cvar_t  v_contentblend;
extern	cvar_t	v_damagecshift;
extern	cvar_t	v_quadcshift;
extern	cvar_t	v_suitcshift;
extern	cvar_t	v_ringcshift;
extern	cvar_t	v_pentcshift;
extern	cvar_t	v_bonusflash;

#ifdef GLQUAKE
extern	cvar_t	gl_crosshairalpha;
extern	cvar_t	gl_crosshairimage;
extern	cvar_t	gl_hwblend;
extern	float	v_blend[4];
void V_AddLightBlend (float r, float g, float b, float a2);
void V_AddWaterfog (int contents);		
#endif

extern	byte	gammatable[256];	// palette is sent through this
extern	byte	current_pal[768];

#ifndef GLQUAKE
extern	cvar_t	lcd_x, lcd_yaw;
#endif

int _view_temp_int;
float _view_temp_float;

#define	ELEMENT_X_COORD(var)	\
(_view_temp_int = Sbar_GetScaledCharacterSize(),\
((var##_x.value < 0) ? vid.width - strlen(str) * _view_temp_int + _view_temp_int * var##_x.value : _view_temp_int * var##_x.value))

#define	ELEMENT_Y_COORD(var)	\
(_view_temp_int = Sbar_GetScaledCharacterSize(), _view_temp_float = Sbar_GetScaleAmount(),\
((var##_y.value < 0) ? vid.height - (int)(sb_lines * _view_temp_float) + _view_temp_int * var##_y.value : _view_temp_int * var##_y.value))

typedef enum
{
	mk_forward, mk_back, mk_moveleft, mk_moveright, mk_jump, NUM_MOVEMENT_KEYS
} movekeytype_t;

void V_Init (void);
void V_RenderView (void);

void V_CalcBlend (void);
char *LocalTime (char *format);
void SCR_DrawClock (void);
void SCR_DrawSpeed (void);
void SCR_DrawFPS (void);
void SCR_DrawStats (void);
void SCR_DrawVolume (void);
void SCR_DrawMovementKeys(void);
void SCR_DrawPlaybackStats (void);

float V_CalcRoll (vec3_t angles, vec3_t velocity);
void V_RestoreAngles (void);
void V_UpdatePalette (void);
