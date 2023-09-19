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
#include "ghost_private.h"
#include "demosummary.h"


static cvar_t ghost_delta = {"ghost_delta", "1", CVAR_ARCHIVE};
static cvar_t ghost_range = {"ghost_range", "64", CVAR_ARCHIVE};
static cvar_t ghost_alpha = {"ghost_alpha", "0.8", CVAR_ARCHIVE};
static cvar_t ghost_marathon_split = {"ghost_marathon_split", "0", CVAR_ARCHIVE};
static cvar_t ghost_bar_x = { "ghost_bar_x", "0", CVAR_ARCHIVE };
static cvar_t ghost_bar_y = { "ghost_bar_y", "-4", CVAR_ARCHIVE };
static cvar_t ghost_bar_alpha = { "ghost_bar_alpha", "0.8", CVAR_ARCHIVE };


static dzip_context_t ghost_dz_ctx;
static ghost_info_t *ghost_info = NULL;
char                ghost_demo_path[MAX_OSPATH] = "";
static char         ghost_map_name[MAX_QPATH];
static ghostrec_t   *ghost_records = NULL;
static int          ghost_num_records = 0;
entity_t            ghost_entity;
static float        ghost_shift = 0.0f;
static float        ghost_finish_time = -1.0f;
static int          ghost_model_indices[GHOST_MODEL_COUNT];
static float        ghost_last_relative_time = 0.0f;
static demo_summary_t ghost_demo_summary;

const char *ghost_model_paths[GHOST_MODEL_COUNT] = {
    "progs/player.mdl",
    "progs/eyes.mdl",
    "progs/h_player.mdl",
};

#define MAX_MARATHON_LEVELS     256
typedef struct {
    char map_name[MAX_QPATH];
    float ghost_time;
    float player_time;
} ghost_marathon_level_t;
typedef struct {
    qboolean valid;         // has there been a ghost time for each level?
    ghost_marathon_level_t levels[MAX_MARATHON_LEVELS];
    float total_split;
    int num_levels;
} ghost_marathon_info_t;
static ghost_marathon_info_t ghost_marathon_info;

ghost_color_info_t ghost_color_info[GHOST_MAX_CLIENTS];


// This could be done more intelligently, no doubt.
static float Ghost_FindClosest (vec3_t origin, qboolean *match)
{
    int idx;
    ghostrec_t *rec;
    vec3_t diff;
    ghostrec_t *closest_rec = NULL;
    float closest_dist_sqr;
    float dist_sqr;

    // Ignore any matches that are not close by.
    closest_dist_sqr = 256.0f * 256.0f;

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records;
         rec++, idx++) {
        VectorSubtract(origin, rec->origin, diff);

        dist_sqr = DotProduct(diff, diff);
        if (dist_sqr < closest_dist_sqr) {
            closest_dist_sqr = dist_sqr;
            closest_rec = rec;
        }
    }

    if (closest_rec != NULL) {
        *match = true;
        return cl.ctime - closest_rec->time;
    } else {
        *match = false;
        return 0.0f;
    }
}


// Find the index of the first record that is >= time.
//
// If speed becomes a concern we could do this iteratively or with a binary
// search.
static int Ghost_FindRecord (float time)
{
    int idx;
    ghostrec_t *rec;

    if (ghost_records == NULL) {
        // not loaded
        return -1;
    }

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records && time > rec->time;
         idx++, rec++);

    if (idx == 0) {
        // not yet at the first record
        return -1;
    }

    if (idx == ghost_num_records) {
        // gone beyond the last record
        return -1;
    }

    return idx;
}


