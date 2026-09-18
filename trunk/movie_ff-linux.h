/*
movie_ffmpeg.h -- capture_mode raw: *.raw + *.pcm on disk
                  capture_mode ffmpeg: pipe into ffmpeg.exe -> container per cvar (Win32)

Include quakedef.h before this header (see movie.h).
*/
#ifndef _MOVIE_FFMPEG_H
#define _MOVIE_FFMPEG_H

char ** Movie_FFmpeg_Args(char * cmd);
qboolean Movie_FFmpeg_Encode_Open (const char *dir, const char *stem, int width, int height, int fps, int sample_rate);
qboolean Movie_FFmpeg_Open (const char *dir, const char *stem);
void Movie_FFmpeg_WriteVideo (const byte *pixel_buffer, int size);
void Movie_FFmpeg_WriteAudio (int samples, const byte *sample_buffer);
void Movie_FFmpeg_Close (void);
qboolean Movie_FFmpeg_IsFinalizing (void);
double Movie_FFmpeg_LastFinalizeSeconds (void);

#endif
