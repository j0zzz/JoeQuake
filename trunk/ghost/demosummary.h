/*
Copyright (C) 2023 Matthew Earl

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

#ifndef __DEMO_SUMMARY_H
#define __DEMO_SUMMARY_H

#ifndef QUAKE_GAME
#include "../quakedef.h"
#endif

#define DS_MAX_MAP_NAMES   128
#define DS_MAP_NAME_SIZE   64
#define DS_MAX_CLIENTS   8

typedef struct
{
    char maps[DS_MAX_MAP_NAMES][DS_MAP_NAME_SIZE];
    long offsets[DS_MAX_MAP_NAMES];
    int num_maps;
    float total_time;
    char client_names[DS_MAX_CLIENTS][MAX_SCOREBOARDNAME];
    int view_entity;
    int skill;

    int kills, total_kills;
    int secrets, total_secrets;
} demo_summary_t;

qboolean DS_GetDemoSummary(FILE *demo_file, demo_summary_t *demo_summary);


#endif /* __DEMO_SUMMARY_H */

