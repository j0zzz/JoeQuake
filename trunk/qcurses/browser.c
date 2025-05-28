/*
 * Module to simplify drawing an interactive menu a la curses and 
 * ncurses. The module detects the amount of space available in the menu,
 * creates a grid of characters, and supports writing to this grid.
 *
 * Functions for auto-wrapping text are included.
 *
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

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <string.h>
#include "../quakedef.h"
#include "qcurses.h"
#include "browser.h"
#include "browser_local.h"
#include "browser_news.h"
#include "cJSON.h"
#include "set.h"
#include <curl/curl.h>
#include "browser_curl.h"
#include <ctype.h>

#define curmap() columns[COL_MAP]->array[columns[COL_MAP]->list.cursor]
#define curtype() columns[COL_TYPE]->array[columns[COL_TYPE]->list.cursor]
#define currec() columns[COL_RECORD]->sda_name[columns[COL_RECORD]->list.cursor]
#define currecitem() columns[COL_RECORD]

qcurses_box_t * main_box = NULL;
qcurses_box_t * local_box = NULL;

int browserscale;
qboolean refresh_demlist;
int oldwidth;
int oldheight;

static enum demos_tabs demos_tab = TAB_LOCAL_DEMOS;
static enum browser_columns browser_col = COL_MAP;
static enum map_filters map_filter = FILTER_DOWNLOADED, prev_map_filter = FILTER_ALL;

static char search_term[41] = "\0";
static qboolean search_input = false;
qboolean mod_changed = false;

qcurses_recordlist_t * columns[COL_RECORD + 1];
qcurses_char_t * comments;
cJSON * json = NULL;

static qboolean demos_update = true;
static simple_set * maps = NULL;
static simple_set * id_maps = NULL;

extern qcurses_demlist_t * demlist;

extern direntry_t * filelist;
extern int num_files;
extern char com_basedir[MAX_OSPATH];

extern char *GetPrintedTime(double time);
extern void SearchForMaps (void);
extern char *toYellow (char *s);

static browser_curl_t * curl = NULL;
static browser_curl_t * history_curl = NULL;

static int comment_page = 0;
extern int comment_rows;
extern char ghost_demo_path[MAX_OSPATH];

/*
 * get time with no decimals, important for filenames
 */
char *GetPrintedTimeNoDec(float time, qboolean strip) {
    int			mins;
    double		secs;
    static char timestring[16];

    mins = time / 60;
    secs = time - (mins * 60);
    Q_snprintfz(timestring, sizeof(timestring), strip ? "%i%02d" : "%2i:%02d", mins, (int)secs);

    return timestring;
}

/*
 * handle keyboard input for the search terms
 */
void M_Demos_KeyHandle_Browser_Search (int k) {

    if (!json)
        return;

    int len = strlen(search_term);

    switch (k) {
    case K_ESCAPE:
    case K_ENTER:
        search_input = false;
        break;
    case K_BACKSPACE:
        if (len)
            search_term[len - 1] = '\0';
        break;
    default:
        if (isalnum(k) || k == '_')
            if (len < 40)
                search_term[len] = k;
        break;
    }
}

/*
 * handle keyboard input for the browser and various columns
 */