static FILE *
Ghost_OpenDemoOrDzip (const char *demo_path)
{
    FILE *demo_file = NULL;
    dzip_status_t dzip_status;

    if (strlen(demo_path) > 3
        && !Q_strcasecmp(demo_path + strlen(demo_path) - 3, ".dz"))
    {
        dzip_status = DZip_Open(&ghost_dz_ctx, demo_path, &demo_file);
        switch (dzip_status) {
            case DZIP_ALREADY_EXTRACTING:
                Sys_Error("Already extracting despite sync only usage");
                break;
            case DZIP_NO_EXIST:
                Con_Printf ("ERROR: couldn't open %s\n", demo_path);
                break;
            case DZIP_EXTRACT_SUCCESS:
                break;
            case DZIP_EXTRACT_FAIL:
                Con_Printf ("ERROR: couldn't extract %s\n", demo_path);
                break;
        }

    } else
    {
        char demo_path_with_ext[MAX_OSPATH + 4];
        Q_strlcpy(demo_path_with_ext, demo_path, MAX_OSPATH);
        COM_DefaultExtension (demo_path_with_ext, ".dem");
        if (COM_FOpenFile (demo_path_with_ext, &demo_file) == -1) {
            Con_Printf("cannot find demo %s", demo_path_with_ext);
            demo_file = NULL;
        }
    }

    return demo_file;
}


static qboolean first_update;

extern char *GetPrintedTime(double time);   // Maybe put the definition somewhere central?
static void Ghost_Load (void)
{
    int i;
    FILE *demo_file;
    ghost_level_t *level;

    ghost_records = NULL;
    ghost_num_records = 0;
    ghost_finish_time = -1.0f;

    if (ghost_demo_path[0] == '\0') {
        if (ghost_info != NULL) {
            Sys_Error("ghost_demo_path empty but ghost_info is not NULL");
        }
        return;
    }
    if (ghost_info == NULL) {
        Sys_Error("ghost_demo_path not empty but ghost info is NULL");
    }

    for (i = 0; i < ghost_info->num_levels; i++) {
        level = &ghost_info->levels[i];
        if (strcmp(ghost_map_name, level->map_name) == 0) {
            break;
        }
    }
    if (i == ghost_info->num_levels) {
        Con_Printf("Map %s not found in ghost demo\n", ghost_map_name);
        return;
    }

    ghost_records = level->records;
    ghost_num_records = level->num_records;
    memcpy(ghost_model_indices,
           level->model_indices,
           sizeof(level->model_indices));

    // Print player names
    Con_Printf("Ghost player(s): ");
    for (i = 0; i < GHOST_MAX_CLIENTS; i++) {
        if (level->client_names[i][0] != '\0') {
            Con_Printf(" %s ", level->client_names[i]);
        }
        ghost_color_info[i].colors = level->client_colors[i];
    }
    Con_Printf("\n");

    // Print finish time
    ghost_finish_time = level->finish_time;
    if (level->finish_time > 0) {
        Con_Printf("Ghost time:       %s\n", GetPrintedTime(level->finish_time));
    }

    ghost_entity.skinnum = 0;
    ghost_entity.modelindex = -1;
    ghost_entity.translate_start_time = 0.0f;
    ghost_entity.frame_start_time = 0.0f;
    ghost_entity.scale = ENTSCALE_DEFAULT;

    ghost_shift = 0.0f;
    ghost_last_relative_time = 0.0f;
    if ((!sv.active && !cls.demoplayback) || cls.marathon_level == 0) {
        ghost_marathon_info.total_split = 0.0f;
        ghost_marathon_info.num_levels = 0;
    }

    first_update = true;
}


static void Ghost_LerpOrigin(vec3_t origin1, vec3_t origin2, float frac,
                             vec3_t origin)
{
    int i;
    float d;

    for (i=0; i<3; i++) {
        d = origin2[i] - origin1[i];
        origin[i] = origin1[i] + frac * d;
    }
}


static void Ghost_LerpAngle(vec3_t angles1, vec3_t angles2, float frac,
                            vec3_t angles)
{
    int i;
    float d;

    for (i=0; i<3; i++) {
        d = angles2[i] - angles1[i];
        if (d > 180)
            d -= 360;
        else if (d < -180)
            d += 360;
        angles[i] = angles1[i] + frac * d;
    }
}


