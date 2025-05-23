/*
 * Module to draw a more detailed list of local demos.
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

#include "../ghost/demosummary.h"
#include "../ghost/ghost.h"
#include "browser.h"
#include "qcurses.h"
#include "browser_local.h"
#include <SDL.h>
#include <SDL_thread.h>
#include <ctype.h>

qcurses_demlist_t * demlist = NULL;
extern qcurses_box_t * main_box;
extern char ghost_demo_path[MAX_OSPATH];

extern int browserscale;

static qboolean search_input = false;

static char search_term[40] = "\0";
static char * qcurses_skills[4] = { "Easy", "Normal", "Hard", "Nightmare" };

SDL_sem *filelist_lock = NULL;

extern void SearchForDemos (void);
extern FILE *Ghost_OpenDemoOrDzip(const char * demo_path);
extern char *GetPrintedTime(double time);
extern char *toYellow(char *s);

/* 
 * mouse callback for the local demo list
 */
void mouse_move_cursor(qcurses_char_t * self, const mouse_state_t *ms) {
    int row = self->callback_data.row - 2;
    if (row < 0 || row >= demlist->list.places || row >= demlist->list.len - demlist->list.window_start)
        return;

    demlist->list.cursor = demlist->list.window_start + row;

    if (ms->button_up == 1) M_Demos_KeyHandle_Local(K_MOUSE1, main_box->rows - 20);
    if (ms->button_up == 2) M_Demos_KeyHandle_Local(K_MOUSE2, main_box->rows - 20);
}

/*
 * Display the local demo list, by compositing boxes together.
 * 
 * Needs thread support from SDL.
 */