void M_Demos_KeyHandle_Browser (int k) {
    if (curl && curl->running)
        return;

    if (search_input) {
        M_Demos_KeyHandle_Browser_Search(k);
        return;
    }

    if (!json)
        return;

    int distance = 1;
    switch (k) {
    case 'b':
        if (!keydown[K_CTRL])
            break;
    case K_PGUP:
    case K_HOME:
        distance = keydown[K_HOME] ? 10000 : 10;
    case K_UPARROW:
    case 'k':
        if (browser_col <= COL_RECORD){
            qcurses_list_move_cursor((qcurses_list_t*)columns[browser_col], -distance);
            Browser_UpdateFurtherColumns(browser_col);
            S_LocalSound("misc/menu1.wav");
        } else if (browser_col == COL_COMMENT_LOADED) {
            comment_page = max(0, comment_page - distance);
        }
        break;
    case 'd':
        if (!keydown[K_CTRL])
            break;
    case K_PGDN:
    case K_END:
        distance = keydown[K_END] ? 10000 : 10;
    case K_DOWNARROW:
    case 'j':
        if (browser_col <= COL_RECORD) {
            qcurses_list_move_cursor((qcurses_list_t*)columns[browser_col], distance);
            Browser_UpdateFurtherColumns(browser_col);
            S_LocalSound("misc/menu1.wav");
        } else if (browser_col == COL_COMMENT_LOADED) {
            comment_page += distance;
        }
        break;
    case K_ENTER:
    case K_RIGHTARROW:
    case 'l':
        if (browser_col == COL_COMMENT_LOADED){
            if (keydown[K_CTRL] && keydown[K_SHIFT]) {
                Cbuf_AddText ("ghost_remove\n");
            } else if (keydown[K_CTRL]) {
                Cbuf_AddText (va("ghost \"../.demo_cache/%s/%s.dz\"\n", curtype(), currec()));
            } else {
                if (!keydown[K_ALT])
                    key_dest = key_game;
                Cbuf_AddText (va("playdemo \"../.demo_cache/%s/%s.dz\"\n", curtype(), currec()));
            }
        }
        if (columns[browser_col]->list.len > 0) {
            browser_col = min(COL_COMMENT_LOADING, browser_col + 1);
            S_LocalSound("misc/menu2.wav");
        }
        break;
    case K_BACKSPACE:
    case K_LEFTARROW:
    case 'h':
        if (browser_col == COL_COMMENT_LOADED)
            browser_col--;
        browser_col = max(COL_MAP, browser_col - 1);
        S_LocalSound("misc/menu2.wav");
        break;
    case 'p':
        if (keydown[K_CTRL])
            map_filter = (map_filter + 1) % (FILTER_ID + 1);
        break;
    case 'f':
        if (keydown[K_CTRL])
    case '/':
            search_input = true;

    default:
        break;
    }

}

/*
 * handle keyboard input for each individual tab
 */
void M_Demos_KeyHandle (int k) {
    demos_update = true;

    switch (k) {
        case K_ESCAPE:
        case K_MOUSE2:
            M_Menu_Main_f ();
            break;
        case K_TAB:
            demos_tab = demos_tab % TAB_SDA_DATABASE + 1;
            S_LocalSound("misc/menu1.wav");
            if (demos_tab == TAB_LOCAL_DEMOS) 
                M_Demos_LocalRead(main_box->rows - 20, NULL);

            break;
    }

    switch (demos_tab) {
        case TAB_LOCAL_DEMOS:
            M_Demos_KeyHandle_Local(k, main_box->rows - 20);
            break;
        case TAB_SDA_NEWS:
            M_Demos_KeyHandle_News(k);
            break;
        case TAB_SDA_DATABASE:
            M_Demos_KeyHandle_Browser(k);
            break;
        default:
            break;
    }
}

/* 
 * create the type column for display in the SDA browser
 */
qcurses_recordlist_t * Browser_CreateTypeColumn(const cJSON * json, int rows) {
    const cJSON *type = NULL;

    qcurses_recordlist_t *column = calloc(1, sizeof(qcurses_recordlist_t));

    column->list.cursor = 0;
    column->list.len = cJSON_GetArraySize(json);
    column->list.places = rows;
    column->list.window_start = 0;
    column->array = calloc(column->list.len, sizeof(char[80]));

    int i = 0;
    cJSON_ArrayForEach(type, json) {
        Q_strncpy(column->array[i++], type->string, 80);
    }

    return column;
}

/* 
 * create the record column for display in the SDA browser
 */
