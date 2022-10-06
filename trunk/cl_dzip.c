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
#define _XOPEN_SOURCE 500	// required for nftw
#include <errno.h>
#include <ftw.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "quakedef.h"


#define DZIP_EXTRACT_DIR	"extracted_dzips"

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
				 "%s/%s/%s/", com_basedir, DZIP_EXTRACT_DIR, prefix);

#ifdef _WIN32
    ctx->proc = NULL;
#else
    ctx->proc = false;
#endif

    ctx->dem_path[0] = '\0';
	ctx->demo_file_p = NULL;
	ctx->initialized = true;
}


static void
DZip_CheckInitialized (dzip_context_t *ctx)
{
	if (!ctx->initialized)
	{
		Sys_Error("Attempt to use uninitialized dzip context");
	}
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
	char	dem_basedir[1024];
	char	*p, dz_name[1024];
	char	tempdir[1024];
	FILE    *demo_file;
#ifdef _WIN32
	char	cmdline[1024];
	char	abs_dz_name[1024];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#else
	char	abs_basedir[PATH_MAX];
	char	abs_dz_name[PATH_MAX];
	pid_t	pid;
	int		wstatus;
#endif

	DZip_CheckInitialized (ctx);

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

	Q_snprintfz (tempdir, sizeof(tempdir), "%s%s", ctx->extract_dir, name);
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

	if (GetFullPathName(dz_name, sizeof(abs_dz_name), abs_dz_name, NULL) == 0)
	{
		Con_Printf("GetFullPathName failed on %s\n", dz_name);
		return DZIP_EXTRACT_FAIL;
	}

	Q_snprintfz(cmdline, sizeof(cmdline), "%s/dzip.exe -x -f \"%s\"", com_basedir, abs_dz_name);
	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, ctx->extract_dir, &si, &pi))
	{
		Con_Printf ("Couldn't execute %s/dzip.exe\n", com_basedir);
		return DZIP_EXTRACT_FAIL;
	}

	ctx->proc = pi.hProcess;
#else

	if (!realpath(com_basedir, abs_basedir))
	{
		Con_Printf ("Couldn't realpath '%s'\n", com_basedir);
		return DZIP_EXTRACT_FAIL;
	}

	if (!realpath(dz_name, abs_dz_name))
	{
		Con_Printf ("Couldn't realpath '%s'\n", dz_name);
		return DZIP_EXTRACT_FAIL;
	}

	switch (pid = fork())
	{
	case -1:
		Con_Printf ("Couldn't create subprocess\n");
		return DZIP_EXTRACT_FAIL;

	case 0:
		if (chdir(ctx->extract_dir) == -1)
		{
			Con_Printf ("Couldn't chdir to '%s'\n", ctx->extract_dir);
			_exit(1);
		}
		if (execlp(va("%s/dzip-linux", abs_basedir), "dzip-linux", "-x", "-f", abs_dz_name, NULL) == -1)
		{
			Con_Printf ("Couldn't execute %s/dzip-linux\n", abs_basedir);
			_exit(1);
		}
		Sys_Error("Shouldn't reach here\n");
		break;
	default:
		Con_Printf ("waitpid(%d)\n", pid);
		if (waitpid(pid, &wstatus, 0) == -1)
		{
			Con_Printf ("waitpid failed\n");
			return DZIP_EXTRACT_FAIL;
		}

		if (!WIFEXITED(wstatus)) {
			Con_Printf ("dzip exited abnormally\n");
			return DZIP_EXTRACT_FAIL;
		}

		if (WEXITSTATUS(wstatus) != 0) {
			Con_Printf ("dzip exited with non-zero status: %d\n", WEXITSTATUS(wstatus));
			return DZIP_EXTRACT_FAIL;
		}

		break;
	}

	ctx->proc = true;
#endif

	return DZIP_EXTRACT_IN_PROGRESS;
}


static dzip_status_t
DZip_CheckOrWaitCompletion (dzip_context_t *ctx, qboolean wait)
{
	FILE *demo_file;

	DZip_CheckInitialized (ctx);

	if (!DZip_Extracting(ctx)) {
		return DZIP_NOT_EXTRACTING;
	}

#ifdef _WIN32
	DWORD	ExitCode;

	if (wait && WaitForSingleObject(ctx->proc, INFINITE) != WAIT_OBJECT_0)
	{
		Con_Printf ("WARNING: Could not wait for process\n");
		return DZIP_EXTRACT_FAIL;
	}
	if (!GetExitCodeProcess(ctx->proc, &ExitCode))
	{
		Con_Printf ("WARNING: GetExitCodeProcess failed\n");
		ctx->proc = NULL;
		return DZIP_EXTRACT_FAIL;
	}

	if (ExitCode == STILL_ACTIVE)
	{
		if (wait) {
			Sys_Error("Process not finished despite waiting");
		}
		return DZIP_EXTRACT_IN_PROGRESS;
	}

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

	*ctx->demo_file_p = demo_file;
	return DZIP_EXTRACT_SUCCESS;
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
	return DZip_CheckOrWaitCompletion(ctx, false);
}


