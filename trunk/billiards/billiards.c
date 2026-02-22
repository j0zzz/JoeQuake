/*
 * p_su
 * Bunnyhopping statistics and practice module
 * Prepares display for various graphics showcasing how well the user is
 * bunnyhopping. 
 *
 * TODO: Allow for setting up practice mode, to hone individual
 * areas of bunnyhopping skill.
 *
 * Special thanks to Shalrathy for inspiration.
 *
 * Copyright (C) 2025 K. Urbański <karol.jakub.urbanski@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../quakedef.h"
#include <string.h>
#include "billiards.h"

extern void SV_Physics_Toss (edict_t *ent);
extern void ED_ClearEdict(edict_t *e);
extern double sv_frametime;
edict_t * grenade_list[MAX_BILLIARDS];
edict_t * current_grenade;

cvar_t	cl_billiards_wh = {"cl_billiards_wh", "0"};

qboolean CL_ShowBilliards(void)
{
    return sv.active && cl_billiards.value != 0 && !cls.demorecording && !cls.demoplayback;
}

void Billiards_Shutdown(void)
{
    for (int i = 0; i < MAX_BILLIARDS; i++) {
        if (grenade_list[i])
            free(grenade_list[i]);

        grenade_list[i] = NULL;
    }

    if (current_grenade) {
        free(current_grenade);
        current_grenade = NULL;
    }
}

void Billiards_SaveTrail(void)
{
    if (Cmd_Argc() <= 1) {
        Con_Printf("command needs slot index (0 to %d)\n", MAX_BILLIARDS);
        return;
    }

    int index = Q_atoi(Cmd_Argv(1));

    if (index > MAX_BILLIARDS || index < 0) {
        Con_Printf("command needs slot index (0 to %d)\n", MAX_BILLIARDS);
        return;
    }

    if (index < 0)
        index = 0;

    if (grenade_list[index]) 
        free(grenade_list[index]);

    grenade_list[index] = current_grenade;
    current_grenade = NULL;
}

void Billiards_DeleteTrail(void)
{

    if (Cmd_Argc() <= 1) {
        Con_Printf("command needs slot index (0 to %d)\n", MAX_BILLIARDS);
        return;
    }

    int index = Q_atoi(Cmd_Argv(1));

    if (index > MAX_BILLIARDS || index < 0) {
        Con_Printf("command needs slot index (0 to %d)\n", MAX_BILLIARDS);
        return;
    }

    if (grenade_list[index]) {
        free(grenade_list[index]);
        grenade_list[index] = NULL;
    }
}

void Billiards_Init(void)
{
    Cvar_Register (&cl_billiards_wh);
    Cmd_AddCommand("billiards_save", Billiards_SaveTrail);
    Cmd_AddCommand("billiards_delete", Billiards_DeleteTrail);
    Cmd_AddCommand("billiards_clear", Billiards_Shutdown);

    for (int i = 0; i < MAX_BILLIARDS; i++)
        grenade_list[i] = NULL;

    current_grenade = NULL;
}

edict_t * Billiards_SpawnTrail(void)
{
    vec3_t forward, right, up;
    vec3_t vel;
    edict_t * nade = Q_malloc(pr_edict_size);
    memset(nade, 0, pr_edict_size);

    nade->v.movetype = MOVETYPE_BOUNCE;
    nade->v.solid = SOLID_NOT;
    nade->v.owner = 1;

    AngleVectors(sv_player->v.v_angle, forward, right, up);
    VectorClear(vel);
    VectorScale(forward, 600, forward);
    VectorScale(up, 200, up);

    VectorAdd(vel, forward, vel);
    VectorAdd(vel, up, vel);

    VectorCopy(vel, nade->v.velocity);
    vectoangles(vel, nade->v.angles);

    VectorCopy(sv_player->v.origin, nade->v.origin);
    VectorSet(nade->v.avelocity, 300, 300, 300);

    return nade;
}

void Billiards_DrawDot(vec3_t loc, vec3_t prev_loc, vec3_t player_loc, qboolean big, double time)
{

    // get distance to scale the dot
    vec3_t conn, prev_conn;
    float point_size;

    VectorCopy(loc, conn);
    VectorCopy(prev_loc, prev_conn);
    VectorSubtract(player_loc, conn, conn);
    VectorSubtract(player_loc, prev_conn, prev_conn);
    point_size = VectorLength(conn);
    if (abs(point_size) < 1.0)
        point_size = 1.0;
    point_size = 800.0 / point_size;

    glCullFace(GL_FRONT);
    glPolygonMode(GL_BACK, GL_LINE);
    glLineWidth(1.0);
    glEnable(GL_LINE_SMOOTH);
    glEnable (GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    if (cl_billiards_wh.value)
        glDepthRange (0, 0.3);

    point_size = big ? 2.0 * point_size: point_size;
    glPointSize(point_size);

    glBegin(GL_POINTS);

    glColor3f(time / 2.5, (2.5 - time) / 2.5, 0.0);
    glVertex3fv(loc);

    glEnd();

    glBegin(GL_LINES);

    glVertex3fv(prev_loc);
    glVertex3fv(loc);

    glEnd();

    glDepthRange (0, 1.0);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glPolygonMode(GL_BACK, GL_FILL);
    glCullFace(GL_BACK);
}

void Billiards_DrawTrail(const edict_t * src)
{
    double time = 0.0;
    int last_step = 0, current_step;
    vec3_t prev_loc;

    edict_t * nade = Q_malloc(pr_edict_size);
    memcpy(nade, src, pr_edict_size);

    VectorCopy(nade->v.origin, prev_loc);

    while (time < 2.5) {
        SV_Physics_Toss(nade);
        nade->v.watertype = 0;
        current_step = (int)(time * 20);

        if (current_step > last_step) {
            last_step = current_step;
            Billiards_DrawDot(nade->v.origin, prev_loc, sv_player->v.origin, current_step % 10 ==0, time);
        }

        VectorCopy(nade->v.origin, prev_loc);
        time += sv_frametime;
    }
}

void R_DrawBilliards(void)
{
    int i;
    edict_t * nade;

    if (!CL_ShowBilliards())
        return;

    if (current_grenade)
        free(current_grenade);
    current_grenade = Billiards_SpawnTrail();
    Billiards_DrawTrail(current_grenade);

    for (i = 0, nade = grenade_list[i]; i < MAX_BILLIARDS; nade = grenade_list[++i]) {
        if (!nade)
            continue;

        Billiards_DrawTrail(nade);
    }

}