qcurses_recordlist_t * Browser_CreateRecordColumn(const cJSON * json, int rows) {
    const cJSON *record = NULL;

    qcurses_recordlist_t *column = calloc(1, sizeof(qcurses_recordlist_t));

    column->list.cursor = 0;
    column->list.len = cJSON_GetArraySize(json);
    column->list.places = rows;
    column->list.window_start = 0;
    column->array = calloc(column->list.len, sizeof(char[80]));
    column->sda_name = calloc(column->list.len, sizeof(char[50]));

    int i = 0;
    cJSON_ArrayForEach(record, json) {
        Q_snprintfz(column->array[i], sizeof(column->array[i]), "\x10%s\x11 %5s %s",
            cJSON_GetObjectItemCaseSensitive(record, "date")->valuestring,
            toYellow(GetPrintedTimeNoDec(cJSON_GetObjectItemCaseSensitive(record, "time")->valueint, false)),
            cJSON_GetObjectItemCaseSensitive(record, "players")->valuestring
        );

        char * sda_name_buf = Q_strdup(cJSON_GetObjectItemCaseSensitive(record, "filename")->valuestring);
        sda_name_buf[Q_strcasestr(sda_name_buf, ".") - sda_name_buf] = '\0';
        Q_snprintfz(column->sda_name[i], sizeof(column->sda_name[i]), "%s", sda_name_buf, true);
        free(sda_name_buf);

        i++;
    }

    return column;
}

/*
 * fire off a cascading update, starting with the updated column and 
 * going down the tree
 */
void Browser_UpdateFurtherColumns (enum browser_columns start_column) {
    const cJSON *item = NULL;
    switch (start_column) {
    case COL_MAP:
        if (columns[COL_TYPE]) {
            if (columns[COL_TYPE]->array)
                free(columns[COL_TYPE]->array);
            free(columns[COL_TYPE]);
        }
        item = cJSON_GetObjectItemCaseSensitive(json, curmap());
        columns[COL_TYPE] = Browser_CreateTypeColumn(item, 15 - 2);
    case COL_TYPE:
        if (columns[COL_RECORD]) {
            if (columns[COL_RECORD]->array)
                free(columns[COL_RECORD]->array);
            if (columns[COL_RECORD]->sda_name)
                free(columns[COL_RECORD]->sda_name);
            free(columns[COL_RECORD]);
        }
        item = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json, curmap()), curtype());
        columns[COL_RECORD] = Browser_CreateRecordColumn(item, 15 - 2);
    case COL_RECORD:
    default:
        break;
    }
}

/*
 * portable function to check if the requested dzip is downloaded
 */
qboolean Browser_DzipDownloaded() {
    char path[50];

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/%s.dz", curtype(), currec());

#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

/*
 * read txt file. Required dzip or dzip-linux in the root of the quake dir.
 */
qcurses_char_t * Browser_TxtFile() {
    char path[50];

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/%s.dz", curtype(), currec());

#ifdef _WIN32
    char	cmdline[1024];
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE child_stdout_read = NULL;
    HANDLE child_stdout_write = NULL;
    SECURITY_ATTRIBUTES sa_attr;
    DWORD dwRead;
#else
    pid_t pid;
    int pipes[2];
    pipe(pipes);
#endif

#ifdef _WIN32
    sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa_attr.bInheritHandle = TRUE;
    sa_attr.lpSecurityDescriptor = NULL;

    CreatePipe(&child_stdout_read, &child_stdout_write, &sa_attr, 0);
    SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0);

    memset (&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = child_stdout_write;
    si.hStdOutput = child_stdout_write;
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

    Q_snprintfz(cmdline, sizeof(cmdline), "./dzip.exe -s \"%s\" \"%s.txt\"", path, currec());
    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, com_basedir, &si, &pi)) {
        return qcurses_parse_txt(va("Couldn't execute %s/dzip.exe\n", com_basedir));
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        CloseHandle(child_stdout_write);
    }

    char * txt = calloc(4096*8, sizeof(char));
    ReadFile(child_stdout_read, txt, 4096*8, &dwRead, NULL);
    CloseHandle(child_stdout_read);
    return qcurses_parse_txt(txt);
#else
    switch (pid = fork()) {
    case -1:
        return qcurses_parse_txt("ERROR: creating dzip process to read txt!\n");
    case 0:
        dup2 (pipes[1], STDERR_FILENO);
        dup2 (pipes[1], STDOUT_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        execlp("./dzip-linux", "dzip-linux", "-s", path, va("%s.txt", currec()), NULL);
        return qcurses_parse_txt("Not supposed to get here.\n");
    default:
        close(pipes[1]);
        char * txt = calloc(4096*8, sizeof(char));
        read(pipes[0], txt, 4096*8);
        waitpid(-1, NULL, 0);
        close(pipes[0]);
        return qcurses_parse_txt(txt);
    }
#endif
}

