/*
 * Module to simplify drawing an interactive menu a la curses and 
 * ncurses. The module detects the amount of space available in the menu,
 * creates a grid of characters, and supports writing to this grid.
 *
 * Functions for auto-wrapping text are included.
 *
 * Also supported are background boxes, to mark certain selections.
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

#include <string.h>
#include <unistd.h>
#include "../quakedef.h"
#include "qcurses.h"
#include "browser.h"
#include "cJSON.h"
#include <curl/curl.h>
#include <sys/wait.h>

#define curmap() columns[COL_MAP]->array[columns[COL_MAP]->cursor]
#define curtype() columns[COL_TYPE]->array[columns[COL_TYPE]->cursor]
#define currec() columns[COL_RECORD]->sda_name[columns[COL_RECORD]->cursor]

qcurses_box_t * main_box = NULL;
qcurses_box_t * local_box = NULL;
static enum demos_tabs demos_tab = TAB_LOCAL_DEMOS;
static enum browser_columns browser_col = COL_MAP;
qcurses_list_t * columns[COL_RECORD + 1];
qcurses_char_t * comments;
static qboolean demos_update = true;
static cJSON * json = NULL;

extern direntry_t * filelist;
extern int num_files;
extern int list_cursor;
extern int list_base;
extern char prevdir[MAX_OSPATH];
extern char *GetPrintedTime(double time);

extern void M_List_Key (int k, int num_elements, int num_lines);
extern void SearchForDemos (void);

static char * qcurses_skills[4] = { "Easy", "Normal", "Hard", "Nightmare" };

qcurses_char_t * news = NULL;

static CURL *curl_http_handle;
static CURLM *curl_multi_handle;
static int curl_running = 0;
static FILE *curl_fp;

static int comment_page = 0;
extern int comment_rows;

char *GetPrintedTimeNoDec(float time, qboolean strip){
    int			mins;
    double		secs;
    static char timestring[16];

    mins = time / 60;
    secs = time - (mins * 60);
    Q_snprintfz(timestring, sizeof(timestring), strip ? "%i%02d" : "%2i:%02d", mins, (int)secs);

    return timestring;
}

static char *toYellow (char *s)
{
    static	char	buf[20];

    Q_strncpyz (buf, s, sizeof(buf));
    for (s = buf ; *s ; s++)
        if (*s >= '0' && *s <= '9')
            *s = *s - '0' + 18;

    return buf;
}

char * browser_read_file(const char * filename){
    FILE *f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    return string;
}

void M_Demos_KeyHandle_Local (int k, int max_lines) {
    M_List_Key (k, num_files, max_lines);

    switch (k) {
        case K_ENTER:
        case K_MOUSE1:
            if (!num_files || filelist[list_base+list_cursor].type == 3)
                break;

            if (keydown[K_CTRL] && keydown[K_SHIFT]) {
                Cbuf_AddText ("ghost_remove\n");
            }
            else if (filelist[list_cursor+list_base].type) {
                if (filelist[list_base+list_cursor].type == 2) {
                    char	*p;

                    if ((p = strrchr(demodir, '/')))
                    {
                        Q_strncpyz (prevdir, p + 1, sizeof(prevdir));
                        *p = 0;
                    }
                } else {
                    strncat (demodir, va("/%s", filelist[list_base+list_cursor].name), sizeof(demodir) - 1);
                }
                SearchForDemos ();
            } else {
                if (keydown[K_CTRL] && !keydown[K_SHIFT]) {
                    Cbuf_AddText (va("ghost \"..%s/%s\"\n", demodir, filelist[list_base+list_cursor].name));
                } else {
                    key_dest = key_game;
                    Cbuf_AddText (va("playdemo \"..%s/%s\"\n", demodir, filelist[list_base+list_cursor].name));
                }
                Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));
            }
    }
}

void M_Demos_KeyHandle_Browser (int k) {
    int distance = 1;
    switch (k) {
    case K_PGUP:
        distance = 10;
    case K_UPARROW:
    case 'k':
        if (browser_col <= COL_RECORD){
            qcurses_list_move_cursor(columns[browser_col], -distance);
            Browser_UpdateFurtherColumns(browser_col);
            S_LocalSound("misc/menu1.wav");
        } else if (browser_col == COL_COMMENT_LOADED) {
            comment_page = max(0, comment_page - distance);
        }
        break;

    case K_PGDN:
        distance = 10;
    case K_DOWNARROW:
    case 'j':
        if (browser_col <= COL_RECORD){
            qcurses_list_move_cursor(columns[browser_col], distance);
            Browser_UpdateFurtherColumns(browser_col);
            S_LocalSound("misc/menu1.wav");
        } else if (browser_col == COL_COMMENT_LOADED){
            comment_page += distance;
        }
        break;
    case K_ENTER:
    case K_RIGHTARROW:
    case 'l':
        if (browser_col == COL_COMMENT_LOADED){
            if (keydown[K_CTRL] && !keydown[K_SHIFT]) {
                Cbuf_AddText (va("ghost \"../.demo_cache/%s/%s.dz\"\n", curtype(), currec()));
            } else {
                key_dest = key_game;
                Cbuf_AddText (va("playdemo \"../.demo_cache/%s/%s.dz\"\n", curtype(), currec()));
            }
        }
        browser_col = min(COL_COMMENT_LOADING, browser_col + 1);
        S_LocalSound("misc/menu1.wav");
        break;
    case K_BACKSPACE:
    case K_LEFTARROW:
    case 'h':
        if (browser_col == COL_COMMENT_LOADED)
            browser_col--;
        browser_col = max(COL_MAP, browser_col - 1);
        S_LocalSound("misc/menu1.wav");
        break;
    default:
        break;
    }
}

void M_Demos_KeyHandle (int k)
{
    demos_update = true;

    switch (k) {
        case K_ESCAPE:
        case K_MOUSE2:
            M_Menu_Main_f ();
            break;
        case K_TAB:
            demos_tab = demos_tab % TAB_SDA_DATABASE + 1;
            S_LocalSound("misc/menu1.wav");
            break;
    }

    switch (demos_tab) {
        case TAB_LOCAL_DEMOS:
            M_Demos_KeyHandle_Local(k, main_box->rows - 8);
            break;
        case TAB_SDA_DATABASE:
            M_Demos_KeyHandle_Browser(k);
            break;
        default:
            break;
    }

    //switch (k)
    //{
    //case K_ESCAPE:
    //case K_MOUSE2:
    //	if (searchbox)
    //	{
    //		KillSearchBox ();
    //	}
    //	else
    //	{
    //		Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));
    //		M_Menu_Main_f ();
    //	}
    //	break;

    //case K_ENTER:
    //case K_MOUSE1:
    //	if (!num_files || filelist[list_base+list_cursor].type == 3)
    //		break;

    //	if (keydown[K_CTRL] && keydown[K_SHIFT])
    //	{
    //		Cbuf_AddText ("ghost_remove\n");
    //	}
    //	else if (filelist[list_cursor+list_base].type)
    //	{
    //		if (filelist[list_base+list_cursor].type == 2)
    //		{
    //			char	*p;

    //			if ((p = strrchr(demodir, '/')))
    //			{
    //				Q_strncpyz (prevdir, p + 1, sizeof(prevdir));
    //				*p = 0;
    //			}
    //		}
    //		else
    //		{
    //			strncat (demodir, va("/%s", filelist[list_base+list_cursor].name), sizeof(demodir) - 1);
    //		}
    //		SearchForDemos ();
    //	}
    //	else
    //	{
    //		if (keydown[K_CTRL] && !keydown[K_SHIFT])
    //		{
    //			Cbuf_AddText (va("ghost \"..%s/%s\"\n", demodir, filelist[list_base+list_cursor].name));
    //		}
    //		else
    //		{
    //			key_dest = key_game;
    //			m_state = m_none;
    //			Cbuf_AddText (va("playdemo \"..%s/%s\"\n", demodir, filelist[list_base+list_cursor].name));
    //		}
    //		Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));
    //	}

    //	if (searchbox)
    //		KillSearchBox ();
    //	break;

    //case K_BACKSPACE:
    //	if (strcmp(searchfile, ""))
    //		searchfile[--num_searchs] = 0;
    //	break;

    //default:
    //	if (k < 32 || k > 127)
    //		break;

    //	searchbox = true;
    //	searchfile[num_searchs++] = k;
    //	worx = false;
    //	for (i=0 ; i<num_files ; i++)
    //	{
    //		if (strstr(filelist[i].name, searchfile) == filelist[i].name)
    //		{
    //			worx = true;
    //			S_LocalSound ("misc/menu1.wav");
    //			list_base = i - 10;
    //			if (list_base < 0 || num_files < MAXLINES)
    //			{
    //				list_base = 0;
    //				list_cursor = i;
    //			}
    //			else if (list_base > (num_files - MAXLINES))
    //			{
    //				list_base = num_files - MAXLINES;
    //				list_cursor = MAXLINES - (num_files - i);
    //			}
    //			else
    //				list_cursor = 10;
    //			break;
    //		}
    //	}
    //	if (!worx)
    //		searchfile[--num_searchs] = 0;
    //	break;
    //}
}

qcurses_list_t * Browser_CreateTypeColumn(const cJSON * json, int rows) {
    const cJSON *type = NULL;

    qcurses_list_t *column = calloc(1, sizeof(qcurses_list_t));

    column->cursor = 0;
    column->len = cJSON_GetArraySize(json);
    column->places = rows;
    column->window_start = 0;
    column->array = calloc(column->len, sizeof(char[80]));

    int i = 0;
    cJSON_ArrayForEach(type, json) {
        Q_strncpy(column->array[i++], type->string, 80);
    }

    return column;
}

qcurses_list_t * Browser_CreateRecordColumn(const cJSON * json, int rows) {
    const cJSON *record = NULL;

    qcurses_list_t *column = calloc(1, sizeof(qcurses_list_t));

    column->cursor = 0;
    column->len = cJSON_GetArraySize(json);
    column->places = rows;
    column->window_start = 0;
    column->array = calloc(column->len, sizeof(char[80]));
    column->sda_name = calloc(column->len, sizeof(char[50]));

    int i = 0;
    cJSON_ArrayForEach(record, json) {
        Q_snprintfz(column->array[i], sizeof(column->array[i]), "\x10%10s\x11 %5s %s",
            cJSON_GetObjectItemCaseSensitive(record, "date")->valuestring,
            toYellow(GetPrintedTimeNoDec(cJSON_GetObjectItemCaseSensitive(record, "time")->valueint, false)),
            cJSON_GetObjectItemCaseSensitive(record, "players")->valuestring
        );
        Q_snprintfz(column->sda_name[i], sizeof(column->sda_name[i]), "%s_%s",
            columns[COL_MAP]->array[columns[COL_MAP]->cursor],
            GetPrintedTimeNoDec(cJSON_GetObjectItemCaseSensitive(record, "time")->valueint, true)
        );

        i++;
    }

    return column;
}

void Browser_UpdateFurtherColumns (enum browser_columns start_column) {
    const cJSON *item = NULL;
    switch (start_column) {
    case COL_MAP:
        if (columns[COL_TYPE])
            free(columns[COL_TYPE]);
        item = cJSON_GetObjectItemCaseSensitive(json, curmap());
        columns[COL_TYPE] = Browser_CreateTypeColumn(item, 20 - 1);
    case COL_TYPE:
        if (columns[COL_RECORD])
            free(columns[COL_RECORD]);
        item = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json, curmap()), curtype());
        columns[COL_RECORD] = Browser_CreateRecordColumn(item, 20 - 1);
    case COL_RECORD:
    default:
        break;
    }
}

qboolean Browser_DzipDownloaded() {
    char path[50];

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/%s.dz", curtype(), currec());

    return access(path, F_OK) == 0;
}

qcurses_char_t * Browser_TxtFile() {
    char path[50];

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/%s.dz", curtype(), currec());

    pid_t pid;
    int pipes[2];
    pipe(pipes);

    switch (pid = fork()) {
    case -1:
        return qcurses_parse_txt("ERROR: creating dzip process to read txt!\n");
    case 0:
        dup2 (pipes[1], STDERR_FILENO);
        dup2 (pipes[1], STDOUT_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        execlp("./dzip-linux", "dzip-linux", "-s", path, va("%s.txt", currec()));
        return qcurses_parse_txt("Not supposed to get here.\n");
    default:
        close(pipes[1]);
        char * txt = calloc(4096*8, sizeof(char));
        int nbytes = read(pipes[0], txt, 4096*8);
        wait(NULL);
        close(pipes[0]);
        return qcurses_parse_txt(txt);
    }
}

void Browser_CurlStart(char *path, char *href) {
    curl_fp = fopen(path, "wb");
    curl_http_handle = curl_easy_init();
    curl_easy_setopt(curl_http_handle, CURLOPT_URL, href);
    curl_easy_setopt(curl_http_handle, CURLOPT_WRITEDATA, curl_fp);
    curl_multi_handle = curl_multi_init();
    curl_multi_add_handle(curl_multi_handle, curl_http_handle);
}

void Browser_CurlClean() {
    curl_multi_remove_handle(curl_multi_handle, curl_http_handle);
    curl_multi_cleanup(curl_multi_handle);
    curl_easy_cleanup(curl_http_handle);
    fclose(curl_fp);
    curl_fp = NULL;
    demos_update = true;
}

void Browser_DownloadDzip() {
    char path[50];
    char href[100];

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/", curtype());
    COM_CreatePath(path);

    Q_snprintfz(path, sizeof(path), ".demo_cache/%s/%s.dz", curtype(), currec());

    Q_snprintfz(href, sizeof(href), "https://speeddemosarchive.com/quake/demos/%s/%s.dz", curtype(), currec());

    if (curl_running) 
        Browser_CurlClean();

    curl_running = 1;
    Browser_CurlStart(path, href);
}

void M_Demos_DisplayLocal (int cols, int rows, int start_col, int start_row) {
    qcurses_box_t * local_box  = qcurses_init(cols, rows);
    qcurses_box_t * map_box    = qcurses_init(15, rows);
    qcurses_box_t * skill_box  = qcurses_init(10, rows);
    qcurses_box_t * kill_box   = qcurses_init(12, rows);
    qcurses_box_t * secret_box = qcurses_init(9, rows);
    qcurses_box_t * time_box   = qcurses_init(15, rows);
    qcurses_box_t * player_box = qcurses_init(15, rows);
    qcurses_box_t * size_box   = qcurses_init(8, rows);
    qcurses_box_t * name_box   = qcurses_init(cols - map_box->cols - skill_box->cols - kill_box->cols - secret_box->cols - time_box->cols - size_box->cols - player_box->cols, rows);

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

    for (int i = 0; i < num_files && i < local_box->rows - 2; i++){
        direntry_t * d = filelist + i;
        char str[100];
        Q_strncpyz (str, d->name, sizeof(str));
        qcurses_print(name_box, 1, i + 2, str, !d->type);

        switch (d->type) {
            case 0:
                qcurses_print(size_box, 0, i + 2, toYellow(va("%7ik", (d->size) >> 10)), false); 
                break;
            case 1:
                qcurses_print(size_box, 0, i + 2, "  folder", false); 
                break;
            case 2: 
                qcurses_print(size_box, 0, i + 2, "      up", false); 
                break;
        }

        if (d->type == 0) {
            if (d->skill >= 0 && d->skill <= 3)
                qcurses_print(skill_box, 0, i + 2, qcurses_skills[d->skill], false); 

            qcurses_print(map_box, 0, i + 2, d->mapname, false); 

            qcurses_print(player_box, 0, i + 2, d->playername, true); 

            Q_snprintfz(str, sizeof(str), "%4d/", d->kills);
            qcurses_print(kill_box, 0, i + 2, str, true); 

            Q_snprintfz(str, sizeof(str), "%4d", d->total_kills);
            qcurses_print(kill_box, 5, i + 2, toYellow(str), true); 

            Q_snprintfz(str, sizeof(str), "%3d/", d->secrets);
            qcurses_print(secret_box, 0, i + 2, str, false); 

            Q_snprintfz(str, sizeof(str), "%3d", d->total_secrets);
            qcurses_print(secret_box, 4, i + 2, toYellow(str), false); 

            if (d->total_time)
                Q_snprintfz(str, sizeof(str), "%15s", GetPrintedTime(d->total_time));
            else
                Q_snprintfz(str, sizeof(str), "%15s", "N/A");
            qcurses_print(time_box, 0, i + 2, str, true); 
        }
    }

    qcurses_print(name_box, 0, 2 + list_cursor, "\x0d", false);

    int filled_cols = 0;
    qcurses_insert(local_box, filled_cols, 0, name_box);
    qcurses_insert(local_box, filled_cols = filled_cols + name_box->cols, 0, map_box);
    qcurses_insert(local_box, filled_cols = filled_cols + map_box->cols, 0, player_box);
    qcurses_insert(local_box, filled_cols = filled_cols + player_box->cols, 0, skill_box);
    qcurses_insert(local_box, filled_cols = filled_cols + skill_box->cols, 0, kill_box);
    qcurses_insert(local_box, filled_cols = filled_cols + kill_box->cols, 0, secret_box);
    qcurses_insert(local_box, filled_cols = filled_cols + secret_box->cols, 0, time_box);
    qcurses_insert(local_box, filled_cols = filled_cols + time_box->cols, 0, size_box);
    qcurses_insert(main_box, start_col, start_row, local_box);

    qcurses_free(name_box);
    qcurses_free(map_box);
    qcurses_free(skill_box);
    qcurses_free(player_box);
    qcurses_free(kill_box);
    qcurses_free(secret_box);
    qcurses_free(time_box);
    qcurses_free(size_box);
    qcurses_free(local_box);
}

void M_Demos_DisplayNews (int cols, int rows, int start_col, int start_row) {
    qcurses_box_t * local_box = qcurses_init(cols, rows);

    if (!news)
        news = qcurses_parse_news(browser_read_file("news.html"));

    qcurses_boxprint_wrapped(local_box, news, local_box->cols * local_box->rows, 0);
    qcurses_insert(main_box, start_col, start_row, local_box);

    qcurses_free(local_box);
}

qcurses_list_t * Browser_CreateMapColumn(const cJSON * json, int rows) {
    const cJSON *map = NULL;

    qcurses_list_t *column = calloc(1, sizeof(qcurses_list_t));

    column->cursor = 0;
    column->len = cJSON_GetArraySize(json);
    column->places = rows;
    column->window_start = 0;
    column->array = calloc(column->len, sizeof(char[80]));

    int i = 0;
    cJSON_ArrayForEach(map, json) {
        Q_strncpy(column->array[i++], map->string, 80);
    }

    return column;
}


void M_Demos_DisplayBrowser (int cols, int rows, int start_col, int start_row) {
    qcurses_box_t * local_box = qcurses_init(cols, rows);
    qcurses_box_t * map_box   = qcurses_init(25, rows);
    qcurses_box_t * skill_box = qcurses_init(15, 20);
    qcurses_box_t * time_box  = qcurses_init(cols - map_box->cols - skill_box->cols, 20);
    qcurses_box_t * comment_box  = qcurses_init_paged(cols - map_box->cols, rows - skill_box->rows);

    qcurses_make_bar(map_box, 0);
    qcurses_make_bar(skill_box, 0);
    qcurses_make_bar(time_box, 0);
    qcurses_make_bar(comment_box, 0);

    if (!json) {
        COM_CreatePath(".demo_cache/");
        char * json_string = browser_read_file("sda_mock.json");
        json = cJSON_Parse(json_string);
        free(json_string);
        columns[COL_MAP] = Browser_CreateMapColumn(json, rows - 1);
        Browser_UpdateFurtherColumns(browser_col);
    }

    for (int i = 0; i < min(columns[COL_MAP]->len, columns[COL_MAP]->places); i++)
        qcurses_print(map_box, 1, 1 + i, columns[COL_MAP]->array[columns[COL_MAP]->window_start + i], columns[COL_MAP]->cursor == columns[COL_MAP]->window_start + i);

    if (browser_col == COL_MAP)
        qcurses_print(map_box, 0, 1 + columns[COL_MAP]->cursor - columns[COL_MAP]->window_start, "\x0d", false);

    if (columns[COL_TYPE]){
        for (int i = 0; i < min(columns[COL_TYPE]->len, columns[COL_TYPE]->places); i++)
            qcurses_print(skill_box, 1, 1 + i, columns[COL_TYPE]->array[columns[COL_TYPE]->window_start + i], browser_col >= COL_TYPE && columns[COL_TYPE]->cursor == columns[COL_TYPE]->window_start + i);

        if (browser_col == COL_TYPE)
            qcurses_print(skill_box, 0, 1 + columns[COL_TYPE]->cursor - columns[COL_TYPE]->window_start, "\x0d", false);
    }

    if (columns[COL_RECORD]){
        for (int i = 0; i < min(columns[COL_RECORD]->len, columns[COL_RECORD]->places); i++)
            qcurses_print(time_box, 1, 1 + i, columns[COL_RECORD]->array[columns[COL_RECORD]->window_start + i], browser_col >= COL_RECORD && columns[COL_RECORD]->cursor == columns[COL_RECORD]->window_start + i);

        if (browser_col == COL_RECORD)
            qcurses_print(time_box, 0, 1 + columns[COL_RECORD]->cursor - columns[COL_RECORD]->window_start, "\x0d", false);
    }

    if (browser_col == COL_COMMENT_LOADING) {
        if (!Browser_DzipDownloaded()) {
            qcurses_print(comment_box, comment_box->cols / 2 - 10, comment_box->rows / 2, "Downloading...", false);
            if (!curl_running) {
                Browser_DownloadDzip();
            }
        } else if (curl_running) {
            qcurses_print(comment_box, comment_box->cols / 2 - 10, comment_box->rows / 2, "Downloading...", false);
            CURLMcode mc = curl_multi_perform(curl_multi_handle, &curl_running);

            if(mc || !curl_running)
                Browser_CurlClean();

            if (curl_running) {
                curl_off_t total, downloaded;
                curl_easy_getinfo(curl_http_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
                curl_easy_getinfo(curl_http_handle, CURLINFO_SIZE_DOWNLOAD_T, &downloaded);

                qcurses_print(comment_box, comment_box->cols / 2 - 9, comment_box->rows / 2 + 1, "\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false);
                qcurses_print(comment_box, comment_box->cols / 2 - 8 + (int)(10.0*downloaded/(float)total), comment_box->rows / 2 + 1, "\x83", false);
            }
        } else {
            if (comments)
                free(comments);
            comments = Browser_TxtFile();
            browser_col = COL_COMMENT_LOADED;
            comment_page = 0;
        }
    }

    if (browser_col == COL_COMMENT_LOADED && comments) {
        comment_page = min(comment_page, comment_rows - comment_box->rows);
        comment_page = max(comment_page, 0);
        comment_box->paged = comment_page;
        qcurses_boxprint_wrapped(comment_box, comments, comment_box->cols * comment_box->rows, 1);
        if (comment_rows - comment_box->rows > 0) {
            for (int i = 0; i < comment_box->rows; i++)
                qcurses_print(comment_box, comment_box->cols - 1, i + comment_page, "\x06", false);
            qcurses_print(comment_box, comment_box->cols - 1, min(comment_page + comment_box->rows * comment_page / (comment_rows - comment_box->rows), comment_box->page_rows - 1), "\x83", false);
        }
    } else if (browser_col == COL_COMMENT_LOADED) {
        qcurses_print(comment_box, comment_box->cols / 2 - 12, comment_box->rows / 2, "Text file not provided.", false);
    }

    int filled_cols = 0;
    qcurses_insert(local_box, filled_cols, 0, map_box);
    qcurses_insert(local_box, filled_cols = filled_cols + map_box->cols, 0, skill_box);
    qcurses_insert(local_box, filled_cols, skill_box->rows, comment_box);
    qcurses_insert(local_box, filled_cols = filled_cols + skill_box->cols, 0, time_box);
    qcurses_insert(main_box, start_col, start_row, local_box);

    qcurses_free(map_box);
    qcurses_free(skill_box);
    qcurses_free(time_box);
    qcurses_free(comment_box);
    qcurses_free(local_box);
}

void M_Demos_Display (int width, int height)
{
    if (!main_box)
        main_box = qcurses_init(width / 8, height / 8);

    if (curl_running)
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

    qcurses_print(main_box, main_box->cols / 4 - 5, 2, "LOCAL DEMOS", demos_tab == TAB_LOCAL_DEMOS);
    qcurses_print(main_box, 2 * main_box->cols / 4 - 4, 2, "SDA NEWS", demos_tab == TAB_SDA_NEWS);
    qcurses_print(main_box, 3 * main_box->cols / 4 - 4, 2, "SDA DEMOS", demos_tab == TAB_SDA_DATABASE);

    qcurses_print(main_box, 3, 4, "\x1d", false);
    for (int i = 4; i < main_box->cols - 4; i++)
        qcurses_print(main_box, i, 4, "\x1e", false);
    qcurses_print(main_box, main_box->cols - 4, 4, "\x1f", false);

display:
    qcurses_display(main_box);
    demos_update = false;
}