static qboolean Ghost_SetAlpha(void)
{
    entity_t *ent = &cl_entities[cl.viewentity];
    float alpha;
    vec3_t diff;
    float dist;

    // distance from player to ghost
    VectorSubtract(ent->origin, ghost_entity.origin, diff);
    dist = VectorLength(diff);

    // fully opaque at range+64, fully transparent at range
    alpha = bound(0.0f, (dist - ghost_range.value) / 64.0f, 1.0f);

    // scale by cvar alpha
    alpha *= bound(0.0f, ghost_alpha.value, 1.0f);

    ghost_entity.transparency = alpha;

    return alpha != 0.0f;
}


static qboolean Ghost_Update (void)
{
    float lookup_time = cl.ctime + ghost_shift;
    int after_idx = Ghost_FindRecord(lookup_time);
    ghostrec_t *rec_before;
    ghostrec_t *rec_after;
    float frac;
    qboolean ghost_show;
    int i, clientnum = 0;   // supporting only 1 ghost (yet)

    ghost_show = (after_idx != -1);

    if (ghost_show) {
        rec_after = &ghost_records[after_idx];
        rec_before = &ghost_records[after_idx - 1];

        ghost_show = false;
        for (i = 0; !ghost_show && i < GHOST_MODEL_COUNT; i++) {
            if (ghost_model_indices[i] != 0
                    && rec_after->model == ghost_model_indices[i]) {
                ghost_show = true;
                ghost_entity.model = Mod_ForName((char *)ghost_model_paths[i],
                                                  false);
                if (first_update) {
                    CL_NewTranslation(clientnum, true);
                    first_update = false;
                }
                if (i == GHOST_MODEL_PLAYER)
                    ghost_entity.colormap = ghost_color_info[clientnum].translations;
                else
                    ghost_entity.colormap = vid.colormap;  // eyes, head
            }
        }
    }

    if (ghost_show) {
        frac = (lookup_time - rec_before->time)
                / (rec_after->time - rec_before->time);

        // TODO: lerp animation frames
        ghost_entity.frame = rec_after->frame;

        Ghost_LerpOrigin(rec_before->origin, rec_after->origin,
                         frac,
                         ghost_entity.origin);
        Ghost_LerpAngle(rec_before->angle, rec_after->angle,
                        frac,
                        ghost_entity.angles);

        // Set alpha based on distance to player.
        ghost_show = Ghost_SetAlpha();
    }

    return ghost_show;
}


void R_DrawAliasModel (entity_t *ent);
void Ghost_Draw (void)
{
    /*
     * Reload the ghost if level has changed.
     */
    if (ghost_demo_path[0] != '\0'
            && strncmp(ghost_map_name, CL_MapName(), MAX_QPATH)) {
        Q_strncpyz(ghost_map_name, CL_MapName(), MAX_QPATH);
        if (ghost_map_name[0] != '\0') {
            Ghost_Load();
        }
    }

    /*
     * These attributes are required by R_DrawAliasModel:
     *  - model
     *  - skinnum
     *  - colormap
     *  - transparency
     *  - origin
     *  - angles
     *  - modelindex
     *  - frame
     */
    if (Ghost_Update()) {
        currententity = &ghost_entity;
        R_DrawAliasModel (&ghost_entity);
    }
}


static void
Ghost_DrawSingleTime (int y, float relative_time, char *label)
{
    int   x, size, bg_color, bar_color;
    float scale, width, alpha;
    char  st[10];

	scale = Sbar_GetScaleAmount();
	size = Sbar_GetScaledCharacterSize();

    if (relative_time < 1e-3) {
        Q_snprintfz (st, sizeof(st), "%.2f s", relative_time);
    } else {
        Q_snprintfz (st, sizeof(st), "+%.2f s", relative_time);
    }

    bg_color = relative_time > 0 ? 251 : 10;
    x = ((vid.width - (int)(160 * scale)) / 2) + (ghost_bar_x.value * size);
    alpha = ghost_bar_alpha.value;
    Draw_AlphaFill(x, y - (int)(1 * scale), 160, 1, bg_color, alpha);
    Draw_AlphaFill(x, y + (int)(9 * scale), 160, 1, bg_color, alpha);
    Draw_AlphaFill(x, y, 160, 9, 52, alpha);

    width = fabs(relative_time) * 160;
    if (width > 160) {
        // abs(relative time) > 1 second
        width *= 0.1;
        bar_color = 36;
    } else {
        // abs(relative time) < 1 second
        bar_color = 100;
    }
    if (width > 160) {
        width = 160;
    }

    if (relative_time < 0) {
        Draw_AlphaFill(x + (160 - width) * scale, y, width, 9, bar_color, alpha);
    } else {
        Draw_AlphaFill(x, y, width, 9, bar_color, alpha);
    }

    Draw_String (x + (int)(7.5 * size) - (strlen(st) * size), y, st, true);
    Draw_String (x - size * (2 + strlen(label)), y, label, true);
}


