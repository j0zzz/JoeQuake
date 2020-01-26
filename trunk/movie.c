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
#include "movie_avi.h"
#include "screen.h"

extern	float	scr_con_current;
extern qboolean	scr_drawloading;
extern	short	*snd_out;
extern	int	snd_linear_count, soundtime;

// Variables for buffering audio
short	capture_audio_samples[44100];	// big enough buffer for 1fps at 44100Hz
int	captured_audio_samples;

static	int	out_size, ssize, outbuf_size;
static	byte	*outbuf, *picture_buf;
static	FILE	*moviefile;

int SCR_ScreenShot (char *name);
int movie_frame_count;

static qboolean OnChange_capture_dir (cvar_t *var, char *string);
cvar_t	capture_dir	= {"capture_dir", "capture", 0, OnChange_capture_dir};

cvar_t	capture_codec	= {"capture_codec", "0"};
cvar_t	capture_fps	= {"capture_fps", "30"};
cvar_t	capture_console	= {"capture_console", "1"};
cvar_t	capture_mp3	= {"capture_mp3", "0"};
cvar_t	capture_mp3_kbps = {"capture_mp3_kbps", "128"};
cvar_t	capture_avi = {"capture_avi", "1"};
// By default split AVI files at 1900 Megabytes, to avoid running over the
// 2 Gigabyte limit of old format AVI files.
cvar_t	capture_avi_split = {"capture_avi_split", "1900"};

static qboolean movie_is_capturing = false;
qboolean	avi_loaded, acm_loaded;

static SYSTEMTIME movie_start_date; 

static int movie_avi_num_segments;
static char movie_avi_path[256];

qboolean Movie_IsActive (void)
{
	// don't output whilst console is down or 'loading' is displayed
	if ((!capture_console.value && scr_con_current > 0) || scr_drawloading)
		return false;

	// otherwise output if a file is open to write to
	return movie_is_capturing;
}

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
			return;
		}
	}

	movie_is_capturing = Capture_Open(path);
	++movie_avi_num_segments;
}

void Movie_Start_f (void)
{
	char	name[MAX_FILELENGTH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("capture_start <filename> : Start capturing to named file\n");
		return;
	}

	if (capture_avi.value)
	{
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
		COM_ForceExtension (name, ".avi");

		Q_snprintfz (movie_avi_path, sizeof(movie_avi_path), "%s/%s", !COM_IsAbsolutePath(capture_dir.string) ? va("%s/%s", com_basedir, capture_dir.string) : capture_dir.string, name);

		movie_avi_num_segments = 0;
		Movie_StartNewAviSegment();
	}
	else
	{
		GetLocalTime(&movie_start_date); 

		movie_frame_count = 0;
		movie_is_capturing = true;
	}
}

void Movie_Stop (void)
{
	movie_is_capturing = false;

	if (capture_avi.value)
	{
		Capture_Close ();
		fclose (moviefile);
	}
}

void Movie_Stop_f (void)
{
	if (!Movie_IsActive())
	{
		Con_Printf ("Not capturing\n");
		return;
	}

	if (cls.capturedemo)
		cls.capturedemo = false;

	Movie_Stop ();

	Con_Printf ("Stopped capturing\n");
}

void Movie_CaptureDemo_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: capturedemo <demoname>\n");
		return;
	}

	Con_Printf ("Capturing %s.dem\n", Cmd_Argv(1));

	CL_PlayDemo_f ();
	if (!cls.demoplayback)
		return;

	Movie_Start_f ();
	cls.capturedemo = true;

	if (!movie_is_capturing)
		Movie_StopPlayback ();
}

void Movie_Init (void)
{
	AVI_LoadLibrary ();
	if (!avi_loaded)
		return;

	captured_audio_samples = 0;

	Cmd_AddCommand ("capture_start", Movie_Start_f);
	Cmd_AddCommand ("capture_stop", Movie_Stop_f);
	Cmd_AddCommand ("capturedemo", Movie_CaptureDemo_f);
	Cvar_Register (&capture_codec);
	Cvar_Register (&capture_fps);
	Cvar_Register (&capture_dir);
	Cvar_Register (&capture_console);
	Cvar_Register (&capture_avi);
	Cvar_Register (&capture_avi_split);

	ACM_LoadLibrary ();
	if (!acm_loaded)
		return;

	Cvar_Register (&capture_mp3);
	Cvar_Register (&capture_mp3_kbps);
}

void Movie_StopPlayback (void)
{
	if (!cls.capturedemo)
		return;

	cls.capturedemo = false;
	Movie_Stop ();
}

double Movie_FrameTime (void)
{
	return 1.0 / (!capture_fps.value ? 30 : bound(10, capture_fps.value, 1000));
}

void Movie_UpdateScreen (void)
{
	if (!Movie_IsActive())
		return;

	if (capture_avi.value)
	{
		if (capture_avi_split.value > 0 && Capture_GetNumWrittenBytes() >= capture_avi_split.value * 1024 * 1024)
		{
			Movie_Stop();
			Movie_StartNewAviSegment();
		}

#ifdef GLQUAKE
		int	i, size = glwidth * glheight * 3;
		byte *buffer, temp;

		buffer = Q_malloc (size);
		glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
		ApplyGamma (buffer, size);

		for (i = 0 ; i < size ; i += 3)
		{
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}
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
				*p++ = current_pal[vid.buffer[rowp]*3+2];
				*p++ = current_pal[vid.buffer[rowp]*3+1];
				*p++ = current_pal[vid.buffer[rowp]*3+0];
				rowp++;
			}
		}

		D_DisableBackBufferAccess ();
#endif

		Capture_WriteVideo (buffer);

		free (buffer);
	}
	else
	{
		char name[128];

		Q_snprintfz(name, sizeof(name), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06i.tga",
		capture_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
		movie_start_date.wHour, movie_start_date.wMinute, movie_start_date.wSecond, movie_frame_count);
		
		movie_frame_count++;
		SCR_ScreenShot(name);
	}
}

void Movie_TransferStereo16 (void)
{
	if (!Movie_IsActive() || !capture_avi.value)
		return;

	// Copy last audio chunk written into our temporary buffer
	memcpy (capture_audio_samples + (captured_audio_samples << 1), snd_out, snd_linear_count * shm->channels);
	captured_audio_samples += (snd_linear_count >> 1);

	if (captured_audio_samples >= Q_rint (host_frametime * shm->speed))
	{
		// We have enough audio samples to match one frame of video
		Capture_WriteAudio (captured_audio_samples, (byte *)capture_audio_samples);
		captured_audio_samples = 0;
	}
}

qboolean Movie_GetSoundtime (void)
{
	if (!Movie_IsActive() || !capture_avi.value)
		return false;

	soundtime += Q_rint (host_frametime * shm->speed * (Movie_FrameTime() / host_frametime));

	return true;
}

static qboolean OnChange_capture_dir (cvar_t *var, char *string)
{
	if (Movie_IsActive())
	{
		Con_Printf ("Cannot change capture_dir whilst capturing. Use `capture_stop` to cease capturing first.\n");
		return true;
	}

	return false;
}
