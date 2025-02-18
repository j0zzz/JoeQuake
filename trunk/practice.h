/*
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

#ifndef _PRACTICE_H_
#define _PRACTICE_H_

/* stores what keys were pressed */
typedef struct bhop_keystate_s
{
    float fwd;
    float left;
    float right;
} bhop_keystate_t;

/* stores the display of the bar */
typedef struct bhop_mark_s
{
    int x_start;
    int x_mid;
    int x_end;
} bhop_mark_t;

typedef struct bhop_data_s
{
    bhop_keystate_t movement;
    bhop_mark_t bar;

	qboolean on_ground;	
    vec3_t velocity;

    float speed;
    float speed_gain;
    float friction_loss;
    float angle_change;
    float viewangle;

	struct bhop_data_s *next;
} bhop_data_t;

typedef struct bhop_summary_s
{
    float fwd_percent;
    float fwd_on_ground_percent;
    float strafe_percent;
    int **keys;
    float *speeds;
    bhop_mark_t *angles;
} bhop_summary_t;

extern cvar_t show_bhop_stats;
extern cvar_t show_bhop_stats_x;
extern cvar_t show_bhop_stats_y;
extern cvar_t show_bhop_histlen;
extern cvar_t show_bhop_window;
extern cvar_t show_bhop_frames;
extern bhop_data_t *bhop_history;

/* side displays */
#define BHOP_AVG_SPEED              1<<0 /* avg speed */
#define BHOP_KEYPRESSES             1<<1 /* keypress stats */
#define BHOP_SPEED_HISTORY_GRAPH    1<<2 /* candlestick chart of speeds in history */
#define BHOP_DSPEED_HISTORY_GRAPH   1<<3 /* history of accel graph */
#define BHOP_FORWARD_HISTORY_GRAPH  1<<4 /* history of how well +fwd and strafing*/

/* crosshair displays */
#define BHOP_ANGLE_MARK             1<<5  /* mark in viewport showing where the optimal direction is, last tick */
#define BHOP_CROSSHAIR_INFO         1<<6 /* info above crosshair */

/* color constants for convenience */
#define BHOP_GREEN 184 /* actually blue because the green sucks shit */ 
#define BHOP_LRED 224
#define BHOP_RED 79
#define BHOP_WHITE 13

#define BHOP_BLACK_RGB 0
#define BHOP_WHITE_RGB 255 + (255 << 8) + (255 << 16)
#define BHOP_RED_RGB 255
#define BHOP_LRED_RGB 96
#define BHOP_ORANGE_RGB 255 + (165<<8)
#define BHOP_GREEN_RGB 255<<8
#define BHOP_LGREEN_RGB 96<<8
#define BHOP_BLUE_RGB 255<<16
#define BHOP_LBLUE_RGB 96<<16

/* width of bars constants */
enum bhop_width {
    BHOP_GAP = 2,
    BHOP_SMALL = 4,
    BHOP_MEDIUM = 17,
    BHOP_BIG = 32,
};

int bhop_inverse_scale(int x);
float bhop_speed(bhop_data_t *data);
float bhop_speed_avg(bhop_data_t *history, int num);
float bhop_ground_delta_v(float vel_len, float angle);
bhop_summary_t bhop_get_summary(bhop_data_t *history, int window, qboolean get_keys);
int bhop_get_fwd_color(bhop_data_t * history);
int bhop_get_lstrafe_color(bhop_data_t * history);
int bhop_get_rstrafe_color(bhop_data_t * history);

void bhop_gather_data(void);
void bhop_cull_history(bhop_data_t *history, int num);
int bhop_histlen(bhop_data_t *history);

int bhop_get_fwd_color(bhop_data_t *history);
int bhop_get_lstrafe_color(bhop_data_t *history);
int bhop_get_rstrafe_color(bhop_data_t *history);
float * bhop_speed_array(bhop_data_t *history, int window);
int ** bhop_key_arrays(bhop_data_t *history, int window);
bhop_mark_t * bhop_angles_array(bhop_data_t *history, int window);
float * bhop_air_gain_roots(vec3_t velocity);

bhop_mark_t bhop_calculate_ground_bar(vec3_t velocity, vec3_t angles, float focal_len);
bhop_mark_t bhop_calculate_air_bar(vec3_t velocity, vec3_t angles, float focal_len);

void bhop_draw_current_mark(bhop_data_t *history, int scale);
void bhop_draw_old_marks(bhop_data_t *history, int window, int scale);
void bhop_draw_crosshair_squares(bhop_data_t *history, int x, int y);
void bhop_draw_crosshair_gain(bhop_data_t *history, int x, int y, int scale, int charsize);
void bhop_draw_crosshair_prestrafe(bhop_data_t *history, int x, int y, int scale, int charsize);
void bhop_draw_key_continuous(int *frames, int window, int x, int y, int height);
void bhop_draw_speed_graph(float *frames, int window, int x, int y);
void bhop_draw_accel_graph(float *frames, int window, int x, int y);
void bhop_print_summary(void);

void BHOP_Init (void);
void BHOP_Shutdown (void);
void BHOP_Start (void);
void BHOP_Stop (void);

void SCR_DrawBHOP(void);
void SCR_DrawBHOPIntermission(void);

#endif /* _PRACTICE_H_ */
