/*
Copyright (C) 2002 Quake done Quick

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
// movie.c -- video capturing

#include "quakedef.h"
#include "movie.h"

#ifdef _WIN32
#include "movie_avi.h"
#include "movie_ff-win.h"
#else
#include "movie_ff-linux.h"
#endif

#include "screen.h"

extern	float	scr_con_current;
extern qboolean	scr_drawloading;
extern	short	*snd_out;
extern	int	snd_linear_count, soundtime;
qboolean ready_for_capture = false;

/* Stereo s16le staging for one video frame (and transient overflow on long hitches). */
#define	CAPTURE_AUDIO_MAX_SHORTS	262144	/* ~2.7s @ 48k stereo; max ~1 DMA slice (~32k shorts) fits after flush */

short	capture_audio_samples[CAPTURE_AUDIO_MAX_SHORTS];
int	captured_audio_samples;

/* Exact speed/fps sample count per captured frame (e.g. 11025 Hz @ 60 fps). */
static int	capture_sample_remainder;
static int	capture_samples_this_frame;

static void Movie_FlushCaptureAudio (void);
static void Movie_ResetCaptureAudioSync (void);
static int Movie_AdvanceCaptureAudioSync (void);

static	int	out_size, ssize, outbuf_size;
static	byte	*outbuf, *picture_buf;
static	FILE	*moviefile;

int SCR_ScreenShot (char *name);
int movie_frame_count;

static qboolean OnChange_capture_dir (cvar_t *var, char *string);
cvar_t	capture_dir	= {"capture_dir", "capture", 0, OnChange_capture_dir};
cvar_t	capture_fps	= {"capture_fps", "30"};
#ifdef _WIN32
cvar_t	capture_codec	= {"capture_codec", "0"};
cvar_t	capture_mp3	= {"capture_mp3", "0"};
cvar_t	capture_mp3_kbps = {"capture_mp3_kbps", "128"};
cvar_t	capture_avi = {"capture_avi", "1"};
cvar_t	capture_avi_split = {"capture_avi_split", "1900"};
/* legacy = AVI or TGA (see capture_avi); raw = *_ffmpeg_*.raw|pcm; ffmpeg = pipe to ffmpeg.exe -> stem.mp4 (Win32) */
cvar_t	capture_mode = {"capture_mode", "legacy"};
#endif
/* If non-zero, queue a 'quit' after a capture finishes (manual stop, demo end, or encode abort). */
cvar_t	capture_autoquit = {"capture_autoquit", "0"};
cvar_t	capture_console	= {"capture_console", "1"};
#ifdef _WIN32
cvar_t	capture_ffmpeg_video_buf_mb     = {"capture_ffmpeg_video_buf_mb", "32"};
cvar_t	capture_ffmpeg_audio_buf_mb     = {"capture_ffmpeg_audio_buf_mb", "4"};
cvar_t	capture_ffmpeg_write_timeout_ms = {"capture_ffmpeg_write_timeout_ms", "5000"};
#endif
cvar_t	capture_ffmpeg_loglevel         = {"capture_ffmpeg_loglevel", "error"};
cvar_t	capture_ffmpeg_report           = {"capture_ffmpeg_report", "0"};
cvar_t	capture_ffmpeg_container        = {"capture_ffmpeg_container", "mp4"};
cvar_t	capture_ffmpeg_video_args       = {"capture_ffmpeg_video_args", "-c:v libx264 -preset fast -crf 13 -pix_fmt yuv420p"};
cvar_t	capture_ffmpeg_audio_args       = {"capture_ffmpeg_audio_args", "-c:a aac -b:a 256k -ar 48000"};

static qboolean movie_is_capturing = false;
qboolean	avi_loaded, acm_loaded;

#ifdef _WIN32
static SYSTEMTIME movie_start_date; 
#endif
static int movie_avi_num_segments;
static char movie_avi_path[256];

typedef enum {
	MOVIE_CAP_NONE,
#ifdef _WIN32
	MOVIE_CAP_AVI,
	MOVIE_CAP_RAW,	/* raw RGB + PCM files (capture_mode raw) */
	MOVIE_CAP_TGA,
#endif
	MOVIE_CAP_ENCODE /* piped ffmpeg -> mp4 */
} movie_capture_kind_t;