/*
 * start download of the requested dz file
 */
void Browser_DownloadDzip() {
    char path[50];
    char href[100];

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/", curtype());
    COM_CreatePath(path);

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/%s.dz", curtype(), currec());

    Q_snprintfz(href, sizeof(href), "https://speeddemosarchive.com/quake/demos/%s/%s.dz", curtype(), currec());

    if (curl && curl->running) {
        browser_curl_clean(curl);
        curl = NULL;
    }

    curl = browser_curl_start(path, href);
}

/*
 * create the map column for display, applying filtering
 */
qcurses_recordlist_t * Browser_CreateMapColumn(const cJSON * json, int rows, enum map_filters filter) {
    const cJSON *map = NULL;

    qcurses_recordlist_t *column = calloc(1, sizeof(qcurses_recordlist_t));

    column->list.cursor = 0;
    column->list.len = cJSON_GetArraySize(json);
    column->list.places = rows;
    column->list.window_start = 0;
    column->array = calloc(column->list.len, sizeof(char[80]));

    int i = 0;
    cJSON_ArrayForEach(map, json) {
        switch (filter) {
        case FILTER_ID:
            if (set_contains(id_maps, map->string) == SET_TRUE)
                goto valid;
            break;
        case FILTER_DOWNLOADED:
            if (set_contains(maps, map->string) == SET_TRUE)
                goto valid;
            break;
        case FILTER_ALL:
        valid:
            if (Q_strcasestr(map->string, search_term))
                Q_strncpy(column->array[i++], map->string, 80);
        }
    }

    column->list.len = i;
    return column;
}

/*
 * create box that displays help for each tab
 */
void M_Demos_HelpBox (qcurses_box_t *help_box, enum demos_tabs tab, char * search_str, qboolean search_input) {
    qcurses_make_bar(help_box, 0);
    qcurses_print_centered(help_box, 2, "Navigation: arrows keys OR hjkl OR enter/backspace", false);
    qcurses_print_centered(help_box, 3, "Paging: page keys OR Ctrl+b and Ctrl+d", false);
    qcurses_print_centered(help_box, 4, "TAB to switch tabs", false);
    if (tab == TAB_SDA_DATABASE) {
        qcurses_print_centered(
            help_box,
            5,
            va("Filter: \x10%c\x11 DOWNLOADED MAPS | \x10%c\x11 ALL MAPS | \x10%c\x11 ID MAPS | (Ctrl+p)", 
                map_filter == FILTER_DOWNLOADED ? 0x0b : ' ',
                map_filter == FILTER_ALL ? 0x0b : ' ',
                map_filter == FILTER_ID ? 0x0b : ' '
            ),
            true
        );
    }

    if (tab != TAB_SDA_NEWS) {
        qcurses_print_centered(
            help_box,
            6,
            va("Search: \x10%-40s\x11 (/ or Ctrl+f)", va("%s%c", search_str, search_input ? blink(0xb) : ' ')),
            search_input
        );
    } else {
        qcurses_print_centered(
            help_box,
            6, 
            "Refresh: F5 or Ctrl + r",
            false
        );
    }

    if ((tab == TAB_SDA_DATABASE && browser_col == COL_COMMENT_LOADED) || tab == TAB_LOCAL_DEMOS){
        qcurses_print_centered(help_box, 8, "Play demo: Enter | Quickplay: Alt+Enter | Set ghost: Ctrl + Enter", true);
        if (ghost_demo_path[0] != '\0')
            qcurses_print_centered(help_box, 9, "Unset ghost: Ctrl + Shift + Enter", true);
    }

}

/*
 * mouse callback that handles the map list
 */
