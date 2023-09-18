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
#define RECORD_REALLOC_LOG2     9   // 512 records per chunk, approx 7 seconds


/*
 * PARSING CALLBACKS
 *
 * Callbacks from the demoparse module that build up the record list.
 */


typedef struct {
    ghost_info_t *ghost_info;
    ghost_level_t *level;

    int view_entity;
    ghostrec_t rec;
    qboolean updated;  // record changed since last append

    FILE *demo_file;

    int model_num;

    vec3_t baseline_origin;
    vec3_t baseline_angle;
    int baseline_frame;
    int baseline_model;

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
    int i;

    if (pctx->model_num == 0) {
        // Just encountered a new server info.  Move to the next level.
        pctx->ghost_info->num_levels ++;
        if (pctx->ghost_info->num_levels == GHOST_MAX_LEVELS) {
            return DP_CBR_STOP;
        }
        pctx->level = &pctx->ghost_info->levels[pctx->ghost_info->num_levels];
        memset(pctx->level, 0, sizeof(ghost_level_t));

        Q_strncpyz(pctx->level->map_name, (char *)model,
                   sizeof(pctx->level->map_name));
        COM_StripExtension(COM_SkipPath(pctx->level->map_name),
                           pctx->level->map_name);
        pctx->level->finish_time = -1.0f;
        pctx->view_entity = 0;
    }

    for (i = 0; i < GHOST_MODEL_COUNT; i++) {
        if (strcmp(model, ghost_model_paths[i]) == 0) {
            pctx->level->model_indices[i] = pctx->model_num + 1;
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

    pctx->rec.time = time;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_SetView_cb (int entity_num, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    pctx->view_entity = entity_num;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Baseline_cb (int entity_num, vec3_t origin, vec3_t angle, int frame,
                   int model, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (pctx->view_entity == -1) {
        Con_Printf("Baseline receieved but entity num not set\n");
        return DP_CBR_STOP;
    }

    if (entity_num == pctx->view_entity) {
        VectorCopy(origin, pctx->baseline_origin);
        VectorCopy(angle, pctx->baseline_angle);

        pctx->baseline_frame = frame;
        pctx->baseline_model = model;
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Update_cb(int entity_num, vec3_t origin, vec3_t angle, byte origin_bits,
                byte angle_bits, int frame, int model, void *ctx)
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

    if (model != -1) {
        pctx->rec.model = model;
    } else {
        pctx->rec.model = pctx->baseline_model;
    }

    pctx->updated = true;

    // Nothing of interest until the next packet.
    return DP_CBR_SKIP_PACKET;
}


static void
Ghost_Append(ghost_level_t *level, ghostrec_t *rec)
{
    int num_chunks;

    if (level->num_records & ((1 << RECORD_REALLOC_LOG2) - 1) == 0)
    {
        num_chunks = (level->num_records >> RECORD_REALLOC_LOG2) + 1;
        level->records = Q_realloc(level->records,
                                   sizeof(ghostrec_t) * num_chunks);
    }
    level->records[level->num_records] = *rec;
    level->num_records ++;
}


static dp_cb_response_t
Ghost_PacketEnd_cb (void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;
    if (pctx->updated) {
        Ghost_Append(pctx->level, &pctx->rec);
        pctx->updated = false;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
Ghost_Intermission_cb (void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (pctx->level->finish_time <= 0) {
        pctx->level->finish_time = pctx->rec.time;
    }

    return DP_CBR_SKIP_PACKET;
}


static dp_cb_response_t
Ghost_UpdateName_cb (int client_num, const char *name, void *ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (client_num >= 0 && client_num < GHOST_MAX_CLIENTS) {
        Q_strncpyz(pctx->level->client_names[client_num], (char *)name,
                   MAX_SCOREBOARDNAME);
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


static dp_cb_response_t
Ghost_UpdateColors_cb(byte client_num, byte colors, void* ctx)
{
    ghost_parse_ctx_t *pctx = ctx;

    if (client_num >= 0 && client_num < GHOST_MAX_CLIENTS) {
        pctx->level->client_colors[client_num] = colors;
    }

    return DP_CBR_CONTINUE;
}


static qboolean
Ghost_ReadDemoNoChain (FILE *demo_file, ghost_info_t *ghost_info,
                       char *next_demo_path)
{
    qboolean ok = true;
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
        .update_colors = Ghost_UpdateColors_cb,
    };
    ghost_parse_ctx_t pctx = {
        .model_num = 0,
        .demo_file = demo_file,
        .updated = false,
    };

    if (ok) {
        next_demo_path[0] = '\0';
        dprc = DP_ReadDemo(&callbacks, &pctx);
        if (dprc == DP_ERR_CALLBACK_STOP) {
            if (pctx.ghost_info->num_levels == GHOST_MAX_LEVELS) {
                Con_Printf("Demo contains more than %d maps, ghost has been "
                           "truncated\n",
                           GHOST_MAX_LEVELS);
                ok = true;
            } else if (pctx.next_demo_path[0]) {
                // There's another .dem file in the chain.
                Q_strlcpy(next_demo_path, pctx.next_demo_path, MAX_OSPATH);
                ok = true;
            } else {
                // Error already printed.
                ok = false;
            }
        } else if (dprc != DP_ERR_SUCCESS) {
            Con_Printf("Error parsing demo: %s\n", DP_StrError(dprc));
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
Ghost_ReadDemo (FILE *demo_file, ghost_info_t *ghost_info)
{
    qboolean ok = true;
    char next_demo_path[MAX_OSPATH];
    int num_demos_searched = 0;

    while (ok && demo_file != NULL && num_demos_searched < MAX_CHAINED_DEMOS) {
        ok = Ghost_ReadDemoNoChain (demo_file, ghost_info, next_demo_path);
        if (ok) {
            if (next_demo_path[0] != '\0') {
                if (COM_FOpenFile (next_demo_path, &demo_file) == -1) {
                    Con_Printf("Could not open demo in marathon %s\n", next_demo_path);
                    ok = false;
                    demo_file = NULL;
                } else {
                    num_demos_searched ++;
                }
            } else {
                demo_file = NULL;
            }
        }
    }

    if (num_demos_searched == MAX_CHAINED_DEMOS) {
        // Best to have a limit in case we have looped demos.
        Con_Printf("Stopped searching for ghost after %d chained demos\n",
                   MAX_CHAINED_DEMOS);
        ok = false;
    }

    if (!ok) {
        Ghost_Free(ghost_info);
    }

    return ok;
}


void
Ghost_Free (ghost_info_t *ghost_info)
{
    int level_idx;

    for (level_idx = 0; level_idx < ghost_info->num_levels; level_idx ++) {
        free(ghost_info->levels[level_idx].records);
        ghost_info->levels[level_idx].records = NULL;
    }
}
