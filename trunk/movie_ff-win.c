/*
   movie_ffmpeg.c -- capture_mode raw: write .raw + .pcm to disk
   capture_mode ffmpeg (Win32): pipe RGB + PCM into ffmpeg.exe -> stem.mp4|mkv

   Requirements for capture_mode ffmpeg:
   - ffmpeg.exe must sit next to the game executable (same folder as joequake-gl.exe etc.).
   - FFmpeg build must expose whatever codecs/options you select in capture_ffmpeg_video_args
   and capture_ffmpeg_audio_args (defaults use libx264 + AAC — common in generic builds).
   - Windows only; other platforms: Movie_FFmpeg_Encode_Open returns false.

   Video: packed RGB24, GL framebuffer row order (bottom row first). ffmpeg gets -vf vflip.
   Audio: interleaved stereo s16le at sample_rate Hz (engine shm->speed).

   Encode: two named OUTBOUND pipes (audio then rawvideo). Child stdin is NUL — avoids
   stdin/rawvideo coupling and tiny anonymous pipe stalls. CLI lists -i audio pipe before
   -i video pipe; ConnectNamedPipe order matches. Failed writes abort capture once.
   No +faststart (avoids post-mux rewrite stalls on pipe shutdown).

   CreateProcess uses STARTUPINFOEX + PROC_THREAD_ATTRIBUTE_HANDLE_LIST to whitelist
   exactly the stdio handles (NUL stdin, stderr log) for inheritance. The pipe SERVER
   handles are created with bInheritHandle=TRUE for the same SECURITY_ATTRIBUTES used
   by stdio, but the whitelist excludes them so ffmpeg.exe receives only its client
   side via CreateFile on the pipe path. Otherwise the inherited unused server handles
   in the child keep the pipe alive after the parent CloseHandle, the client read
   never sees EOF, and the muxer never finalizes the MP4.

   Finalize wait: Movie_FFmpeg_WaitOrKillProcess polls in 100 ms slices and pumps
   Sys_SendKeyEvents + SCR_UpdateScreen each iteration so the window stays painted
   and Windows does not declare the game unresponsive while ffmpeg writes the moov.
   m_finalizing is exposed via Movie_FFmpeg_IsFinalizing so movie.c can refuse new
   captures during the wait.
   */

#include "quakedef.h"
#include "movie_ff-win.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

extern void Movie_Stop (void);
extern void Movie_MaybeAutoQuit (void);
extern void Movie_CancelCaptureStats (void);

extern cvar_t capture_ffmpeg_video_buf_mb;
extern cvar_t capture_ffmpeg_audio_buf_mb;
extern cvar_t capture_ffmpeg_loglevel;
extern cvar_t capture_ffmpeg_report;
extern cvar_t capture_ffmpeg_write_timeout_ms;
extern cvar_t capture_ffmpeg_container;
extern cvar_t capture_ffmpeg_video_args;
extern cvar_t capture_ffmpeg_audio_args;

typedef enum {
	FFMPEG_SINK_NONE,
	FFMPEG_SINK_FILES,
	FFMPEG_SINK_ENCODE
} ffmpeg_sink_mode_t;

/* wait for ffmpeg to finish mux after closing pipes; then force-kill (normal: seconds) */
#define FFMPEG_EXIT_WAIT_MS	30000
/* poll granularity for the finalize wait; small enough that the window keeps repainting */
#define FFMPEG_EXIT_POLL_MS	100
/* Fallback args if cvar is empty after trim — keep aligned with defaults in movie.c */
#define CAP_FFMPEG_DEFAULT_VIDEO_ENCODE	"-c:v libx265 -preset medium -crf 18 -pix_fmt yuv420p"
#define CAP_FFMPEG_DEFAULT_AUDIO_ENCODE	"-c:a aac -b:a 256k -ar 48000"
#define CAP_FFMPEG_ENCODE_ARG_CAP	6144

static ffmpeg_sink_mode_t	m_sink_mode = FFMPEG_SINK_NONE;
static FILE			*m_ffmpeg_video;
static FILE			*m_ffmpeg_audio;

char				m_ffmpeg_exe[MAX_OSPATH];
static piped_process_t * process_video;
static char			m_outpath_video[MAX_OSPATH];
static piped_process_t* process_audio;
static char			m_outpath_audio[MAX_OSPATH];
static char			m_outpath[MAX_OSPATH];

