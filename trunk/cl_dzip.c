/*
 * Async and concurrent dzip extraction.
 *
 * Functions to open a demo file contained with a dzip file.  The demo file that
 * is opened matches the name of the dzip, only with changed extension, eg.
 * opening `e1m1_029.dz` will attempt to open the file `e1m1_029.dem` within the
 * dzip archive.
 *
 * Extraction is async in the sense that extraction is started by one function
 * (`DZip_StartExtract`) and the result is polled with another
 * (`DZip_CheckCompletion`), meaning the screen can be redrawn as dzip runs. The
 * result of this process is a file handle referring to the open demo.
 *
 * Any extraction must be run in a context.  Concurrent extractions are only
 * permitted with different contexts.  A context is initialized with
 * `DZip_Init`.
 *
 * A typical calling pattern is:
 *	- Call `DZip_Init` to initialize a context.
 *	- Call `DZip_StartExtract` to began a dzip extraction.
 *	- Call `DZip_CheckCompletion` repeatedly, until the extraction has finished.
 */

#ifndef _WIN32
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "quakedef.h"


/*
 * Initialize a dzip context.
 *
 * Arguments:
 *	ctx: DZip context to be initialized.
 *	prefix: The directory under the base dir into which dzips will be extracted.
 */
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


static qboolean
DZip_Extracting (dzip_context_t *ctx)
{
#ifdef _WIN32
	return ctx->proc != NULL;
#else
	return ctx->proc;
#endif
}


/*
 * Start extracting a demo from a dzip file.
 *
 * Arguments:
 *	ctx: DZip context.
 *	name: Name of the dzip file to be extracted, relative to the game dir.
 *	demo_file_p: Once the extraction is complete, the file pointer will be
 *		stored here.
 *
 * Return values:
 *	DZIP_ALREADY_EXTRACTING: An extraction is already in progress under this
 *		context, and so no new extraction has been started.
 *	DZIP_NO_EXIST: The dzip file specified by `name` could not be found.
 *	DZIP_EXTRACT_SUCCESS: The dzip has previously been extracted and so has
 *		been immediately opened.  In this case `*demo_file_p` can immediately be
 *		read from.
 *	DZIP_EXTRACT_FAIL: There was a problem extracting. Detailed error printed to
 *		console.
 *	DZIP_EXTRACT_IN_PROGRESS: Extraction started.  Call `DZip_CheckCompletion`
 *		to poll the dzip process.
 */
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

	if (DZip_Extracting(ctx)) {
		return DZIP_ALREADY_EXTRACTING;
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
	// TODO: Handle errors?
	COM_CreatePath(ctx->extract_dir);

	// check if the file exists
	if (Sys_FileTime(dz_name) == -1)
	{
		return DZIP_NO_EXIST;
	}

	Q_snprintfz (tempdir, sizeof(tempdir), "%s/%s", ctx->extract_dir, name);
	COM_StripExtension (tempdir, ctx->dem_path);
	Q_strlcat(ctx->dem_path, ".dem", sizeof(ctx->dem_path));

	if ((demo_file = fopen(ctx->dem_path, "rb")))
	{
		Con_Printf ("Opened demo file %s\n", ctx->dem_path);
		*demo_file_p = demo_file;
		return DZIP_EXTRACT_SUCCESS;
	}

	// Will be set later on if and when dzip succeeds.
	ctx->demo_file_p = demo_file_p;

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
		return DZIP_EXTRACT_FAIL;

	case 0:
		if (chdir(ctx->extract_dir) == -1)
		{
			Con_Printf ("Couldn't chdir to '%s'\n", ctx->extract_dir);
			return DZIP_EXTRACT_FAIL;
		}
		if (execlp(va("%s/dzip-linux", com_basedir), "dzip-linux", "-x", "-f", dz_name, NULL) == -1)
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


/*
 * Poll whether an extraction has completed.
 *
 * Called after `DZip_StartExtract`.  This function should be called repeatedly
 * until it returns either `DZIP_EXTRACT_FAIL` and `DZIP_EXTRACT_SUCCESS`.
 *
 * Arguments:
 *	ctx: DZip context.
 *
 * Return values:
 *	DZIP_NOT_EXTRACTING: No extraction in progress, ie. either
 *		`DZip_StartExtract` has not been called, or its result has already been
 *		collected with a previous call to this function.
 *	DZIP_EXTRACT_IN_PROGRESS: The dzip process is still running.
 *	DZIP_EXTRACT_FAIL: There was a problem extracting. Detailed error printed to
 *		console.
 *	DZIP_EXTRACT_SUCCESS: The dzip has been successfully extracted.
 *		`*demo_file_p` now refers to the open file.
 */
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
	demo_file = fopen(ctx->dem_path, "rb");
	if (!demo_file)
	{
		Con_Printf ("WARNING: Could not read extracted demo file %s\n", ctx->dem_path);
		return DZIP_EXTRACT_FAIL;
	}

	return DZIP_EXTRACT_SUCCESS;
}


/*
 * Cleanup all demos that have been extracted under the given context.
 *
 * Arguments:
 *	ctx: The DZip context.
 */
void
DZip_Cleanup(dzip_context_t *ctx)
{
	// TODO: Delete all files in `ctx->extract_dir`.
}
