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


typedef struct {
    float time;
    float origin[3];
    float angle[3];
    unsigned int frame;
} ghostrec_t;


qboolean Ghost_ReadDemo(const char *demo_path, ghostrec_t **records, int *num_records,
                        const char *expected_map_name);

#endif /* __GHOST_PRIVATE */
