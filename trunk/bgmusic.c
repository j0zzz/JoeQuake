/*
 * Background music handling for Quakespasm (adapted from uHexen2)
 * Handles streaming music as raw sound samples and runs the midi driver
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2010-2018 O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "quakedef.h"
#include "snd_codec.h"
#include "snd_codeci.h"
#include "bgmusic.h"

#define MUSIC_DIRNAME	"music"

qboolean	bgmloop;
static qboolean OnChange_bgm_extmusic (cvar_t *var, char *string);
cvar_t		bgm_extmusic = {"bgm_extmusic", "1", CVAR_ARCHIVE, OnChange_bgm_extmusic};

static qboolean	no_extmusic= false;
static float	old_volume = -1.0f;

typedef enum _bgm_player
{
	BGM_NONE = -1,
	BGM_MIDIDRV = 1,
	BGM_STREAMER
} bgm_player_t;

typedef struct music_handler_s
{
	unsigned int	type;	/* 1U << n (see snd_codec.h)	*/
	bgm_player_t	player;	/* Enumerated bgm player type	*/
	int	is_available;	/* -1 means not present		*/
	const char	*ext;	/* Expected file extension	*/
	const char	*dir;	/* Where to look for music file */
	struct music_handler_s	*next;
} music_handler_t;

static music_handler_t wanted_handlers[] =
{
	{ CODECTYPE_MP3,  BGM_STREAMER, -1, "mp3", MUSIC_DIRNAME, NULL },
	{ CODECTYPE_VORBIS,BGM_STREAMER,-1, "ogg", MUSIC_DIRNAME, NULL },
	{ CODECTYPE_NONE, BGM_NONE,     -1,  NULL,          NULL, NULL }
};

static music_handler_t *music_handlers = NULL;

#define ANY_CODECTYPE	0xFFFFFFFF
#define CDRIP_TYPES	(CODECTYPE_VORBIS | CODECTYPE_MP3)
#define CDRIPTYPE(x)	(((x) & CDRIP_TYPES) != 0)

static snd_stream_t *bgmstream = NULL;

static qboolean OnChange_bgm_extmusic (cvar_t *var, char *string)
{
	float	newval = Q_atof(string);

	if (newval == bgm_extmusic.value)
		return false;
	
	Cvar_SetValue(&bgm_extmusic, newval);
	if (newval)
	{
		if ((cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1))
			BGM_PlayCDtrack((byte)cls.forcetrack, true);
		else
			BGM_PlayCDtrack((byte)cl.cdtrack, true);
	}
	else
		BGM_Stop();

	return true;
}

static void BGM_Play_f (void)
{
	if (Cmd_Argc() == 2) {
		BGM_Play (Cmd_Argv(1));
	}
	else {
		if (bgmstream)
		{
			char path[MAX_QPATH];
			COM_StripExtension (COM_SkipPath (bgmstream->name), path);
			Con_Printf ("Playing %s, use 'music <musicfile>' to change\n", path);
		}
		else
			Con_Printf ("music <musicfile>\n");
	}
}

static void BGM_Pause_f (void)
{
	BGM_Pause ();
}

static void BGM_Resume_f (void)
{
	BGM_Resume ();
}

static void BGM_Loop_f (void)
{
	if (Cmd_Argc() == 2) {
		if (Q_strcasecmp(Cmd_Argv(1),  "0") == 0 ||
				Q_strcasecmp(Cmd_Argv(1),"off") == 0)
			bgmloop = false;
		else if (Q_strcasecmp(Cmd_Argv(1), "1") == 0 ||
				Q_strcasecmp(Cmd_Argv(1),"on") == 0)
			bgmloop = true;
		else if (Q_strcasecmp(Cmd_Argv(1),"toggle") == 0)
			bgmloop = !bgmloop;

		if (bgmstream) bgmstream->loop = bgmloop;
	}

	if (bgmloop)
		Con_Printf("Music will be looped\n");
	else
		Con_Printf("Music will not be looped\n");
}

static void BGM_Stop_f (void)
{
	BGM_Stop();
}

static void BGM_Jump_f (void)
{
	if (Cmd_Argc() != 2) {
		Con_Printf ("music_jump <ordernum>\n");
	}
	else if (bgmstream) {
		S_CodecJumpToOrder(bgmstream, atoi(Cmd_Argv(1)));
	}
}

