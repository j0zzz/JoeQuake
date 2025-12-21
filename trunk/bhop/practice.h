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

enum bhop_kb_state {
    BHOP_KB_NONE = 0, 
    BHOP_KB_BOTH,
    BHOP_KB_LEFT,
    BHOP_KB_RIGHT
};

enum bhop_m_state {
    BHOP_M_NONE = 0,
    BHOP_M_LEFT,
    BHOP_M_RIGHT
};

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
    vec3_t angles;

    vec3_t position;

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
extern cvar_t show_bhop_prestrafe_diff;
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
#define BHOP_CROSSHAIR_TAP_PREC     1<<7 /* tap info above crosshair */
#define BHOP_CROSSHAIR_PRESTRAFE    1<<8 /* prestrafe info above crosshair */
#define BHOP_CROSSHAIR_SYNC         1<<9 /* strafes info near crosshair */

/* bhop circle chart */
#define BHOP_CIRCLE                 1<<10 /* draw bhop circle */

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
#define BHOP_LGREEN_RGB (96<<8)
#define BHOP_BLUE_RGB 255<<16
#define BHOP_LBLUE_RGB 96<<16

/* width of bars constants */
enum bhop_width {
    BHOP_GAP = 2,
    BHOP_SMALL = 4,
    BHOP_MEDIUM = 17,
    BHOP_BIG = 32,
};

int Bhop_InverseScale(int x);
float Bhop_Speed(bhop_data_t *data);
float Bhop_SpeedAvg(bhop_data_t *history, int num);
float Bhop_GroundDeltaV(float vel_len, float angle);
bhop_summary_t Bhop_GetSummary(bhop_data_t *history, int window, qboolean get_keys);
int Bhop_GetFwdColor(bhop_data_t * history);
int Bhop_GetLStrafeColor(bhop_data_t * history);
int Bhop_GetRStrafeColor(bhop_data_t * history);

void Bhop_GatherData(void);
void Bhop_CullHistory(bhop_data_t *history, int num);
int Bhop_Len(bhop_data_t *history);

float * Bhop_SpeedArray(bhop_data_t *history, int window);
int ** Bhop_KeyArrays(bhop_data_t *history, int window);
bhop_mark_t * Bhop_AngleArray(bhop_data_t *history, int window);
float * Bhop_AirGainRoots(vec3_t velocity);

bhop_mark_t Bhop_CalcGroundBar(vec3_t velocity, vec3_t angles, float focal_len);
bhop_mark_t Bhop_CalcAirBar(vec3_t velocity, vec3_t angles, float focal_len);
int Bhop_BoundOffset(int x, int bound);

void Bhop_DrawCurrentMark(bhop_data_t *history, int scale);
void Bhop_DrawOldMarks(bhop_data_t *history, int window, int scale);
void Bhop_DrawCrosshairSquares(bhop_data_t *history, int x, int y);
void Bhop_DrawCrosshairGain(bhop_data_t *history, int x, int y, int scale, int charsize);
void Bhop_DrawCrosshairPrestrafeVis(bhop_data_t *history, int x, int y, int scale, int charsize);
void Bhop_DrawKeyContinuous(int *frames, int window, int x, int y, int height);
void Bhop_DrawKeyGraph(int **frames, int window, int x, int y, int offset);
void Bhop_DrawSpeedGraph(float *frames, int window, int x, int y);
void Bhop_DrawAccelGraph(float *frames, int window, int x, int y);
void Bhop_DrawAngleGraph(bhop_mark_t *frames, int window, int x, int y);
void Bhop_PrintSummary(void);

void BHOP_Init (void);
void BHOP_Shutdown (void);
void BHOP_Start (void);
void BHOP_Stop (void);

void SCR_DrawBHOP(void);
void SCR_DrawBHOPIntermission(void);

#endif /* _PRACTICE_H_ */