#ifdef _WIN32
typedef enum {
	CAPMODE_LEGACY,
	CAPMODE_RAW,
	CAPMODE_FFMPEG
} capture_mode_kind_t;
#endif

static movie_capture_kind_t movie_capture_kind = MOVIE_CAP_NONE;

/* capturedemo stats; printed from Movie_StopPlayback */
static qboolean	capture_report_stats;
static int	capture_frames;
static int	capture_fps_used;
static qboolean	capture_was_encode;
static double	capture_t0;
static double	capture_t_capture_end;
static double	capture_t_total_end;

static void Movie_PrintCaptureStats (void)
{
	double	video_sec, wall_capture, wall_total, speed, cap_fps;

	if (!capture_report_stats || capture_frames <= 0)
	{
		capture_report_stats = false;
		return;
	}

	video_sec = (double) capture_frames / (double) capture_fps_used;
	wall_capture = capture_t_capture_end - capture_t0;
	if (wall_capture < 0.001)
		wall_capture = 0.001;
	speed = video_sec / wall_capture;
	cap_fps = (double) capture_frames / wall_capture;

	Con_Printf (
		"capture: %i frames, %.1f s @ %i fps, %.1f s wall (%.2fx), %.1f fps\n",
		capture_frames, video_sec, capture_fps_used, wall_capture, speed, cap_fps);

	if (capture_was_encode)
	{
		wall_total = capture_t_total_end - capture_t0;
		Con_Printf ("finalize: %.1f s\n", Movie_FFmpeg_LastFinalizeSeconds ());
		Con_Printf ("total: %.1f s wall\n", wall_total);
	}

	capture_report_stats = false;
}

void Movie_CancelCaptureStats (void)
{
	capture_report_stats = false;
}
#ifdef _WIN32
static capture_mode_kind_t Movie_ParseCaptureMode (void)
{
	const char *s = capture_mode.string;

	if (!s || !*s)
		return CAPMODE_LEGACY;
	if (!Q_strcasecmp (s, "legacy"))
		return CAPMODE_LEGACY;
	if (!Q_strcasecmp (s, "raw"))
		return CAPMODE_RAW;
	if (!Q_strcasecmp (s, "ffmpeg"))
		return CAPMODE_FFMPEG;
	Con_Printf ("Unknown capture_mode \"%s\" (expected legacy, raw, or ffmpeg); using legacy\n", s);
	return CAPMODE_LEGACY;
}
#endif

qboolean Movie_IsActive (void)
{
	// don't output whilst console is down or 'loading' is displayed
	if ((!capture_console.value && scr_con_current > 0) || scr_drawloading)
		return false;

	// otherwise output if a file is open to write to
	return movie_is_capturing;
}

#ifdef _WIN32
void Movie_StartNewAviSegment()
{
	char segment_name[5];
	char path[sizeof(movie_avi_path) + sizeof(segment_name)];

	if (movie_avi_num_segments > 0)
	{
		COM_StripExtension(movie_avi_path, path);
		Q_snprintfz(segment_name, sizeof(segment_name), "_%03d", movie_avi_num_segments);
		strncat(path, segment_name, sizeof(path));
		COM_ForceExtension(path, ".avi");
	}
	else
	{
		Q_strncpyz(path, movie_avi_path, sizeof(path));
	}

	if (!(moviefile = fopen(path, "wb")))
	{
		COM_CreatePath(path);
		if (!(moviefile = fopen(path, "wb")))
		{
			Con_Printf("ERROR: Couldn't open %s\n", path);
			movie_capture_kind = MOVIE_CAP_NONE;
			return;
		}
	}

	movie_is_capturing = Capture_Open(path);
	if (movie_is_capturing)
	{
		movie_capture_kind = MOVIE_CAP_AVI;
		if (movie_avi_num_segments == 0)
			Movie_ResetCaptureAudioSync ();
	}
	else
		movie_capture_kind = MOVIE_CAP_NONE;
	++movie_avi_num_segments;
}
#endif

void Movie_MaybeAutoQuit (void)
{
	if (capture_autoquit.value)
	{
		Con_Printf ("capture_autoquit: exiting...\n");
		Cbuf_AddText ("quit\n");
	}
}

void Movie_Start_f (void)
{
	char	name[MAX_FILELENGTH], stem[MAX_FILELENGTH], dir[MAX_OSPATH];

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("capture_start <filename> : Start capturing to named file\n");
		return;
	}

	if (Movie_FFmpeg_IsFinalizing ())
	{
		Con_Printf ("Still finalizing previous capture, please wait\n");
		return;
	}