void mouse_map_cursor(qcurses_char_t * self, const mouse_state_t *ms) {
    int row = self->callback_data.row - 2;

    if (!json)
        return;

    qcurses_recordlist_t * ls = columns[COL_MAP];

    if (ms->button_up == 1 || browser_col == COL_MAP) {
        if (row < 0 || row >= ls->list.places || row >= ls->list.len - ls->list.window_start)
            return;

        ls->list.cursor = ls->list.window_start + row;
    }

    if (ms->button_up == 1) {
        browser_col = COL_MAP;
        qcurses_list_move_cursor((qcurses_list_t*)columns[browser_col], 0);
        Browser_UpdateFurtherColumns(browser_col);
        M_Demos_KeyHandle_Browser(K_ENTER);
    }
}

/*
 * mouse callback that handles the type list
 */
void mouse_type_cursor(qcurses_char_t * self, const mouse_state_t *ms) {
    int row = self->callback_data.row - 2;

    if (!json || browser_col < COL_TYPE)
        return;

    qcurses_recordlist_t * ls = columns[COL_TYPE];

    if (ms->button_up == 1 || browser_col <= COL_TYPE) {
        if (row < 0 || row >= ls->list.places || row >= ls->list.len - ls->list.window_start)
            return;

        ls->list.cursor = ls->list.window_start + row;
    }

    if (ms->button_up == 1) {
        browser_col = COL_TYPE;
        qcurses_list_move_cursor((qcurses_list_t*)columns[browser_col], 0);
        Browser_UpdateFurtherColumns(browser_col);
        M_Demos_KeyHandle_Browser(K_ENTER);
    }
}

/*
 * mouse callback that handles the record list
 */
void mouse_record_cursor(qcurses_char_t * self, const mouse_state_t *ms) {
    int row = self->callback_data.row - 2;
    qboolean play = false;

    if (!json || browser_col < COL_RECORD)
        return;

    qcurses_recordlist_t * ls = columns[COL_RECORD];

    if (ms->button_up == 1 || browser_col <= COL_RECORD) {
        if (row < 0 || row >= ls->list.places || row >= ls->list.len - ls->list.window_start)
            return;

        if (ls->list.cursor == ls->list.window_start + row)
            play = true;
        ls->list.cursor = ls->list.window_start + row;
    }

    if (ms->button_up == 1) {
        if (browser_col == COL_COMMENT_LOADED && play) {
            M_Demos_KeyHandle_Browser(K_ENTER);
        } else {
            browser_col = COL_RECORD;
            qcurses_list_move_cursor((qcurses_list_t*)columns[browser_col], 0);
            Browser_UpdateFurtherColumns(browser_col);
            M_Demos_KeyHandle_Browser(K_ENTER);
        }
    }
}

/*
 * display the browser, by compositing together various boxes
 */
