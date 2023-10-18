#include "quakedef.h"
#include "demoparse.h"


typedef struct
{
    long packet_offset;
    FILE *demo_file;

    dseek_info_t *dseek_info;
} dseek_ctx_t;


static dp_cb_response_t
DSeek_PacketStart_cb (void *ctx)
{
    dseek_ctx_t *pctx = ctx;

    pctx->packet_offset = ftell(pctx->demo_file);

    return DP_CBR_CONTINUE;
}


static qboolean
DSeek_Read_cb (void *dest, unsigned int size, void *ctx)
{
    dseek_ctx_t *pctx = ctx;
    return fread (dest, size, 1, pctx->demo_file) != 0;
}


static dp_cb_response_t
DSeek_ServerInfoModel_cb (const char *model, void *ctx)
{
    dseek_ctx_t *pctx = ctx;
    dseek_info_t *dsi = pctx->dseek_info;
    dseek_map_info_t *dsmi;

    if (dsi->num_maps < DSEEK_MAX_MAPS) {
        dsmi = &dsi->maps[dsi->num_maps];
        Q_strncpyz(dsmi->name, (char *)model, DSEEK_MAP_NAME_SIZE);
        COM_StripExtension(COM_SkipPath(dsmi->name), dsmi->name);
        dsmi->offset = pctx->packet_offset;
        dsmi->min_time = -1.0f;     // updated in `DSeek_Time_cb`.
        dsmi->finish_time = -1.0f;  // updated in `DSeek_Intermission_cb`.
        dsmi->max_time = -1.0f;     // updated in `DSeek_Time_cb`.

        dsi->num_maps++;
    }

    return DP_CBR_SKIP_PACKET;
}


static dp_cb_response_t
DSeek_Time_cb (float time, void *ctx)
{
    dseek_ctx_t *pctx = ctx;
    dseek_info_t *dsi = pctx->dseek_info;
    dseek_map_info_t *dsmi = NULL;

    if (dsi->num_maps > 0) {
        dsmi = &dsi->maps[dsi->num_maps - 1];
        if (dsmi->min_time < 0)
            dsmi->min_time = time;
        dsmi->max_time = time;
    }

    return DP_CBR_SKIP_PACKET;
}


static dp_cb_response_t
DSeek_Intermission_cb (void *ctx)
{
    dseek_ctx_t *pctx = ctx;
    dseek_info_t *dsi = pctx->dseek_info;
    dseek_map_info_t *dsmi = NULL;

    if (dsi->num_maps > 0) {
        dsmi = &dsi->maps[dsi->num_maps - 1];
        if (dsmi->finish_time == -1) {
            dsmi->finish_time = dsmi->max_time;
        }
    }

    return DP_CBR_SKIP_PACKET;
}


qboolean
DSeek_Parse (FILE *demo_file, dseek_info_t *dseek_info)
{
    qboolean ok = true;

    dp_err_t dprc;
    dp_callbacks_t callbacks = {
        .packet_start = DSeek_PacketStart_cb,
        .read = DSeek_Read_cb,
        .server_info_model = DSeek_ServerInfoModel_cb,
        .time = DSeek_Time_cb,
        .intermission = DSeek_Intermission_cb,
        .finale = DSeek_Intermission_cb,
    };
    dseek_ctx_t ctx = {
        .demo_file = demo_file,
        .dseek_info = dseek_info,
    };

    memset(dseek_info, 0, sizeof(*dseek_info));

    dprc = DP_ReadDemo(&callbacks, &ctx);

    if (dprc != DP_ERR_SUCCESS) {
        Con_Printf("Error parsing demo: %u\n", dprc);
        ok = false;
    }

    return ok;
}
