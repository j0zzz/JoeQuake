/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
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


#include "../quakedef.h"
#include "demoparse.h"


#define EXCEPTION(errno)    (ctx->err_line = __LINE__, (errno))

#define CHECK_RC(expr)                \
    do {                              \
        dp_err_t __rc;                \
        __rc = (expr);                \
        if (__rc != DP_ERR_SUCCESS) { \
            return __rc;              \
        }                             \
    } while(0)

#define CALL_CALLBACK_CTX(cb_name, ...)                               \
    do {                                                              \
        if (ctx->callbacks->cb_name != NULL) {                        \
            dp_cb_response_t __resp;                                  \
            __resp = ctx->callbacks->cb_name(__VA_ARGS__);            \
            if (__resp != DP_CBR_CONTINUE) {                          \
                if (__resp == DP_CBR_SKIP_PACKET) {                   \
                    return EXCEPTION(DP_ERR_CALLBACK_SKIP_PACKET);    \
                } else if (__resp == DP_CBR_STOP) {                   \
                    return EXCEPTION(DP_ERR_CALLBACK_STOP);           \
                } else {                                              \
                    return EXCEPTION(DP_ERR_INVALID_CB_RESPONSE);     \
                }                                                     \
            }                                                         \
        }                                                             \
    } while(0)

#define CALL_CALLBACK(cb_name, ...) \
    CALL_CALLBACK_CTX(cb_name, __VA_ARGS__, ctx->callback_ctx)

// Extra definition because of awkward comma handling.  In theory we could use
// __VA_OPT__(,) if everything supports it?
#define CALL_CALLBACK_NO_ARGS(cb_name)  \
    CALL_CALLBACK_CTX(cb_name, ctx->callback_ctx)



typedef struct {
    int version;
    int flags;
} protocol_t;


typedef struct {
    const byte *buf;
    unsigned long file_offset;
    byte packet[64000];
    const byte *packet_end;

    protocol_t protocol;
    qboolean connected;

    dp_callbacks_t *callbacks;
    void *callback_ctx;

    int err_line;
} ctx_t;


