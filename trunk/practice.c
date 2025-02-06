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
#include "quakedef.h"
#include "practice.h"

cvar_t show_bhop_stats = {"show_bhop_stats", "0"};
cvar_t show_bhop_stats_x = {"show_bhop_stats_x", "1"};
cvar_t show_bhop_stats_y = {"show_bhop_stats_y", "4"};
cvar_t show_bhop_histlen = {"show_bhop_histlen", "0"};
cvar_t show_bhop_window = {"show_bhop_window", "144"};
cvar_t show_bhop_frames = {"show_bhop_frames", "7"};

/* this is intended as a FIFO structure; new items will be added at the beginning */
bhop_data_t *bhop_history = NULL;
bhop_mark_t bhop_last_ground;
static float lastangle;
static float last_ground_speed;

/* items required for calculation logic */
extern qboolean onground;
extern usercmd_t cmd;
extern kbutton_t in_forward, in_moveleft, in_moveright;
extern vec3_t pre_sv_velocity;
extern vec3_t post_friction_velocity;
extern double sv_frametime;

/* screen size stuff */
extern vrect_t	scr_vrect;
extern refdef_t	r_refdef;
extern viddef_t vid;
extern int glx, glwidth;

int bhop_inverse_scale(int x){
    int scale = Sbar_GetScaleAmount();
    return x / scale;
}

/* 
 * cull history after <num> newest datapoints 
 */