#ifdef _WIN32
	switch (Movie_ParseCaptureMode ())
	{
	case CAPMODE_RAW:
		Q_strncpyz (stem, Cmd_Argv (1), sizeof (stem));
		COM_StripExtension (stem, stem);
		if (!stem[0])
		{
			Con_Printf ("ERROR: Invalid capture name\n");
			return;
		}
		Q_snprintfz (dir, sizeof (dir), "%s", !COM_IsAbsolutePath (capture_dir.string) ? va ("%s/%s", com_basedir, capture_dir.string) : capture_dir.string);
		if (!Movie_FFmpeg_Open (dir, stem))
			return;
		movie_capture_kind = MOVIE_CAP_RAW;
		movie_is_capturing = true;
		Movie_ResetCaptureAudioSync ();
		return;

	case CAPMODE_FFMPEG:
	{
#endif
		int	w, h, fps;
		Q_strncpyz (stem, Cmd_Argv (1), sizeof (stem));
		COM_StripExtension (stem, stem);
		if (!stem[0])
		{
			Con_Printf ("ERROR: Invalid capture name\n");
			return;
		}
		Q_snprintfz (dir, sizeof (dir), "%s", !COM_IsAbsolutePath (capture_dir.string) ? va ("%s/%s", com_basedir, capture_dir.string) : capture_dir.string);
		if (!shm)
		{
			Con_Printf ("ERROR: capture_mode ffmpeg: sound not initialized (shm)\n");
			return;
		}
		fps = (int)(!capture_fps.value ? 30 : bound (10, capture_fps.value, 100000));
	#ifdef GLQUAKE
		w = glwidth;
		h = glheight;
	#else
		w = vid.width;
		h = vid.height;
	#endif
	    Con_Printf("Settings: %d %d %d", w, h, shm->speed);
		if (w <= 0 || h <= 0 || shm->speed <= 0)
		{
			Con_Printf ("ERROR: capture_mode ffmpeg: invalid size or sample rate\n");
			return;
		}
		if (!Movie_FFmpeg_Encode_Open (dir, stem, w, h, fps, shm->speed))
		{
			Con_Printf ("capture_mode ffmpeg failed (need Win32 build, ffmpeg.exe next to exe, usable codecs for capture_ffmpeg_video_args / capture_ffmpeg_audio_args)\n");
			return;
		}
		movie_capture_kind = MOVIE_CAP_ENCODE;
		movie_is_capturing = true;
		Movie_ResetCaptureAudioSync ();
		return;
#ifdef _WIN32
	}

	case CAPMODE_LEGACY:
	default:
		break;
	}

	if (capture_avi.value)
	{
		if (!avi_loaded)
		{
			Con_Printf ("ERROR: AVI capture unavailable (avifil32 not loaded). Use capture_mode raw.\n");
			return;
		}

		Q_strncpyz (name, Cmd_Argv (1), sizeof (name));
		COM_ForceExtension (name, ".avi");

		Q_snprintfz (movie_avi_path, sizeof (movie_avi_path), "%s/%s", !COM_IsAbsolutePath (capture_dir.string) ? va ("%s/%s", com_basedir, capture_dir.string) : capture_dir.string, name);

		movie_avi_num_segments = 0;
		Movie_StartNewAviSegment ();
		return;
	}

	GetLocalTime (&movie_start_date); 

	movie_frame_count = 0;
	movie_capture_kind = MOVIE_CAP_TGA;
	movie_is_capturing = true;
#endif
}

static void Movie_ResetCaptureAudioSync (void)
{
	capture_sample_remainder = 0;
	capture_samples_this_frame = 0;
}

static int Movie_AdvanceCaptureAudioSync (void)
{
	int	fps, samples;

	if (!shm || shm->speed <= 0)
		return 0;

	fps = (int) (!capture_fps.value ? 30 : bound (10, capture_fps.value, 100000));
	capture_sample_remainder += shm->speed;
	samples = capture_sample_remainder / fps;
	capture_sample_remainder %= fps;
	capture_samples_this_frame = samples;
	return samples;
}

