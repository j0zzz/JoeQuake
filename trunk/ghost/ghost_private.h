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


#define GHOST_MAX_LEVELS     128
#define GHOST_MAP_NAME_SIZE  64


typedef enum {
    GHOST_MODEL_PLAYER = 0,
    GHOST_MODEL_EYES,
    GHOST_MODEL_HEAD,
    GHOST_MODEL_COUNT,
} ghost_model_t;


typedef struct {
    float time;
    float origin[3];
    float angle[3];
    unsigned int frame;
    unsigned int model;
} ghostrec_t;


typedef struct {
    char map_name[GHOST_MAP_NAME_SIZE];

    int view_entity;
    char client_names[GHOST_MAX_CLIENTS][MAX_SCOREBOARDNAME];
    byte client_colors[GHOST_MAX_CLIENTS];
    float finish_time;
    ghostrec_t *records;
    int num_records;
    int model_indices[GHOST_MODEL_COUNT];
} ghost_level_t;


typedef struct {
    ghost_level_t levels[GHOST_MAX_LEVELS];
    int num_levels;
} ghost_info_t;


extern const char *ghost_model_paths[GHOST_MODEL_COUNT];

qboolean Ghost_ReadDemo (FILE *demo_file, ghost_info_t **ghost_info);
void Ghost_Free (ghost_info_t **ghost_info);

#endif /* __GHOST_PRIVATE */