static const char *RedString(char *str)
{
    static char out[256];
    int i;

    for (i = 0; i < sizeof(out) - 1 && str[i] != '\0'; i++) {
        if (str[i] >= 0x20) {
            out[i] = str[i] | 0x80;
        } else {
            out[i] = str[i];
        }
    }
    out[i] = '\0';

    return out;
}


#define GHOST_MAX_SUMMARY_DESCS 8
#define GHOST_MAX_SUMMARY_LINES 4

static int Ghost_DrawDemoSummary (void)
{
    extern char *skill_modes[];
    demo_summary_t *gds = &ghost_demo_summary;
    char stat_descs[GHOST_MAX_SUMMARY_DESCS][32];
    char lines[GHOST_MAX_SUMMARY_LINES][256];
    int num_lines;
    int num_players;
    int num_descs = 0;
    int size, y, i, col, max_line_width;

    // Player name(s).
    if (num_descs < GHOST_MAX_SUMMARY_DESCS) {
        num_players = 0;
        for (i = 0; i < GHOST_MAX_CLIENTS; i++) {
            if (gds->client_names[i][0] != '\0') {
                num_players ++;
            }
        }
        if (num_players == 1) {
            // Single player.
            Q_snprintfz(stat_descs[num_descs],
                        sizeof(stat_descs[num_descs]),
                        "%s %s",
                        RedString("Ghost:"),
                        gds->client_names[0]);
        } else if (gds->view_entity - 1 >= 0
                       && gds->view_entity - 1 < GHOST_MAX_CLIENTS
                       && gds->client_names[gds->view_entity - 1][0] != '\0') {
            // Normal co-op demo.
            Q_snprintfz(stat_descs[num_descs],
                        sizeof(stat_descs[num_descs]),
                        "%s %s +%d",
                        RedString("Ghost:"),
                        gds->client_names[gds->view_entity - 1],
                        num_players - 1);
        } else {
            // Co-op demo recam, or something weird.
            Q_snprintfz(stat_descs[num_descs],
                        sizeof(stat_descs[num_descs]),
                        "%s %d players",
                        RedString("Ghost:"),
                        num_players);
        }
        num_descs ++;
    }

    // Total finish time.
    if (num_descs < GHOST_MAX_SUMMARY_DESCS) {
        Q_snprintfz(stat_descs[num_descs],
                    sizeof(stat_descs[num_descs]),
                    "%s",
                    GetPrintedTime(gds->total_time));
        num_descs ++;
    }

    // Skill.
    if (gds->skill >= 0 && gds->skill <= 3) {
        Q_snprintfz(stat_descs[num_descs],
                    sizeof(stat_descs[num_descs]),
                    "%s",
                    skill_modes[gds->skill]);
        num_descs ++;
    }

    // Number of maps in demo.
    if (gds->num_maps > 1) {
        Q_snprintfz(stat_descs[num_descs],
                    sizeof(stat_descs[num_descs]),
                    "%d maps",
                    gds->num_maps);
        num_descs ++;
    }

    // Kills.
    if (num_descs < GHOST_MAX_SUMMARY_DESCS) {
        Q_snprintfz(stat_descs[num_descs],
                    sizeof(stat_descs[num_descs]),
                    "%.1f%% kills",
                    100.0f * (gds->kills + 1e-4) / (gds->total_kills + 1e-4));
        num_descs ++;
    }

    // Secrets.
    if (num_descs < GHOST_MAX_SUMMARY_DESCS) {
        Q_snprintfz(stat_descs[num_descs],
                    sizeof(stat_descs[num_descs]),
                    "%.1f%% secrets",
                    100.0f * (gds->secrets + 1e-4)
                        / (gds->total_secrets + 1e-4));
        num_descs ++;
    }

    // Format each stat into lines that do not wrap.
    size = Sbar_GetScaledCharacterSize();
    max_line_width = (vid.width / size) + 1;
    if (max_line_width > sizeof(lines[0])) {
        max_line_width = sizeof(lines[0]);
    }
    col = 0;
    num_lines = 0;
    for (i = 0; i < num_descs && num_lines < GHOST_MAX_SUMMARY_LINES; i++) {
        if (col != 0
            && strlen(stat_descs[i]) + col + 3 >= max_line_width) {
            num_lines ++;
            col = 0;
        }

        if (num_lines < GHOST_MAX_SUMMARY_LINES) {
            if (col != 0) {
                Q_snprintfz(lines[num_lines] + col,
                            max_line_width - col,
                            " \x8f ");
                col += 3;
            }
            Q_snprintfz(lines[num_lines] + col,
                        max_line_width - col,
                        "%s",
                        stat_descs[i]);
            col += strlen(stat_descs[i]);
        }
    }
    if (col != 0) {
        num_lines++;
    }

    // Draw the lines onto the screen, centred at the bottom.
    y = vid.height - (int)(1.25 * size);
    for (i = num_lines - 1; i >= 0; i--) {
        Draw_String ((vid.width - size * strlen(lines[i])) / 2,
                     y, lines[i], true);

        if (i > 0) {
            y -= size;
        }
    }

    return y;
}


