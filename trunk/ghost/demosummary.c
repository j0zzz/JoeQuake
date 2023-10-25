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


#include "../quakedef.h"
#include "../demoparse.h"
#include "demosummary.h"

#define DS_PRINT_BUFFER_SIZE    256

typedef struct
{
    FILE *demo_file;
    float level_time;
    demo_summary_t *demo_summary;
    int model_num;
    qboolean intermission_seen;
    qboolean any_intermission_seen;
    long packet_offset;

    char print_buffer[DS_PRINT_BUFFER_SIZE];
    int print_buffer_len;

    int kills, total_kills;
    int secrets, total_secrets;
} ds_ctx_t;


static char *map_name;


static dp_cb_response_t
DS_PacketStart_cb (void *ctx)
{
    ds_ctx_t *pctx = ctx;

    pctx->packet_offset = ftell(pctx->demo_file);

    return DP_CBR_CONTINUE;
}


static qboolean
DS_Read_cb (void *dest, unsigned int size, void *ctx)
{
    ds_ctx_t *pctx = ctx;
    return fread (dest, size, 1, pctx->demo_file) != 0;
}


static dp_cb_response_t
DS_ServerInfoModel_cb (const char *model, void *ctx)
{
    ds_ctx_t *pctx = ctx;
    demo_summary_t *ds = pctx->demo_summary;

    if (pctx->model_num == 0 && ds->num_maps < DS_MAX_MAP_NAMES) {
        map_name = ds->maps[ds->num_maps];
		Q_strncpyz(map_name, (char *)model, DS_MAP_NAME_SIZE);
        COM_StripExtension(COM_SkipPath(map_name), map_name);
        ds->offsets[ds->num_maps] = pctx->packet_offset;

        // `num_maps` is incremented on intermission, so that we don't list
        // start map.
    }

    pctx->model_num ++;

    return DP_CBR_CONTINUE;
}

static void
DS_UpdateStats (ds_ctx_t *pctx)
{
    if (pctx->intermission_seen) {
        pctx->demo_summary->kills += pctx->kills;
        pctx->demo_summary->total_kills += pctx->total_kills;
        pctx->demo_summary->secrets += pctx->secrets;
        pctx->demo_summary->total_secrets += pctx->total_secrets;
    }
}