void M_Demos_DisplayLocal (int cols, int rows, int start_col, int start_row) {
    if (!filelist_lock)
        filelist_lock = SDL_CreateSemaphore(1);

    qcurses_box_t * local_box  = qcurses_init(cols, rows);
    qcurses_box_t * help_box   = qcurses_init(cols, 10);
    qcurses_box_t * map_box    = qcurses_init_callback(15, rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * skill_box  = qcurses_init_callback(10, rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * kill_box   = qcurses_init_callback(12, rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * secret_box = qcurses_init_callback(9 , rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * time_box   = qcurses_init_callback(15, rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * player_box = qcurses_init_callback(15, rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * size_box   = qcurses_init_callback(8 , rows - help_box->rows, mouse_move_cursor);
    qcurses_box_t * name_box   = qcurses_init_callback(cols - map_box->cols - skill_box->cols - kill_box->cols - secret_box->cols - time_box->cols - size_box->cols - player_box->cols, rows - help_box->rows, mouse_move_cursor);

    /* header */
    qcurses_print(name_box, 0, 0, "NAME", true);
    qcurses_make_bar(name_box, 1);
    qcurses_print(map_box, 0, 0, "MAP", true);
    qcurses_make_bar(map_box, 1);
    qcurses_print(player_box, 0, 0, "PLAYER", true);
    qcurses_make_bar(player_box, 1);
    qcurses_print(skill_box, 0, 0, "SKILL", true);
    qcurses_make_bar(skill_box, 1);
    qcurses_print(kill_box, 0, 0, "KILLS", true);
    qcurses_make_bar(kill_box, 1);
    qcurses_print(secret_box, 0, 0, "SECRETS", true);
    qcurses_make_bar(secret_box, 1);
    qcurses_print(time_box, 0, 0, "TIME", true);
    qcurses_make_bar(time_box, 1);
    qcurses_print(size_box, 0, 0, "SIZE", true);
    qcurses_make_bar(size_box, 1);

    /* parse each individual potential file */
    for (int i = 0; i < min(demlist->list.len, demlist->list.places); i++) {
        direntry_t * d = demlist->entries + demlist->list.window_start + i;
        demo_summary_t * s = *(demlist->summaries + demlist->list.window_start + i);
        if (ghost_demo_path[0] != '\0' && strcmp(ghost_demo_path, va("..%s/%s", demodir, demlist->entries[demlist->list.window_start + i].name)) == 0)
            qcurses_print(name_box, 0, i + 2, "\x84", false);
        qcurses_print(name_box, 1, i + 2, d->name, !d->type);

        switch (d->type) {
        case 0:
            qcurses_print(size_box, 0, i + 2, toYellow(va("%7ik", (d->size) >> 10)), false); 
            if (search_input) {
                qcurses_print(map_box, 0, i + 2, "not in search", false); 
            } else {
                SDL_SemWait(filelist_lock);
                if (s->total_time != -1) {
                    if (s->skill >= 0 && s->skill <= 3)
                        qcurses_print(skill_box, 0, i + 2, qcurses_skills[s->skill], false); 
                    qcurses_print(map_box, 0, i + 2, s->maps[0], false); 
                    qcurses_print(player_box, 0, i + 2, s->client_names[0], true); 
                    qcurses_print(kill_box, 0, i + 2, va("%4d/%s", s->kills, toYellow(va("%4d", s->total_kills))), true);
                    qcurses_print(secret_box, 0, i + 2, va("%3d/%s", s->secrets, toYellow(va("%3d", s->total_secrets))), true);
                    qcurses_print(time_box, 0, i + 2, s->total_time ? GetPrintedTime(s->total_time) : "N/A", true); 
                }
                SDL_SemPost(filelist_lock);
            }
            break;
        case 1:
            qcurses_print(size_box, 0, i + 2, "  folder", false); 
            break;
        case 2: 
            qcurses_print(size_box, 0, i + 2, "      up", false); 
            break;
        }
    }

    if (demlist)
        qcurses_print(name_box, 0, 2 + demlist->list.cursor - demlist->list.window_start, blinkstr(0x0d), false);

    M_Demos_HelpBox (help_box, TAB_LOCAL_DEMOS, search_term, search_input);

    int filled_cols = 0;
    qcurses_insert(local_box, filled_cols, 0, name_box);
    qcurses_insert(local_box, filled_cols = filled_cols + name_box->cols, 0, map_box);
    qcurses_insert(local_box, filled_cols = filled_cols + map_box->cols, 0, player_box);
    qcurses_insert(local_box, filled_cols = filled_cols + player_box->cols, 0, skill_box);
    qcurses_insert(local_box, filled_cols = filled_cols + skill_box->cols, 0, kill_box);
    qcurses_insert(local_box, filled_cols = filled_cols + kill_box->cols, 0, secret_box);
    qcurses_insert(local_box, filled_cols = filled_cols + secret_box->cols, 0, time_box);
    qcurses_insert(local_box, filled_cols = filled_cols + time_box->cols, 0, size_box);
    qcurses_insert(local_box, 0, name_box->rows, help_box);
    qcurses_insert(main_box, start_col, start_row, local_box);

    qcurses_free(name_box);
    qcurses_free(map_box);
    qcurses_free(skill_box);
    qcurses_free(player_box);
    qcurses_free(kill_box);
    qcurses_free(secret_box);
    qcurses_free(time_box);
    qcurses_free(size_box);
    qcurses_free(help_box);
    qcurses_free(local_box);
}

/*
 * get summary, in a separate thread.
 */
int get_summary_thread(void * entry) {
    demo_summary_t *summary = calloc(1, sizeof(demo_summary_t));

    FILE *demo_file = Ghost_OpenDemoOrDzip(((thread_data_t *) entry)->path);;
    int ok = DS_GetDemoSummary(demo_file, summary);
    if (ok) {
        SDL_SemWait(filelist_lock);
        memcpy(((thread_data_t *) entry)->summary, summary, sizeof(demo_summary_t));
        SDL_SemPost(filelist_lock);
    }

    fclose(demo_file);
    free((thread_data_t *) entry);
    free(summary);
    return ok;
}

/*
 * read the demo list from filesystem and parse it
 */
void M_Demos_LocalRead(int rows, char * prevdir) {
    SearchForDemos ();

    if (demlist) {
        for (int i = 0; i < demlist->list.len; i++) {
            free(demlist->entries[i].name);
            free(demlist->summaries[i]);
        }
        free(demlist->entries);
        free(demlist->summaries);
        free(demlist);
    }

    demlist = calloc(1, sizeof(qcurses_demlist_t));
    demlist->list.len = num_files;
    demlist->list.places = rows;
    demlist->list.cursor = 0;
    demlist->list.window_start = 0;
    demlist->entries = calloc(demlist->list.len, sizeof(direntry_t));
    demlist->summaries = calloc(demlist->list.len, sizeof(demo_summary_t*));


    int j = 0;
    for (int i = 0; i < num_files; i++) {
        if (!search_term[0] || Q_strcasestr(filelist[i].name, search_term)) {
            demlist->entries[j].name = Q_strdup(filelist[i].name);
            demlist->entries[j].type = filelist[i].type;
            demlist->entries[j].size = filelist[i].size;

            demlist->summaries[j] = calloc(1, sizeof(demo_summary_t));
            demlist->summaries[j]->total_time = -1;

            if (prevdir && !strcmp(demlist->entries[j].name, prevdir)) {
                demlist->list.cursor = j;
                demlist->list.window_start = 0;
            }

            if (!search_input && demlist->entries[j].type == 0 && !Q_strcasestr(demlist->entries[j].name, ".dz")) {
                thread_data_t * data = calloc(1, sizeof(thread_data_t));

                data->summary = *(demlist->summaries + j);
                Q_snprintfz(data->path, sizeof(data->path), "..%s/%s", demodir, demlist->entries[j].name);
                SDL_CreateThread(get_summary_thread, "make summary", (void *) data);
            }
            j++;
        }
    }

    if (search_term[0]) {
        demlist->list.len = j;
    }
    qcurses_list_move_cursor((qcurses_list_t *)demlist, 0);
}

/*
 * keyboard input handler for search of local demos
 */
void M_Demos_KeyHandle_Local_Search (int k, int max_lines) {
    int len = strlen(search_term);

    switch (k) {
    case K_ESCAPE:
    case K_ENTER:
        search_input = false;
        break;
    case K_BACKSPACE:
        if (len){
            search_term[len - 1] = '\0';
        }
        break;
    default:
        if (isalnum(k) || k == '_'){
            if (len < 40)
                search_term[len] = k;
        }
        break;
    }

    M_Demos_LocalRead(max_lines, NULL);
}

/*
 * keyboard input handler for local demos
 */
void M_Demos_KeyHandle_Local (int k, int max_lines) {
    if (search_input) {
        M_Demos_KeyHandle_Local_Search(k, max_lines);
        return;
    }

    int distance = 1;

    switch (k) {
    case 'b':
        if (!keydown[K_CTRL])
            break;
    case K_PGUP:
    case K_HOME:
    case K_MWHEELUP:
        distance = keydown[K_HOME] ? 10000 : 10;
    case K_UPARROW:
    case 'k':
        qcurses_list_move_cursor((qcurses_list_t*)demlist, -distance);
        S_LocalSound("misc/menu1.wav");
        break;
    case 'd':
        if (!keydown[K_CTRL])
            break;
    case K_PGDN:
    case K_END:
    case K_MWHEELDOWN:
        distance = keydown[K_END] ? 10000 : 10;
    case K_DOWNARROW:
    case 'j':
        qcurses_list_move_cursor((qcurses_list_t*)demlist, distance);
        S_LocalSound("misc/menu1.wav");
        break;
    case 'f':
        if (keydown[K_CTRL])
    case '/':
            search_input = true;
        break;
    case K_ENTER:
    case K_MOUSE1:
        if (!demlist || demlist->entries[demlist->list.cursor].type == 3)
            break;

        if (keydown[K_CTRL] && keydown[K_SHIFT]) {
            Cbuf_AddText ("ghost_remove\n");
        }
        else if (demlist->entries[demlist->list.cursor].type) {
            char * prevdir = NULL;
            if (demlist->entries[demlist->list.cursor].type == 2) {
                char * p;
                if ((p = strrchr(demodir, '/'))) {
                    prevdir = p + 1;
                    *p = 0;
                }
            } else {
                strncat (demodir, va("/%s", demlist->entries[demlist->list.cursor].name), sizeof(demodir) - 1);
            }
            M_Demos_LocalRead(max_lines, prevdir);
        } else {
            if (keydown[K_CTRL] && !keydown[K_SHIFT]) {
                Cbuf_AddText (va("ghost \"..%s/%s\"\n", demodir, demlist->entries[demlist->list.cursor].name));
            } else {
                if (!keydown[K_ALT])
                    key_dest = key_game;
                Cbuf_AddText (va("playdemo \"..%s/%s\"\n", demodir, demlist->entries[demlist->list.cursor].name));
            }
        }
    }
}
