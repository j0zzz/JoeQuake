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
    float friction_loss;
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
} bhop_summary_t;

extern cvar_t show_bhop_stats;
extern cvar_t show_bhop_stats_x;
extern cvar_t show_bhop_stats_y;
extern cvar_t show_bhop_histlen;
extern cvar_t show_bhop_window;
extern cvar_t show_bhop_frames;
extern bhop_data_t *bhop_history;

/* general history graphs */
#define BHOP_SPEED_HISTORY_GRAPH    1<<0 /* candlestick chart of history DONE */
#define BHOP_DSPEED_HISTORY_GRAPH    1<<1 /* history of best angle graph on side */
#define BHOP_ANGLE_HISTORY_GRAPH    1<<2 /* history of best angle graph on side */
#define BHOP_FORWARD_HISTORY_GRAPH  1<<3 /* history of how well forward was pressed (unnecessary on ground) DONE */

/* best speed angle display */
#define BHOP_ANGLE_ERROR            1<<4 /* angle showing current deviation from optimal angle, last tick */
#define BHOP_ANGLE_MARK             1<<5  /* mark in viewport showing where the optimal direction is, last tick */
#define BHOP_ANGLE_GRAPH            1<<6 /* plot of speed per angle display */

/* best angle last time land touched display */
#define BHOP_ANGLE_JUMP_ERROR       1<<8 /* angle showing current deviation from optimal, last jump */
#define BHOP_ANGLE_JUMP_MARK        1<<9 /* mark in viewport showing where the optimal direction is, last on land */
#define BHOP_CROSSHAIR_INFO         1<<10 /* info above crosshair */

/* text data */
#define BHOP_AVG_SPEED              1<<12 /* avg speed DONE */
#define BHOP_KEYPRESSES             1<<13 /* keypress stats DONE */

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

int bhop_inverse_scale(int x);
float bhop_speed(bhop_data_t *data);
float bhop_speed_avg(bhop_data_t *history, int num);
float bhop_ground_delta_v(float vel_len, float angle, float friction_loss);
bhop_summary_t bhop_get_summary(bhop_data_t *history, int window);

void bhop_gather_data(void);
void bhop_cull_history(bhop_data_t *history, int num);
int bhop_histlen(bhop_data_t *history);

int bhop_get_fwd_color(bhop_data_t *history);
int bhop_get_lstrafe_color(bhop_data_t *history);
int bhop_get_rstrafe_color(bhop_data_t *history);
float * bhop_speed_array(bhop_data_t *history, int window);
int ** bhop_key_arrays(bhop_data_t *history, int window);
float * bhop_air_gain_roots(vec3_t velocity);

bhop_mark_t bhop_calculate_ground_bar(vec3_t velocity, vec3_t angles, float focal_len);
bhop_mark_t bhop_calculate_air_bar(vec3_t velocity, vec3_t angles, float focal_len);

void bhop_draw_angle_mark(bhop_data_t *history, int scale);
void bhop_draw_crosshair_squares(bhop_data_t *history, int x, int y);
void bhop_draw_crosshair_gain(bhop_data_t *history, int x, int y, int scale, int charsize);
void bhop_print_summary(void);

void BHOP_Init (void);
void BHOP_Shutdown (void);
void BHOP_Start (void);
void BHOP_Stop (void);

void SCR_DrawBHOP (void);

#endif /* _PRACTICE_H_ */