void Movie_Stop (void)
{
#ifdef _WIN32
	qboolean	was_avi = (movie_capture_kind == MOVIE_CAP_AVI);
	qboolean	was_raw_or_encode = (movie_capture_kind == MOVIE_CAP_RAW || movie_capture_kind == MOVIE_CAP_ENCODE);
	qboolean	was_capturing = (was_avi || was_raw_or_encode);
#else
	qboolean	was_raw_or_encode = movie_capture_kind == MOVIE_CAP_ENCODE;
	qboolean	was_capturing = was_raw_or_encode;
#endif

	/* Flush partly-buffered PCM so ffmpeg/raw files see audio aligned with video at EOF */
	if (was_raw_or_encode && captured_audio_samples > 0)
		Movie_FlushCaptureAudio ();

	Movie_ResetCaptureAudioSync ();

	if (capture_report_stats)
	{
		capture_t_capture_end = Sys_DoubleTime ();
		capture_was_encode = (movie_capture_kind == MOVIE_CAP_ENCODE);
	}

	movie_is_capturing = false;
	movie_capture_kind = MOVIE_CAP_NONE;

#ifdef _WIN32
	if (was_avi)
	{
		Capture_Close ();
		fclose (moviefile);
	}
#endif
	if (was_raw_or_encode)
		Movie_FFmpeg_Close ();

	/*
	 * Rebaseline the audio subsystem after capture.
	 *
	 *  - S_ResetTime: while capturing, Movie_GetSoundtime drove soundtime by
	 *    samples-per-video-frame (engine-tick rate), pushing paintedtime
	 *    ~capture_seconds ahead of the real DSound DMA cursor. Without
	 *    this, S_Update_ paints nothing until real time catches up and
	 *    audio "freezes" for the length of the captured content.
	 *
	 *  - s_rawend = 0: the BGM streaming write head was advanced in lockstep
	 *    with paintedtime during capture. After resetting paintedtime, the
	 *    mixer would otherwise see s_rawend >= paintedtime and copy a huge
	 *    range of stale samples out of s_rawsamples on top of every paint
	 *    pass — extremely loud garbage that lasts ~capture_seconds.
	 *
	 *  - S_StopAllSounds(true): kills any active channels (their .end times
	 *    are in the now-defunct synthetic timebase) and calls S_ClearBuffer
	 *    which zeros the DSound ring + s_rawsamples. Without it, residual
	 *    samples left looping in the DSound ring leak through as screeching
	 *    until the ring is overwritten by fresh paint.
	 *
	 * Done after movie_capture_kind is cleared so any S_Update_ that fires
	 * from here on uses real DMA time.
	 */
	if (was_capturing)
	{
		S_ResetTime ();
		s_rawend = 0;
		S_StopAllSounds (true);
	}

	if (capture_report_stats)
		capture_t_total_end = Sys_DoubleTime ();
}

void Movie_Stop_f (void)
{
	if (Movie_FFmpeg_IsFinalizing ())
	{
		Con_Printf ("Still finalizing previous capture, please wait\n");
		return;
	}

	if (!movie_is_capturing)
	{
		Con_Printf ("Not capturing\n");
		return;
	}

	Movie_CancelCaptureStats ();

	if (cls.capturedemo)
		cls.capturedemo = false;

	Movie_Stop ();

	Con_Printf ("Stopped capturing\n");
	Movie_MaybeAutoQuit ();
}

void Movie_CaptureDemo_f (void)
{
	if (Cmd_Argc () != 2)
	{
		Con_Printf ("Usage: capturedemo <demoname>\n");
		return;
	}

	if (Movie_FFmpeg_IsFinalizing ())
	{
		Con_Printf ("Still finalizing previous capture, please wait\n");
		return;
	}

#ifndef _WIN32
	if (!ready_for_capture)
	{
		Con_Printf ("Still initializing system...\n");
		char buf[128];
		Q_snprintfz(buf, sizeof(buf) - 1, "wait; wait; wait; wait; wait; wait; wait; capturedemo %s\n", Cmd_Argv(1));
		Cbuf_AddText (buf);
		return;
	}
#else 
	int w, h;
#ifdef GLQUAKE
	w = glwidth;
	h = glheight;
#else
	w = vid.width;
	h = vid.height;
#endif

	if (w <= 0 || h <= 0 || shm->speed <= 0) {
		Con_Printf("Still initializing system...\n");
		char buf[128];
		Q_snprintfz(buf, sizeof(buf) - 1, "wait; wait; wait; wait; wait; wait; wait; capturedemo %s\n", Cmd_Argv(1));
		Cbuf_AddText(buf);
		return;
	}
#endif

	Con_Printf ("Capturing %s.dem\n", Cmd_Argv (1));

	CL_PlayDemo_f ();
	if (!cls.demoplayback)
		return;

	Movie_Start_f ();
	if (!movie_is_capturing)
	{
		Con_Printf ("capturedemo: capture failed, stopping demo playback\n");
		CL_Disconnect ();
		return;
	}

	cls.capturedemo = true;
	capture_report_stats = true;
	capture_frames = 0;
	capture_t0 = Sys_DoubleTime ();
	capture_fps_used = (int) (!capture_fps.value ? 30 : bound (10, capture_fps.value, 100000));
}

