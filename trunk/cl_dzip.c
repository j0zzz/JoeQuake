typedef enum {
    DZIP_NOT_EXTRACTING,
    DZIP_EXTRACT_IN_PROGRESS,
    DZIP_EXTRACT_FAIL,
    DZIP_EXTRACT_SUCCESS,
} dzip_status_t;


typedef struct {
    const char prefix[MAX_OSPATH];

#ifdef _WIN32
    static HANDLE proc = NULL;
#else
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
    static qboolean proc = false;
#endif
} dzip_context_t;


void
DZip_Init (dzip_context_t *ctx, char *prefix)
{
    Q_strncpyz (ctx->prefix, prefix, sizeof(ctx->prefix));

#ifdef _WIN32
    ctx->proc = NULL;
#else
    ctx->proc = false;
#endif
}


static bool
DZip_Extracting (dzip_context_t *ctx)
{
#ifdef _WIN32
    return ctx->proc != NULL;
#else
    return ctx->proc;
#endif
}


dzip_status_t
DZip_CheckCompletion (dzip_context_t *ctx)
{
    if (!DZip_Extracting(ctx)) {
        return DZIP_NOT_EXTRACTING;
    }

#ifdef _WIN32
	DWORD	ExitCode;

	if (!GetExitCodeProcess(hDZipProcess, &ExitCode))
	{
		Con_Printf ("WARNING: GetExitCodeProcess failed\n");
		ctx->proc = NULL;

        // Move into caller
		//dz_unpacking = dz_playback = cls.demoplayback = false;
		//StopDZPlayback ();
		return DZIP_EXTRACT_FAIL;
	}

	if (ExitCode == STILL_ACTIVE)
		return DZIP_EXTRACT_IN_PROGRESS;

	ctx->proc = NULL;
#else
	ctx->proc = false;
#endif

    return DZIP_EXTRACT_SUCCESS;
}
