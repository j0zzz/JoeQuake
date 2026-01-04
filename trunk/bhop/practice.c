/*
 * p_su
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

#include <string.h>
#include "../quakedef.h"
#include "practice.h"

cvar_t show_bhop_stats = {"show_bhop_stats", "0", true};
cvar_t show_bhop_stats_x = {"show_bhop_stats_x", "1", true};
cvar_t show_bhop_stats_y = {"show_bhop_stats_y", "4", true};
cvar_t show_bhop_histlen = {"show_bhop_histlen", "0", true};
cvar_t show_bhop_window = {"show_bhop_window", "144", true};
cvar_t show_bhop_frames = {"show_bhop_frames", "7", true};
cvar_t show_bhop_prestrafe_diff = {"show_bhop_prestrafe_diff", "8", true};

/* new items will be added at the beginning; newest is close to the pointer */
bhop_data_t *bhop_history = NULL;
bhop_mark_t bhop_last_ground;
bhop_summary_t *bhop_summary = NULL;
static float lastangle;
static float last_ground_speed;

/* items required for calculation logic */
extern qboolean onground;
extern usercmd_t cmd;
extern kbutton_t in_forward, in_moveleft, in_moveright;
extern vec3_t pre_sv_velocity;
extern vec3_t post_friction_velocity;
extern double sv_frametime;
extern qboolean physframe;

/* screen size stuff */
extern vrect_t scr_vrect;
extern refdef_t r_refdef;
extern viddef_t vid;
extern int glx, glwidth;

int Bhop_InverseScale(int x)
{
    float scale = Sbar_GetScaleAmount();
    return x / scale;
}

/*
 * cull history after <num> newest datapoints 
 */
void Bhop_CullHistory(bhop_data_t * history, int num)
{
    bhop_data_t * prev = NULL;

    while (num-- && history) {
        prev = history;
        history = history->next;
    }

    if (prev)
        prev->next = NULL;

    while (history){
        bhop_data_t * temp = history;
        history = history->next;
        free(temp);
    }
}

/*
 * get entire history len
 */
int Bhop_Len(bhop_data_t * history)
{
    int len = 0;

    while(history){
        history = history->next;
        len++;
    }

    return len;
}

/*
 * speed vector modulus
 */
float Bhop_Speed(bhop_data_t * data)
{
    return sqrt(data->velocity[0]*data->velocity[0] + data->velocity[1]*data->velocity[1]);
}

/*
 * average bhop speed calculation
 */
float Bhop_SpeedAvg(bhop_data_t * history, int window)
{
    float avg = 0.0;
    int i = 0;

    while (i++ < window && history){
        avg += Bhop_Speed(history);
        history = history->next;
    }

    return avg / window;
}

/*
 * retrieve summary for entire window of history
 */
bhop_summary_t Bhop_GetSummary(bhop_data_t * history, int window, qboolean get_keys)
{
    int i = 0, i_ground = 0;
    float strafesum;
    bhop_summary_t summary = {
        .fwd_on_ground_percent = 0.0,
        .fwd_percent = 0.0,
        .strafe_percent = 0.0,
        .keys = NULL,
        .speeds = NULL,
        .angles = NULL
    };

    if (get_keys) {
        summary.keys = Bhop_KeyArrays(history, window);
        summary.speeds = Bhop_SpeedArray(history, window);
        summary.angles = Bhop_AngleArray(history, window);
    }

    while (i < window && history) {
        /* fwd */
        if (history->on_ground) {
            i_ground++;
            summary.fwd_on_ground_percent += history->movement.fwd;
            summary.fwd_percent += history->movement.fwd;
        } else {
            summary.fwd_percent += 1.0 - history->movement.fwd;
        }

        /* strafe sync */
        strafesum = history->movement.left - history->movement.right;
        /* this checks if same direction is used */
        if (strafesum * history->angle_change < 0)
            summary.strafe_percent += 1.0;
        history = history->next;
        i++;
    }

    if (i_ground)
        summary.fwd_on_ground_percent = 100.0 * summary.fwd_on_ground_percent / i_ground;
    else
        summary.fwd_on_ground_percent = 0.0;
    summary.fwd_percent = 100.0 * summary.fwd_percent / i;
    summary.strafe_percent = 100.0 * summary.strafe_percent / i;


    return summary;
}

/*
 * array of speeds in each frame in window
 */
float * Bhop_SpeedArray(bhop_data_t * history, int window)
{
    float * frames = calloc(window, sizeof(float));

    /* filled in backwards, so that it displays newest on the right */
    while (history && window--){
        frames[window] = Bhop_Speed(history);
        history = history->next;
    }

    return frames;
}

/*
 * array of angle histories
 */
bhop_mark_t * Bhop_AngleArray(bhop_data_t * history, int window)
{
    bhop_mark_t * frames = calloc(window, sizeof(bhop_mark_t));

    /* filled in backwards, so that it displays newest on the right */
    while (history && window--) {
        frames[window] = history->bar;
        history = history->next;
    }

    return frames;
}

/*
 * get color for fwd tap displays
 */
int Bhop_GetFwdColor(bhop_data_t * history)
{
    if (!history)
        return 0;

    if (history->on_ground)
        return history->movement.fwd ? BHOP_GREEN_RGB : BHOP_RED_RGB;
    else
        return history->movement.fwd ? BHOP_LRED_RGB : 0;
}

/*
 * get color for +left strafesync displays
 */
int Bhop_GetLStrafeColor(bhop_data_t * history)
{
    int color = 0;
    if (!history)
        return color;

    if (history->movement.left)
        color = BHOP_LRED_RGB;

    if (history->angle_change < 0.0)
        color |= BHOP_LGREEN_RGB;

    return color;
}

enum bhop_kb_state Bhop_GetStrafeState(bhop_data_t * history) {
    if (!history)
        return BHOP_KB_NONE;

    /* keyboard */
    if (history->movement.left && history->movement.right)
        return BHOP_KB_BOTH;
    else if (history->movement.left)
        return BHOP_KB_LEFT;
    else if (history->movement.right)
        return BHOP_KB_RIGHT;

    return BHOP_KB_NONE;
}

enum bhop_m_state Bhop_GetMouseState(bhop_data_t * history) {
    if (!history)
        return BHOP_M_NONE;

    /* keyboard */
    if (history->angle_change < 0.0)
        return BHOP_M_LEFT;
    else if (history->angle_change)
        return BHOP_M_RIGHT;

    return BHOP_M_NONE;
}

/*
 * get color for +right strafesync displays
 */
int Bhop_GetRStrafeColor(bhop_data_t * history)
{
    int color = 0;
    if (!history)
        return color;

    if (history->movement.right)
        color = BHOP_LRED_RGB;

    if (history->angle_change > 0.0)
        color |= BHOP_LGREEN_RGB;

    return color;
}

/*
 * get arrays of colors for fwd/strafesync display
 */