static void Ghost_DrawIntermissionTimes (void)
{
    int size, x, y, i, min_y;
    float relative_time, scale;
    float total;
    char st[16];
    ghost_marathon_level_t *gml;
    ghost_marathon_info_t *gmi = &ghost_marathon_info;

    scale = Sbar_GetScaleAmount();
    size = Sbar_GetScaledCharacterSize();

    y = Ghost_DrawDemoSummary();

    // Don't draw over the usual end of level screen or the finale text.
    min_y = ((cl.intermission == 2) ? 182 : 174) * scale;

    total = gmi->total_split;
    for (i = gmi->num_levels - 1; i >= 0; i--) {
        y -= 2 * size;
        if (y <= min_y) {
            break;
        }
        gml = &gmi->levels[i];
        relative_time = gml->player_time - gml->ghost_time;
        Ghost_DrawSingleTime(y, relative_time, gml->map_name);

        if (i > 0) {
            if (total < 1e-3) {
                Q_snprintfz (st, sizeof(st), "%.2f s", total);
            } else {
                Q_snprintfz (st, sizeof(st), "+%.2f s", total);
            }
            total -= relative_time;
        } else {
            Q_snprintfz (st, sizeof(st), "%s", RedString("Total"));
        }

        if (gmi->num_levels > 1) {
            x = ((vid.width + (int)(184 * scale)) / 2) + (ghost_bar_x.value * size);
            Draw_String (x, y, st, true);
        }
    }
}


// Modified from the JoeQuake speedometer
void Ghost_DrawGhostTime (qboolean intermission)
{
    int size, y;
    float relative_time;
    qboolean match;
    entity_t *ent;

    if (!ghost_delta.value || ghost_records == NULL)
        return;

    if (!intermission) {
        if (cl.intermission) {
            return;
        }
        ent = &cl_entities[cl.viewentity];
        if (!ghost_delta.value || ghost_records == NULL)
            return;
        relative_time = Ghost_FindClosest(ent->origin, &match);
        if (!match)
            return;

        size = Sbar_GetScaledCharacterSize();

        // Don't show small changes, to reduce flickering.
        if (fabs(relative_time - ghost_last_relative_time) < 1e-4 + 1./72)
            relative_time = ghost_last_relative_time;
        ghost_last_relative_time = relative_time;

        if (ghost_marathon_split.value) {
            // Include the marathon time in the split.
            relative_time += ghost_marathon_info.total_split;
        }

        //joe: Position can be customized by the ghost_bar_y cvar
        // By default, it is drawn just above the speedo
        y = ELEMENT_Y_COORD(ghost_bar);
        if (cl.intermission)
            y = vid.height - (2 * size);

        Ghost_DrawSingleTime(y, relative_time, "");
    } else {
        Ghost_DrawIntermissionTimes();
    }
}