void M_Demos_DisplayBrowser (int cols, int rows, int start_col, int start_row) {
    qcurses_box_t * local_box = qcurses_init(cols, rows);
    qcurses_box_t * map_box   = qcurses_init_callback(25, rows - 10, mouse_map_cursor);
    qcurses_box_t * skill_box = qcurses_init_callback(6, 15, mouse_type_cursor);
    qcurses_box_t * time_box  = qcurses_init_callback(cols - map_box->cols - skill_box->cols, 15, mouse_record_cursor);
    qcurses_box_t * comment_box  = qcurses_init_paged(cols - map_box->cols, rows - skill_box->rows - 10);
    qcurses_box_t * help_box  = qcurses_init(cols, rows);

    qcurses_print(map_box, 0, 0, "MAP", true);
    qcurses_make_bar(map_box, 1);
    qcurses_print(skill_box, 0, 0, "SKILL", true);
    qcurses_make_bar(skill_box, 1);
    qcurses_print_centered(time_box, 0, "RECORD HISTORY", true);
    qcurses_make_bar(time_box, 1);
    qcurses_make_bar(comment_box, 0);

    if (!json) {
        if (!history_curl || !history_curl->running) { /* start curl */
            COM_CreatePath(Q_strdup(".demo_cache/"));
            qcurses_print_centered(local_box, local_box->rows / 2, "Downloading...", false);
            history_curl = browser_curl_start(NULL, "https://speeddemosarchive.com/quake/mkt.pl?dump");
        } else if (history_curl->running == CURL_DOWNLOADING) { /* progress curl download */
            float progress = browser_curl_step(history_curl);
            if (progress == -1.0) { /* error during step */
                qcurses_print_centered(local_box, local_box->rows / 2, "Error during download!", false);
                history_curl = NULL;
            } else {
                progress = progress < 0 ? 0.0 : progress;
                qcurses_print_centered(local_box, local_box->rows / 2, "Downloading...", false);
                qcurses_print_centered(local_box, local_box->rows / 2 + 1, "\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false);
                qcurses_print(local_box, local_box->cols / 2 - 5 + (int)(10.0 * progress), local_box->rows / 2 + 1, "\x83", false);
            }
        } else if (history_curl->running == CURL_FINISHED) { /* download finished, parse */
            json = cJSON_Parse(history_curl->mem.buf);
            browser_curl_clean(history_curl);
            history_curl = NULL;
        }

        goto display;
    }

    /* handle map column display */
    if (prev_map_filter != map_filter || search_input || mod_changed) { /* apply filtering */
        if (columns[COL_MAP]){
            free(columns[COL_MAP]->array);
            free(columns[COL_MAP]);
        }
        columns[COL_MAP] = Browser_CreateMapColumn(json, rows - 12, map_filter);
        Browser_UpdateFurtherColumns(COL_MAP);
        prev_map_filter = map_filter;
        mod_changed = false;
    }

    for (int i = 0; i < min(columns[COL_MAP]->list.len, columns[COL_MAP]->list.places); i++)
        qcurses_print(
            map_box, 
            1,
            2 + i,
            columns[COL_MAP]->array[columns[COL_MAP]->list.window_start + i], 
            columns[COL_MAP]->list.cursor == columns[COL_MAP]->list.window_start + i /* currently active */
        );

    if (browser_col == COL_MAP) /* cursor display for map */
        qcurses_print(
            map_box,
            0, 
            2 + columns[COL_MAP]->list.cursor - columns[COL_MAP]->list.window_start, 
            blinkstr(0x0d), 
            false
        );

    /* handle type column display */
    if (columns[COL_TYPE]){
        for (int i = 0; i < min(columns[COL_TYPE]->list.len, columns[COL_TYPE]->list.places); i++)
            qcurses_print(
                skill_box,
                1,
                2 + i,
                columns[COL_TYPE]->array[columns[COL_TYPE]->list.window_start + i],
                browser_col >= COL_TYPE && columns[COL_TYPE]->list.cursor == columns[COL_TYPE]->list.window_start + i
            );

        if (browser_col == COL_TYPE) /* cursor display */
            qcurses_print(
                skill_box,
                0,
                2 + columns[COL_TYPE]->list.cursor - columns[COL_TYPE]->list.window_start,
                blinkstr(0x0d),
                false
            );
    }

    /* handle record column display */
    if (columns[COL_RECORD]){
        for (int i = 0; i < min(columns[COL_RECORD]->list.len, columns[COL_RECORD]->list.places); i++) {
            qcurses_print(
                time_box,
                1,
                2 + i,
                columns[COL_RECORD]->array[columns[COL_RECORD]->list.window_start + i],
                browser_col >= COL_RECORD && columns[COL_RECORD]->list.cursor == columns[COL_RECORD]->list.window_start + i
            );

            /* display marker for ghost */
            if (ghost_demo_path[0] != '\0' && strcmp(ghost_demo_path, va("../.demo_cache/%s/%s.dz", curtype(), columns[COL_RECORD]->sda_name[columns[COL_RECORD]->list.window_start + i])) == 0)
                qcurses_print(time_box, 0, 2 + i, "\x0b", false);
        }

        if (browser_col == COL_RECORD)
            qcurses_print(
                time_box,
                0,
                2 + columns[COL_RECORD]->list.cursor - columns[COL_RECORD]->list.window_start,
                blinkstr(0x0d),
                false
            );
    }

    if (browser_col == COL_COMMENT_LOADING) {
        if (!Browser_DzipDownloaded()) { /* start download of demo */
            qcurses_print_centered(comment_box, comment_box->rows / 2, "Downloading...", false);
            if (!curl || !curl->running) {
                Browser_DownloadDzip();
            }
        } else if (curl && curl->running == CURL_DOWNLOADING) { /* progress demo download */
            qcurses_print_centered(comment_box, comment_box->rows / 2, "Downloading...", false);
            float progress = browser_curl_step(curl);

            if (progress == -1.0) { /* error during step */
                qcurses_print_centered(comment_box, comment_box->rows / 2, "Error during download!", false);
                curl = NULL;
            } else {
                qcurses_print_centered(comment_box, comment_box->rows / 2 + 1, "\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false);
                qcurses_print(comment_box, comment_box->cols / 2 - 5 + (int)(10.0 * progress), comment_box->rows / 2 + 1, "\x83", false);
            }
        } else { /* demo downloaded, parse txt */
            if (comments)
                free(comments);
            if (curl)
                browser_curl_clean(curl);
            curl = NULL;
            comments = Browser_TxtFile();
            browser_col = COL_COMMENT_LOADED;
            comment_page = 0;
        }
    }

    /* display comments */
    if (browser_col == COL_COMMENT_LOADED && comments) {
        comment_rows = qcurses_boxprint_wrapped(comment_box, comments, comment_box->cols * comment_box->rows, 1);
        comment_page = min(comment_page, comment_rows - comment_box->rows);
        comment_page = min(comment_page, comment_box->rows * 14); /* limit row display */
        comment_page = max(comment_page, 0);
        comment_box->paged = comment_page;
        if (comment_rows - comment_box->rows > 0) {
            for (int i = 0; i < comment_box->rows; i++)
                qcurses_print(comment_box, comment_box->cols - 1, i + comment_page, "\x06", false);

            qcurses_print(
                comment_box,
                comment_box->cols - 1,
                min(
                    comment_page + (comment_box->rows - 1) * comment_page / (comment_rows - comment_box->rows),
                    comment_box->page_rows - 1
                ), 
                "\x83", 
                false
            );
        }
    } else if (browser_col == COL_COMMENT_LOADED) {
        qcurses_print_centered(comment_box, comment_box->rows / 2, "Text file not provided.", false);
    }

    M_Demos_HelpBox (help_box, demos_tab, search_term, search_input);

    int filled_cols = 0;
    qcurses_insert(local_box, filled_cols, 0, map_box);
    qcurses_insert(local_box, filled_cols = filled_cols + map_box->cols, 0, skill_box);
    qcurses_insert(local_box, filled_cols, skill_box->rows, comment_box);
    qcurses_insert(local_box, filled_cols = filled_cols + skill_box->cols, 0, time_box);
    qcurses_insert(local_box, 0, map_box->rows, help_box);
display:
    qcurses_insert(main_box, start_col, start_row, local_box);

    qcurses_free(help_box);
    qcurses_free(map_box);
    qcurses_free(skill_box);
    qcurses_free(time_box);
    qcurses_free(comment_box);
    qcurses_free(local_box);
}

/*
 * create map set of id1 maps for filtering
 */
void Browser_CreateMapSet() {
    SearchForMaps();

    if (maps) {
        set_destroy(maps);
        free(maps);
    }
    maps = calloc(1, sizeof(simple_set));
    set_init(maps);

    if (id_maps) {
        set_destroy(id_maps);
        free(id_maps);
    }
    id_maps = calloc(1, sizeof(simple_set));
    set_init(id_maps);

    for (int i = 0; i < num_files; i++)
        set_add(maps, Q_strdup(filelist[i].name));

    for (int i = 1; i <= 4; i++){
        for (int j = 1; j <= 8; j++) 
            set_add(id_maps, va("e%dm%d", i, j));
        set_add(id_maps, va("ep%d", i));
        set_add(maps, va("ep%d", i));
    }
    set_add(maps, "id1");
    set_add(maps, "e1m6long");
    set_add(maps, "e1m8norm");
    set_add(maps, "e2m1long");
    set_add(maps, "e3m2long");
    set_add(maps, "e3m3zom");
    set_add(maps, "e4m5long");
    set_add(id_maps, "end");
    set_add(id_maps, "id1");
    set_add(id_maps, "e1m6long");
    set_add(id_maps, "e1m8norm");
    set_add(id_maps, "e2m1long");
    set_add(id_maps, "e3m2long");
    set_add(id_maps, "e3m3zom");
    set_add(id_maps, "e4m5long");
}

/*
 * Handle mouse events.
 *
 * Mouse events are handled by identifying the character from the display
 * qcurses grid, and then applying the callback found attached to this character.
 *
 * Since the boxes are composited by copying them wholesale, we can set behavior
 * locally and yet have it handled in the final composition.
 */
qboolean M_Demos_Mouse_Event(const mouse_state_t *ms) {
    int col = mouse_col(ms->x);
    int row = mouse_row(ms->y);

    if (curl && curl->running)
        return true;

    if (0 <= col && col < main_box->cols && 0 <= row && row < main_box->rows)
        if (main_box->grid[row][col].callback)
            main_box->grid[row][col].callback(main_box->grid[row] + col, ms);

    return true;
}

/* mouse tab names callbacks */
void mouse_tab_news  (qcurses_char_t * self, const mouse_state_t *ms) { if (ms->button_up == 1) demos_tab = TAB_SDA_NEWS; }
void mouse_tab_local (qcurses_char_t * self, const mouse_state_t *ms) { if (ms->button_up == 1) demos_tab = TAB_LOCAL_DEMOS; }
void mouse_tab_remote(qcurses_char_t * self, const mouse_state_t *ms) { if (ms->button_up == 1) demos_tab = TAB_SDA_DATABASE; }

/*
 * display the main menu, handing off to submenus depending on tab.
 */
void M_Demos_Display (int width, int height) {
    if (!main_box) {
        oldwidth = width;
        oldheight = height;
        main_box = qcurses_init(width / 8, height / 8);
        if (curl_global_init(CURL_GLOBAL_DEFAULT))
            Con_Printf("curl global init failure!\n");
    }

    if (oldwidth != width || oldheight != height) {
        qcurses_free(main_box);
        oldwidth = width;
        oldheight = height;
        main_box = qcurses_init(width / 8, height / 8);
    }

    if (!maps || mod_changed)
        Browser_CreateMapSet();

    if (!demlist || refresh_demlist) {
        refresh_demlist = false;
        M_Demos_LocalRead(main_box->rows - 20, NULL);
    }

    if (curl && curl->running)
        demos_update = true;

    demos_update = true;
    if (!demos_update)
        goto display;

    switch (demos_tab) {
        case TAB_LOCAL_DEMOS:
            M_Demos_DisplayLocal(main_box->cols - 6, main_box->rows - 8, 3, 5);
            break;
        case TAB_SDA_NEWS:
            M_Demos_DisplayNews(main_box->cols - 6, main_box->rows - 8, 3, 5);
            break;
        case TAB_SDA_DATABASE:
            M_Demos_DisplayBrowser(main_box->cols - 6, main_box->rows - 8, 3, 5);
            break;
    }

    qcurses_print_callback(main_box, main_box->cols / 4 - 5, 2, "LOCAL DEMOS", demos_tab == TAB_LOCAL_DEMOS, mouse_tab_local);
    qcurses_print_callback(main_box, 2 * main_box->cols / 4 - 4, 2, "SDA NEWS", demos_tab == TAB_SDA_NEWS, mouse_tab_news);
    qcurses_print_callback(main_box, 3 * main_box->cols / 4 - 4, 2, "SDA DEMOS", demos_tab == TAB_SDA_DATABASE, mouse_tab_remote);

    qcurses_print(main_box, 3, 4, "\x1d", false);
    for (int i = 4; i < main_box->cols - 4; i++)
        qcurses_print(main_box, i, 4, "\x1e", false);
    qcurses_print(main_box, main_box->cols - 4, 4, "\x1f", false);

display:
    qcurses_display(main_box);
    demos_update = false;
}