static qboolean			m_encode_aborting	= false;
static qboolean			m_finalizing		= false;
static double			m_finalize_seconds	= 0;

pipe_status_t * ffmpeg_set_status(enum status_type type)
{
	pipe_status_t * st = Q_calloc(1, sizeof(pipe_status_t));

	st->type = type;
	st->last_error = GetLastError();

	return st;
}

void ffmpeg_print_status(pipe_status_t * pipe_status)
{
	if (pipe_status->type == FFMPEG_OK) {
		Con_Printf("Successful creation of ffmpeg pipes.\n");
		return;
	}

	switch (pipe_status->type)
	{
		case FFMPEG_CREATE_PIPE:     Con_Printf("Failed to create pipes (err %ld)\n", pipe_status->last_error); break;
		case FFMPEG_CREATE_PROCESS:  Con_Printf("Failed to create process (err %ld)\n", pipe_status->last_error); break;
		case FFMPEG_WRITE_PIPE:      Con_Printf("Failed to write to pipe (err %ld)\n", pipe_status->last_error); break;
		case FFMPEG_WAIT_FAILURE:    Con_Printf("Timeout or failure (err %ld)\n", pipe_status->last_error); break;
		case FFMPEG_OTHER:           Con_Printf("Other error (err %ld)\n", pipe_status->last_error); break;
		default:                     Con_Printf("Unknown error (err %ld)\n", pipe_status->last_error); break;
	}
}

qboolean ffmpeg_create_pipe_pair(
	const char * name,
	HANDLE * out_read_pipe,
	HANDLE * out_write_pipe,
	DWORD buffer_size, 
	DWORD timeout_ms
)
{   
	SECURITY_ATTRIBUTES sa = {
		.nLength = sizeof(SECURITY_ATTRIBUTES),
		.lpSecurityDescriptor = NULL,
		.bInheritHandle = TRUE
	};
	char full_name[2048];
	HANDLE read_pipe;
	HANDLE write_pipe;

	Q_snprintfz(full_name, sizeof(full_name), "\\\\.\\pipe\\ffmpipe_%d_%d_%s",
		GetCurrentProcessId(),
		out_read_pipe,
		name
	);

	if ((read_pipe = CreateNamedPipeA(
		full_name,
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_BYTE | PIPE_WAIT,
		1,
		buffer_size, buffer_size,
		timeout_ms, &sa
	)) == INVALID_HANDLE_VALUE)
		return false;

	if ((write_pipe = CreateFileA(
		full_name,
		GENERIC_WRITE,
		0,
		&sa,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL /* template */
	)) == INVALID_HANDLE_VALUE)
	{
		CloseHandle(read_pipe);
		return false;
	}

	*out_write_pipe = write_pipe;
	*out_read_pipe = read_pipe;
	return true;
}