static dp_cb_response_t
DS_ServerInfo_cb (int protocol, unsigned int protocol_flags,
                  const char *level_name, void *ctx)
{
    ds_ctx_t *pctx = ctx;

    if (pctx->intermission_seen) {
        DS_UpdateStats(pctx);
    }
    pctx->kills = 0;
    pctx->total_kills = 0;
    pctx->secrets = 0;
    pctx->total_secrets = 0;

    pctx->model_num = 0;
    pctx->level_time = 0.0;
    pctx->intermission_seen = false;
    pctx->print_buffer_len = 0;

    if (!pctx->any_intermission_seen) {
        memset(pctx->demo_summary->client_names, 0,
               sizeof(pctx->demo_summary->client_names));
        pctx->demo_summary->view_entity = -1;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_SetView_cb (int entity_num, void *ctx)
{
    ds_ctx_t *pctx = ctx;

    if (pctx->demo_summary->view_entity == -1) {
        // Take the view entity as the first view entity of the first level with
        // an intermission (or the last level if there is no intermission).
        pctx->demo_summary->view_entity = entity_num;
    }

    return DP_CBR_CONTINUE;
}


static void
DS_OnPrintLine (ds_ctx_t *pctx, const char *line)
{
    if (!pctx->any_intermission_seen) {
        if (!strcmp(line, "Playing on Easy skill")) {
            pctx->demo_summary->skill = 0;
        } else if (!strcmp(line, "Playing on Normal skill")) {
            pctx->demo_summary->skill = 1;
        } else if (!strcmp(line, "Playing on Hard skill")) {
            pctx->demo_summary->skill = 2;
        } else if (!strcmp(line, "Playing on Nightmare skill")) {
            pctx->demo_summary->skill = 3;
        }
    }
}


static dp_cb_response_t
DS_Print_cb (const char *string, void *ctx)
{
    const char *src;
    ds_ctx_t *pctx = ctx;

    src = string;

    while (*src != '\0') {
        if (*src == '\n') {
            if (pctx->print_buffer_len < sizeof(pctx->print_buffer)) {
                pctx->print_buffer[pctx->print_buffer_len] = '\0';
                DS_OnPrintLine(pctx, pctx->print_buffer);
            }

            pctx->print_buffer_len = 0;
        } else {
            if (pctx->print_buffer_len < sizeof(pctx->print_buffer) - 1) {
                pctx->print_buffer[pctx->print_buffer_len] = *src;
            }
            pctx->print_buffer_len ++;
        }

        src ++;
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_UpdateName_cb (int client_num, const char *name, void *ctx)
{
    ds_ctx_t *pctx = ctx;

    // Take client names from the first level with an intermission.
    if (!pctx->any_intermission_seen
            && client_num >= 0 && client_num < DS_MAX_CLIENTS) {
        Q_strncpyz(pctx->demo_summary->client_names[client_num], (char *)name,
                   MAX_SCOREBOARDNAME);
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_Time_cb (float time, void *ctx)
{
    ds_ctx_t *pctx = ctx;

    pctx->level_time = time;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_Intermission_cb (void *ctx)
{
    ds_ctx_t *pctx = ctx;

    if (!pctx->intermission_seen) {
        pctx->demo_summary->total_time += pctx->level_time;
        pctx->demo_summary->num_maps ++;
        pctx->intermission_seen = true;
        pctx->any_intermission_seen = true;
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_Cutscene_cb (void *ctx)
{
    ds_ctx_t *pctx = ctx;

    if (MapHasCutsceneAsIntermission(map_name)) {
        if (!pctx->intermission_seen) {
            pctx->demo_summary->total_time += pctx->level_time;
            pctx->demo_summary->num_maps ++;
            pctx->intermission_seen = true;
            pctx->any_intermission_seen = true;
        }
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_UpdateStat_cb (byte stat, int count, void *ctx)
{
    ds_ctx_t *pctx = ctx;

    switch (stat) {
        case STAT_SECRETS: pctx->secrets = count; break;
        case STAT_TOTALSECRETS: pctx->total_secrets = count; break;
        case STAT_MONSTERS: pctx->kills = count; break;
        case STAT_TOTALMONSTERS: pctx->total_kills = count; break;
        default: break;
    }

    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_KilledMonster_cb (void *ctx)
{
    ds_ctx_t *pctx = ctx;

    pctx->kills ++;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
DS_FoundSecret_cb (void *ctx)
{
    ds_ctx_t *pctx = ctx;

    pctx->secrets ++;
    return DP_CBR_CONTINUE;
}


qboolean
DS_GetDemoSummary (FILE *demo_file, demo_summary_t *demo_summary)
{
    qboolean ok = true;

    dp_err_t dprc;
    dp_callbacks_t callbacks = {
        .packet_start = DS_PacketStart_cb,
        .read = DS_Read_cb,
        .server_info_model = DS_ServerInfoModel_cb,
        .server_info = DS_ServerInfo_cb,
        .set_view = DS_SetView_cb,
        .print = DS_Print_cb,
        .update_name = DS_UpdateName_cb,
        .time = DS_Time_cb,
        .intermission = DS_Intermission_cb,
        .finale = DS_Intermission_cb,
        .cut_scene = DS_Cutscene_cb,
        .update_stat = DS_UpdateStat_cb,
        .killed_monster = DS_KilledMonster_cb,
        .found_secret = DS_FoundSecret_cb,
    };
    ds_ctx_t ctx = {
        .demo_file = demo_file,
        .level_time = 0.0,
        .demo_summary = demo_summary,
    };

    memset(demo_summary, 0, sizeof(*demo_summary));
    demo_summary->skill = -1;
    demo_summary->view_entity = -1;

    dprc = DP_ReadDemo(&callbacks, &ctx);
    DS_UpdateStats(&ctx);

    if (dprc != DP_ERR_SUCCESS) {
        Con_Printf("Error parsing demo: %u\n", dprc);
        ok = false;
    }

    return ok;
}