void bhop_cull_history(bhop_data_t * history, int num) {
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
int bhop_histlen(bhop_data_t * history) {
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
float bhop_speed(bhop_data_t * data) {
    return sqrt(data->velocity[0]*data->velocity[0] + data->velocity[1]*data->velocity[1]);
}

/*
 * average bhop speed calculation
 */
float bhop_speed_avg(bhop_data_t * history, int num){
    float avg = 0.0;
    int i = 0;

    while (i++ < num && history){
        avg += bhop_speed(history);
        history = history->next;
    }

    return avg / num;
}

/*
 * retrieve summary for entire window of history
 */
bhop_summary_t bhop_get_summary(bhop_data_t * history, int window){
    int i = 0, i_ground = 0;
    float strafesum;
    bhop_summary_t summary = {
        .fwd_on_ground_percent = 0.0,
        .fwd_percent = 0.0,
        .strafe_percent = 0.0
    };

    while (i < window && history){
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
float * bhop_speed_array(bhop_data_t * history, int window) {
    float * frames = calloc(window, sizeof(float));

    /* filled in backwards, so that it displays newest on the right */
    while (history && window--){
        frames[window] = bhop_speed(history);
        history = history->next;
    }

    return frames;
}

/*
 * get color for fwd tap displays
 */
int bhop_get_fwd_color(bhop_data_t * history){
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
int bhop_get_lstrafe_color(bhop_data_t * history){
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
int bhop_get_rstrafe_color(bhop_data_t * history){
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
int ** bhop_key_arrays(bhop_data_t * history, int window) {
    if (window < 0)
        return NULL;

    int ** frames = malloc(3 * sizeof(int *)); /* fwd, left, right */
    for(int i = 0; i < 3; i++)
        frames[i] = calloc(window, sizeof(int));

    while (history && window--) {
        /* fwd structure */
        frames[0][window] = bhop_get_fwd_color(history);

        /* strafing */
        frames[1][window] = bhop_get_lstrafe_color(history);
        frames[2][window] = bhop_get_rstrafe_color(history);

        history = history->next;
    }

    return frames;
}

/* 
 * gets the angles between which wishvel will retain speed relative
 * to velocity. hitting between these is a guaranteed speed increase 
 */
float * bhop_air_gain_roots(vec3_t velocity) {
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
float bhop_ground_delta_v(float vel_len, float angle, float friction_loss) {
    float w_len, delta_v;

    w_len = max(320 - vel_len * cos(angle), 0.0);
    w_len = min(sv_maxspeed.value * sv_accelerate.value * sv_frametime, w_len);

    delta_v = (vel_len + w_len * cos(angle));
    delta_v *= delta_v;

    delta_v += w_len * w_len * sin(angle) * sin(angle);

    delta_v = sqrt(delta_v) - vel_len - friction_loss;

    return delta_v;
}

/* 
 * the equation for this is not so simple because of multiple clippings. We will just find
 * the maximal values closest to the forward viewpoint and draw it.
 */
bhop_mark_t bhop_calculate_ground_bar(vec3_t velocity, vec3_t angles, float focal_len) {
    vec3_t wishvel;
    vec3_t fwd_vec, right_vec, up_vec;
    bhop_mark_t bar = {.x_start = 0, .x_end = 0, .x_mid = 0};
    int x, best_x;
    float angle_pixel, wish_speed, vel_speed, dot_product;
    float * delta_v = calloc(vid.width, sizeof(float));

    bar.friction_loss = sqrt(pre_sv_velocity[0]*pre_sv_velocity[0] + pre_sv_velocity[1]*pre_sv_velocity[1]);
    bar.friction_loss -= vel_speed = sqrt(velocity[0]*velocity[0] + velocity[1]*velocity[1]);

    AngleVectors(angles, fwd_vec, right_vec, up_vec);

    for (int i = 0; i < 3; i++)
        wishvel[i] = fwd_vec[i] * cmd.forwardmove + right_vec[i] * cmd.sidemove;
    wishvel[2] = 0;
    fwd_vec[2] = 0;

    wish_speed = VectorNormalize(wishvel);
    if (wish_speed == 0){
        for (x = 0; x < vid.width; x++) 
            delta_v[x] = -10000;
        return bar;
    }
    VectorNormalize(fwd_vec);

    dot_product = DotProduct(wishvel, fwd_vec);
    if(dot_product > 1.0)
        dot_product = 1.0;
    if(dot_product < -1.0)
        dot_product = -1.0;

    for (x = 0; x < vid.width; x++) {
        angle_pixel = atan((x - vid.width/2.0) / focal_len);
        delta_v[cmd.sidemove < 0 ? vid.width - x - 1 : x] = bhop_ground_delta_v(vel_speed, angle_pixel - acos(dot_product), bar.friction_loss);
    }

    return bar;
}

/* 
 * create display bar in air
 */
bhop_mark_t bhop_calculate_air_bar(vec3_t velocity, vec3_t angles, float focal_len) {
    float * bhop_roots;
    vec3_t wishvel, fwd_vec, right_vec, up_vec;
    float angle_diff, midpoint;
    bhop_mark_t bar;

    AngleVectors(angles, fwd_vec, right_vec, up_vec);

    for (int i = 0; i < 3; i++)
        wishvel[i] = fwd_vec[i] * cmd.forwardmove + right_vec[i] * cmd.sidemove;
    wishvel[2] = 0;
    velocity[2] = 0;

    bhop_roots = bhop_air_gain_roots(velocity);

    VectorNormalize(wishvel);
    VectorNormalize(velocity);
    angle_diff = acos(DotProduct(wishvel, velocity));

    bhop_roots[0] -= angle_diff; 
    bhop_roots[1] -= angle_diff;

    midpoint = (bhop_roots[0] + bhop_roots[1]) / 2.0;
    bar.x_start = (int)(tan(bhop_roots[0]) * focal_len) + vid.width / 2;
    bar.x_mid = (int)(tan(midpoint) * focal_len) + vid.width / 2;
    bar.x_end = (int)(tan(bhop_roots[1]) * focal_len) + vid.width / 2;

    /* flip angles if moving to the left */
    if(cmd.sidemove < 0){
        bar.x_start = vid.width - bar.x_start;
        bar.x_mid = vid.width - bar.x_mid;
        bar.x_end = vid.width - bar.x_end;
    }

    free(bhop_roots);

    bar.friction_loss = 0.0;
    return bar;
}

/*void bhop_draw_ground_mark(bhop_data_t * history) {
    int x, y;

    if(!history || history->on_ground || !bhop_last_ground.x_delta_v)
        return;

    y = scr_vrect.y + scr_vrect.height / 2 + 50;
    for (x = 0; x < vid.width; x++){
        if(bhop_last_ground.x_delta_v[x] >= -bhop_last_ground.friction_loss) {
            Draw_AlphaFill(x, y, 1, 1, BHOP_WHITE, 0.8);
            if (bhop_last_ground.x_delta_v[x] >= 0)
                Draw_AlphaFillRGB(x, y + 1 * Sbar_GetScaleAmount(), 1, 4, BHOP_GREEN_RGB, 0.5);
            else
                Draw_AlphaFillRGB(x, y + 1 * Sbar_GetScaleAmount(), 1, 4, BHOP_ORANGE_RGB, 0.5);
            Draw_AlphaFill(x, y + 5 * Sbar_GetScaleAmount(), 1, 1, BHOP_WHITE, 0.8);
        }
    }
}*/

void bhop_draw_angle_mark(bhop_data_t *history, int scale) {
    vec3_t vel, angles;
    float y, last_gain, d_last_gain;
    int x;
    float focal_len = (vid.width / 2.0) / tan( r_refdef.fov_x * M_PI / 360.0);
    bhop_mark_t bar = {.x_start = 0, .x_end = 0, .x_mid = 0};

    if(!history)
        return;

    VectorCopy(sv_player->v.angles, angles);

    if(!history->on_ground){
        VectorCopy(pre_sv_velocity, vel);
        bar = bhop_calculate_air_bar(vel, angles, focal_len);
    } else if (history->on_ground) {
        VectorCopy(post_friction_velocity, vel);
        bar = bhop_calculate_ground_bar(vel, angles, focal_len);
    }
    /* display */
    y = scr_vrect.y + scr_vrect.height / 2.0 + 12 * scale;

    if(bar.x_start != bar.x_end){
        Draw_AlphaFill(bar.x_start, y, (bar.x_end - bar.x_start) / scale, 1, BHOP_WHITE, 0.8);
        Draw_AlphaFillRGB(bar.x_start, y + 1 * scale, (bar.x_end - bar.x_start) / scale, 4, BHOP_LGREEN_RGB, 0.5);
        Draw_AlphaFillRGB(bar.x_mid - 1 * scale, y + 1 * scale, 2, 4, BHOP_RED_RGB, 1.0);
        Draw_AlphaFill(bar.x_start, y + 5 * scale, (bar.x_end - bar.x_start) / scale, 1, BHOP_WHITE, 0.8);
    } 
}

void bhop_gather_data(void) {
    if (sv_player->v.health <= 0 || cl.intermission)
        return;

    bhop_data_t * data_frame = malloc(sizeof(bhop_data_t));

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

    data_frame->next = bhop_history;
    bhop_history = data_frame;

    if (show_bhop_histlen.value != 0) 
        bhop_cull_history(bhop_history, (int) show_bhop_histlen.value);
}

void bhop_print_summary(void) {
    bhop_summary_t summary = bhop_get_summary(bhop_history, bhop_histlen(bhop_history));
    Con_Printf(
        "overall bhop summary:\n+forward ground accuracy: %3.1f%%\n+forward overall accuracy: %3.1f%%\nstrafe accuracy: %3.1f\n%%",
        summary.fwd_on_ground_percent, summary.fwd_percent, summary.strafe_percent
    );
}

void bhop_draw_key_graph(bhop_data_t *bhop_history, int window, int x, int y) {
    int ** frames = bhop_key_arrays(bhop_history, window);
    for (int i = 0; i < window; i++){
        Draw_BoxScaledOrigin(x + i, y    , 1, 1, BHOP_WHITE_RGB, 1.0);
        Draw_BoxScaledOrigin(x + i, y + 1, 1, 3, frames[0][i], 1.0);
        Draw_BoxScaledOrigin(x + i, y + 4, 1, 1, BHOP_WHITE_RGB, 1.0);
        Draw_BoxScaledOrigin(x + i, y + 5, 1, 2, frames[1][i], 1.0);
        Draw_BoxScaledOrigin(x + i, y + 7, 1, 2, frames[2][i], 1.0);
        Draw_BoxScaledOrigin(x + i, y + 9, 1, 1, BHOP_WHITE_RGB, 1.0);
    }

    for (int i = 0; i < 3; i++)
        free(frames[i]);
    free(frames);
}

void bhop_draw_speed_graph(bhop_data_t *bhop_history, int window, int x, int y) {
    /* works like the speed bar */
    float * frames = bhop_speed_array(bhop_history, window);
    int colors[4] = {BHOP_GREEN_RGB, BHOP_RED_RGB, BHOP_ORANGE_RGB, BHOP_BLUE_RGB};

    for (int i = window - 1; i >= 0; i--){
        int color = 0;
        float speed = frames[i];

        /* backdrop and borders */
        Draw_BoxScaledOrigin(x + i, y, 1, 1, BHOP_WHITE_RGB, 1.0);
        Draw_BoxScaledOrigin(x + i, y + 1, 1, 32, BHOP_BLACK_RGB, 1.0);
        Draw_BoxScaledOrigin(x + i, y + 33, 1, 1, BHOP_WHITE_RGB, 1.0);

        /* draw the bars */
        while (speed > 0.0){
            if (speed > 320.0)
                Draw_BoxScaledOrigin(x + i, y + 33, 1, -32, colors[color], 1.0);
            else
                Draw_BoxScaledOrigin(x + i, y + 33, 1, -(int)speed / 10, colors[color], 1.0);
            speed -= 320.0;
            color = (color + 1) % 4;
        }
    }

    /* guide lines */
    Draw_BoxScaledOrigin(x, y + 17, 2, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 9 , 1, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x, y + 25, 1, 1, BHOP_WHITE_RGB, 1.0);

    Draw_BoxScaledOrigin(x + window, y + 17, -2, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x + window, y + 9 , -1, 1, BHOP_WHITE_RGB, 1.0);
    Draw_BoxScaledOrigin(x + window, y + 25, -1, 1, BHOP_WHITE_RGB, 1.0);

    free(frames);
}

void bhop_draw_accel_graph(bhop_data_t *bhop_history, int window, int x, int y) {
    float * frames = bhop_speed_array(bhop_history, window);
    float prev_speed = -1.0;

    for (int i = window - 1; i >= 0; i--){
        float speed = frames[i];
        float speed_delta = (prev_speed != -1.0) ? speed - prev_speed : 0.0;

        /* use a logarithm for data presentation: log_2 (|speed|) + 4 = log_2(4 * |speed|) */
        if (fabs(speed_delta) < 1.0 / 16.0)
            speed_delta = 0.0;
        else
            speed_delta = copysign(min(log2f(16 * fabs(speed_delta)), 9.0), speed_delta);

        /* backdrop and borders */
        Draw_BoxScaledOrigin(x + i, y, 1, 1, BHOP_WHITE_RGB, 1.0);
        Draw_BoxScaledOrigin(x + i, y + 1, 1, 17, BHOP_BLACK_RGB, 1.0);
        Draw_BoxScaledOrigin(x + i, y + 18, 1, 1, BHOP_WHITE_RGB, 1.0);

        /* draw the bars */
        if (speed_delta > 0.0)
            Draw_BoxScaledOrigin(x + i, y + 9, 1, (int) speed_delta, BHOP_RED_RGB, 1.0);
        else
            Draw_BoxScaledOrigin(x + i, y + 10, 1, (int) speed_delta, BHOP_GREEN_RGB, 1.0);

        /* guide line */
        Draw_BoxScaledOrigin(x + i, y + 9, 1, 1, BHOP_WHITE_RGB, 1.0);

        /* set for next iteration */
        prev_speed = frames[i];
    }
    free(frames);
}

/*
 * draw squares representing frames around the last grounding, forward taps.
 */
void bhop_draw_crosshair_squares(bhop_data_t *history, int x, int y) {
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
            if (i - count == 0){ /* central box */
                Draw_BoxScaledOrigin(x - 1, y + j - 1, 1, 6, BHOP_ORANGE_RGB, alpha);
                Draw_BoxScaledOrigin(x + 4, y + j - 1, 1, 6, BHOP_ORANGE_RGB, alpha);
                Draw_BoxScaledOrigin(x    , y + j - 1, 4, 1, BHOP_ORANGE_RGB, alpha);
                Draw_BoxScaledOrigin(x    , y + j + 4, 4, 1, BHOP_ORANGE_RGB, alpha);
            } else { /* other boxes */
                if (i - count < 0)
                    Draw_BoxScaledOrigin(x + 4, y + j - 1, 1, 6, BHOP_WHITE_RGB, alpha);
                else
                    Draw_BoxScaledOrigin(x - 1, y + j - 1, 1, 6, BHOP_WHITE_RGB, alpha);
                Draw_BoxScaledOrigin(x, y + j - 1, 4, 1, BHOP_WHITE_RGB, alpha);
                Draw_BoxScaledOrigin(x, y + j + 4, 4, 1, BHOP_WHITE_RGB, alpha);
            }
            Draw_BoxScaledOrigin(x, y, 4, 4, bhop_get_fwd_color(history), alpha);
            x -= 5;
        }

        i++;
        history = history->next;
    }
}

void bhop_draw_crosshair_gain(bhop_data_t *history, int x, int y, int scale, int charsize) {
    bhop_data_t * history_it = history;
    char str[20];
    int i = 0, j = 0;

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


void BHOP_Init (void) {
    bhop_history = NULL;
	Cvar_Register (&show_bhop_stats);
	Cvar_Register (&show_bhop_stats_x);
	Cvar_Register (&show_bhop_stats_y);
	Cvar_Register (&show_bhop_histlen);
	Cvar_Register (&show_bhop_window);
	Cvar_Register (&show_bhop_frames);
}

void BHOP_Start (void) {
    bhop_history = NULL;
    lastangle = 0;
    last_ground_speed = 0;
}

void BHOP_Stop (void) {
    bhop_print_summary();
    bhop_cull_history(bhop_history, 0);
    bhop_history = NULL;
}

void BHOP_Shutdown (void) {
    BHOP_Stop();
}

void SCR_DrawBHOP (void)
{
    int     x, y;
    int     charsize;
    int     scale;
    char    str[80];
    static int window;

    x = ELEMENT_X_COORD(show_bhop_stats);
    y = ELEMENT_Y_COORD(show_bhop_stats);

    charsize = Sbar_GetScaledCharacterSize();
    scale = Sbar_GetScaleAmount();

    if (!sv.active && !cls.demoplayback) return;

    window = (int)show_bhop_window.value;
    bhop_gather_data();

    if((int)show_bhop_stats.value & BHOP_AVG_SPEED) {
        Q_snprintfz (str, sizeof(str), "avg_speed %3.1f", bhop_speed_avg(bhop_history, window));
        Draw_String (x, y, str, true);
        y += charsize;
    }

    if((int)show_bhop_stats.value & BHOP_KEYPRESSES) {
        bhop_summary_t summary = bhop_get_summary(bhop_history, window);
        Q_snprintfz (
            str, sizeof(str), "f%% %03.f/%03.f s%% %03.f", 
            summary.fwd_on_ground_percent, 
            summary.fwd_percent, 
            summary.strafe_percent
        );
        Draw_String (x, y, str, true);
        y += charsize;
    }

    if((int)show_bhop_stats.value & BHOP_FORWARD_HISTORY_GRAPH) {
        bhop_draw_key_graph(bhop_history, window, x / scale, y / scale);
        y += 12 * scale;
    }

    if((int)show_bhop_stats.value & BHOP_SPEED_HISTORY_GRAPH) {
        bhop_draw_speed_graph(bhop_history, window, x / scale, y / scale);
        y += 36 * scale;
    }

    if((int)show_bhop_stats.value & BHOP_DSPEED_HISTORY_GRAPH) {
        /* candlestick graph */
        bhop_draw_accel_graph(bhop_history, window, x / scale, y / scale);
        y += 20 * scale;
    }

    if((int)show_bhop_stats.value & BHOP_ANGLE_MARK) {
        bhop_draw_angle_mark(bhop_history, scale);
    }

    if((int)show_bhop_stats.value & BHOP_CROSSHAIR_INFO) {
        x = bhop_inverse_scale(scr_vrect.x + scr_vrect.width / 2.0);
        y = bhop_inverse_scale(scr_vrect.y + scr_vrect.height / 2.0);

        bhop_draw_crosshair_squares(bhop_history, x - 2 + (show_bhop_frames.value - 1) * 5, y - 12);

        bhop_draw_crosshair_gain(bhop_history, x, y - 24, scale, charsize);
        // draw optimal speed loss/gain on ground
    }

    //if((int)show_bhop_stats.value & BHOP_ANGLE_MARK) {
    //    bhop_draw_ground_mark(bhop_history);       
    //}
}