void Ghost_Finish (void)
{
    int i;
    float split;
    ghost_marathon_level_t *gml;
    ghost_marathon_info_t *gmi = &ghost_marathon_info;

    if (ghost_finish_time > 0) {
        if (!sv.active && !cls.demoplayback) {
            // Can't track marathons for remote servers.
            gmi->valid = false;
        } else if (cls.marathon_level == 1) {
            gmi->valid = true;
        }
        if (!gmi->valid) {
            // If the ghost did not finish one of the levels, just print this
            // level's splits.
            gmi->num_levels = 1;
        } else {
            gmi->num_levels = cls.marathon_level;
        }

        if (gmi->num_levels - 1 < MAX_MARATHON_LEVELS) {
            gml = &gmi->levels[gmi->num_levels - 1];
            Q_strncpyz(gml->map_name, CL_MapName(), MAX_QPATH);
            gml->player_time = cl.mtime[0];
            gml->ghost_time = ghost_finish_time;
        }

        Con_Printf("ghost splits:\n");
        Con_Printf("  map                time    split    total\n");
        Con_Printf("  \x9d\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9f "
                   "\x9d\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9f "
                   "\x9d\x9e\x9e\x9e\x9e\x9e\x9e\x9f "
                   "\x9d\x9e\x9e\x9e\x9e\x9e\x9e\x9f\n");
        gmi->total_split = 0.0f;
        for (i = 0; i < gmi->num_levels; i++) {
            gml = &gmi->levels[i];
            split = (gml->player_time - gml->ghost_time);
            gmi->total_split += split;
            Con_Printf("  %-10s %12s %+8.2f %+8.2f\n",
                       gml->map_name,
                       GetPrintedTime(gml->player_time),
                       split,
                       gmi->total_split);
        }
    } else {
        if (gmi->valid) {
            Con_Printf("Ghost did not finish, marathon stopped\n");
        }
        gmi->valid = false;
    }
}


static void Ghost_PrintSummary (void)
{
    extern char *skill_modes[];
    demo_summary_t *gds = &ghost_demo_summary;
    int i;
    qboolean possible_recam = false;

    Con_Printf("ghost %s has been added\n", ghost_demo_path);
    if (gds->total_time != 0.) {
        if (gds->view_entity - 1 >= 0
                && gds->view_entity - 1 < GHOST_MAX_CLIENTS
                && gds->client_names[gds->view_entity - 1][0] != '\0') {
            Con_Printf("  %s %s\n",
                       RedString("View Player"),
                       gds->client_names[gds->view_entity - 1]);
        } else {
            possible_recam = true;
        }
        Con_Printf("    %s", RedString("Player(s)"));
        for (i = 0; i < GHOST_MAX_CLIENTS; i++) {
            if (gds->client_names[i][0] != '\0') {
                Con_Printf(" %s ", gds->client_names[i]);
            }
        }
        Con_Printf("\n");
        Con_Printf("       %s", RedString("Map(s)"));
        for (i = 0; i < gds->num_maps && i < 4; i++) {
            Con_Printf(" %s", gds->maps[i]);
        }
        if (i < gds->num_maps) {
            Con_Printf("... (%d more)", gds->num_maps - i);
        }
        Con_Printf("\n");
        if (gds->skill >= 0 && gds->skill <= 3) {
            Con_Printf("        %s %s\n",
                       RedString("Skill"),
                       skill_modes[gds->skill]);
        }
        if (gds->total_time > 0) {
            Con_Printf("         %s %s\n",
                       RedString("Time"),
                       GetPrintedTime(gds->total_time));
            Con_Printf("        %s %d / %d\n",
                       RedString("Kills"),
                       gds->kills,
                       gds->total_kills);
            Con_Printf("      %s %d / %d\n",
                       RedString("Secrets"),
                       gds->secrets,
                       gds->total_secrets);

        }
    } else {
        Con_Printf("%s Ghost demo does not contain an intermission screen\n",
                   RedString("WARNING:"));
    }
    if (possible_recam) {
        Con_Printf("%s Ghost demo appears to be a recam\n",
                   RedString("WARNING:"));
    }
    Con_Printf("\n");
}