int ** Bhop_KeyArrays(bhop_data_t * history, int window)
{
    if (window <= 0)
        return NULL;

    int ** frames = malloc(3 * sizeof(int *)); /* fwd, left, right */
    for (int i = 0; i < 3; i++)
        frames[i] = calloc(window, sizeof(int));

    while (history && window--) {
        /* fwd structure */
        frames[0][window] = Bhop_GetFwdColor(history);

        /* strafing */
        frames[1][window] = Bhop_GetLStrafeColor(history);
        frames[2][window] = Bhop_GetRStrafeColor(history);

        history = history->next;
    }

    return frames;
}

/* 
 * gets the angles between which wishvel will retain speed relative
 * to velocity. hitting between these is a guaranteed speed increase 
 */
float * Bhop_AirGainRoots(vec3_t velocity)
{
    float * angles = calloc(2, sizeof(float));
    float speed = sqrt(velocity[0]*velocity[0] + velocity[1]*velocity[1]);
    float arccos;

    if (speed <= 30) /* the equation we'll use is not viable for speeds under 30 */
        return angles;

    arccos = acos(-30.0 / speed);
    angles[0] = arccos;
    angles[1] = M_PI - arccos;
    return angles;
}

/*
 * gets the delta-v dependent on angle
 */
float Bhop_GroundDeltaV(float vel_len, float angle)
{
    float w_len, delta_v;

    w_len = max(320 - vel_len * cos(angle), 0.0);
    w_len = min(sv_maxspeed.value * sv_accelerate.value * sv_frametime, w_len);

    delta_v = (vel_len + w_len * cos(angle));
    delta_v *= delta_v;

    delta_v += w_len * w_len * sin(angle) * sin(angle);

    delta_v = sqrt(delta_v) - vel_len;

    return delta_v;
}

/* 
 * the equation for this is not so simple because of multiple clippings. We will just find
 * the maximal values closest to the forward viewpoint and draw it.
 */
bhop_mark_t Bhop_CalcGroundBar(vec3_t velocity, vec3_t angles, float focal_len)
{
    vec3_t wishvel;
    vec3_t fwd_vec, right_vec, up_vec;
    bhop_mark_t bar = {.x_start = -10, .x_end = -10, .x_mid = -10};
    int x, best_x;
    float angle_pixel, wish_speed, vel_speed, dot_product;
    float max_delta = -99999.0, temp;

    AngleVectors(angles, fwd_vec, right_vec, up_vec);

    for (int i = 0; i < 3; i++)
        wishvel[i] = fwd_vec[i] * cmd.forwardmove + right_vec[i] * cmd.sidemove;
    wishvel[2] = 0;
    fwd_vec[2] = 0;

    wish_speed = VectorNormalize(wishvel);
    if (wish_speed == 0)
        return bar;

    VectorNormalize(fwd_vec);
    vel_speed = VectorNormalize(velocity);

    dot_product = DotProduct(wishvel, fwd_vec);
    if(dot_product > 1.0)
        dot_product = 1.0;
    if(dot_product < -1.0)
        dot_product = -1.0;

    for (x = 0; x < vid.width; x++) {
        angle_pixel = atan((x - vid.width/2.0) / focal_len);
        temp = Bhop_GroundDeltaV(vel_speed, angle_pixel - acos(dot_product));
        if (temp > max_delta){
            best_x = x;
            max_delta = temp;
        }
    }

    bar.x_start = bar.x_end = bar.x_mid = best_x;

    return bar;
}

/* 
 * create display bar in air
 */
bhop_mark_t Bhop_CalcAirBar(vec3_t velocity, vec3_t angles, float focal_len)
{
    float * bhop_roots;
    vec3_t wishvel, fwd_vec, right_vec, up_vec;
    float angle_diff, midpoint;
    bhop_mark_t bar;

    AngleVectors(angles, fwd_vec, right_vec, up_vec);

    for (int i = 0; i < 3; i++)
        wishvel[i] = fwd_vec[i] * cmd.forwardmove + right_vec[i] * cmd.sidemove;
    wishvel[2] = 0;
    velocity[2] = 0;

    bhop_roots = Bhop_AirGainRoots(velocity);

    VectorNormalize(wishvel);
    VectorNormalize(velocity);
    angle_diff = acos(DotProduct(wishvel, velocity));

    bhop_roots[0] -= angle_diff; 
    bhop_roots[1] -= angle_diff;

    midpoint = (bhop_roots[0] + bhop_roots[1]) / 2.0;
    bar.x_start = (int)(tan(bhop_roots[0]) * focal_len) + vid.width / 2;
    bar.x_mid = (int)(tan(midpoint) * focal_len) + vid.width / 2;
    bar.x_end = (int)(tan(bhop_roots[1]) * focal_len) + vid.width / 2;

    free(bhop_roots);

    return bar;
}

/*
 * draw the markings for optimal angles
 */
void Bhop_DrawCurrentMark(bhop_data_t *history, int scale)
{
    float y = scr_vrect.y + scr_vrect.height / 2.0 + 12 * scale;

    if(!history)
        return;

    /* display; the scale has to appear to provide scale independent granularity of display */

    Draw_AlphaFill(history->bar.x_start, y, (history->bar.x_end - history->bar.x_start) / scale, 1, BHOP_WHITE, 0.8);
    Draw_AlphaFillRGB(history->bar.x_start, y + 1 * scale, (history->bar.x_end - history->bar.x_start) / scale, BHOP_SMALL, BHOP_LGREEN_RGB, 0.5);
    int color = history->on_ground ? BHOP_BLUE_RGB : BHOP_RED_RGB;
    Draw_AlphaFillRGB(history->bar.x_mid - 1 * scale, y + 1 * scale, 2, BHOP_SMALL, color, 1.0);
    Draw_AlphaFill(history->bar.x_start, y + (1 + BHOP_SMALL) * scale, (history->bar.x_end - history->bar.x_start) / scale, 1, BHOP_WHITE, 0.8);
}

/*
 * draw the markings for previous frames
 */
void Bhop_DrawOldMarks(bhop_data_t *history, int window, int scale)
{
    int y = scr_vrect.y + scr_vrect.height / 2.0 + (12 + BHOP_SMALL + 2) * scale;
    int x = scr_vrect.x + scr_vrect.width / 2.0;
    int i = 0;
    if (!history)
        return;

    Draw_AlphaFillRGB(x - 1 * scale, y, 2, window, BHOP_GREEN_RGB, 0.3);

    while (history && i++ < window) {
        int color = history->on_ground ? BHOP_BLUE_RGB : BHOP_RED_RGB;
        Draw_AlphaFillRGB(history->bar.x_mid - 1 * scale, y, 2, 1, color, 1.0 * (float)(window - i) / window);
        y += scale;
        history = history->next;
    }
}


/*
 * gather the history data necessary for stats
 */
