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

#include <string.h>
#include "../quakedef.h"
#include "practice.h"

cvar_t show_bhop_stats = {"show_bhop_stats", "0", true};
cvar_t show_bhop_stats_x = {"show_bhop_stats_x", "1", true};
cvar_t show_bhop_stats_y = {"show_bhop_stats_y", "4", true};
cvar_t show_bhop_histlen = {"show_bhop_histlen", "0", true};
cvar_t show_bhop_window = {"show_bhop_window", "144", true};
cvar_t show_bhop_frames = {"show_bhop_frames", "7", true};

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
    Draw_AlphaFillRGB(history->bar.x_mid - 1 * scale, y + 1 * scale, 2, BHOP_SMALL, BHOP_RED_RGB, 1.0);
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
        Draw_AlphaFillRGB(history->bar.x_mid - 1 * scale, y, 2, 1, BHOP_RED_RGB, 0.8 * (float)(window - i) / window);
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

    vec3_t vel, angles;
    bhop_data_t * data_frame = malloc(sizeof(bhop_data_t));
    bhop_mark_t bar;
    float focal_len = (vid.width / 2.0) / tan( r_refdef.fov_x * M_PI / 360.0);

    VectorCopy(sv_player->v.velocity, data_frame->velocity);

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

    VectorCopy(sv_player->v.angles, angles);

    if(!data_frame->on_ground){
        VectorCopy(pre_sv_velocity, vel);
        bar = Bhop_CalcAirBar(vel, angles, focal_len);
    } else if (data_frame->on_ground) {
        VectorCopy(post_friction_velocity, vel);
        bar = Bhop_CalcGroundBar(vel, angles, focal_len);
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
 * print a summary to console
 */
void Bhop_PrintSummary(void)
{
    bhop_summary_t summary = Bhop_GetSummary(bhop_history, Bhop_Len(bhop_history), false);
    Con_Printf(
        "overall bhop summary:\n+forward ground accuracy: %3.1f%%\n+forward overall accuracy: %3.1f%%\nstrafe accuracy: %3.1f\n%%",
        summary.fwd_on_ground_percent, summary.fwd_percent, summary.strafe_percent
    );
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
    while (history && i < 36 && count == -1) {
        if (history->on_ground)
            count = i;
        i++;
        history = history->next;
    }

    if (count < show_bhop_frames.value) 
        return;

    history = history_it;
    alpha = min((36 - 10 - count) / 10.0, 1.0);
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
 * draw info about prestrafe
 */
void Bhop_DrawCrosshairPrestrafe(bhop_data_t *history, int x, int y, int scale, int charsize)
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

            if (speed && history->speed <= 321.0)
                break;
            j++;
        } else if (speed != 0.0) {
            break;
        }
        history = history->next;
    }

    if (!speed || lastground >= 36 || lastground < show_bhop_frames.value)
        return;

    Q_snprintfz (str, sizeof(str), "v: %3.0f", speed);
    Draw_String (x * scale - (strlen(str) + 1) * charsize, y * scale, str, true);

    Q_snprintfz (str, sizeof(str), "t: %1.2f", j / 72.0);
    Draw_String (x * scale + 1 * charsize, y * scale, str, true);
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
        if (show_bhop_stats.value > 0)
            Bhop_PrintSummary();
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

    if ((int)show_bhop_stats.value & BHOP_CROSSHAIR_INFO) {
        x = Bhop_InverseScale(scr_vrect.x + scr_vrect.width / 2.0);
        y = Bhop_InverseScale(scr_vrect.y + scr_vrect.height / 2.0);

        Bhop_DrawCrosshairSquares(bhop_history, x - 2 + (show_bhop_frames.value - 1) * 5, y - 12);

        Bhop_DrawCrosshairGain(bhop_history, x, y - 24, scale, charsize);

        Bhop_DrawCrosshairPrestrafe(bhop_history, x, y - 24 - charsize / scale, scale, charsize);
    }
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