static void Ghost_Command_f (void)
{
    qboolean ok = true;
    FILE *demo_file = NULL;
    char demo_path[MAX_OSPATH];

    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 2) {
        if (ghost_demo_path[0] == '\0') {
            Con_Printf("no ghost has been added\n");
        } else {
            Ghost_PrintSummary();
        }
        Con_Printf("ghost <demoname> : add a ghost\n");
        return;
    }

    Q_strlcpy(demo_path, Cmd_Argv(1), sizeof(demo_path));
    demo_file = Ghost_OpenDemoOrDzip(demo_path);
    ok = (demo_file != NULL);

    if (ok) {
        ok = DS_GetDemoSummary(demo_file, &ghost_demo_summary);
    }

    if (ok) {
        fseek(demo_file, 0, SEEK_SET);
        if (ghost_info != NULL) {
            Ghost_Free(&ghost_info);
        }
        ok = Ghost_ReadDemo(demo_file, &ghost_info);
        // file is now closed.
    }

    if (ok) {
        Q_strlcpy(ghost_demo_path, demo_path, sizeof(ghost_demo_path));
        Ghost_PrintSummary();
        ghost_map_name[0] = '\0';  // force ghost to be loaded next time map is rendered.
    }

    if (!ok) {
        ghost_demo_path[0] = '\0';
    }

    DZip_Cleanup(&ghost_dz_ctx);
}


static void Ghost_RemoveCommand_f (void)
{
    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 1)
    {
        Con_Printf("ghost_remove : remove ghost\n");
        return;
    }

    if (ghost_demo_path[0] == '\0') {
        Con_Printf("no ghost has been added\n");
    } else {
        Ghost_Free(&ghost_info);
        ghost_demo_path[0] = '\0';
    }
}


static void Ghost_ShiftCommand_f (void)
{
    float delta;
    qboolean match;
    entity_t *ent = &cl_entities[cl.viewentity];

    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 2)
    {
        Con_Printf("ghost_shift <t> : place ghost <t> seconds ahead of "
                   "player\n");
        return;
    }

    if (ghost_records == NULL) {
        Con_Printf("ghost not loaded\n");
        return;
    }


    delta = Ghost_FindClosest(ent->origin, &match);
    if (match) {
        ghost_shift = Q_atof(Cmd_Argv(1)) - delta;
    } else {
        Con_Printf("Cannot shift: Player cannot be matched to ghost's path\n");
    }
}


static void Ghost_ShiftResetCommand_f (void)
{
    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 1)
    {
        Con_Printf("ghost_reset_shift : undo ghost shift\n");
        return;
    }

    if (ghost_records == NULL) {
        Con_Printf("ghost not loaded\n");
        return;
    }

    ghost_shift = 0.0f;
}


void Ghost_Init (void)
{
    DZip_Init (&ghost_dz_ctx, "ghost");
    DZip_Cleanup(&ghost_dz_ctx);
    Cmd_AddCommand ("ghost", Ghost_Command_f);
    Cmd_AddCommand ("ghost_remove", Ghost_RemoveCommand_f);
    Cmd_AddCommand ("ghost_shift", Ghost_ShiftCommand_f);
	Cmd_AddCommand ("ghost_shift_reset", Ghost_ShiftResetCommand_f);

    Cvar_Register (&ghost_delta);
    Cvar_Register (&ghost_range);
    Cvar_Register (&ghost_alpha);
    Cvar_Register (&ghost_marathon_split);
    Cvar_Register (&ghost_bar_x);
    Cvar_Register (&ghost_bar_y);
    Cvar_Register (&ghost_bar_alpha);
}


void Ghost_Shutdown (void)
{
}