void Bhop_GatherData(void)
{
    if (sv_player->v.health <= 0 || cl.intermission)
        return;

    vec3_t vel;
    bhop_data_t * data_frame = malloc(sizeof(bhop_data_t));
    bhop_mark_t bar;
    float focal_len = (vid.width / 2.0) / tan( r_refdef.fov_x * M_PI / 360.0);

    VectorCopy(sv_player->v.velocity, data_frame->velocity);

    VectorCopy(sv_player->v.origin, data_frame->position);

    bhop_keystate_t keystate = {
        .fwd = CL_KeyState(&in_forward), 
        .left = CL_KeyState(&in_moveleft), 
        .right = CL_KeyState(&in_moveright)
    };
    data_frame->movement = keystate;

    data_frame->on_ground = onground;
    data_frame->angle_change = lastangle - sv_player->v.angles[1];
    data_frame->viewangle = lastangle = sv_player->v.angles[1];

    data_frame->friction_loss = 
        sqrt(pre_sv_velocity[0]*pre_sv_velocity[0] + pre_sv_velocity[1]*pre_sv_velocity[1]);
    data_frame->friction_loss -= data_frame->speed = 
        sqrt(data_frame->velocity[0]*data_frame->velocity[0] + data_frame->velocity[1]*data_frame->velocity[1]);

    data_frame->speed_gain = 0;
    if (data_frame->on_ground){
        data_frame->speed_gain = data_frame->speed - last_ground_speed;
        last_ground_speed = data_frame->speed;
    }

    VectorCopy(sv_player->v.angles, data_frame->angles);

    if(!data_frame->on_ground){
        VectorCopy(pre_sv_velocity, vel);
        bar = Bhop_CalcAirBar(vel, data_frame->angles, focal_len);
    } else if (data_frame->on_ground) {
        VectorCopy(post_friction_velocity, vel);
        bar = Bhop_CalcGroundBar(vel, data_frame->angles, focal_len);
    }

    /* flip angles if moving to the left */
    if (cmd.sidemove < 0) {
        bar.x_start = vid.width - bar.x_start;
        bar.x_mid = vid.width - bar.x_mid;
        bar.x_end = vid.width - bar.x_end;
    }

    data_frame->bar = bar;

    data_frame->next = bhop_history;
    bhop_history = data_frame;

    if (show_bhop_histlen.value > 0)
        Bhop_CullHistory(bhop_history, (int) show_bhop_histlen.value);
}

/*
 * speed up drawing of the key arrays by detecting continuous chunks
 */
void Bhop_DrawKeyContinuous(int *frames, int window, int x, int y, int height)
{
    int last = frames[0], last_index = 0;

    for (int i = 0; i <= window; i++) {
        if (i == window || frames[i] != last){
            Draw_BoxScaledOrigin(x + last_index, y, i - last_index, height, last, 1.0);
            if (i != window)
                last = frames[last_index = i];
        }
    }
}

/*
 * draw graph showing keypresses for strafes and forward taps
 */
void Bhop_DrawKeyGraph(int **frames, int window, int x, int y, int offset)
{
    Draw_BoxScaledOrigin(x, y    , window,  10, BHOP_BLACK_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y    , window,  1 , BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + (1 + BHOP_SMALL), window,  1 , BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + (2 + 3 * BHOP_SMALL), window, 1 , BHOP_WHITE_RGB, 1.0);

    Bhop_DrawKeyContinuous(frames[0] + offset, window, x, y + 1 , 4);
    Bhop_DrawKeyContinuous(frames[1] + offset, window, x, y + 2 + BHOP_SMALL , 4);
    Bhop_DrawKeyContinuous(frames[2] + offset, window, x, y + 2 + 2 * BHOP_SMALL, 4);
}

/*
 * draw graph showing speed; bit of a performance hog due to use of the
 * drawing functions per frame.
 */
void Bhop_DrawSpeedGraph(float *frames, int window, int x, int y)
{
    /* works like the speed bar */
    int colors[4] = {BHOP_GREEN_RGB, BHOP_RED_RGB, BHOP_ORANGE_RGB, BHOP_BLUE_RGB};

    /* backdrop and borders */
    Draw_BoxScaledOrigin(x, y     , window, 1             , BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 , window, BHOP_BIG, BHOP_BLACK_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 + BHOP_BIG, window, 1, BHOP_WHITE_RGB, 1.0);
    for (int i = window - 1; i >= 0; i--) {
        int color = 0;
        float speed = frames[i];

        /* draw the bars */
        while (speed > 0.0) {
            if (speed > 320.0)
                Draw_BoxScaledOrigin(x + i, y + 1 + BHOP_BIG, 1, -BHOP_BIG, colors[color], 1.0);
            else
                Draw_BoxScaledOrigin(x + i, y + 1 + BHOP_BIG, 1, -(int)speed / 10, colors[color], 1.0);
            speed -= 320.0;
            color = (color + 1) % 4;
        }
    }

    /* guide lines */
    Draw_BoxScaledOrigin(x, y + 1 + BHOP_BIG/4 , 1, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 + 2 * BHOP_BIG/4, 2, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 + 3 * BHOP_BIG/4, 1, 1, BHOP_WHITE_RGB, 1.0);

    Draw_BoxScaledOrigin(x + window, y + 1 + BHOP_BIG/4 , -1, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x + window, y + 1 + 2 * BHOP_BIG/4, -2, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x + window, y + 1 + 3 * BHOP_BIG/4, -1, 1, BHOP_WHITE_RGB, 1.0);
}

/*
 * draw acceleration on a logarithmic scale
 */
void Bhop_DrawAccelGraph(float *frames, int window, int x, int y)
{
    float prev_speed = -1.0;

    /* backdrop and borders */
    Draw_BoxScaledOrigin(x, y     , window, 1 , BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 , window, BHOP_MEDIUM, BHOP_BLACK_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 + BHOP_MEDIUM, window, 1 , BHOP_WHITE_RGB, 1.0);

    for (int i = window - 1; i >= 0; i--) {
        float speed = frames[i];
        float speed_delta = (prev_speed != -1.0) ? speed - prev_speed : 0.0;

        /* use a logarithm for data presentation: log_2 (|speed|) + 4 = log_2(4 * |speed|) */
        if (fabs(speed_delta) < 1.0 / 16.0)
            speed_delta = 0.0;
        else
            speed_delta = copysign(min(log2f(16 * fabs(speed_delta)), 9.0), speed_delta);


        /* draw the bars */
        if (speed_delta > 0.0)
            Draw_BoxScaledOrigin(x + i, y + 1 + BHOP_MEDIUM / 2, 1, (int) speed_delta, BHOP_RED_RGB, 1.0);
        else
            Draw_BoxScaledOrigin(x + i, y + 2 + BHOP_MEDIUM / 2, 1, (int) speed_delta, BHOP_GREEN_RGB, 1.0);

        /* set for next iteration */
        prev_speed = frames[i];
    }

    /* guide line */
    Draw_BoxScaledOrigin(x, y + 1 + BHOP_MEDIUM / 2, window, 1, BHOP_WHITE_RGB, 1.0);
}

