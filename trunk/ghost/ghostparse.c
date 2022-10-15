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


#include "../quakedef.h"
#include "demoparse.h"
#include "ghost_private.h"


#define MAX_CHAINED_DEMOS   64


/*
 * RECORD LISTS
 *
 * Records are pushed onto a linked list before being compressed into a dense
 * array.
 */

typedef struct ghostreclist_s {
    ghostrec_t rec;
    struct ghostreclist_s *next;
} ghostreclist_t;


static void Ghost_Append(ghostreclist_t ***next_ptr, ghostrec_t *rec)
{
    ghostreclist_t *new_entry = Hunk_AllocName(sizeof(ghostreclist_t), "ghost_list_element");

    new_entry->rec = *rec;
    new_entry->next = NULL;
    **next_ptr = new_entry;
    *next_ptr = &new_entry->next;
}


static int Ghost_ListLen(ghostreclist_t *list)
{
    int count = 0;

    while (list != NULL) {
        list = list->next;
        count ++;
    }

    return count;
}


static void Ghost_ListToArray(ghostreclist_t *list, ghostrec_t **records_out,
                              int *num_records_out)
{
    ghostrec_t *records;
    int num_records;
    int record_num;

    num_records = Ghost_ListLen(list);
    records = Hunk_AllocName(num_records * sizeof(ghostrec_t),
                             "ghostrecords");

    for (record_num = 0; list != NULL; list = list->next, record_num++) {
        records[record_num] = list->rec;
    }

    *records_out = records;
    *num_records_out = num_records;
}


/*
 * PARSING CALLBACKS
 *
 * Callbacks from the demoparse module that build up the record list.
 */


typedef struct {
    int view_entity;
    ghostreclist_t **next_ptr;
    ghostrec_t rec;
    qboolean map_found;
    qboolean updated;  // record changed since last append

    FILE *demo_file;

    int model_num;
    const char *expected_map_name;

    vec3_t baseline_origin;
    vec3_t baseline_angle;
    int baseline_frame;

    char (*client_names)[MAX_SCOREBOARDNAME];
    float finish_time;
    char next_demo_path[MAX_OSPATH];
} ghost_parse_ctx_t;


static qboolean
Ghost_Read_cb (void *dest, unsigned int size, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;
    return fread (dest, size, 1, pctx->demo_file) != 0;
}