void Movie_Init (void)
{
	captured_audio_samples = 0;

	Cmd_AddCommand ("capture_start", Movie_Start_f);
	Cmd_AddCommand ("capture_stop", Movie_Stop_f);
	Cmd_AddCommand ("capturedemo", Movie_CaptureDemo_f);

	Cvar_Register (&capture_fps);
	Cvar_Register (&capture_dir);
	Cvar_Register (&capture_console);
	Cvar_Register (&capture_autoquit);
#ifdef _WIN32
	Cvar_Register (&capture_mode);
	Cvar_Register (&capture_codec);
	Cvar_Register (&capture_avi);
	Cvar_Register (&capture_avi_split);
	Cvar_Register (&capture_ffmpeg_video_buf_mb);
	Cvar_Register (&capture_ffmpeg_audio_buf_mb);
	Cvar_Register (&capture_ffmpeg_write_timeout_ms);
#endif
	Cvar_Register (&capture_ffmpeg_loglevel);
	Cvar_Register (&capture_ffmpeg_report);
	Cvar_Register (&capture_ffmpeg_container);
	Cvar_Register (&capture_ffmpeg_video_args);
	Cvar_Register (&capture_ffmpeg_audio_args);

#ifdef _WIN32
	AVI_LoadLibrary ();
	ACM_LoadLibrary ();
	if (acm_loaded)
	{
		Cvar_Register (&capture_mp3);
		Cvar_Register (&capture_mp3_kbps);
	}
#endif
}

void Movie_StopPlayback (void)
{
	if (!cls.capturedemo)
		return;

	cls.capturedemo = false;
	Movie_Stop ();
	Movie_PrintCaptureStats ();
	Movie_MaybeAutoQuit ();
}

double Movie_FrameTime (void)
{
	return 1.0 / (!capture_fps.value ? 30 : bound (10, capture_fps.value, 100000));
}