int Bhop_BoundOffset(int x, int bound)
{
    x = x - vid.width / 2;
    return max(min(x, bound), -bound);
}

void Bhop_DrawAngleGraph(bhop_mark_t *frames, int window, int x, int y)
{
    float scale = Sbar_GetScaleAmount();

    /* backdrop and borders */
    Draw_BoxScaledOrigin(x, y, window, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1, window, BHOP_MEDIUM, BHOP_BLACK_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 + BHOP_MEDIUM / 2, window, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 1 + BHOP_MEDIUM, window, 1, BHOP_WHITE_RGB, 1.0);

    for (int i = window - 1; i >= 0; i--) {
        int offset_start = Bhop_BoundOffset(frames[i].x_start, scale * 9);
        int offset_mid = Bhop_BoundOffset(frames[i].x_mid, scale * 9);
        int offset_end = Bhop_BoundOffset(frames[i].x_end, scale * 9);
        if (offset_start != offset_end)
            Draw_AlphaFillRGB((x + i) * scale, (y + 9) * scale + offset_start, 1, (offset_end - offset_start) / scale, BHOP_LGREEN_RGB, 0.8);
        if (abs(offset_mid) < 9 * scale)
            Draw_AlphaFillRGB((x + i) * scale, (y + 9) * scale + offset_mid, 1, 1, BHOP_RED_RGB, 0.8);
    }
}

/*
 * draw squares representing frames around the last grounding, forward taps.
 */
void Bhop_DrawCrosshairSquares(bhop_data_t *history, int x, int y)
{
    bhop_data_t * history_it = history;
    int count = -1; 
    int i = 0, j = 0;
    float alpha = 1.0;

    if (!history)
        return;

    /* find the first on_ground */
    while (history && i < 48 && count == -1) {
        if (history->on_ground)
            count = i;
        i++;
        history = history->next;
    }

    if (count < show_bhop_frames.value) 
        return;

    history = history_it;
    alpha = min((48 - count) / 10.0, 1.0);
    i = 0;

    while (history && i < count + show_bhop_frames.value) {
        if (i > count - show_bhop_frames.value){
            if (i - count == 0) { /* central box */
                Draw_BoxScaledOrigin(x - 1, y + j - 1, 1, 2 + BHOP_SMALL, BHOP_ORANGE_RGB, alpha);
                Draw_BoxScaledOrigin(x + BHOP_SMALL, y + j - 1, 1, 2 + BHOP_SMALL, BHOP_ORANGE_RGB, alpha);
                Draw_BoxScaledOrigin(x    , y + j - 1, BHOP_SMALL, 1, BHOP_ORANGE_RGB, alpha);
                Draw_BoxScaledOrigin(x    , y + j + BHOP_SMALL, BHOP_SMALL, 1, BHOP_ORANGE_RGB, alpha);
            } else { /* other boxes */
                if (i - count < 0)
                    Draw_BoxScaledOrigin(x + BHOP_SMALL, y + j - 1, 1, 2 + BHOP_SMALL, BHOP_WHITE_RGB, alpha);
                else
                    Draw_BoxScaledOrigin(x - 1, y + j - 1, 1, 2 + BHOP_SMALL, BHOP_WHITE_RGB, alpha);
                Draw_BoxScaledOrigin(x, y + j - 1, BHOP_SMALL, 1, BHOP_WHITE_RGB, alpha);
                Draw_BoxScaledOrigin(x, y + j + BHOP_SMALL, BHOP_SMALL, 1, BHOP_WHITE_RGB, alpha);
            }
            Draw_BoxScaledOrigin(x, y, BHOP_SMALL, BHOP_SMALL, Bhop_GetFwdColor(history), alpha);
            x -= 5;
        }

        i++;
        history = history->next;
    }
}

/*
 * draw info about strafes changes midair
 */
