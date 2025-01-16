/*
Copyright (C) 2000	LordHavoc, Ender

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
// nehahra.h


#define NEHAHRA_VERSION	2.54

// system types
#define	TYPE_DEMO	0
#define	TYPE_GAME	1
#define	TYPE_BOTH	2

extern	int	NehGameType;

extern	char	prev_skybox[64];

extern	cvar_t	r_oldsky, r_nospr32;
extern	cvar_t	gl_fogenable, gl_fogdensity, gl_fogred, gl_foggreen, gl_fogblue;

extern	sfx_t	*known_sfx;
extern	int	num_sfx, num_sfxorig;

void Neh_DoBindings (void);
void Neh_Init (void);
void Neh_SetupFrame (void);
void Neh_GameStart (void);
void SHOWLMP_drawall (void);
void SHOWLMP_clear (void);
void SHOWLMP_decodehide (void);
void SHOWLMP_decodeshow (void);