void Movie_UpdateScreen (void)
{
	if (!Movie_IsActive ())
		return;

#ifdef _WIN32
	/* TGA sequence: no shared RGB buffer path */
	if (movie_capture_kind == MOVIE_CAP_TGA)
	{
		char name[128];

		Q_snprintfz (name, sizeof (name), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06i.tga",
			capture_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
			movie_start_date.wHour, movie_start_date.wMinute, movie_start_date.wSecond, movie_frame_count);

		movie_frame_count++;
		if (capture_report_stats)
			capture_frames++;
		SCR_ScreenShot (name);
		return;
	}

	if (movie_capture_kind == MOVIE_CAP_AVI)
	{
		if (capture_avi_split.value > 0 && Capture_GetNumWrittenBytes () >= capture_avi_split.value * 1024 * 1024)
		{
			Movie_Stop ();
			Movie_StartNewAviSegment ();
			if (!movie_is_capturing)
				return;
		}
	}

	if (movie_capture_kind == MOVIE_CAP_AVI || movie_capture_kind == MOVIE_CAP_RAW || movie_capture_kind == MOVIE_CAP_ENCODE)
#else
	if (movie_capture_kind == MOVIE_CAP_ENCODE)
#endif
	{
#ifdef GLQUAKE
		int	i, size = glwidth * glheight * 3;
		byte *buffer, temp;

		buffer = Q_malloc (size);
		glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
		ApplyGamma (buffer, size);
#ifdef _WIN32
		if (movie_capture_kind == MOVIE_CAP_AVI)
		{
			for (i = 0 ; i < size ; i += 3)
			{
				temp = buffer[i];
				buffer[i] = buffer[i + 2];
				buffer[i + 2] = temp;
			}
			Capture_WriteVideo (buffer);
		}
		else
#endif
			Movie_FFmpeg_WriteVideo (buffer, size); /* RAW or ENCODE */

		free (buffer);
#else
		int i, j, rowp;
		byte *buffer, *p;

		buffer = Q_malloc (vid.width * vid.height * 3);

		D_EnableBackBufferAccess ();

		p = buffer;
		for (i = vid.height - 1 ; i >= 0 ; i--)
		{
			rowp = i * vid.rowbytes;
			for (j = 0 ; j < vid.width ; j++)
			{
				*p++ = current_pal[vid.buffer[rowp] * 3 + 2];
				*p++ = current_pal[vid.buffer[rowp] * 3 + 1];
				*p++ = current_pal[vid.buffer[rowp] * 3 + 0];
				rowp++;
			}
		}

		D_DisableBackBufferAccess ();

		{
			int size = vid.width * vid.height * 3;
#ifdef _WIN32
			if (movie_capture_kind == MOVIE_CAP_RAW || movie_capture_kind == MOVIE_CAP_ENCODE)
#else
			if (movie_capture_kind == MOVIE_CAP_ENCODE)
#endif
			{
				int k;
				/* software buffer is BGR order; ffmpeg rgb24 wants R,G,B */
				for (k = 0 ; k < size ; k += 3)
				{
					byte t = buffer[k];
					buffer[k] = buffer[k + 2];
					buffer[k + 2] = t;
				}
				Movie_FFmpeg_WriteVideo (buffer, size);
			}
#ifdef _WIN32
			else
				Capture_WriteVideo (buffer);
#endif
		}

		free (buffer);
#endif

		if (capture_report_stats)
			capture_frames++;
	}
}

static void Movie_FlushCaptureAudio (void)
{
	if (captured_audio_samples <= 0)
		return;
#ifdef _WIN32
	if (movie_capture_kind == MOVIE_CAP_RAW || movie_capture_kind == MOVIE_CAP_ENCODE)
#else
	if (movie_capture_kind == MOVIE_CAP_ENCODE)
#endif
		Movie_FFmpeg_WriteAudio (captured_audio_samples, (byte *)capture_audio_samples);
#ifdef _WIN32
	else
		Capture_WriteAudio (captured_audio_samples, (byte *)capture_audio_samples);
#endif
	captured_audio_samples = 0;
}

void Movie_TransferStereo16 (void)
{
	if (!Movie_IsActive ())
		return;
#ifdef _WIN32
	if (movie_capture_kind != MOVIE_CAP_AVI && movie_capture_kind != MOVIE_CAP_RAW && movie_capture_kind != MOVIE_CAP_ENCODE)
#else
	if (movie_capture_kind != MOVIE_CAP_ENCODE)
#endif
		return;

	/* Flush early if a long hitch made host_frametime*speed larger than our staging window. */
	while (captured_audio_samples * 2 + snd_linear_count > CAPTURE_AUDIO_MAX_SHORTS)
	{
		if (captured_audio_samples > 0)
			Movie_FlushCaptureAudio ();
		else
		{
			Con_Printf ("ERROR: capture audio chunk (%i shorts) exceeds buffer\n", snd_linear_count);
			return;
		}
	}

	memcpy (capture_audio_samples + (captured_audio_samples << 1), snd_out, snd_linear_count * shm->channels);
	captured_audio_samples += (snd_linear_count >> 1);

	if (capture_samples_this_frame > 0
	    && captured_audio_samples >= capture_samples_this_frame)
		Movie_FlushCaptureAudio ();
}

qboolean Movie_GetSoundtime (void)
{
	if (!Movie_IsActive ())
		return false;
#ifdef _WIN32
	if (movie_capture_kind != MOVIE_CAP_AVI && movie_capture_kind != MOVIE_CAP_RAW && movie_capture_kind != MOVIE_CAP_ENCODE)
#else
	if (movie_capture_kind != MOVIE_CAP_ENCODE)
#endif
		return false;

	soundtime += Movie_AdvanceCaptureAudioSync ();

	return true;
}

static qboolean OnChange_capture_dir (cvar_t *var, char *string)
{
	if (Movie_IsActive ())
	{
		Con_Printf ("Cannot change capture_dir whilst capturing. Use `capture_stop` to cease capturing first.\n");
		return true;
	}

	return false;
}