void Bhop_DrawStrafeSquares(bhop_data_t *history, int x, int y)
{
    bhop_data_t * history_it = history;
    bhop_data_t * prev_ground_frame = NULL;
    bhop_data_t * last_ground_frame = NULL;
    int count = -1; 
    int i = 0, j = 0;
    float alpha = 1.0;
    int last_ground, previous_ground;

    if (!history)
        return;

    /* find the first on_ground */
    while (history && i < 48 && count == -1) {
        if (history->on_ground) {
            count = i;
            last_ground_frame = history;
        }
        i++;
        history = history->next;
    }

    if (count < show_bhop_frames.value) 
        return;

    last_ground = count;
    history_it = history;

    /* find the second on_ground */
    while (history && i < 144 && count == last_ground) {
        if (history->on_ground) {
            count = i;
            prev_ground_frame = history;
        }
        i++;
        history = history->next;
    }

    previous_ground = count;

    int jump_len = previous_ground - last_ground;

    if (jump_len <= 1)
        return;

    /* collect states between the two */
    enum bhop_kb_state * kb = Q_calloc(previous_ground - last_ground, sizeof(enum bhop_kb_state));
    enum bhop_m_state * m = Q_calloc(previous_ground - last_ground, sizeof(enum bhop_m_state));
    bhop_data_t ** air_history = Q_calloc(previous_ground - last_ground, sizeof(bhop_data_t));

    history = history_it;
    i = 0;
    alpha = min((48 - last_ground) / 10.0, 1.0);

    while (history && i < jump_len) {
        kb[i] = Bhop_GetStrafeState(history);
        m[i] = Bhop_GetMouseState(history);
        air_history[i] = history;
        i++;
        history = history->next;
    }

    /* now, detect the frames where mouse direction change happens */

    int * strafe_starts = Q_calloc(previous_ground - last_ground, sizeof(int));
    int strafe_count = 0;

    enum bhop_m_state last_m_dir = BHOP_M_NONE;
    // calculate distance curved from ground to ground
    float diff_x = last_ground_frame->position[0] - air_history[0]->position[0];
    float diff_y = last_ground_frame->position[1] - air_history[0]->position[1];

    float distance_curved = sqrt(diff_x * diff_x + diff_y * diff_y);

    for (i = 0; i < jump_len; i++) {
        if (i > 0) {
            float diff_x = air_history[i]->position[0] - air_history[i-1]->position[0];
            float diff_y = air_history[i]->position[1] - air_history[i-1]->position[1];
            distance_curved += sqrt(diff_x * diff_x + diff_y * diff_y);
        }

        if (last_m_dir == BHOP_M_NONE && m[i] != BHOP_M_NONE)
            last_m_dir = m[i];

        if (last_m_dir != BHOP_M_NONE && m[i] != BHOP_M_NONE && m[i] != last_m_dir){
            last_m_dir = m[i];
            strafe_starts[strafe_count++] = i;
        }
    }

    diff_x = prev_ground_frame->position[0] - last_ground_frame->position[0];
    diff_y = prev_ground_frame->position[1] - last_ground_frame->position[1];

    float distance_straight = sqrt(diff_x * diff_x + diff_y * diff_y);

    char str[24];

    int scale = Sbar_GetScaleAmount();
    int charsize = Sbar_GetScaledCharacterSize();

    Q_snprintfz (str, sizeof(str), "%3.1f (%.3f)", distance_straight, distance_straight/distance_curved);
    Draw_String (x * scale, y * scale, str, true);
    /* for each strafe, detect: last frame the things were synced, first frame they were synced, draw between */

    int cur_x = x - 16 * strafe_count;
    for (int strafe = 0; strafe < strafe_count; strafe++){
        int start_lim = 0;
        if (strafe > 0)
            start_lim = strafe_starts[strafe - 1];

        int frame = strafe_starts[strafe] - 1;
        while (frame >= start_lim) {
            if (m[frame] == BHOP_M_LEFT && kb[frame] == BHOP_KB_LEFT)
                break;
            if (m[frame] == BHOP_M_RIGHT && kb[frame] == BHOP_KB_RIGHT)
                break;
            frame--;
        }

        int prev_sync = frame;

        int end_lim = jump_len;
        if (strafe < strafe_count - 1)
            end_lim = strafe_starts[strafe + 1];

        frame = strafe_starts[strafe];
        while (frame < end_lim) {
            if (m[frame] == BHOP_M_LEFT && kb[frame] == BHOP_KB_LEFT)
                break;
            if (m[frame] == BHOP_M_RIGHT && kb[frame] == BHOP_KB_RIGHT)
                break;
            frame++;
        }
        int cur_sync = frame;

        int cur_y = y;

        // data for each strafe
        for (i = prev_sync; i <= cur_sync; i++) {
            Draw_BoxScaledOrigin(cur_x, cur_y, BHOP_SMALL * 2 + 1, 1, BHOP_WHITE_RGB, alpha);
            Draw_BoxScaledOrigin(cur_x, cur_y, 1, BHOP_SMALL * 2 + 1, BHOP_WHITE_RGB, alpha);
            Draw_BoxScaledOrigin(cur_x + BHOP_SMALL * 2, cur_y, 1, BHOP_SMALL * 2 + 1, BHOP_WHITE_RGB, alpha);
            Draw_BoxScaledOrigin(cur_x + 1, cur_y + BHOP_SMALL, BHOP_SMALL * 2 - 1, 1, BHOP_BLACK_RGB, alpha);
            Draw_BoxScaledOrigin(cur_x + BHOP_SMALL, cur_y + 1, 1, BHOP_SMALL * 2 - 1, BHOP_BLACK_RGB, alpha);
            if (i == cur_sync)
                Draw_BoxScaledOrigin(cur_x, cur_y + BHOP_SMALL * 2, BHOP_SMALL * 2 + 1, 1, BHOP_WHITE_RGB, alpha);

            /* draw LEFT upper - mouse state */
            int color = BHOP_BLACK_RGB;
            if (m[i] == BHOP_M_LEFT) {
                if(kb[i] == BHOP_KB_LEFT)
                    color = BHOP_GREEN_RGB;
                else if (kb[i] == BHOP_KB_RIGHT)
                    color = BHOP_RED_RGB;
                else
                    color = BHOP_ORANGE_RGB;
            }
            if (color != BHOP_BLACK_RGB)
                Draw_BoxScaledOrigin(cur_x + 1, cur_y + 1, BHOP_SMALL - 1, BHOP_SMALL - 1, color, alpha);

            /* draw RIGHT upper - mouse state */
            color = BHOP_BLACK_RGB;
            if (m[i] == BHOP_M_RIGHT) {
                if(kb[i] == BHOP_KB_RIGHT)
                    color = BHOP_GREEN_RGB;
                else if (kb[i] == BHOP_KB_LEFT)
                    color = BHOP_RED_RGB;
                else
                    color = BHOP_ORANGE_RGB;
            }
            if (color != BHOP_BLACK_RGB)
                Draw_BoxScaledOrigin(cur_x + 1 + BHOP_SMALL, cur_y + 1, BHOP_SMALL - 1, BHOP_SMALL - 1, color, alpha);

            /* draw LEFT lower - kb state */
            color = BHOP_BLACK_RGB;
            if (kb[i] == BHOP_KB_LEFT) {
                if (m[i] == BHOP_M_LEFT)
                    color = BHOP_GREEN_RGB;
                else if (m[i] == BHOP_M_RIGHT)
                    color = BHOP_RED_RGB;
                else
                    color = BHOP_ORANGE_RGB;
            }
            if (kb[i] == BHOP_KB_BOTH)
                color = BHOP_ORANGE_RGB;

            if (color != BHOP_BLACK_RGB)
                Draw_BoxScaledOrigin(cur_x + 1, cur_y + 1 + BHOP_SMALL, BHOP_SMALL - 1, BHOP_SMALL - 1, color, alpha);

            /* draw RIGHT lower - kb state */
            color = BHOP_BLACK_RGB;
            if (kb[i] == BHOP_KB_RIGHT) {
                if (m[i] == BHOP_M_RIGHT)
                    color = BHOP_GREEN_RGB;
                else if (m[i] == BHOP_M_LEFT)
                    color = BHOP_RED_RGB;
                else
                    color = BHOP_ORANGE_RGB;
            }
            if (kb[i] == BHOP_KB_BOTH)
                color = BHOP_ORANGE_RGB;

            if (color != BHOP_BLACK_RGB)
                Draw_BoxScaledOrigin(cur_x + 1 + BHOP_SMALL, cur_y + 1 + BHOP_SMALL, BHOP_SMALL - 1, BHOP_SMALL - 1, color, alpha);

            cur_y += BHOP_SMALL * 2;
        }
        cur_x += 16;
    }

    
    if (kb)
        free(kb);
    if (m)
        free(m);
    if (air_history)
        free(air_history);
    if (strafe_starts)
        free(strafe_starts);
}

/*
 * draw info around crosshair
 */
void Bhop_DrawCrosshairGain(bhop_data_t *history, int x, int y, int scale, int charsize)
{
    char str[20];
    int i = 0;

    if (!history)
        return;

    /* find the first on_ground */
    while (history && i < 36) {
        if (history->on_ground) {
            if (i >= show_bhop_frames.value) {
                Q_snprintfz (str, sizeof(str), "a: %3.1f", history->speed_gain + history->friction_loss);
                Draw_String (x * scale - (strlen(str) + 1) * charsize, y * scale, str, true);

                Q_snprintfz (str, sizeof(str), "g: %3.1f", -history->friction_loss);
                Draw_String (x * scale + 1 * charsize, y * scale, str, true);
            }

            return;
        }

        i++;
        history = history->next;
    }
}

/*
 * draw visual feedback about prestrafe
 */
