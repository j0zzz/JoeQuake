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
*/

#include "quakedef.h"
#include <errno.h>
#include "movie_ff-linux.h"

#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

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

static ffmpeg_sink_mode_t	m_sink_mode = FFMPEG_SINK_NONE;
static FILE			*m_ffmpeg_video;
static FILE			*m_ffmpeg_audio;

static int           ffmpeg_pipe_video = 0;
static int           ffmpeg_pipe_audio = 0;


static char				outpath_combined[MAX_OSPATH];
static char				outpath_video[MAX_OSPATH];
static char				outpath_audio[MAX_OSPATH];
static char			m_stderr_path[MAX_OSPATH];
static char			m_outpath[MAX_OSPATH];
static qboolean			m_encode_aborting	= false;
static qboolean			m_finalizing		= false;
static double			m_finalize_seconds	= 0;

static char				pipename_audio[128];
static char				pipename_video[128];

/* the output from this function is always used in forked/execvp 
 * processes, so no need to worry about freeing the memory */
#define MOVIE_ARGC 256 /* amount of args */
#define MOVIE_ARGLEN 256 /* len of one arg */
char ** Movie_FFmpeg_Args(char * cmd)
{
	char ** argv = Q_calloc(MOVIE_ARGC, sizeof(char*));
	/* not using strtok since it's stupid and unsafe
	 * doing this without strtok is what peak performance 
	 * looks like. don't use that dumb standard lib function */
	char * s = cmd, *e = cmd, *q, *r;
	int i = 0;
	int quoted = 0;

	while (*e != '\0')
		e++;

	q = s;
	while (q < e) {
		while (*q == ' ')
			q++;

		if (q == e)
			break;

		if (*q == '"')
			quoted = 1;

		r = q;
		if (!quoted) {
			while (*r != ' ' && *r != '\0')
				r++;
		} else {
			r++;
			while (*r != '"' && *r != '\0')
				r++;
		}

		argv[i] = Q_calloc(MOVIE_ARGLEN, sizeof(char));
		Q_strncpy(argv[i], q + quoted, r - q - quoted);

		if (quoted) {
			r++;
			quoted = 0;
		}

		q = r;
		i++;
	}

	return argv;
}

qboolean Movie_FFmpeg_Encode_Open (const char *dir, const char *stem, int width, int height, int fps, int sample_rate)
{
	(void)dir;
	(void)stem;
	(void)width;
	(void)height;
	(void)fps;
	(void)sample_rate;

	pid_t pid;

	int status = 0;
	char				cmdline[1024 * 16];
	Movie_FFmpeg_Close ();

	if (!stem || !stem[0] || width <= 0 || height <= 0 || fps <= 0 || sample_rate <= 0)
		return false;

	{
		const char *ext = capture_ffmpeg_container.string ? capture_ffmpeg_container.string : "mp4";

		if (strcmp (ext, "mp4") != 0 && strcmp (ext, "mkv") != 0)
		{
			Con_Printf ("WARNING: capture_ffmpeg_container '%s' not in {mp4,mkv}, using mp4\n", ext);
			ext = "mp4";
		}
		Q_snprintfz (outpath_video, sizeof (outpath_video), "%s/%s.video.%s", dir, stem, ext);
		Q_snprintfz (outpath_audio, sizeof (outpath_audio), "%s/%s.audio.%s", dir, stem, ext);
		Q_snprintfz (outpath_combined, sizeof (outpath_combined), "%s/%s.%s", dir, stem, ext);
	}
	COM_CreatePath (outpath_video);

	pid = getpid();
	long now = time(NULL);
	Q_snprintfz (
		pipename_audio,
		sizeof (pipename_audio),
		"%s/a_%d_%lu.pipe", "/tmp", pid, now
	);
	Q_snprintfz (
		pipename_video,
		sizeof (pipename_video),
		"%s/v_%d_%lu.pipe", "/tmp", pid, now
	);

	if (mkfifo(pipename_video, 0666))
	{
		Con_Printf("Failed opening pipe %s!\n", pipename_video);
		return false;
	}

	if (mkfifo(pipename_audio, 0666))
	{
		Con_Printf("Failed opening pipe %s!\n", pipename_audio);
		return false;
	}

	switch (pid = fork()) {
		case -1:
			Con_Printf("Error forking process!\n");
			return false;
		case 0:
			Q_snprintfz(
				cmdline, sizeof(cmdline),
				"ffmpeg -hide_banner -loglevel error -report -y "
				"-f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate %d -i %s "
				"-map 0:v -vf vflip -shortest %s \"%s\"",
				width, height, fps, pipename_video,
				capture_ffmpeg_video_args.string, outpath_video
			);

			Con_Printf("%s\n", cmdline);
			status = execvp("ffmpeg", Movie_FFmpeg_Args(cmdline));
			Con_Printf("Error starting: %d %d\n", status, errno);
			return false;
		default:
			break;
	}

	switch (pid = fork()) {
		case -1:
			Con_Printf("Error forking process!\n");
			return false;
		case 0:
			Q_snprintfz(
				cmdline, sizeof(cmdline),
				"ffmpeg -hide_banner -loglevel error -report -y "
				"-f s16le -ac 2 -ar %d -i %s "
				"-map 0:a %s \"%s\"",
				sample_rate, pipename_audio,
				capture_ffmpeg_audio_args.string, outpath_audio
			);
			status = execvp("ffmpeg", Movie_FFmpeg_Args(cmdline));
			Con_Printf("Error starting: %d %d\n", status, errno);
			return false;
		default:
			break;
	}

	ffmpeg_pipe_audio = open(pipename_audio, O_WRONLY);
	ffmpeg_pipe_video = open(pipename_video, O_WRONLY);

	return true;
}