piped_process_t * ffmpeg_create_piped_process(
	char * ffmpeg_path, 
	char * ffmpeg_args, 
	char * ffmpeg_dir,
	pipe_status_t * pipe_status
)
{
    STARTUPINFOA startup_info;
	char cmdline[4096];
	piped_process_t * piped_process = Q_calloc(1, sizeof(piped_process_t));
	memset(&(piped_process->m_stdin_r), 0, sizeof(piped_process->m_stdin_r));
	piped_process->m_stdin_r = piped_process->m_stdin_w = 
		piped_process->m_stdout_r = piped_process->m_stdout_w = INVALID_HANDLE_VALUE;
	piped_process->m_event = NULL;
	piped_process->m_timeout_ms = 10000;

	if (pipe_status) *pipe_status = *ffmpeg_set_status(FFMPEG_OK);

	piped_process->m_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!piped_process->m_event)
	{
		if (pipe_status) *pipe_status = *ffmpeg_set_status(FFMPEG_OTHER);
		free(piped_process);
		return NULL;
	}

	/* create pipe pairs */
	if (
		!ffmpeg_create_pipe_pair("stdout", &piped_process->m_stdout_r, &piped_process->m_stdout_w, 1024*128, piped_process->m_timeout_ms)
		|| !SetHandleInformation(piped_process->m_stdout_r, HANDLE_FLAG_INHERIT, 0)
	) {
		if (pipe_status) *pipe_status = *ffmpeg_set_status(FFMPEG_CREATE_PIPE);
		free(piped_process);
		return NULL;
	}

	if (
		!ffmpeg_create_pipe_pair("stdin", &piped_process->m_stdin_r, &piped_process->m_stdin_w, 1024*128, piped_process->m_timeout_ms)
		|| !SetHandleInformation(piped_process->m_stdin_w, HANDLE_FLAG_INHERIT, 0)
	) {
		if (pipe_status) *pipe_status = *ffmpeg_set_status(FFMPEG_CREATE_PIPE);
		free(piped_process);
		return NULL;
	}

	/* create child */

	memset(&startup_info, 0, sizeof(startup_info));
	startup_info.cb = sizeof(startup_info);
	startup_info.hStdError = piped_process->m_stdout_w;
	startup_info.hStdOutput = piped_process->m_stdout_w;
	startup_info.hStdInput = piped_process->m_stdin_r;
	startup_info.dwFlags = STARTF_USESTDHANDLES;

	Q_snprintfz(cmdline, sizeof(cmdline), 
		"\"%s\" %s",
		ffmpeg_path, ffmpeg_args
	);

	if (!CreateProcessA(
		NULL,
		cmdline,
		NULL,
		NULL,
		TRUE,
		CREATE_NO_WINDOW,
		NULL,
		ffmpeg_dir,
		&startup_info,
		&piped_process->m_procinfo
	)) {
		if (pipe_status) *pipe_status = *ffmpeg_set_status(FFMPEG_CREATE_PROCESS);
		free(piped_process);
		return NULL;
	}

	return piped_process;
}

void ffmpeg_close_piped_process(piped_process_t * process, qboolean terminate)
{
	DWORD result;
	CloseHandle(process->m_stdin_w);
	process->m_stdin_w = INVALID_HANDLE_VALUE;
	result = WaitForSingleObject(process->m_procinfo.hProcess, process->m_timeout_ms);

	if (result != STATUS_WAIT_0 && terminate)
		TerminateProcess(process->m_procinfo.hProcess, -1);
	ffmpeg_read_from_piped_process(process);
}

void ffmpeg_destruct_piped_process(piped_process_t * process)
{
	HANDLE inv[4] = { process->m_stdin_r, process->m_stdin_w, process->m_stdout_r, process->m_stdout_w };
	HANDLE nul[3] = { process->m_event, process->m_procinfo.hProcess, process->m_procinfo.hThread };
	int i;

	for (i = 0; i < 3; i++)
		if (nul[i])
			CloseHandle(nul[i]);

	for (i = 0; i < 4; i++)
		if (inv[i] != INVALID_HANDLE_VALUE)
			CloseHandle(inv[i]);

	free(process);
}

pipe_status_t * ffmpeg_write_to_piped_process(
	piped_process_t * process,
	const char * data,
	size_t len
)
{
	DWORD total_written = 0;
	OVERLAPPED overlapped = {0};
	overlapped.hEvent = process->m_event;

	while (total_written < len)
	{
		qboolean ok = WriteFile(
			process->m_stdin_w, 
			data + total_written, 
			(DWORD)len - total_written,
			NULL, 
			&overlapped
		);

		if (!ok)
		{
			if (GetLastError() != ERROR_IO_PENDING)
				return ffmpeg_set_status(FFMPEG_OTHER);
			SetLastError(ERROR_SUCCESS);
		}

		HANDLE wait_objects[2] = { process->m_event, process->m_procinfo.hProcess };
		if (WaitForMultipleObjects(2, wait_objects, FALSE, process->m_timeout_ms) != STATUS_WAIT_0)
			return ffmpeg_set_status(FFMPEG_WAIT_FAILURE);

		DWORD written = 0;
		if (!GetOverlappedResult(process->m_stdin_w, &overlapped, &written, FALSE))
			return ffmpeg_set_status(FFMPEG_OTHER);

		total_written += written;

		//ffmpeg_read_from_piped_process(process);
	}
	return ffmpeg_set_status(FFMPEG_OK);
}