void Bhop_DrawCrosshairPrestrafeVis(bhop_data_t *history, int x, int y, int scale, int charsize)
{
    char str[20];
    int i = 0, j = 0, lastground = 0;
    float speed = 0.0;

    if (!history || history->on_ground)
        return;

    /* find the first on_ground */
    while (history && i++ < 36 * 4) {
        if (history->on_ground) {
            if (!lastground){
                lastground = i;
                speed = history->speed;
            }

            j++;

            if (speed && history->speed <= 321.0)
                break;
        } else if (speed != 0.0) {
            break;
        }
        history = history->next;
    }

    if (!speed || lastground >= 36 || lastground < show_bhop_frames.value)
        return;

    if (j <= 1) /* we are not interested in hops */
        return;

    float color_pos = 0.0;

    if (speed >= 480)
        color_pos = 1.0;
    else if (speed >= 320)
        color_pos = ((float)(speed - 320)) / 160.0;

    float difficulty = max(show_bhop_prestrafe_diff.value, 1.0);

    int color_green = ((int)round(255.0 * pow(color_pos, difficulty)))<<8;
    int color_red = (int)round(255.0 * (1.0 - pow(color_pos, difficulty)));
    int color_bar = color_green + color_red;

    Draw_AlphaFillRGB((x - 40) * scale, (y - 1) * scale, (int)round(80.0), 10, BHOP_WHITE_RGB, 1.0);
    Draw_AlphaFillRGB((x - 40) * scale, y * scale, (int)round(80.0), 8, BHOP_BLACK_RGB, 1.0);
    Draw_AlphaFillRGB((x - 40) * scale, y * scale, (int)round(80.0 * color_pos), 8, color_bar, 1.0);

    Q_snprintfz (str, sizeof(str), "%3.0f", speed);
    Draw_String (x * scale - (strlen(str) + 1) * charsize, y * scale, str, true);

    /* amount of frames detected prestrafe */
    Q_snprintfz (str, sizeof(str), "(%d)", j);
    Draw_String (x * scale + 1 * charsize, y * scale, str, true);
}

// Interpolate between two colors (r,g,b) in [0,255]
static void interpolate_color(float t, int r1, int g1, int b1, int r2, int g2, int b2, int* r, int* g, int* b) {
    *r = (int)(r1 + (r2 - r1) * t);
    *g = (int)(g1 + (g2 - g1) * t);
    *b = (int)(b1 + (b2 - b1) * t);
}

// Draws a colored circle/arc with per-segment color based on values[] in [-1,1]
void Draw_CircleWithColoredSegments(int x_center, int y_center, float radius, float segment_start, float segment_end, float thickness, const float* values, int num_segments, float rotate, qboolean colorize) {
    float angle_range = segment_end - segment_start;
    float angle_step = -angle_range / num_segments;
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);

    glBegin(GL_LINES);
    glLineWidth(thickness);

    for (int i = 0; i < num_segments; ++i) {
        float angle0 = segment_start + i * angle_step + rotate;
        float angle1 = segment_start + (i + 1) * angle_step + rotate;

        float v = values[i];
        if (v < -1.0f) v = -1.0f;
        else if (v < 1000.0f && v > 1.0f) v = 1.0f;

        int r, g, b;
        if (v < 0.0f) {
            if (colorize)
                // TODO PK: replace color values with constants as defined in practice.h
                interpolate_color(v + 1.0f, 255, 0, 0, 128, 128, 128, &r, &g, &b);
            else
                interpolate_color(v + 1.0f, 10, 10, 10, 128, 128, 128, &r, &g, &b);
        }
        else if (v >= 1000.f) {
            if (colorize) {
                r = 0;
                g = 0;
                b = 255;
            }
            else
                interpolate_color(v, 10, 10, 10, 10, 10, 10, &r, &g, &b);
        }
        else {
            if (colorize)
                interpolate_color(v, 128, 128, 128, 0, 255, 0, &r, &g, &b);
            else
                interpolate_color(v, 10, 10, 10, 10, 10, 10, &r, &g, &b);
        }

        glColor4ub(r, g, b, 255);

        glVertex2f(x_center + cosf(angle0) * radius, y_center + sinf(angle0) * radius);
        glVertex2f(x_center + cosf(angle1) * radius, y_center + sinf(angle1) * radius);
    }
    glEnd();

    glLineWidth(1.0f); // Reset to default
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

}

void Bhop_CalculateAirAcceleration(vec3_t velocity, vec3_t angles, vec3_t velocity_new, vec3_t wishvel_original, vec3_t velocity_increase)
{
    vec3_t wishdir, wishvel, fwd_vec, right_vec, up_vec;
    float addspeed, currentspeed, wishspd, accelspeed;

    AngleVectors(angles, fwd_vec, right_vec, up_vec);

    for (int i = 0; i < 3; i++)
        wishvel[i] = fwd_vec[i] * cmd.forwardmove + right_vec[i] * cmd.sidemove;
    wishvel[2] = 0;

    VectorCopy(wishvel, wishvel_original);
    VectorCopy(wishvel, wishdir);
    float wishspeed = VectorNormalize(wishdir);

    wishspd = VectorNormalize(wishvel);
    if (wishspd > 30)
        wishspd = 30;

    currentspeed = DotProduct(velocity, wishvel);
    addspeed = wishspd - currentspeed;

    VectorCopy(velocity, velocity_new);

    if (addspeed <= 0) {
        addspeed = 0;
        for (int i = 0; i < 3; i++)
            velocity_increase[i] = 0.f;
    }
    else {
        //	accelspeed = sv_accelerate.value * sv_frametime;
        accelspeed = sv_accelerate.value * wishspeed * sv_frametime;
        if (accelspeed > addspeed)
            accelspeed = addspeed;

        for (int i = 0; i < 3; i++) {
            velocity_increase[i] = accelspeed * wishvel[i];
            velocity_new[i] += velocity_increase[i];
        }
    }
}

/*
 * draw wishdir chart
 */