qboolean BGM_Init (void)
{
	music_handler_t *handlers = NULL;
	int i;

	Cvar_Register(&bgm_extmusic);
	Cmd_AddCommand("music", BGM_Play_f);
	Cmd_AddCommand("music_pause", BGM_Pause_f);
	Cmd_AddCommand("music_resume", BGM_Resume_f);
	Cmd_AddCommand("music_loop", BGM_Loop_f);
	Cmd_AddCommand("music_stop", BGM_Stop_f);
	Cmd_AddCommand("music_jump", BGM_Jump_f);

	if (COM_CheckParm("-noextmusic") != 0)
		no_extmusic = true;

	bgmloop = true;

	for (i = 0; wanted_handlers[i].type != CODECTYPE_NONE; i++)
	{
		switch (wanted_handlers[i].player)
		{
			case BGM_MIDIDRV:
				/* not supported in quake */
				break;
			case BGM_STREAMER:
				wanted_handlers[i].is_available =
					S_CodecIsAvailable(wanted_handlers[i].type);
				break;
			case BGM_NONE:
			default:
				break;
		}
		if (wanted_handlers[i].is_available != -1)
		{
			if (handlers)
			{
				handlers->next = &wanted_handlers[i];
				handlers = handlers->next;
			}
			else
			{
				music_handlers = &wanted_handlers[i];
				handlers = music_handlers;
			}
		}
	}

	return true;
}

void BGM_Shutdown (void)
{
	BGM_Stop();
	/* sever our connections to
	 * midi_drv and snd_codec */
	music_handlers = NULL;
}

static void BGM_Play_noext (const char *filename, unsigned int allowed_types)
{
	char tmp[MAX_QPATH];
	music_handler_t *handler;

	handler = music_handlers;
	while (handler)
	{
		if (! (handler->type & allowed_types))
		{
			handler = handler->next;
			continue;
		}
		if (!handler->is_available)
		{
			handler = handler->next;
			continue;
		}
		Q_snprintfz(tmp, sizeof(tmp), "%s/%s.%s",
				handler->dir, filename, handler->ext);
		switch (handler->player)
		{
			case BGM_MIDIDRV:
				/* not supported in quake */
				break;
			case BGM_STREAMER:
				bgmstream = S_CodecOpenStreamType(tmp, handler->type, bgmloop);
				if (bgmstream)
					return;		/* success */
				break;
			case BGM_NONE:
			default:
				break;
		}
		handler = handler->next;
	}

	Con_Printf("Couldn't handle music file %s\n", filename);
}

void BGM_Play (const char *filename)
{
	char tmp[MAX_QPATH];
	const char *ext;
	music_handler_t *handler;

	BGM_Stop();

	if (music_handlers == NULL)
		return;

	if (!filename || !*filename)
	{
		Con_DPrintf("null music file name\n");
		return;
	}

	ext = COM_FileExtension((char *) filename);
	if (! *ext)	/* try all things */
	{
		BGM_Play_noext(filename, ANY_CODECTYPE);
		return;
	}

	handler = music_handlers;
	while (handler)
	{
		if (handler->is_available &&
				!Q_strcasecmp(ext, handler->ext))
			break;
		handler = handler->next;
	}
	if (!handler)
	{
		Con_Printf("Unhandled extension for %s\n", filename);
		return;
	}
	Q_snprintfz(tmp, sizeof(tmp), "%s/%s", handler->dir, filename);
	switch (handler->player)
	{
		case BGM_MIDIDRV:
			/* not supported in quake */
			break;
		case BGM_STREAMER:
			bgmstream = S_CodecOpenStreamType(tmp, handler->type, bgmloop);
			if (bgmstream)
				return;		/* success */
			break;
		case BGM_NONE:
		default:
			break;
	}

	Con_Printf("Couldn't handle music file %s\n", filename);
}

void BGM_PlayCDtrack (byte track, qboolean looping)
{
	char tmp[MAX_QPATH];
	const char *ext;
	unsigned int type;
	music_handler_t *handler;

	/* if replaying the same track, just resume playing instead of stopping and restarting*/
	if (bgmstream)
	{
		Q_snprintfz(tmp, sizeof (tmp), "%s/track%02d.%s", MUSIC_DIRNAME, track, bgmstream->codec->ext);
		if (strcmp (tmp, bgmstream->name) == 0)
		{
			BGM_Resume ();
			return;
		}
	}

	BGM_Stop();
	/* TODO: change these to be int so they return success
	   if (CDAudio_Play(track, looping) == 0)
	   return;			// success */

	if (music_handlers == NULL)
		return;

	if (no_extmusic || !bgm_extmusic.value)
		return;

	type = 0;
	ext  = NULL;
	handler = music_handlers;
	while (handler)
	{
		if (! handler->is_available)
			goto _next;
		if (! CDRIPTYPE(handler->type))
			goto _next;
		Q_snprintfz(tmp, sizeof(tmp), "%s/track%02d.%s",
				MUSIC_DIRNAME, (int)track, handler->ext);
		if (! COM_FindFile(tmp))
			goto _next;
		type = handler->type;
		ext = handler->ext;
	_next:
		handler = handler->next;
	}
	if (ext == NULL)
		Con_Printf("Couldn't find a cdrip for track %d\n", (int)track);
	else
	{
		Q_snprintfz(tmp, sizeof(tmp), "%s/track%02d.%s",
				MUSIC_DIRNAME, (int)track, ext);
		bgmstream = S_CodecOpenStreamType(tmp, type, bgmloop);
		if (! bgmstream)
			Con_Printf("Couldn't handle music file %s\n", tmp);
	}
}