size_t ffmpeg_read_from_piped_process(piped_process_t * process)
{
	DWORD available;
	if (!PeekNamedPipe(process->m_stdout_r, NULL, 0, NULL, &available, NULL))
		return 0;

	char buffer[256];
	DWORD total_read = 0;

	while (total_read < available)
	{
		DWORD read = available - total_read;
		if (read > sizeof(buffer))
			read = sizeof(buffer);

		if (!ReadFile(process->m_stdout_r, buffer, read, &read, NULL))
			return total_read;

		total_read += read;
		Sys_Printf("%s\n", buffer);
	}

	return total_read;
}

qboolean Movie_FFmpeg_IsFinalizing (void)
{
	return m_finalizing;
}

static void Movie_FFmpeg_Encode_ClosePipes (void)
{
	if (process_video) {
		ffmpeg_close_piped_process(process_video, true);
		ffmpeg_destruct_piped_process(process_video);
		process_video = NULL;
	}
	if (process_audio) {
		ffmpeg_close_piped_process(process_audio, true);
		ffmpeg_destruct_piped_process(process_audio);
		process_audio = NULL;
	}
}

/*
 * Copy trimmed cvar text into dst; blank after trim selects fallback_nonempty.
 */
static qboolean Movie_FFmpeg_CopyTrimmedEncodeArg (const char *raw,
		const char *fallback_nonempty,
		char *dst, size_t dstsize)
{
	const char *s = raw ? raw : "";
	const char *e;
	size_t		len;

	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;

	if (!*s)
	{
		/* Avoid Q_strncpyz here: MSVC C4090 if src were const-qualified in the API. */
		strncpy (dst, fallback_nonempty, dstsize - 1);
		dst[dstsize - 1] = 0;
		return true;
	}

	e = s + strlen (s);
	while (e > s && (*(e - 1) == ' ' || *(e - 1) == '\t' || *(e - 1) == '\r'
				|| *(e - 1) == '\n'))
		e--;

	len = (size_t) (e - s);
	if (len >= dstsize)
		return false;

	memcpy (dst, s, len);
	dst[len] = 0;
	return true;
}

double Movie_FFmpeg_LastFinalizeSeconds(void)
{
	return m_finalize_seconds;
}

static void Movie_FFmpeg_GetExeDir (char *out, size_t outsize)
{
	char	mod[MAX_PATH];
	char	*slash;

	if (!GetModuleFileNameA (NULL, mod, sizeof (mod)) || !mod[0])
	{
		out[0] = '\0';
		return;
	}
	Q_strncpyz (out, mod, outsize);
	slash = strrchr (out, '\\');
	if (!slash)
		slash = strrchr (out, '/');
	if (slash)
		slash[1] = '\0';
}

