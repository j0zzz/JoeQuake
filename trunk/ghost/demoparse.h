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


#ifndef __DEMOPARSE_H
#define __DEMOPARSE_H

#ifndef QUAKE_GAME
#include "../quakedef.h"
#endif


#define DP_FOREACH_ERR(F)          \
    F(DP_ERR_SUCCESS)              \
    F(DP_ERR_UNEXPECTED_END)       \
    F(DP_ERR_PROTOCOL_NOT_SET)     \
    F(DP_ERR_STRING_TOO_LONG)      \
    F(DP_ERR_CALLBACK_STOP)        \
    F(DP_ERR_CALLBACK_SKIP_PACKET) \
    F(DP_ERR_BAD_SIZE)             \
    F(DP_ERR_INVALID_VERSION)      \
    F(DP_ERR_UNKNOWN_MESSAGE_TYPE) \
    F(DP_ERR_INVALID_CB_RESPONSE)  \
    F(DP_ERR_READ_FAILED)

#define DP_GENERATE_ENUM(ENUM) ENUM,
#define DP_GENERATE_STRING(STRING) #STRING,


typedef enum {
    DP_FOREACH_ERR(DP_GENERATE_ENUM)
} dp_err_t;


typedef enum {
    DP_CBR_CONTINUE,
    DP_CBR_SKIP_PACKET,
    DP_CBR_STOP,
} dp_cb_response_t;


typedef struct {
    qboolean (*read)(void *dest, unsigned int size, void *ctx);

    dp_cb_response_t (*server_info)(int protocol, unsigned int protocol_flags,
                                    const char *level_name, void *ctx);
    dp_cb_response_t (*server_info_model)(const char *level_name, void *ctx);
    dp_cb_response_t (*server_info_sound)(const char *level_name, void *ctx);
    dp_cb_response_t (*time)(float time, void *ctx);
    dp_cb_response_t (*baseline)(int entity_num, vec3_t origin, vec3_t angle,
                                 int frame, void *ctx);
    dp_cb_response_t (*update)(int entity_num, vec3_t origin, vec3_t angle,
                               byte origin_bits, byte angle_bits, int frame,
                               void *ctx);
    dp_cb_response_t (*packet_end)(void *ctx);
    dp_cb_response_t (*set_view)(int entity_num, void *ctx);
    dp_cb_response_t (*intermission)(void *ctx);
    dp_cb_response_t (*finale)(void *ctx);
    dp_cb_response_t (*cut_scene)(void *ctx);
    dp_cb_response_t (*disconnect)(void *ctx);
    dp_cb_response_t (*update_stat)(byte stat, int count, void *ctx);
    dp_cb_response_t (*killed_monster)(void *ctx);
    dp_cb_response_t (*found_secret)(void *ctx);
    dp_cb_response_t (*update_name)(int client_num, const char *name,
                                    void *ctx);
    dp_cb_response_t (*stuff_text)(const char *string, void *ctx);
} dp_callbacks_t;


dp_err_t DP_ReadDemo(dp_callbacks_t *callbacks, void *callback_ctx);


#endif /* __DEMOPARSE_H */