/*
 * Synchronously open a demo from a dzip file.
 *
 * Arguments:
 *	ctx: DZip context.
 *	name: Name of the dzip file to be extracted, relative to the game dir.
 *	demo_file_p: On success, the file pointer will be stored here.
 *
 * Return values:
 *	DZIP_ALREADY_EXTRACTING: An extraction is already in progress under this
 *		context, and so no new extraction has been started.
 *	DZIP_NO_EXIST: The dzip file specified by `name` could not be found.
 *	DZIP_EXTRACT_SUCCESS: The demo was successfully opened. In this case
 *		`*demo_file_p` can be read from.
 *	DZIP_EXTRACT_FAIL: There was a problem extracting. Detailed error printed to
 *		console.
 */
dzip_status_t
DZip_Open(dzip_context_t *ctx, const char *name, FILE **demo_file_p)
{
	dzip_status_t dzip_status;

	dzip_status = DZip_StartExtract(ctx, name, demo_file_p);
	if (dzip_status == DZIP_EXTRACT_IN_PROGRESS)
	{
		dzip_status = DZip_CheckOrWaitCompletion(ctx, true);
		if (dzip_status != DZIP_EXTRACT_SUCCESS
			&& dzip_status != DZIP_EXTRACT_FAIL)
		{
			Sys_Error("Unexpected error from DZip_CheckOrWaitCompletion");
		}
	}

	return dzip_status;
}


/*
 * Check that the given path is safe to be deleted.
 */
static void
DZip_DeletePathSanityCheck(char *path)
{
	char *expected_prefix[1024];

	Q_snprintfz(expected_prefix, sizeof(expected_prefix), "%s/%s/",
				com_basedir, DZIP_EXTRACT_DIR);

	if (strncmp(path, expected_prefix, strlen(expected_prefix)))
	{
		Sys_Error("Path being deleted %s is not under %s",
				  path, expected_prefix);
	}
	if (strstr(path + strlen(com_basedir), ".."))
	{
		Sys_Error("Parent directory found in path %s", path);
	}

	Con_Printf("Removing %s\n", path);
}


#ifdef _WIN32
static qboolean
DZip_RecursiveDirectoryCleanup(const char *dir_path)
{
	qboolean ok = true;
	char glob[MAX_PATH];
	char child_path[MAX_PATH];
	WIN32_FIND_DATAA find_file_data;
	HANDLE hFind;

	Q_snprintfz(glob, sizeof(glob), "%s/*", dir_path);

	hFind = FindFirstFile(glob, &find_file_data)
	do
	{
		if (strcmp(find_file_data.cFileName, "..") == 0
			|| strcmp(find_file_data.cFileName, ".") == 0)
		{
			continue;
		}

		Q_snprintfz(child_path, sizeof(child_path), "%s/%s",
					dir_path, find_file_data.cFileName);

		if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!DZip_RecursiveDirectoryCleanup (child_path))
			{
				ok = false;
				break;
			}
		}

		DZip_DeletePathSanityCheck(child_path);
		if (!DeleteFile(child_path))
		{
			Con_Printf("Failed to remove %s in dzip cleanup\n");
			ok = false;
			break;
		}
	} while (FindNextFile(hFind, &find_file_data));

	FindClose(hFind);

	return ok;
}
#else
static int
DZip_Cleanup_Callback(const char *fpath, const struct stat *sb, int typeflag,
					  struct FTW *ftwbuf)
{
	int rc = 0;

	DZip_DeletePathSanityCheck(fpath);
	rc = remove(fpath);
	if (rc)
	{
		Con_Printf("Failed to remove %s in dzip cleanup\n");
	}

	return rc;
}
#endif


/*
 * Cleanup all demos that have been extracted under the given context.
 *
 * Arguments:
 *	ctx: The DZip context.
 */
void
DZip_Cleanup(dzip_context_t *ctx)
{
	DZip_CheckInitialized (ctx);
#ifdef _WIN32
	DZip_RecursiveDirectoryCleanup(ctx->extract_dir);
#else
	nftw(ctx->extract_dir, DZip_Cleanup_Callback, 32, FTW_DEPTH | FTW_PHYS);
#endif
}
