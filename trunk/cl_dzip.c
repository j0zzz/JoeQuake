#ifndef _WIN32
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "quakedef.h"


typedef enum {
    DZIP_NOT_EXTRACTING,
    DZIP_NO_EXIST,
    DZIP_EXTRACT_IN_PROGRESS,
    DZIP_EXTRACT_FAIL,
    DZIP_EXTRACT_SUCCESS,
} dzip_status_t;


typedef struct {
    // Directory into which dzip files will be extracted.
    const char extract_dir[MAX_OSPATH];

    // Full path of the extracted demo file.
    const char dem_path[MAX_OSPATH];

	// When opened, file pointer will be put here.
	FILE **demo_file_p;

#ifdef _WIN32
    static HANDLE proc = NULL;
#else
    static qboolean proc = false;
#endif
} dzip_context_t;


void
DZip_Init (dzip_context_t *ctx, const char *prefix)
{
	memset(ctx, 0, sizeof(*ctx));

    Q_snprintfz (ctx->extract_dir, sizeof(ctx->extract_dir),
				 "%s/%s", com_basedir, prefix);

#ifdef _WIN32
    ctx->proc = NULL;
#else
    ctx->proc = false;
#endif

    ctx->dem_path[0] = '\0';
	ctx->demo_file_p = NULL;
}


dzip_status_t
DZip_StartExtract (dzip_context_t *ctx, const char *name, FILE **demo_file_p)
{
	char	dem_basedir[MAX_OSPATH];
	char	*p, dz_name[MAX_OSPATH];
	char	tempdir[MAX_OSPATH];
	FILE    *demo_file;
#ifdef _WIN32
	char	cmdline[512];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#else
	pid_t	pid;
#endif

	if (Dzip_Extracting(ctx)) {
		// Caller should check DZip_Extracting first
		Sys_Error("Already extracting");
	}

	// Set `name` to the base name of the dzip, and `dem_basedir` to the rest of the path,
	// and `dz_name` to the concatenation of the two.
	if (strstr(name, "..") == name)
	{
		Q_snprintfz (dem_basedir, sizeof(dem_basedir), "%s%s", com_basedir, name + 2);
		p = strrchr (dem_basedir, '/');
		*p = 0;
		name = strrchr (name, '/') + 1;	// we have to cut off the path for the name
	}
	else
	{
		Q_strncpyz (dem_basedir, com_gamedir, sizeof(dem_basedir));
	}
	Q_snprintfz (dz_name, sizeof(dz_name), "%s/%s", dem_basedir, name);

	// Create directory into which dzip will be extracted.
	COM_CreatePath(ctx->extract_dir);

	// check if the file exists
	if (Sys_FileTime(dz_name) == -1)
	{
		return DZIP_NO_EXIST;
	}

	Q_snprintfz (tempdir, sizeof(tempdir), "%s/%s", ctx->extract_dir, name);
	COM_StripExtension (tempdir, ctx->dem_path);
	Q_strlcat(ctx->dem_path, sizeof(ctx->dem_path), ".dem");

	if ((demo_file = fopen(ctx->dem_path, "rb")))
	{
		Con_Printf ("Opened demo file %s\n", ctx->dem_path);
		*demo_file_p = demo_file;
		return DZIP_EXTRACT_SUCCESS;
	}

	// Will be set later on if and when dzip succeeds.
	ctx->demo_file_p = demo_file_out;

	// start DZip to unpack the demo
#ifdef _WIN32
	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	Q_snprintfz (cmdline, sizeof(cmdline), "%s/dzip.exe -x -f \"%s\"", com_basedir, name);
	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, ctx->extract_dir, &si, &pi))
	{
		Con_Printf ("Couldn't execute %s/dzip.exe\n", com_basedir);
		return DZIP_EXTRACT_FAIL;
	}

	ctx->proc = pi.hProcess;
#else

	switch (pid = fork())
	{
	case -1:
		Con_Printf ("Couldn't create subprocess\n");
		return;

	case 0:
		if (chdir(ctx->extract_dir) == -1)
		{
			Con_Printf ("Couldn't chdir to '%s'\n", ctx->extract_dir);
			return DZIP_EXTRACT_FAIL;
		}
		if (execlp(va("%s/dzip-linux", dzipRealPath), "dzip-linux", "-x", "-f", dz_name, NULL) == -1)
		{
			Con_Printf ("Couldn't execute %s/dzip-linux\n", com_basedir);
			exit (-1);
		}

	default:
		if (waitpid(pid, NULL, 0) == -1)
		{
			Con_Printf ("waitpid failed\n");
			return DZIP_EXTRACT_FAIL;
		}
		break;
	}

	ctx->proc = true;
#endif

	return DZIP_EXTRACT_IN_PROGRESS;
}


bool
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
	FILE *demo_file;

	if (!DZip_Extracting(ctx)) {
		return DZIP_NOT_EXTRACTING;
	}

#ifdef _WIN32
	DWORD	ExitCode;

	if (!GetExitCodeProcess(ctx->proc, &ExitCode))
	{
		Con_Printf ("WARNING: GetExitCodeProcess failed\n");
		ctx->proc = NULL;
		return DZIP_EXTRACT_FAIL;
	}

	if (ExitCode == STILL_ACTIVE)
		return DZIP_EXTRACT_IN_PROGRESS;

	ctx->proc = NULL;
#else
	ctx->proc = false;
#endif
	demo_file = fopen(ctx->dem_path, "rb")
	if (!demo_file)
	{
		Con_Printf ("WARNING: Could not read extracted demo file %s\n", ctx->dem_path);
		return DZIP_EXTRACT_FAIL;
	}

	return DZIP_EXTRACT_SUCCESS;
}


void
DZip_Cleanup(dzip_context_t *ctx)
{
	// TODO: Delete all files in `ctx->extract_dir`.
}
