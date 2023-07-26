/*
Copyright (C) 2022 Matthew Earl

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


#ifndef __CL_GHOST_H
#define __CL_GHOST_H

#define GHOST_MAX_CLIENTS   8

void Ghost_Load (const char *map_name);
void Ghost_Draw (void);
void Ghost_DrawGhostTime (qboolean intermission);
void Ghost_Init (void);
void Ghost_Finish (void);
void Ghost_Shutdown (void);

typedef struct {
    byte colors;
    byte translations[VID_GRADES*256];
} ghost_color_info_t;

extern char         ghost_demo_path[MAX_OSPATH];
extern entity_t		*ghost_entity;
extern ghost_color_info_t ghost_color_info[GHOST_MAX_CLIENTS];


#endif /* __CL_GHOST_H */