void Bhop_DrawWishDir(bhop_data_t* history)
{
	// TODO PK: replace color values with constants as defined in practice.h
    color_t velocity_color = RGBA_TO_COLOR(255, 255, 255, 255);
    color_t wish_color = RGBA_TO_COLOR(255, 0, 0, 255);
    color_t velocity_increase_color = RGBA_TO_COLOR(0, 255, 0, 255);
    color_t onground_color = RGBA_TO_COLOR(10, 10, 10, 255);
    vec3_t velocity;
    vec3_t center = { 0.f, 0.f, 0.f };
    vec3_t velocity_pointer;
    vec3_t wishvel_pointer;
    vec3_t velocity_increase_pointer;
    vec3_t velocity_new_pointer;
    vec3_t angles;

    vec3_t velocity_new, velocity_increase, wishvel_original;

    if (!history)
        return;

    VectorCopy(history->velocity, velocity);
    VectorCopy(history->angles, angles);

    Bhop_CalculateAirAcceleration(velocity, angles, velocity_new, wishvel_original, velocity_increase);
    float speed = VectorLength(velocity);
    float speed_diff[360];
    memset(speed_diff, 0, sizeof(speed_diff));
    float max_test_speed_left = -10.f;
    int max_test_segment_index_left = -10000;
    float max_test_speed_right = -10.f;
    int max_test_segment_index_right = -10000;
    for (int i = -180; i < 180; i++) { // test all segments to see how much speed they would yield or lose
        vec3_t test_angles;
        vec3_t test_velocity_new, test_velocity_increase, test_wishvel_original;
        VectorCopy(history->angles, test_angles);
        test_angles[1] += i;
        if (test_angles[1] >= 180.f)
            test_angles[1] -= 360.f;
        if (test_angles[1] < -180.f)
            test_angles[1] += 360.f;
        Bhop_CalculateAirAcceleration(velocity, test_angles, test_velocity_new, test_wishvel_original, test_velocity_increase);
        float test_speed = VectorLength(test_velocity_new);
        VectorNormalize(test_wishvel_original);
        vectoangles(test_wishvel_original, test_angles);
        int segment_index = (int)(test_angles[1]);
        segment_index %= 360; // vectoangles allow 360 as a result, turn it into a zero to avoid index out of range error
		speed_diff[segment_index] = (test_speed - speed) * 1.f;
        if (i >= 0 && speed_diff[segment_index] > max_test_speed_left) {
            max_test_speed_left = speed_diff[segment_index];
            max_test_segment_index_left = segment_index;
        }
        if (i < 0 && speed_diff[segment_index] > max_test_speed_right) {
            max_test_speed_right = speed_diff[segment_index];
            max_test_segment_index_right = segment_index;
        }
    }

    // highlight the best segments
	if (max_test_segment_index_left != -10000)
        speed_diff[max_test_segment_index_left] = 1000.f;
	if (max_test_segment_index_right != -10000)
        speed_diff[max_test_segment_index_right] = 1000.f;

    center[0] = scr_vrect.x + scr_vrect.width / 2;
    center[1] = scr_vrect.y + scr_vrect.height / 2;

    vec3_t rotation_vector;
    vectoangles(velocity, rotation_vector);
    float rotate = rotation_vector[1] - 90.f; // rotate all vectors and the circle to make velocity point up
    if (rotate > 180.f)
        rotate -= 360.f;
    if (rotate < -180.f)
        rotate += 360.f;

    // flip x axis, so that they match the mouse movement direction
    velocity[0] = -velocity[0];
    velocity_new[0] = -velocity_new[0];
    wishvel_original[0] = -wishvel_original[0];
    velocity_increase[0] = -velocity_increase[0];

    vec3_t up = { 0.f, 0.f, 1 };  // rotate around z axis
    vec3_t tmp_vector;
    VectorCopy(velocity, tmp_vector);           RotatePointAroundVector(velocity, up, tmp_vector, rotate);
    VectorCopy(velocity_new, tmp_vector);       RotatePointAroundVector(velocity_new, up, tmp_vector, rotate);
    VectorCopy(wishvel_original, tmp_vector);   RotatePointAroundVector(wishvel_original, up, tmp_vector, rotate);
    VectorCopy(velocity_increase, tmp_vector);  RotatePointAroundVector(velocity_increase, up, tmp_vector, rotate);

    float ui_scale = 0.5f; // vectors get too large for the screen
    VectorScale(velocity, ui_scale, velocity);
    VectorScale(velocity_new, ui_scale, velocity_new);
    VectorScale(wishvel_original, ui_scale, wishvel_original);
    VectorScale(velocity_increase, ui_scale, velocity_increase);

    // add vectors to center
    VectorSubtract(center, velocity, velocity_pointer);
    VectorSubtract(center, velocity_new, velocity_new_pointer);
    VectorSubtract(center, wishvel_original, wishvel_pointer);
    VectorSubtract(velocity_pointer, velocity_increase, velocity_increase_pointer);

    // draw vectors
    Draw_AlphaLineRGB(center[0], center[1], velocity_pointer[0], velocity_pointer[1], 2, history->on_ground ? onground_color : velocity_color);
    Draw_AlphaLineRGB(center[0], center[1], velocity_new_pointer[0], velocity_new_pointer[1], 2, history->on_ground ? onground_color : velocity_color);
    Draw_AlphaLineRGB(center[0], center[1], wishvel_pointer[0], wishvel_pointer[1], 2, history->on_ground ? onground_color : wish_color);
    Draw_AlphaLineRGB(velocity_pointer[0], velocity_pointer[1], velocity_increase_pointer[0], velocity_increase_pointer[1], 4, history->on_ground ? onground_color : velocity_increase_color);

    // TODO PK: convert parameters to degrees instead radians for consistency
    // draw acceleration circle
    Draw_CircleWithColoredSegments(center[0], center[1], 200.f, 0.f, 2 * M_PI, 1, speed_diff, 360, rotate * (float)(M_PI / 180.0f), !history->on_ground);
}

/*
 * BHOP_Init() initialises cvars and global vars
 */
void BHOP_Init (void)
{
    bhop_history = NULL;
    bhop_summary = NULL;
    Cvar_Register (&show_bhop_stats);
    Cvar_Register (&show_bhop_stats_x);
    Cvar_Register (&show_bhop_stats_y);
    Cvar_Register (&show_bhop_histlen);
    Cvar_Register (&show_bhop_window);
    Cvar_Register (&show_bhop_frames);
    Cvar_Register (&show_bhop_prestrafe_diff);
}

/*
 * BHOP_Start() runs every map restart
 */
void BHOP_Start (void)
{
    bhop_history = NULL;
    bhop_summary = NULL;
    lastangle = 0;
    last_ground_speed = 0;
}

/*
 * BHOP_Stop() runs when gameplay finishes
 */
void BHOP_Stop (void)
{
    Bhop_CullHistory(bhop_history, 0);
    bhop_history = NULL;
    if (bhop_summary) {
        if (bhop_summary->speeds)
            free(bhop_summary->speeds);
        if (bhop_summary->keys) {
            for (int i = 0; i < 3; i++)
                free(bhop_summary->keys[i]);
            free(bhop_summary->keys);
        }
        if (bhop_summary->angles)
            free(bhop_summary->angles);
        bhop_summary = NULL;
    }
}

/*
 * BHOP_Shutdown() runs on shutdown (duh)
 */
void BHOP_Shutdown (void)
{
    BHOP_Stop();
}

/*
 * Function for drawing live stats and markings during gameplay
 */