static dp_err_t
DP_Read(ctx_t *ctx, unsigned int size, void *dest)
{
    if (ctx->buf + size > ctx->packet_end) {
        return EXCEPTION(DP_ERR_UNEXPECTED_END);
    }

    if (dest != NULL) {
        memcpy(dest, ctx->buf, size);
    }

    ctx->buf += size;
    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseFloat(ctx_t *ctx, float *out)
{
    CHECK_RC(DP_Read(ctx, sizeof(*out), out));
    if (out != NULL) {
        *out = LittleFloat(*out);
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseByte(ctx_t *ctx, byte *out)
{
    CHECK_RC(DP_Read(ctx, sizeof(*out), out));

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseChar(ctx_t *ctx, char *out)
{
    CHECK_RC(DP_Read(ctx, sizeof(*out), out));

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseShort(ctx_t *ctx, short *out)
{
    CHECK_RC(DP_Read(ctx, sizeof(*out), out));
    if (out != NULL) {
        *out = LittleShort(*out);
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseUnsignedShort(ctx_t *ctx, unsigned short *out)
{
    CHECK_RC(DP_Read(ctx, sizeof(*out), out));
    if (out != NULL) {
        *out = LittleShort(*out);
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseLong(ctx_t *ctx, int *out)
{
    CHECK_RC(DP_Read(ctx, sizeof(*out), out));
    if (out != NULL) {
        *out = LittleLong(*out);
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseCoord(ctx_t *ctx, float *out)
{
    if (ctx->protocol.flags == -1) {
        return EXCEPTION(DP_ERR_PROTOCOL_NOT_SET);
    }
    if (ctx->protocol.flags & PRFL_FLOATCOORD) {
        CHECK_RC(DP_ParseFloat(ctx, out));
    } else if (ctx->protocol.flags & PRFL_INT32COORD) {
        int l;
        CHECK_RC(DP_ParseLong(ctx, &l));
        if (out != NULL) {
            *out = l / 16.0f;
        }
    } else if (ctx->protocol.flags & PRFL_24BITCOORD) {
        short s;
        byte b;
        CHECK_RC(DP_ParseShort(ctx, &s));
        CHECK_RC(DP_ParseByte(ctx, &b));
        if (out != NULL) {
            *out = s + b / 255.0f;
        }
    } else {
        short s;
        CHECK_RC(DP_ParseShort(ctx, &s));
        if (out != NULL) {
            *out = s / 8.0f;
        }
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseCoords(ctx_t *ctx, vec3_t out)
{
    int i;

    for (i = 0; i < 3; i ++) {
        CHECK_RC(DP_ParseCoord(ctx, (out != NULL) ? (out + i) : NULL));
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseAngle(ctx_t *ctx, float *out)
{
    if (ctx->protocol.flags == -1) {
        return EXCEPTION(DP_ERR_PROTOCOL_NOT_SET);
    }
    if (ctx->protocol.flags & PRFL_FLOATANGLE) {
        CHECK_RC(DP_ParseFloat(ctx, out));
    } else if (ctx->protocol.flags & PRFL_SHORTANGLE) {
        short s;
        CHECK_RC(DP_ParseShort(ctx, &s));
        if (out != NULL) {
            *out = s * 360.0f / 65536.0f;
        }
    } else {
        char c;
        CHECK_RC(DP_ParseChar(ctx, &c));
        if (out != NULL) {
            *out = c * 360.0f / 256.0f;
        }
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseAngles(ctx_t *ctx, vec3_t out)
{
    int i;

    for (i = 0; i < 3; i ++) {
        CHECK_RC(DP_ParseAngle(ctx, (out != NULL) ? (out + i) : NULL));
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseString(ctx_t *ctx, char *s, int buf_size, int *len_out)
{
    int len = 0;
    char c;

    CHECK_RC(DP_ParseChar(ctx, &c));
    while (c != -1 && c != 0) {
        if (s != NULL) {
            if (len >= buf_size - 1) {
                return EXCEPTION(DP_ERR_STRING_TOO_LONG);
            }
            s[len] = c;
        }
        len ++;
        CHECK_RC(DP_ParseChar(ctx, &c));
    }

    if (s != NULL) {
        s[len] = '\0';
    }

    if (len_out != NULL) {
        *len_out = len;
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseServerInfo(ctx_t *ctx)
{
    char level_name[128];
    char model[64];
    char sound[64];
    int len;

    CHECK_RC(DP_ParseLong(ctx, &ctx->protocol.version));

    if (ctx->protocol.version == PROTOCOL_RMQ) {
        CHECK_RC(DP_ParseLong(ctx, &ctx->protocol.flags));
    } else {
        ctx->protocol.flags = 0;
    }

    CHECK_RC(DP_ParseByte(ctx, NULL));  // max clients
    CHECK_RC(DP_ParseByte(ctx, NULL));  // game type
    CHECK_RC(DP_ParseString(ctx, level_name, sizeof(level_name), NULL));

    // model precache
    CHECK_RC(DP_ParseString(ctx, model, sizeof(model), &len));
    while (len != 0) {
        CALL_CALLBACK(server_info_model, model);
        CHECK_RC(DP_ParseString(ctx, model, sizeof(model), &len));
    }

    // sound precache
    CHECK_RC(DP_ParseString(ctx, sound, sizeof(sound), &len));
    while (len != 0) {
        CALL_CALLBACK(server_info_sound, sound);
        CHECK_RC(DP_ParseString(ctx, sound, sizeof(sound), &len));
    }

    CALL_CALLBACK(server_info, ctx->protocol.version, ctx->protocol.flags,
                  level_name);

    return DP_ERR_SUCCESS;

}


static dp_err_t
DP_ParseSoundStart(ctx_t *ctx)
{
    byte field_mask;

    CHECK_RC(DP_ParseByte(ctx, &field_mask));

    if (field_mask & SND_VOLUME) {
        CHECK_RC(DP_ParseByte(ctx, NULL));
    }
    if (field_mask & SND_ATTENUATION) {
        CHECK_RC(DP_ParseByte(ctx, NULL));
    }

    if (field_mask & SND_LARGEENTITY) {
        CHECK_RC(DP_ParseShort(ctx, NULL));     // entity id
        CHECK_RC(DP_ParseByte(ctx, NULL));      // channel
    } else {
        CHECK_RC(DP_ParseShort(ctx, NULL));     // entity+channel
    }

    if (field_mask & SND_LARGESOUND) {
        CHECK_RC(DP_ParseUnsignedShort(ctx, NULL));
    } else {
        CHECK_RC(DP_ParseByte(ctx, NULL));
    }

    CHECK_RC(DP_ParseCoords(ctx, NULL));        // position

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseTempEntity(ctx_t *ctx)
{
    byte type;

    CHECK_RC(DP_ParseByte(ctx, &type));

    switch (type) {
        case TE_LIGHTNING1:
        case TE_LIGHTNING2:
        case TE_LIGHTNING3:
        case TE_BEAM:
            CHECK_RC(DP_ParseShort(ctx, NULL));   // entity
            CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
            CHECK_RC(DP_ParseCoords(ctx, NULL));  // end
            break;
        case TE_EXPLOSION2:
            CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
            CHECK_RC(DP_ParseByte(ctx, NULL));    // color start
            CHECK_RC(DP_ParseByte(ctx, NULL));    // color length
            break;
        default:
            CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
            break;
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseBaseline(ctx_t *ctx, qboolean static_, int version)
{
    short entity_num;
    int frame;
    int model;
    byte bits = 0;
    vec3_t origin, angle;
    int i;

    if (!static_) {
        CHECK_RC(DP_ParseShort(ctx, &entity_num));
    }

    if (version == 2) {
        CHECK_RC(DP_ParseByte(ctx, &bits));
    }

    if (bits & B_LARGEMODEL) {
        short model_s;
        CHECK_RC(DP_ParseShort(ctx, &model_s));
        model = model_s;
    } else {
        byte model_b;
        CHECK_RC(DP_ParseByte(ctx, &model_b));
        model = model_b;
    }

    if (bits & B_LARGEFRAME) {
        short frame_s;
        CHECK_RC(DP_ParseShort(ctx, &frame_s));
        frame = frame_s;
    } else {
        byte frame_b;
        CHECK_RC(DP_ParseByte(ctx, &frame_b));
        frame = frame_b;
    }

    CHECK_RC(DP_ParseByte(ctx, NULL));  // colormap
    CHECK_RC(DP_ParseByte(ctx, NULL));  // skin

    for (i = 0; i < 3; i ++) {
        CHECK_RC(DP_ParseCoord(ctx, &origin[i]));
        CHECK_RC(DP_ParseAngle(ctx, &angle[i]));
    }

    if (bits & B_ALPHA) {
        CHECK_RC(DP_ParseByte(ctx, NULL));  // alpha
    }

    if (!static_) {
        CALL_CALLBACK(baseline, entity_num, origin, angle, frame, model);
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseUpdateFlags(ctx_t *ctx, byte base_flags, unsigned int *flags_out)
{
    byte more_flags, extend1_flags, extend2_flags;
    unsigned int flags = base_flags;

    if (flags & U_MOREBITS) {
        CHECK_RC(DP_ParseByte(ctx, &more_flags));
        flags |= more_flags << 8;
    }

    if (ctx->protocol.version != PROTOCOL_NETQUAKE) {
        if (flags & U_EXTEND1) {
            CHECK_RC(DP_ParseByte(ctx, &extend1_flags));
            flags |= extend1_flags << 16;
        }
        if (flags & U_EXTEND2) {
            CHECK_RC(DP_ParseByte(ctx, &extend2_flags));
            flags |= extend2_flags << 24;
        }
    }

    if (flags_out != NULL) {
        *flags_out = flags;
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseUpdate(ctx_t *ctx, byte base_flags)
{
    unsigned int flags;
    int entity_num;
    int frame = -1;
    int model = -1;
    vec3_t origin;
    vec3_t angle;
    byte origin_bits = 0;
    byte angle_bits = 0;

    VectorClear(origin);
    VectorClear(angle);

    CHECK_RC(DP_ParseUpdateFlags(ctx, base_flags, &flags));

    if (flags & U_LONGENTITY) {
        short entity_num_s;
        CHECK_RC(DP_ParseShort(ctx, &entity_num_s));
        entity_num = entity_num_s;
    } else {
        byte entity_num_b;
        CHECK_RC(DP_ParseByte(ctx, &entity_num_b));
        entity_num = entity_num_b;
    }
    if (flags & U_MODEL) {
        byte model_b;
        CHECK_RC(DP_ParseByte(ctx, &model_b));
        model = model_b;
    }
    if (flags & U_FRAME) {
        byte frame_b;
        CHECK_RC(DP_ParseByte(ctx, &frame_b));
        frame = frame_b;
    }
    if (flags & U_COLORMAP) {
        CHECK_RC(DP_ParseByte(ctx, NULL));  // colormap
    }
    if (flags & U_SKIN) {
        CHECK_RC(DP_ParseByte(ctx, NULL));  // skin
    }
    if (flags & U_EFFECTS) {
        CHECK_RC(DP_ParseByte(ctx, NULL));  // effects
    }

    if (flags & U_ORIGIN1) {
        CHECK_RC(DP_ParseCoord(ctx, &origin[0]));
        origin_bits |= (1 << 0);
    }
    if (flags & U_ANGLE1) {
        CHECK_RC(DP_ParseAngle(ctx, &angle[0]));
        angle_bits |= (1 << 0);
    }
    if (flags & U_ORIGIN2) {
        CHECK_RC(DP_ParseCoord(ctx, &origin[1]));
        origin_bits |= (1 << 1);
    }
    if (flags & U_ANGLE2) {
        CHECK_RC(DP_ParseAngle(ctx, &angle[1]));
        angle_bits |= (1 << 1);
    }
    if (flags & U_ORIGIN3) {
        CHECK_RC(DP_ParseCoord(ctx, &origin[2]));
        origin_bits |= (1 << 2);
    }
    if (flags & U_ANGLE3) {
        CHECK_RC(DP_ParseAngle(ctx, &angle[2]));
        angle_bits |= (1 << 2);
    }

    if (ctx->protocol.version == PROTOCOL_FITZQUAKE
        || ctx->protocol.version == PROTOCOL_RMQ)
    {
        if (flags & U_ALPHA) {
            CHECK_RC(DP_ParseByte(ctx, NULL));  // alpha
        }
        if (flags & U_SCALE) {
            CHECK_RC(DP_ParseByte(ctx, NULL));  // scale
        }
        if (flags & U_FRAME2) {
            byte frame_upper;
            CHECK_RC(DP_ParseByte(ctx, &frame_upper));
            frame |= frame_upper << 8;
        }
        if (flags & U_MODEL2) {
            byte model_upper;
            CHECK_RC(DP_ParseByte(ctx, &model_upper));
            model |= model_upper << 8;
        }
        if (flags & U_LERPFINISH) {
            CHECK_RC(DP_ParseByte(ctx, NULL));  // lerp finish
        }
    } else if (ctx->protocol.version == PROTOCOL_NETQUAKE
               && (flags & U_TRANS)) {
        float a;
        CHECK_RC(DP_ParseFloat(ctx, &a));
        CHECK_RC(DP_ParseFloat(ctx, NULL));
        if (a == 2) {
            CHECK_RC(DP_ParseFloat(ctx, NULL));
        }
    }

    CALL_CALLBACK(update, entity_num, origin, angle, origin_bits, angle_bits,
                  frame, model);

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseClientDataFlags(ctx_t *ctx, unsigned int *flags_out)
{
    unsigned short base_flags;
    byte extend1_flags, extend2_flags;
    unsigned int flags = 0;

    CHECK_RC(DP_ParseUnsignedShort(ctx, &base_flags));
    flags |= base_flags;

    if (flags & U_EXTEND1) {
        CHECK_RC(DP_ParseByte(ctx, &extend1_flags));
        flags |= extend1_flags << 16;
    }
    if (flags & U_EXTEND2) {
        CHECK_RC(DP_ParseByte(ctx, &extend2_flags));
        flags |= extend2_flags << 24;
    }

    if (flags_out != NULL) {
        *flags_out = flags;
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseClientData(ctx_t *ctx)
{
    unsigned int flags;
    int skip;

    CHECK_RC(DP_ParseClientDataFlags(ctx, &flags));

    skip = ((flags & SU_VIEWHEIGHT) != 0)
           + ((flags & SU_IDEALPITCH) != 0)
           + ((flags & SU_PUNCH1) != 0)
           + ((flags & SU_PUNCH2) != 0)
           + ((flags & SU_PUNCH3) != 0)
           + ((flags & SU_VELOCITY1) != 0)
           + ((flags & SU_VELOCITY2) != 0)
           + ((flags & SU_VELOCITY3) != 0)
           + 4  // items
           + ((flags & SU_WEAPONFRAME) != 0)
           + ((flags & SU_ARMOR) != 0)
           + ((flags & SU_WEAPON) != 0)
           + 2 + 6  // health,ammo,shells,nails,rockets,cells,active_weapon
           + ((flags & SU_WEAPON2) != 0)
           + ((flags & SU_ARMOR2) != 0)
           + ((flags & SU_AMMO2) != 0)
           + ((flags & SU_SHELLS2) != 0)
           + ((flags & SU_NAILS2) != 0)
           + ((flags & SU_ROCKETS2) != 0)
           + ((flags & SU_CELLS2) != 0)
           + ((flags & SU_WEAPONFRAME2) != 0)
           + ((flags & SU_WEAPONALPHA) != 0);

    CHECK_RC(DP_Read(ctx, skip, NULL));

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseTime(ctx_t *ctx)
{
    float time;

    CHECK_RC(DP_ParseFloat(ctx, &time));
    CALL_CALLBACK(time, time);

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseVersion(ctx_t *ctx)
{
    CHECK_RC(DP_ParseLong(ctx, &ctx->protocol.version));

    if (ctx->protocol.version != PROTOCOL_NETQUAKE
            && ctx->protocol.version != PROTOCOL_FITZQUAKE
            && ctx->protocol.version != PROTOCOL_RMQ) {
        return EXCEPTION(DP_ERR_INVALID_VERSION);
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_SetView(ctx_t *ctx)
{
    short entity_num;

    CHECK_RC(DP_ParseShort(ctx, &entity_num));
    CALL_CALLBACK(set_view, entity_num);

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseLocalSound(ctx_t *ctx)
{
    byte flags;

    CHECK_RC(DP_ParseByte(ctx, &flags));

    if (flags & SND_LARGESOUND) {
        CHECK_RC(DP_ParseShort(ctx, NULL));    // sound num
    } else {
        CHECK_RC(DP_ParseByte(ctx, NULL));     // sound num
    }

    return DP_ERR_SUCCESS;
}

static dp_err_t
DP_UpdateStat(ctx_t *ctx)
{
    byte stat;
    int count;

    CHECK_RC(DP_ParseByte(ctx, &stat));
    CHECK_RC(DP_ParseLong(ctx, &count));

    CALL_CALLBACK(update_stat, stat, count);

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseUpdateName(ctx_t *ctx)
{
    byte client_num;
    char name[MAX_SCOREBOARDNAME];

    CHECK_RC(DP_ParseByte(ctx, &client_num));
    CHECK_RC(DP_ParseString(ctx, name, sizeof(name), NULL));

    CALL_CALLBACK(update_name, client_num, name);

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseStuffText(ctx_t *ctx)
{
    char string[2048];

    CHECK_RC(DP_ParseString(ctx, string, sizeof(string), NULL));
    CALL_CALLBACK(stuff_text, string);

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ParseMessage(ctx_t *ctx)
{
    byte cmd;

    CHECK_RC(DP_ParseByte(ctx, &cmd));

    if (cmd & U_SIGNAL) {
        CHECK_RC(DP_ParseUpdate(ctx, cmd & 127));
    } else {
        switch (cmd) {
            case svc_time:
                CHECK_RC(DP_ParseTime(ctx));
                break;
            case svc_nop:
            case svc_sellscreen:
            case svc_bf:
                break;
            case svc_clientdata:
                CHECK_RC(DP_ParseClientData(ctx));
                break;
            case svc_version:
                CHECK_RC(DP_ParseVersion(ctx));
                break;
            case svc_print:
            case svc_centerprint:
            case svc_skybox:
                CHECK_RC(DP_ParseString(ctx, NULL, 0, NULL));
                break;
            case svc_stufftext:
                CHECK_RC(DP_ParseStuffText(ctx));
                break;
            case svc_damage:
                CHECK_RC(DP_ParseByte(ctx, NULL));    // armor
                CHECK_RC(DP_ParseByte(ctx, NULL));    // blood
                CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
                break;
            case svc_serverinfo:
                CHECK_RC(DP_ParseServerInfo(ctx));
                break;
            case svc_setangle:
                CHECK_RC(DP_ParseAngles(ctx, NULL));
                break;
            case svc_setview:
                CHECK_RC(DP_SetView(ctx));
                break;
            case svc_lightstyle:
                CHECK_RC(DP_ParseByte(ctx, NULL));
                CHECK_RC(DP_ParseString(ctx, NULL, 0, NULL));
                break;
            case svc_sound:
                CHECK_RC(DP_ParseSoundStart(ctx));
                break;
            case svc_stopsound:
                CHECK_RC(DP_ParseShort(ctx, NULL));
                break;
            case svc_updatename:
                CHECK_RC(DP_ParseUpdateName(ctx));
                break;
            case svc_updatefrags:
                CHECK_RC(DP_ParseByte(ctx, NULL));    // entity
                CHECK_RC(DP_ParseShort(ctx, NULL));   // frags
                break;
            case svc_updatecolors:
                CHECK_RC(DP_ParseByte(ctx, NULL));    // entity
                CHECK_RC(DP_ParseByte(ctx, NULL));    // colors
                break;
            case svc_particle:
                CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
                CHECK_RC(DP_Read(ctx, 3, NULL));      // dir
                CHECK_RC(DP_ParseByte(ctx, NULL));    // msg count
                CHECK_RC(DP_ParseByte(ctx, NULL));    // color
                break;
            case svc_spawnbaseline:
                CHECK_RC(DP_ParseBaseline(ctx, false, 1));
                break;
            case svc_spawnstatic:
                CHECK_RC(DP_ParseBaseline(ctx, true, 1));
                break;
            case svc_temp_entity:
                CHECK_RC(DP_ParseTempEntity(ctx));
                break;
            case svc_setpause:
                CHECK_RC(DP_ParseByte(ctx, NULL));
                break;
            case svc_signonnum:
                CHECK_RC(DP_ParseByte(ctx, NULL));
                break;
            case svc_updatestat:
                CHECK_RC(DP_UpdateStat(ctx));
                break;
            case svc_spawnstaticsound:
                CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
                CHECK_RC(DP_ParseByte(ctx, NULL));    // sound num
                CHECK_RC(DP_ParseByte(ctx, NULL));    // volume
                CHECK_RC(DP_ParseByte(ctx, NULL));    // atten
                break;
            case svc_cdtrack:
                CHECK_RC(DP_ParseByte(ctx, NULL));    // cd track
                CHECK_RC(DP_ParseByte(ctx, NULL));    // loop
                break;
            case svc_fog:
                CHECK_RC(DP_ParseByte(ctx, NULL));    // density
                CHECK_RC(DP_ParseByte(ctx, NULL));    // red
                CHECK_RC(DP_ParseByte(ctx, NULL));    // green
                CHECK_RC(DP_ParseByte(ctx, NULL));    // blue
                CHECK_RC(DP_ParseShort(ctx, NULL));   // time
                break;
            case svc_spawnbaseline2:
                CHECK_RC(DP_ParseBaseline(ctx, false, 2));
                break;
            case svc_spawnstatic2:
                CHECK_RC(DP_ParseBaseline(ctx, true, 2));
                break;
            case svc_spawnstaticsound2:
                CHECK_RC(DP_ParseCoords(ctx, NULL));  // origin
                CHECK_RC(DP_ParseShort(ctx, NULL));   // sound num
                CHECK_RC(DP_ParseByte(ctx, NULL));    // volume
                CHECK_RC(DP_ParseByte(ctx, NULL));    // atten
                break;
            case svc_intermission:
                CALL_CALLBACK_NO_ARGS(intermission);
                break;
            case svc_finale:
                CHECK_RC(DP_ParseString(ctx, NULL, 0, NULL));
                CALL_CALLBACK_NO_ARGS(finale);
                break;
            case svc_cutscene:
                CHECK_RC(DP_ParseString(ctx, NULL, 0, NULL));
                CALL_CALLBACK_NO_ARGS(cut_scene);
                break;
            case svc_disconnect:
                CALL_CALLBACK_NO_ARGS(disconnect);
                // Stop parsing at a disconnect
                ctx->connected = false;
                break;
            case svc_killedmonster:
                CALL_CALLBACK_NO_ARGS(killed_monster);
                break;
            case svc_foundsecret:
                CALL_CALLBACK_NO_ARGS(found_secret);
                break;
            case svc_achievement:
                CHECK_RC(DP_ParseString(ctx, NULL, 0, NULL));
                break;
            case svc_localsound:
                CHECK_RC(DP_ParseLocalSound(ctx));
                break;

            default:
                return EXCEPTION(DP_ERR_UNKNOWN_MESSAGE_TYPE);
        }
    }

    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ReadFromFile(ctx_t *ctx, void *dest, unsigned int len)
{
    if (!ctx->callbacks->read(dest, len, ctx->callback_ctx)) {
        return EXCEPTION(DP_ERR_READ_FAILED);
    }
    ctx->file_offset += len;
    return DP_ERR_SUCCESS;
}


static dp_err_t
DP_ReadPacket(ctx_t *ctx) {
    dp_err_t rc;
    int packet_len;
    vec3_t view_angle;

    // Read packet header and packet
    CHECK_RC(DP_ReadFromFile(ctx, &packet_len, sizeof(packet_len)));
    packet_len = LittleLong(packet_len);
    if (packet_len < 0 || packet_len > sizeof(ctx->packet)) {
        return EXCEPTION(DP_ERR_BAD_SIZE);
    }
    CHECK_RC(DP_ReadFromFile(ctx, view_angle, sizeof(view_angle)));
    CHECK_RC(DP_ReadFromFile(ctx, ctx->packet, packet_len));

    // Read messages from the packet.
    ctx->buf = ctx->packet;
    ctx->packet_end = ctx->packet + packet_len;
    while (ctx->buf < ctx->packet_end) {
        rc = DP_ParseMessage(ctx);
        if (rc == DP_ERR_CALLBACK_SKIP_PACKET) {
            ctx->buf = ctx->packet_end;
        } else {
            CHECK_RC(rc);
        }
    }

    CALL_CALLBACK_NO_ARGS(packet_end);

    return DP_ERR_SUCCESS;
}


// Parse demo, but don't handle errors
static dp_err_t
DP_ReadDemo_NoHandle(ctx_t *ctx)
{
    char c = '\0';

    // Skip the force track command.
    while (c != '\n') {
        CHECK_RC(DP_ReadFromFile(ctx, &c, sizeof(c)));
    }

    // The rest of the demo file is a sequence of packets.
    while (ctx->connected) {
        CHECK_RC(DP_ReadPacket(ctx));
    }

    return DP_ERR_SUCCESS;
}


static const char *dp_err_strings[] = {
    DP_FOREACH_ERR(DP_GENERATE_STRING)
};


dp_err_t
DP_ReadDemo(dp_callbacks_t *callbacks, void *callback_ctx)
{
    dp_err_t rc;
    ctx_t ctx = {
        .file_offset = 0,
        .protocol = {-1, -1},
        .connected = true,
        .callbacks = callbacks,
        .callback_ctx = callback_ctx,
    };

    rc = DP_ReadDemo_NoHandle(&ctx);
    if (rc == DP_ERR_READ_FAILED) {
        // Quake silently ignores truncated files --- see CL_GetDemoMessage
        // handling of fread().  Do the same here.
        rc = DP_ERR_SUCCESS;
    }

    if (rc != DP_ERR_SUCCESS && rc != DP_ERR_CALLBACK_STOP) {
        unsigned long offs = ctx.file_offset;
        if (ctx.packet_end != NULL) {
            offs -= ctx.packet_end - ctx.buf;
        }
        fprintf(stderr, "Demo parse failed at offset %ld (0x%lx), "
                "line %d: %s\n",
                offs, offs, ctx.err_line, dp_err_strings[rc]);
    }

    return rc;
}