static dp_cb_response_t
Ghost_ServerInfoModel_cb (const char *model, void *ctx)
{
    char map_name[MAX_OSPATH];
    ghost_parse_ctx_t *pctx = ctx;

    if (pctx->model_num == 0) {
        if (pctx->map_found) {
            // Hit a new server info message after already having found one with
            // the map name we want.  This means that we have fully parsed the
            // part of the demo that we need for the ghost.
            return DP_CBR_STOP;
        }
		Q_strncpyz(map_name, (char *)model, sizeof(map_name));  // truncate to MAX_OSPATH
        COM_StripExtension(COM_SkipPath(map_name), map_name);
        if (!strcmp(map_name, pctx->expected_map_name)) {
            pctx->map_found = true;
        }
    }
    pctx->model_num += 1;

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_ServerInfo_cb (int protocol, unsigned int protocol_flags,
                     const char *level_name, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    pctx->model_num = 0;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Time_cb (float time, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (!pctx->map_found) {
        // Still scanning for map, so skip to next packet.
        return DP_CBR_SKIP_PACKET;
    }
    pctx->rec.time = time;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_SetView_cb (int entity_num, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (!pctx->map_found) {
        // Still scanning for map, so skip to next packet.
        return DP_CBR_SKIP_PACKET;
    }
    pctx->view_entity = entity_num;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Baseline_cb (int entity_num, vec3_t origin, vec3_t angle, int frame,
                   void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (!pctx->map_found) {
        // Still scanning for map, so skip to next packet.
        return DP_CBR_SKIP_PACKET;
    }

    if (pctx->view_entity == -1) {
        Con_Printf("Baseline receieved but entity num not set\n");
        return DP_CBR_STOP;
    }

    if (entity_num == pctx->view_entity) {
        VectorCopy(origin, pctx->baseline_origin);
        VectorCopy(angle, pctx->baseline_angle);
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Update_cb(int entity_num, vec3_t origin, vec3_t angle,
                byte origin_bits, byte angle_bits, int frame, void *ctx)
{
    int i;
    ghost_parse_ctx_t *pctx = ctx;

    if (pctx->view_entity == -1) {
        Con_Printf("Update receieved but entity num not set\n");
        return DP_CBR_STOP;
    }

    if (entity_num != pctx->view_entity) {
        return DP_CBR_CONTINUE;
    }

    for (i = 0; i < 3; i++) {
        if (origin_bits & (1 << i)) {
            pctx->rec.origin[i] = origin[i];
        } else {
            pctx->rec.origin[i] = pctx->baseline_origin[i];
        }
        if (angle_bits & (1 << i)) {
            pctx->rec.angle[i] = angle[i];
        } else {
            pctx->rec.angle[i] = pctx->baseline_angle[i];
        }
    }

    if (frame != -1) {
        pctx->rec.frame = frame;
    } else {
        pctx->rec.frame = pctx->baseline_frame;
    }

    pctx->updated = true;

    // Nothing of interest until the next packet.
    return DP_CBR_SKIP_PACKET;
}


static dp_cb_response_t
Ghost_PacketEnd_cb (void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;
    if (pctx->updated) {
        Ghost_Append(&pctx->next_ptr, &pctx->rec);
        pctx->updated = false;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Intermission_cb (void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (!pctx->map_found) {
        // Still scanning for map, so skip to next packet.
        return DP_CBR_SKIP_PACKET;
    }

    if (pctx->finish_time <= 0) {
        pctx->finish_time = pctx->rec.time;
    }

    return DP_CBR_STOP;
}


static dp_cb_response_t
Ghost_UpdateName_cb (int client_num, const char *name, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (!pctx->map_found) {
        // Still scanning for map, so skip to next packet.
        return DP_CBR_SKIP_PACKET;
    }

    if (client_num >= 0 && client_num < GHOST_MAX_CLIENTS) {
        Q_strncpyz(pctx->client_names[client_num], (char *)name, MAX_SCOREBOARDNAME);
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_StuffText_cb (const char *string, void *ctx)
{
    int len;
    ghost_parse_ctx_t *pctx = ctx;

    // Skip to next demo if we encounter a `playdemo` stuff command.
    if (strncmp(string, "playdemo ", 9) == 0) {
        Q_strlcpy(pctx->next_demo_path, string + 9, sizeof(pctx->next_demo_path));
        len = strlen(pctx->next_demo_path);
        if (len > 0 && pctx->next_demo_path[len - 1] == '\n') {
            pctx->next_demo_path[len - 1] = '\0';
            COM_DefaultExtension (pctx->next_demo_path, ".dem");
        }

        return DP_CBR_STOP;
    }

    return DP_CBR_CONTINUE;
}


static qboolean
Ghost_ReadDemoNoChain (const char *demo_path, ghost_info_t *ghost_info,
                       const char *expected_map_name,
                       char *next_demo_path)
{
    qboolean ok = true;
    ghostreclist_t *list = NULL;
    dp_err_t dprc;
    dp_callbacks_t callbacks = {
        .read = Ghost_Read_cb,
        .server_info_model = Ghost_ServerInfoModel_cb,
        .server_info = Ghost_ServerInfo_cb,
        .time = Ghost_Time_cb,
        .set_view = Ghost_SetView_cb,
        .baseline = Ghost_Baseline_cb,
        .update = Ghost_Update_cb,
        .packet_end = Ghost_PacketEnd_cb,
        .intermission = Ghost_Intermission_cb,
        .finale = Ghost_Intermission_cb,
        .cut_scene = Ghost_Intermission_cb,
        .update_name = Ghost_UpdateName_cb,
        .stuff_text = Ghost_StuffText_cb,
    };
    ghost_parse_ctx_t pctx = {
        .view_entity = -1,
        .next_ptr = &list,
        .expected_map_name = expected_map_name,
        .client_names = ghost_info->client_names,
        .finish_time = -1,
    };

    COM_FOpenFile ((char *)demo_path, &pctx.demo_file);
    if (!pctx.demo_file)
    {
        Con_Printf ("ERROR: couldn't open %s\n", demo_path);
        ok = false;
    }

    if (ok) {
        next_demo_path[0] = '\0';
        dprc = DP_ReadDemo(&callbacks, &pctx);
        if (dprc == DP_ERR_CALLBACK_STOP) {
            if (pctx.finish_time > 0) {
                // Map was found, and intermission reached.
                ok = true;
            } else if (pctx.next_demo_path[0] && !pctx.map_found) {
                // Map not found, but there's another .dem file in the chain.
                Q_strlcpy(next_demo_path, pctx.next_demo_path, MAX_OSPATH);
                ok = true;
            } else {
                // Errors from callbacks print their own error messages.
                ok = false;
            }
        } else if (dprc != DP_ERR_SUCCESS) {
            Con_Printf("Error parsing demo %s: %u\n", demo_path, dprc);
            ok = false;
        } else if (!pctx.map_found) {
            Con_Printf("Map %s not found in demo\n", expected_map_name);
            ok = false;
        }
    }

    if (ok && pctx.finish_time > 0) {
        Ghost_ListToArray(list, &ghost_info->records, &ghost_info->num_records);
        ghost_info->finish_time = pctx.finish_time;

        if (ghost_info->num_records == 0) {
            Con_Printf("ERROR: No records found in demo\n");
            ok = false;
        }
    }

    // Free everything
    if (pctx.demo_file) {
        fclose(pctx.demo_file);
    }

    return ok;
}


/*
 * ENTRYPOINT
 *
 * Call the demo parse module and return record array.
 */


qboolean
Ghost_ReadDemo (const char *demo_path, ghost_info_t *ghost_info,
                const char *expected_map_name)
{
    qboolean ok = true;
    char next_demo_path[MAX_OSPATH];
    int num_demos_searched = 0;

    Q_strlcpy(next_demo_path, demo_path, sizeof(next_demo_path));
    while (ok && next_demo_path[0] && num_demos_searched < MAX_CHAINED_DEMOS) {
        ok = Ghost_ReadDemoNoChain (next_demo_path, ghost_info,
                                    expected_map_name, next_demo_path);
        if (ok && next_demo_path[0]) {
            num_demos_searched ++;
        }
    }
    Hunk_HighMark();

    if (num_demos_searched == MAX_CHAINED_DEMOS) {
        // Best to have a limit in case we have looped demos.
        Con_Printf("Stopped searching for ghost after %d chained demos\n",
                   MAX_CHAINED_DEMOS);
        ok = false;
    }

    return ok;
}

