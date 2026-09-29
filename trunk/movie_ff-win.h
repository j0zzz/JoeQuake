/*
   movie_ffmpeg.h -- capture_mode raw: *.raw + *.pcm on disk
   capture_mode ffmpeg: pipe into ffmpeg.exe -> container per cvar (Win32)

   Include quakedef.h before this header (see movie.h).
   */
#ifndef _MOVIE_FFMPEG_H
#define _MOVIE_FFMPEG_H

#include <windows.h>

/* pipe structure and methods */
enum status_type {
	FFMPEG_OK = 0,
	FFMPEG_CREATE_PIPE,
	FFMPEG_CREATE_PROCESS,
	FFMPEG_WRITE_PIPE,
	FFMPEG_WAIT_FAILURE,
	FFMPEG_OTHER
};

typedef struct pipe_status_s {
	enum status_type type;
	DWORD last_error;

} pipe_status_t;

pipe_status_t * ffmpeg_set_status(enum status_type type);
void ffmpeg_print_status(pipe_status_t * pipe_status);

typedef struct piped_process_s {
	PROCESS_INFORMATION m_procinfo;
	HANDLE m_stdin_r;
	HANDLE m_stdin_w;
	HANDLE m_stdout_r;
	HANDLE m_stdout_w;
	HANDLE m_event;
	DWORD m_timeout_ms;
} piped_process_t;

piped_process_t * ffmpeg_create_piped_process(char * ffmpeg_path, char * ffmpeg_args, char * ffmpeg_dir, pipe_status_t * pipe_status);
void ffmpeg_destruct_piped_process(piped_process_t * process);
qboolean ffmpeg_create_pipe_pair(const char* name, HANDLE* out_read_pipe, HANDLE* out_write_pipe, DWORD buffer_size, DWORD timeout_ms);
pipe_status_t * ffmpeg_write_to_piped_process(piped_process_t * process, const char * data, size_t len);
void ffmpeg_close_piped_process(piped_process_t * process, qboolean terminate);
size_t ffmpeg_read_from_piped_process(piped_process_t * process);

/* functions dispatching the flow */
qboolean Movie_FFmpeg_Encode_Open (const char *dir, const char *stem, int width, int height, int fps, int sample_rate);
qboolean Movie_FFmpeg_Open (const char *dir, const char *stem);
void Movie_FFmpeg_WriteVideo (const byte *pixel_buffer, int size);
void Movie_FFmpeg_WriteAudio (int samples, const byte *sample_buffer);
void Movie_FFmpeg_Close (void);
qboolean Movie_FFmpeg_IsFinalizing (void);
double Movie_FFmpeg_LastFinalizeSeconds (void);

#endif