void Movie_FFmpeg_WriteVideo (const byte *pixel_buffer, int size)
{
	if (!pixel_buffer || size <= 0)
		return;

	if (!ffmpeg_pipe_video)
		return;

	if (write (ffmpeg_pipe_video, pixel_buffer, size) != size)
		Con_Printf ("ERROR: ffmpeg video write %d bytes failed ERRNO %d\n", size, errno);

}

void Movie_FFmpeg_WriteAudio (int samples, const byte *sample_buffer)
{
	int	nbytes;

	if (!sample_buffer || samples <= 0)
		return;
	nbytes = samples * 4;

	if (!ffmpeg_pipe_audio)
		return;

	if (write (ffmpeg_pipe_audio, sample_buffer, nbytes) != nbytes)
		Con_Printf ("ERROR: ffmpeg audio write %d bytes failed ERRNO %d\n", nbytes, errno);

}

void Movie_FFmpeg_Close (void)
{
	pid_t pid;
	int status = 0;
	char cmdline[16*1024];
	if (!ffmpeg_pipe_video && !ffmpeg_pipe_audio)
		return;

	if (ffmpeg_pipe_video)
		close(ffmpeg_pipe_video);

	if (ffmpeg_pipe_audio)
		close(ffmpeg_pipe_audio);

	ffmpeg_pipe_audio = 0;
	ffmpeg_pipe_video = 0;

	SCR_EndLoadingPlaque();
	Con_Printf("Muxing...\n");

	waitpid(0, &status, 0);
	if (!WIFEXITED(status))
		Con_Printf("Error quitting ffmpeg!\n");
	waitpid(0, &status, 0);
	if (!WIFEXITED(status))
		Con_Printf("Error quitting ffmpeg!\n");

	unlink(pipename_audio);
	unlink(pipename_video);

	switch (pid = fork()) {
		case -1:
			Con_Printf("Error forking process!\n");
			return;
		case 0:
			Q_snprintfz(cmdline, sizeof(cmdline), 
				"ffmpeg -y -i \"%s\" -i \"%s\" -c:v copy -c:a copy -map 0:v:0 -map 1:a:0 \"%s\"",
				outpath_video, outpath_audio, outpath_combined
			);

			status = execvp("ffmpeg", Movie_FFmpeg_Args(cmdline));
			Con_Printf("Error starting: %d %d\n", status, errno);
			return;
		default:
			m_sink_mode = FFMPEG_SINK_NONE;
			waitpid(0, NULL, 0);
			if (!WIFEXITED(status))
				Con_Printf("Error quitting mux ffmpeg!\n");
			Con_Printf("Muxing completed to filename %s\n", outpath_combined);
			return;
	}

}

qboolean Movie_FFmpeg_IsFinalizing (void)
{
	return m_finalizing;
}

double Movie_FFmpeg_LastFinalizeSeconds (void)
{
	return 0;
}