void BGM_Stop (void)
{
	if (bgmstream)
	{
		bgmstream->status = STREAM_NONE;
		S_CodecCloseStream(bgmstream);
		bgmstream = NULL;
		s_rawend = 0;
	}
}

void BGM_Pause (void)
{
	if (bgmstream)
	{
		if (bgmstream->status == STREAM_PLAY)
		{
			bgmstream->status = STREAM_PAUSE;
			bgmstream->volume = 0.f;
		}
	}
}

void BGM_Resume (void)
{
	if (bgmstream)
	{
		if (bgmstream->status == STREAM_PAUSE)
			bgmstream->status = STREAM_PLAY;
	}
}

static void BGM_UpdateStream (void)
{
	qboolean did_rewind = false;
	int	res;	/* Number of bytes read. */
	int	bufferSamples;
	int	fileSamples;
	int	fileBytes;
	byte	raw[16384];

	if (bgmstream->status != STREAM_PLAY)
		return;

	/* don't bother playing anything if musicvolume is 0 */
	if (bgmvolume.value <= 0)
		return;

	/* see how many samples should be copied into the raw buffer */
	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	while (s_rawend < paintedtime + MAX_RAW_SAMPLES)
	{
		bufferSamples = MAX_RAW_SAMPLES - (s_rawend - paintedtime);

		/* ramp up volume after stream was paused */
		if (bgmstream->volume < 1.f)
		{
			bgmstream->volume += bufferSamples / (bgmstream->info.rate * 1.f);
			bgmstream->volume = 1.f > bgmstream->volume ? bgmstream->volume : 1.f;
		}

		/* decide how much data needs to be read from the file */
		fileSamples = bufferSamples * bgmstream->info.rate / shm->speed;
		if (!fileSamples)
			return;

		/* our max buffer size */
		fileBytes = fileSamples * (bgmstream->info.width * bgmstream->info.channels);
		if (fileBytes > (int) sizeof(raw))
		{
			fileBytes = (int) sizeof(raw);
			fileSamples = fileBytes /
				(bgmstream->info.width * bgmstream->info.channels);
		}

		/* Read */
		res = S_CodecReadStream(bgmstream, fileBytes, raw);
		if (res < fileBytes)
		{
			fileBytes = res;
			fileSamples = res / (bgmstream->info.width * bgmstream->info.channels);
		}

		if (res > 0)	/* data: add to raw buffer */
		{
			S_RawSamples(fileSamples, bgmstream->info.rate,
					bgmstream->info.width,
					bgmstream->info.channels,
					raw, bgmvolume.value * bgmstream->volume);
			did_rewind = false;
		}
		else if (res == 0)	/* EOF */
		{
			if (bgmloop)
			{
				if (did_rewind)
				{
					Con_Printf("Stream keeps returning EOF.\n");
					BGM_Stop();
					return;
				}

				res = S_CodecRewindStream(bgmstream);
				if (res != 0)
				{
					Con_Printf("Stream seek error (%i), stopping.\n", res);
					BGM_Stop();
					return;
				}
				did_rewind = true;
			}
			else
			{
				BGM_Stop();
				return;
			}
		}
		else	/* res < 0: some read error */
		{
			Con_Printf("Stream read error (%i), stopping.\n", res);
			BGM_Stop();
			return;
		}
	}
}

void BGM_Update (void)
{
	if (old_volume != bgmvolume.value)
	{
		if (bgmvolume.value < 0.0)
			Cvar_Set(&bgmvolume, "0.0");
		else if (bgmvolume.value > 1.0)
			Cvar_Set(&bgmvolume, "1.0");
		old_volume = bgmvolume.value;
	}
	if (bgmstream)
		BGM_UpdateStream ();
}

