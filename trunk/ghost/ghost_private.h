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


#ifndef __GHOST_PRIVATE
#define __GHOST_PRIVATE

#ifndef QUAKE_GAME
#include "../quakedef.h"
#endif

extern const char *ghost_model_paths[GHOST_MODEL_COUNT];

qboolean Ghost_ReadDemo (FILE *demo_file, ghost_info_t **ghost_info);


#endif /* __GHOST_PRIVATE */