void SCR_DrawBHOP (void)
{
    int     x, y;
    int     charsize;
    int     scale;
    char    str[80];
    static int window;
    float *frames = NULL;

    x = ELEMENT_X_COORD(show_bhop_stats);
    y = ELEMENT_Y_COORD(show_bhop_stats);

    charsize = Sbar_GetScaledCharacterSize();
    scale = Sbar_GetScaleAmount();

    if (!sv.active && !cls.demoplayback) return;

    window = (int)show_bhop_window.value;
    window = min(144, window);
    window = max(1, window);

    if (physframe)
        Bhop_GatherData();

    if ((int)show_bhop_stats.value & BHOP_AVG_SPEED) {
        Q_snprintfz (str, sizeof(str), "avg_speed %3.1f", Bhop_SpeedAvg(bhop_history, window));
        Draw_String (x, y, str, true);
        y += charsize;
    }

    if ((int)show_bhop_stats.value & BHOP_KEYPRESSES) {
        bhop_summary_t summary = Bhop_GetSummary(bhop_history, window, false);
        Q_snprintfz (
                str, sizeof(str), "f%% %03.f/%03.f s%% %03.f", 
                summary.fwd_on_ground_percent, 
                summary.fwd_percent, 
                summary.strafe_percent
                );
        Draw_String (x, y, str, true);
        y += charsize;
    }

    if ((int)show_bhop_stats.value & BHOP_FORWARD_HISTORY_GRAPH) {
        int ** frames_keys = Bhop_KeyArrays(bhop_history, window);

        Bhop_DrawKeyGraph(frames_keys, window, x / scale, y / scale, 0);

        for (int i = 0; i < 3; i++)
            free(frames_keys[i]);
        free(frames_keys);

        y += (3 + BHOP_GAP + BHOP_SMALL * 3) * scale;
    }

    if ((int)show_bhop_stats.value & BHOP_SPEED_HISTORY_GRAPH) {
        if (!frames)
            frames = Bhop_SpeedArray(bhop_history, window);

        Bhop_DrawSpeedGraph(frames, window, x / scale, y / scale);

        y += (2 + BHOP_BIG + BHOP_GAP) * scale;
    }

    if ((int)show_bhop_stats.value & BHOP_DSPEED_HISTORY_GRAPH) {
        if (!frames)
            frames = Bhop_SpeedArray(bhop_history, window);

        Bhop_DrawAccelGraph(frames, window, x / scale, y / scale);

        y += (2 + BHOP_GAP + BHOP_MEDIUM) * scale;
    }

    if (frames){
        free(frames);
        frames = NULL;
    }

    if ((int)show_bhop_stats.value & BHOP_ANGLE_MARK) {
        Bhop_DrawCurrentMark(bhop_history, scale);
        Bhop_DrawOldMarks(bhop_history, window, scale);
    }

    x = Bhop_InverseScale(scr_vrect.x + scr_vrect.width / 2.0);
    y = Bhop_InverseScale(scr_vrect.y + scr_vrect.height / 2.0);

    if ((int)show_bhop_stats.value & BHOP_CROSSHAIR_INFO)
        Bhop_DrawCrosshairGain(bhop_history, x, y - 36, scale, charsize);

    if ((int)show_bhop_stats.value & BHOP_CROSSHAIR_TAP_PREC)
        Bhop_DrawCrosshairSquares(bhop_history, x - 2 + (show_bhop_frames.value - 1) * 5, y - 12);

    if ((int)show_bhop_stats.value & BHOP_CROSSHAIR_PRESTRAFE)
        Bhop_DrawCrosshairPrestrafeVis(bhop_history, x, y - 24, scale, charsize);

    if ((int)show_bhop_stats.value & BHOP_CROSSHAIR_SYNC)
        Bhop_DrawStrafeSquares(bhop_history, x - 32, y - 24);

    if ((int)show_bhop_stats.value & BHOP_CIRCLE)
        Bhop_DrawWishDir(bhop_history);
}

/*
 * Function for drawing summary on intermission screen
 */
void SCR_DrawBHOPIntermission(void)
{
    int  x, y;
    int  charsize;
    char str[80];
    static int histlen = -1;
    static int offset = -144;
    int offset_temp;
    float scale;

    if (!sv.active && !cls.demoplayback) return;

    charsize = Sbar_GetScaledCharacterSize();
    scale = Sbar_GetScaleAmount();
    x = scr_vrect.x + scr_vrect.width / 2.0;
    y = scr_vrect.y + scr_vrect.height -
        (9 + BHOP_GAP * 3 + BHOP_SMALL * 3 + BHOP_MEDIUM * 2 + BHOP_BIG) * scale -
        2 * charsize;


    if (!bhop_summary){
        offset = -144;
        histlen = Bhop_Len(bhop_history);
        bhop_summary = malloc(sizeof(bhop_summary_t));
        *bhop_summary = Bhop_GetSummary(bhop_history, histlen, true);
    }

    if (!bhop_summary->keys) {
        free(bhop_summary);
        bhop_summary = NULL;
        return;
    }

    if ((int)show_bhop_stats.value & BHOP_AVG_SPEED) {
        Q_snprintfz (str, sizeof(str), "avg speed: %3.1f", Bhop_SpeedAvg(bhop_history, histlen));
        Draw_String (x - (strlen(str) + 1) * charsize / 2.0, y, str, true);
        y += charsize;
    }

    if ((int)show_bhop_stats.value & BHOP_KEYPRESSES) {
        Q_snprintfz (
            str, sizeof(str), "fwd gr: %.1f%% | fwd air: %.1f%% | strafe: %.1f%%", 
            bhop_summary->fwd_on_ground_percent,
            bhop_summary->fwd_percent,
            bhop_summary->strafe_percent
        );
        Draw_String (x - (strlen(str) + 1) * charsize / 2.0, y, str, true);
        y += charsize;
    }

    x = Bhop_InverseScale(x);
    y = Bhop_InverseScale(y);

    offset_temp = offset > histlen - 320 ? histlen - 320 : offset;
    offset_temp = offset_temp < 0 ? 0 : offset_temp;

    if ((int)show_bhop_stats.value & BHOP_FORWARD_HISTORY_GRAPH) {
        Bhop_DrawKeyGraph(bhop_summary->keys, min(320, histlen), x - 160, y, offset_temp);

        y += 3 + BHOP_GAP + BHOP_SMALL * 3;
    }

    if ((int)show_bhop_stats.value & BHOP_SPEED_HISTORY_GRAPH) {
        Bhop_DrawSpeedGraph(bhop_summary->speeds + offset_temp, min(320, histlen), x - 160, y);

        y += (2 + BHOP_BIG + BHOP_GAP);
    }

    if ((int)show_bhop_stats.value & BHOP_DSPEED_HISTORY_GRAPH) {
        Bhop_DrawAccelGraph(bhop_summary->speeds + offset_temp, min(320, histlen), x - 160, y);
        y += (2 + BHOP_GAP + BHOP_MEDIUM);
    }

    if ((int)show_bhop_stats.value & BHOP_ANGLE_MARK) {
        Bhop_DrawAngleGraph(bhop_summary->angles + offset_temp, min(320, histlen), x - 160, y);
    }

    if (physframe)
        offset = offset + 1 < histlen ? offset + 1 : -144;
}