qboolean Movie_FFmpeg_Encode_Open (const char *dir, const char *stem, int width, int height, int fps, int sample_rate)
{
	char				exedir[MAX_OSPATH];
	char				cmdline_video[16384];
	char				cmdline_audio[16384];
	char				aenc_args[CAP_FFMPEG_ENCODE_ARG_CAP];
	char				venc_args[CAP_FFMPEG_ENCODE_ARG_CAP];

	memset (&m_outpath_video, 0, sizeof (m_outpath_video));
	Movie_FFmpeg_Close ();

	if (!stem || !stem[0] || width <= 0 || height <= 0 || fps <= 0 || sample_rate <= 0)
		return false;

	Movie_FFmpeg_GetExeDir (exedir, sizeof (exedir));
	if (!exedir[0])
	{
		Con_Printf ("ERROR: Movie_FFmpeg_Encode_Open: GetModuleFileName failed\n");
		return false;
	}
	Q_snprintfz (m_ffmpeg_exe, sizeof (m_ffmpeg_exe), "%sffmpeg.exe", exedir);

	if (GetFileAttributesA (m_ffmpeg_exe) == INVALID_FILE_ATTRIBUTES)
	{
		Con_Printf ("ERROR: %s not found (place ffmpeg.exe next to the game executable)\n", m_ffmpeg_exe);
		return false;
	}

	{
		const char *ext = capture_ffmpeg_container.string ? capture_ffmpeg_container.string : "mp4";

		if (strcmp (ext, "mp4") != 0 && strcmp (ext, "mkv") != 0)
		{
			Con_Printf ("WARNING: capture_ffmpeg_container '%s' not in {mp4,mkv}, using mp4\n", ext);
			ext = "mp4";
		}
		Q_snprintfz(m_outpath_video, sizeof (m_outpath_video), "%s/%s_v.%s", dir, stem, ext);
		Q_snprintfz(m_outpath_audio, sizeof(m_outpath_audio), "%s/%s_a.%s", dir, stem, ext);
		Q_snprintfz(m_outpath, sizeof(m_outpath), "%s/%s.%s", dir, stem, ext);

	}

	{
		const char *loglevel = (capture_ffmpeg_loglevel.string && capture_ffmpeg_loglevel.string[0])
			? capture_ffmpeg_loglevel.string : "error";
		const char *report   = capture_ffmpeg_report.value ? "-report" : "";

		if (!Movie_FFmpeg_CopyTrimmedEncodeArg (capture_ffmpeg_video_args.string,
					CAP_FFMPEG_DEFAULT_VIDEO_ENCODE,
					venc_args, sizeof (venc_args))
				|| !Movie_FFmpeg_CopyTrimmedEncodeArg (capture_ffmpeg_audio_args.string,
					CAP_FFMPEG_DEFAULT_AUDIO_ENCODE,
					aenc_args, sizeof (aenc_args)))
		{
			Con_Printf (
					"ERROR: capture_ffmpeg_video_args or capture_ffmpeg_audio_args "
					"exceed %u characters (trimmed)\n",
					(unsigned) (CAP_FFMPEG_ENCODE_ARG_CAP - 1));
			return false;
		}

		Q_snprintfz (
			cmdline_audio,
			sizeof (cmdline_audio),
			"-hide_banner -loglevel %s %s -y "
			"-f s16le -ac 2 -ar %d -i - "
			"-map 0:a -shortest %s \"%s\"",
			loglevel, report,
			sample_rate,
			aenc_args, m_outpath_audio
		);

		Q_snprintfz (
			cmdline_video,
			sizeof (cmdline_video),
			"-hide_banner -loglevel %s %s -y "
			"-f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate %d "
			"-i - -vf vflip %s \"%s\"",
			loglevel, report,
			width, height, fps,
			venc_args, m_outpath_video
		);
	}

	pipe_status_t * status_video = ffmpeg_set_status(FFMPEG_OK);
	process_video = ffmpeg_create_piped_process(m_ffmpeg_exe, cmdline_video, exedir, status_video);
	if (!process_video || status_video->type != FFMPEG_OK) {
		ffmpeg_print_status(status_video);
		process_video = NULL;
		free(status_video);
		return false;
	}
	free(status_video);

	pipe_status_t* status_audio = ffmpeg_set_status(FFMPEG_OK);
	process_audio = ffmpeg_create_piped_process(m_ffmpeg_exe, cmdline_audio, exedir, status_audio);
	if (!process_audio || status_audio->type != FFMPEG_OK) {
		ffmpeg_print_status(status_audio);
		process_audio = NULL;
		free(status_audio);
		return false;
	}
	free(status_audio);

	m_sink_mode = FFMPEG_SINK_ENCODE;
	Con_Printf (
			"capture_mode ffmpeg: PCM -ar %i Hz (must match engine rate); video %ix%i @ %i fps\n",
			sample_rate, width, height, fps);
	Con_Printf ("capture_mode ffmpeg: %s\n", cmdline_video);
	Con_Printf ("capture_mode ffmpeg: output %s\n", m_outpath_video);
	return true;

}

qboolean Movie_FFmpeg_Open (const char *dir, const char *stem)
{
	char path[MAX_OSPATH];

	Movie_FFmpeg_Close ();

	if (!stem || !stem[0])
		return false;

	m_sink_mode = FFMPEG_SINK_FILES;

	Q_snprintfz (path, sizeof(path), "%s/%s_ffmpeg_video.raw", dir, stem);
	COM_CreatePath (path);
	if (!(m_ffmpeg_video = fopen (path, "wb")))
	{
		Con_Printf ("ERROR: Couldn't open %s\n", path);
		m_sink_mode = FFMPEG_SINK_NONE;
		return false;
	}

	Q_snprintfz (path, sizeof(path), "%s/%s_ffmpeg_audio.pcm", dir, stem);
	COM_CreatePath (path);
	if (!(m_ffmpeg_audio = fopen (path, "wb")))
	{
		Con_Printf ("ERROR: Couldn't open %s\n", path);
		fclose (m_ffmpeg_video);
		m_ffmpeg_video = NULL;
		m_sink_mode = FFMPEG_SINK_NONE;
		return false;
	}

	Con_Printf ("capture_mode raw: writing %s/%s_ffmpeg_*.raw|pcm\n", dir, stem);
	return true;
}

void Movie_FFmpeg_WriteVideo (const byte *pixel_buffer, int size)
{
	if (!pixel_buffer || size <= 0)
		return;

	if (m_sink_mode == FFMPEG_SINK_ENCODE)
	{
		pipe_status_t * status = ffmpeg_set_status(FFMPEG_OK);
		if (process_video)
			status = ffmpeg_write_to_piped_process(process_video, pixel_buffer, size);
		if(status->type != FFMPEG_OK) {
			ffmpeg_print_status(status);
			free(status);
			Movie_FFmpeg_Close();
		}
		free(status);
		return;
	}

	if (m_sink_mode != FFMPEG_SINK_FILES || !m_ffmpeg_video)
		return;
	if (fwrite (pixel_buffer, 1, size, m_ffmpeg_video) != (size_t) size)
		Con_Printf ("ERROR: ffmpeg video write failed\n");
	fflush (m_ffmpeg_video);
}

void Movie_FFmpeg_WriteAudio (int samples, const byte *sample_buffer)
{
	int	nbytes;

	if (!sample_buffer || samples <= 0)
		return;
	
	nbytes = samples * 4;

	if (m_sink_mode == FFMPEG_SINK_ENCODE)
	{
		pipe_status_t* status = ffmpeg_set_status(FFMPEG_OK);
		if (process_video)
			status = ffmpeg_write_to_piped_process(process_audio, sample_buffer, nbytes);
		if (status->type != FFMPEG_OK) {
			ffmpeg_print_status(status);
			free(status);
			Movie_FFmpeg_Close();
		}
		free(status);
		return;
	}

	if (m_sink_mode != FFMPEG_SINK_FILES || !m_ffmpeg_audio)
		return;
	if (fwrite (sample_buffer, 1, nbytes, m_ffmpeg_audio) != (size_t) nbytes)
		Con_Printf ("ERROR: ffmpeg audio write failed\n");
	fflush (m_ffmpeg_audio);
}

void Movie_FFmpeg_Close (void)
{
	if (m_sink_mode == FFMPEG_SINK_ENCODE)
	{
		char				cmdline[16384];
		DWORD		wexit;
		PROCESS_INFORMATION		pi;
		STARTUPINFO si;
		Movie_FFmpeg_Encode_ClosePipes ();
		SCR_EndLoadingPlaque();
		Q_snprintfz(cmdline, sizeof(cmdline),
			"\"%s\" -y -i \"%s\" -i \"%s\" -c:v copy -c:a copy -map 0:v:0 -map 1:a:0 \"%s\"",
			m_ffmpeg_exe, m_outpath_video, m_outpath_audio, m_outpath
		);
		if (!CreateProcessA(
			NULL,
			cmdline,
			NULL,
			NULL,
			TRUE,
			CREATE_NO_WINDOW,
			NULL,
			com_basedir,
			&si,
			&pi
		)) {
			Con_Printf("ERROR: CreateProcess ffmpeg failed (%lu)\n", (unsigned long)GetLastError());
			return;
		};

		WaitForSingleObject(pi.hProcess, 10000);
		GetExitCodeProcess(pi.hProcess, &wexit);

		if (wexit != 0)
			Con_Printf("ERROR: Mux ffmpeg didn't quit properly\n");

		m_sink_mode = FFMPEG_SINK_NONE;
		m_outpath_video[0] = '\0';
		m_outpath_audio[0] = '\0';
		m_outpath[0] = '\0';
		return;
	}

	if (m_ffmpeg_video)
	{
		fclose (m_ffmpeg_video);
		m_ffmpeg_video = NULL;
	}
	if (m_ffmpeg_audio)
	{
		fclose (m_ffmpeg_audio);
		m_ffmpeg_audio = NULL;
	}
	m_sink_mode = FFMPEG_SINK_NONE;
}
