/*
Copyright (C) 2001 Quake done Quick

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
// movie_avi.c

#include "quakedef.h"
#include "movie_avi.h"
#include <windows.h>
#include <vfw.h>

static void (CALLBACK *qAVIFileInit)(void);
static HRESULT (CALLBACK *qAVIFileOpen)(PAVIFILE *, LPCTSTR, UINT, LPCLSID);
static HRESULT (CALLBACK *qAVIFileCreateStream)(PAVIFILE, PAVISTREAM *, AVISTREAMINFO *);
static HRESULT (CALLBACK *qAVIMakeCompressedStream)(PAVISTREAM *, PAVISTREAM, AVICOMPRESSOPTIONS *, CLSID *);
static HRESULT (CALLBACK *qAVIStreamSetFormat)(PAVISTREAM, LONG, LPVOID, LONG);
static HRESULT (CALLBACK *qAVIStreamWrite)(PAVISTREAM, LONG, LONG, LPVOID, LONG, DWORD, LONG *, LONG *);
static ULONG (CALLBACK *qAVIStreamRelease)(PAVISTREAM);
static ULONG (CALLBACK *qAVIFileRelease)(PAVIFILE);
static void (CALLBACK *qAVIFileExit)(void);

static MMRESULT (ACMAPI *qacmDriverOpen)(LPHACMDRIVER, HACMDRIVERID, DWORD);
static MMRESULT (ACMAPI *qacmDriverDetails)(HACMDRIVERID, LPACMDRIVERDETAILS, DWORD);
static MMRESULT (ACMAPI *qacmDriverEnum)(ACMDRIVERENUMCB, DWORD, DWORD);
static MMRESULT (ACMAPI *qacmFormatTagDetails)(HACMDRIVER, LPACMFORMATTAGDETAILS, DWORD);
static MMRESULT (ACMAPI *qacmStreamOpen)(LPHACMSTREAM, HACMDRIVER, LPWAVEFORMATEX, LPWAVEFORMATEX, LPWAVEFILTER, DWORD, DWORD, DWORD);
static MMRESULT (ACMAPI *qacmStreamSize)(HACMSTREAM, DWORD, LPDWORD, DWORD);
static MMRESULT (ACMAPI *qacmStreamPrepareHeader)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamUnprepareHeader)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamConvert)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamClose)(HACMSTREAM, DWORD);
static MMRESULT (ACMAPI *qacmDriverClose)(HACMDRIVER, DWORD);

static HINSTANCE avi_handle = NULL, acm_handle = NULL;

PAVIFILE	m_file;
PAVISTREAM	m_uncompressed_video_stream, m_compressed_video_stream, m_audio_stream;

unsigned long m_codec_fourcc;
int			m_video_frame_counter;
int			m_video_frame_size;
LONG		m_bytes_written;

qboolean	m_audio_is_mp3;
int			m_audio_frame_counter;
WAVEFORMATEX m_wave_format;
MPEGLAYER3WAVEFORMAT m_mp3_format;
qboolean	mp3_encoder_exists;
HACMDRIVER	m_mp3_driver;
HACMSTREAM	m_mp3_stream;
ACMSTREAMHEADER	m_mp3_stream_header;

extern qboolean avi_loaded, acm_loaded;
extern	cvar_t	capture_codec, capture_fps, capture_mp3, capture_mp3_kbps;

#define AVI_GETFUNC(f) (qAVI##f = (void *)GetProcAddress(avi_handle, "AVI" #f))
#define ACM_GETFUNC(f) (qacm##f = (void *)GetProcAddress(acm_handle, "acm" #f))

void AVI_LoadLibrary (void)
{
	avi_loaded = false;

	if (!(avi_handle = LoadLibrary("avifil32.dll")))
	{
		Con_Printf ("\x02" "Avi capturing module not found\n");
		goto fail;
	}

	AVI_GETFUNC(FileInit);
	AVI_GETFUNC(FileOpen);
	AVI_GETFUNC(FileCreateStream);
	AVI_GETFUNC(MakeCompressedStream);
	AVI_GETFUNC(StreamSetFormat);
	AVI_GETFUNC(StreamWrite);
	AVI_GETFUNC(StreamRelease);
	AVI_GETFUNC(FileRelease);
	AVI_GETFUNC(FileExit);

	avi_loaded = qAVIFileInit && qAVIFileOpen && qAVIFileCreateStream && 
			qAVIMakeCompressedStream && qAVIStreamSetFormat && qAVIStreamWrite && 
			qAVIStreamRelease && qAVIFileRelease && qAVIFileExit;

	if (!avi_loaded)
	{
		Con_Printf ("\x02" "Avi capturing module not initialized\n");
		goto fail;
	}

	Con_Printf ("Avi capturing module initialized\n");
	return;

fail:
	if (avi_handle)
	{
		FreeLibrary (avi_handle);
		avi_handle = NULL;
	}
}

void ACM_LoadLibrary (void)
{
	acm_loaded = false;

	if (!(acm_handle = LoadLibrary("msacm32.dll")))
	{
		Con_Printf ("\x02" "ACM module not found\n");
		goto fail;
	}

	ACM_GETFUNC(DriverOpen);
	ACM_GETFUNC(DriverEnum);
	ACM_GETFUNC(StreamOpen);
	ACM_GETFUNC(StreamSize);
	ACM_GETFUNC(StreamPrepareHeader);
	ACM_GETFUNC(StreamUnprepareHeader);
	ACM_GETFUNC(StreamConvert);
	ACM_GETFUNC(StreamClose);
	ACM_GETFUNC(DriverClose);
	qacmDriverDetails = (void *)GetProcAddress (acm_handle, "acmDriverDetailsA");
	qacmFormatTagDetails = (void *)GetProcAddress (acm_handle, "acmFormatTagDetailsA");

	acm_loaded = qacmDriverOpen && qacmDriverDetails && qacmDriverEnum && 
			qacmFormatTagDetails && qacmStreamOpen && qacmStreamSize && 
			qacmStreamPrepareHeader && qacmStreamUnprepareHeader && 
			qacmStreamConvert && qacmStreamClose && qacmDriverClose;

	if (!acm_loaded)
	{
		Con_Printf ("\x02" "ACM module not initialized\n");
		goto fail;
	}

	Con_Printf ("ACM module initialized\n");
	return;

fail:
	if (acm_handle)
	{
		FreeLibrary (acm_handle);
		acm_handle = NULL;
	}
}

PAVISTREAM Capture_VideoStream (void)
{
	return m_codec_fourcc ? m_compressed_video_stream : m_uncompressed_video_stream;
}

BOOL CALLBACK acmDriverEnumCallback (HACMDRIVERID mp3_driver_id, DWORD dwInstance, DWORD fdwSupport)
{
	if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC)
	{
		int	i;
		ACMDRIVERDETAILS drvDetails;

		memset (&drvDetails, 0, sizeof(drvDetails));
		drvDetails.cbStruct = sizeof(drvDetails);
		qacmDriverDetails (mp3_driver_id, &drvDetails, 0);
		qacmDriverOpen (&m_mp3_driver, mp3_driver_id, 0);

		for (i = 0 ; i < drvDetails.cFormatTags ; i++)
		{
			ACMFORMATTAGDETAILS	fmtDetails;

			memset (&fmtDetails, 0, sizeof(fmtDetails));
			fmtDetails.cbStruct = sizeof(fmtDetails);
			fmtDetails.dwFormatTagIndex = i;
			qacmFormatTagDetails (m_mp3_driver, &fmtDetails, ACM_FORMATTAGDETAILSF_INDEX);
			if (fmtDetails.dwFormatTag == WAVE_FORMAT_MPEGLAYER3)
			{
				MMRESULT	mmr;

				Con_DPrintf ("MP3-capable ACM codec found: %s\n", drvDetails.szLongName);

				m_mp3_stream = NULL;
				if ((mmr = qacmStreamOpen(&m_mp3_stream, m_mp3_driver, &m_wave_format, &m_mp3_format.wfx, NULL, 0, 0, 0)))
				{
					switch (mmr)
					{
					case MMSYSERR_INVALPARAM:
						Con_DPrintf ("ERROR: Invalid parameters passed to acmStreamOpen\n");
						break;

					case ACMERR_NOTPOSSIBLE:
						Con_DPrintf ("ERROR: No ACM filter found capable of encoding MP3\n");
						break;

					default:
						Con_DPrintf ("ERROR: Couldn't open ACM encoding stream\n");
						break;
					}
					continue;
				}
				mp3_encoder_exists = true;

				return false;
			}
		}

		qacmDriverClose (m_mp3_driver, 0);
	}

	return true;
}

qboolean Capture_Open (char *filename)
{
	HRESULT			hr;
	BITMAPINFOHEADER bitmap_info_header;
	AVISTREAMINFO	stream_header;
	char			*fourcc;

	m_video_frame_counter = m_audio_frame_counter = 0;
	m_bytes_written = 0;
	m_file = NULL;
	m_codec_fourcc = 0;
	m_uncompressed_video_stream = m_compressed_video_stream = m_audio_stream = NULL;
	m_audio_is_mp3 = (qboolean)capture_mp3.value;

	if (*(fourcc = capture_codec.string) != '0')	// codec fourcc supplied
		m_codec_fourcc = mmioFOURCC (fourcc[0], fourcc[1], fourcc[2], fourcc[3]);

	qAVIFileInit ();
	hr = qAVIFileOpen (&m_file, filename, OF_WRITE | OF_CREATE, NULL);
	if (FAILED(hr))
	{
		Con_Printf ("ERROR: Couldn't open AVI file\n");
		return false;
	}

	// initialize video data
#ifdef GLQUAKE
	m_video_frame_size = glwidth * glheight * 3;
#else
	m_video_frame_size = vid.width * vid.height * 3;
#endif

	memset (&bitmap_info_header, 0, sizeof(bitmap_info_header));
	bitmap_info_header.biSize = sizeof(BITMAPINFOHEADER);
#ifdef GLQUAKE
	bitmap_info_header.biWidth = glwidth;
	bitmap_info_header.biHeight = glheight;
#else
	bitmap_info_header.biWidth = vid.width;
	bitmap_info_header.biHeight = vid.height;
#endif
	bitmap_info_header.biPlanes = 1;
	bitmap_info_header.biBitCount = 24;
	bitmap_info_header.biCompression = BI_RGB;
	bitmap_info_header.biSizeImage = m_video_frame_size;

	memset (&stream_header, 0, sizeof(stream_header));
	stream_header.fccType = streamtypeVIDEO;
	stream_header.fccHandler = m_codec_fourcc;
	stream_header.dwScale = 1;
	stream_header.dwRate = (unsigned long)(0.5 + capture_fps.value);
	stream_header.dwSuggestedBufferSize = bitmap_info_header.biSizeImage;
	SetRect (&stream_header.rcFrame, 0, 0, bitmap_info_header.biWidth, bitmap_info_header.biHeight);

	hr = qAVIFileCreateStream (m_file, &m_uncompressed_video_stream, &stream_header);
	if (FAILED(hr))
	{
		Con_Printf ("ERROR: Couldn't create video stream\n");
		return false;
	}

	if (m_codec_fourcc)
	{
		AVICOMPRESSOPTIONS	opts;

		memset (&opts, 0, sizeof(opts));
		opts.fccType = stream_header.fccType;
		opts.fccHandler = m_codec_fourcc;

		// Make the stream according to compression
		hr = qAVIMakeCompressedStream (&m_compressed_video_stream, m_uncompressed_video_stream, &opts, NULL);
		if (FAILED(hr))
		{
			Con_Printf ("ERROR: Couldn't make compressed video stream\n");
			return false;
		}
	}

	hr = qAVIStreamSetFormat (Capture_VideoStream(), 0, &bitmap_info_header, bitmap_info_header.biSize);
	if (FAILED(hr))
	{
		Con_Printf ("ERROR: Couldn't set video stream format\n");
		return false;
	}

	// initialize audio data
	memset (&m_wave_format, 0, sizeof(m_wave_format));
	m_wave_format.wFormatTag = WAVE_FORMAT_PCM;
	m_wave_format.nChannels = 2;		// always stereo in Quake sound engine
	m_wave_format.nSamplesPerSec = shm->speed;
	m_wave_format.wBitsPerSample = 16;	// always 16bit in Quake sound engine
	m_wave_format.nBlockAlign = m_wave_format.wBitsPerSample/8 * m_wave_format.nChannels;
	m_wave_format.nAvgBytesPerSec = m_wave_format.nSamplesPerSec * m_wave_format.nBlockAlign;

	memset (&stream_header, 0, sizeof(stream_header));
	stream_header.fccType = streamtypeAUDIO;
	stream_header.dwScale = m_wave_format.nBlockAlign;
	stream_header.dwRate = stream_header.dwScale * (unsigned long)m_wave_format.nSamplesPerSec;
	stream_header.dwSampleSize = m_wave_format.nBlockAlign;

	hr = qAVIFileCreateStream (m_file, &m_audio_stream, &stream_header);
	if (FAILED(hr))
	{
		Con_Printf ("ERROR: Couldn't create audio stream\n");
		return false;
	}

	if (m_audio_is_mp3)
	{
		memset (&m_mp3_format, 0, sizeof(m_mp3_format));
		m_mp3_format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
		m_mp3_format.wfx.nChannels = 2;
		m_mp3_format.wfx.nSamplesPerSec = shm->speed;
		m_mp3_format.wfx.wBitsPerSample = 0;
		m_mp3_format.wfx.nBlockAlign = 1;
		m_mp3_format.wfx.nAvgBytesPerSec = capture_mp3_kbps.value * 125;
		m_mp3_format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
		m_mp3_format.wID = MPEGLAYER3_ID_MPEG;
		m_mp3_format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
		m_mp3_format.nBlockSize = m_mp3_format.wfx.nAvgBytesPerSec / capture_fps.value;
		m_mp3_format.nFramesPerBlock = 1;
		m_mp3_format.nCodecDelay = 1393;

		// try to find an MP3 codec
		m_mp3_driver = NULL;
		mp3_encoder_exists = false;
		qacmDriverEnum (acmDriverEnumCallback, 0, 0);
		if (!mp3_encoder_exists)
		{
			Con_Printf ("ERROR: Couldn't find any MP3 encoder\n");
			return false;
		}

		hr = qAVIStreamSetFormat (m_audio_stream, 0, &m_mp3_format, sizeof(MPEGLAYER3WAVEFORMAT));
		if (FAILED(hr))
		{
			Con_Printf ("ERROR: Couldn't set audio stream format\n");
			return false;
		}
	}
	else
	{
		hr = qAVIStreamSetFormat (m_audio_stream, 0, &m_wave_format, sizeof(WAVEFORMATEX));
		if (FAILED(hr))
		{
			Con_Printf ("ERROR: Couldn't set audio stream format\n");
			return false;
		}
	}

	return true;
}

void Capture_Close (void)
{
	if (m_uncompressed_video_stream)
		qAVIStreamRelease (m_uncompressed_video_stream);
	if (m_compressed_video_stream)
		qAVIStreamRelease (m_compressed_video_stream);
	if (m_audio_stream)
		qAVIStreamRelease (m_audio_stream);
	if (m_audio_is_mp3)
	{
		qacmStreamClose (m_mp3_stream, 0);
		qacmDriverClose (m_mp3_driver, 0);
	}
	if (m_file)
		qAVIFileRelease (m_file);

	m_bytes_written = 0;
	qAVIFileExit ();
}

void Capture_WriteVideo (byte *pixel_buffer)
{
	HRESULT	hr;
	LONG frame_bytes_written;
#ifdef GLQUAKE
	int	size = glwidth * glheight * 3;
#else
	int	size = vid.width * vid.height * 3;
#endif

	if (m_video_frame_size != size)
	{
		Con_Printf ("ERROR: Frame size changed\n");
		return;
	}

	if (!Capture_VideoStream())
	{
		Con_Printf ("ERROR: Video stream is NULL\n");
		return;
	}

	hr = qAVIStreamWrite (Capture_VideoStream(), m_video_frame_counter++, 1, pixel_buffer, m_video_frame_size, AVIIF_KEYFRAME, NULL, &frame_bytes_written);
	if (FAILED(hr))
	{
		Con_Printf ("ERROR: Couldn't write video stream\n");
		return;
	}

	m_bytes_written += frame_bytes_written;
}

void Capture_WriteAudio (int samples, byte *sample_buffer)
{
	HRESULT		hr;
	LONG		frame_bytes_written;
	unsigned long sample_bufsize;

	if (!m_audio_stream)
	{
		Con_Printf ("ERROR: Audio stream is NULL\n");
		return;
	}

	sample_bufsize = samples * m_wave_format.nBlockAlign;
	if (m_audio_is_mp3)
	{
		MMRESULT	mmr;
		byte		*mp3_buffer;
		unsigned long mp3_bufsize;

		if ((mmr = qacmStreamSize(m_mp3_stream, sample_bufsize, &mp3_bufsize, ACM_STREAMSIZEF_SOURCE)))
		{
			Con_Printf ("ERROR: Couldn't get mp3bufsize\n");
			return;
		}
		if (!mp3_bufsize)
		{
			Con_Printf ("ERROR: mp3bufsize is zero\n");
			return;
		}
		mp3_buffer = Q_calloc (mp3_bufsize, 1);

		memset (&m_mp3_stream_header, 0, sizeof(m_mp3_stream_header));
		m_mp3_stream_header.cbStruct = sizeof(m_mp3_stream_header);
		m_mp3_stream_header.pbSrc = sample_buffer;
		m_mp3_stream_header.cbSrcLength = sample_bufsize;
		m_mp3_stream_header.pbDst = mp3_buffer;
		m_mp3_stream_header.cbDstLength = mp3_bufsize;

		if ((mmr = qacmStreamPrepareHeader(m_mp3_stream, &m_mp3_stream_header, 0)))
		{
			Con_Printf ("ERROR: Couldn't prepare header\n");
			free (mp3_buffer);
			return;
		}

		if ((mmr = qacmStreamConvert(m_mp3_stream, &m_mp3_stream_header, ACM_STREAMCONVERTF_BLOCKALIGN)))
		{
			Con_Printf ("ERROR: Couldn't convert audio stream\n");
			goto clean;
		}

		hr = qAVIStreamWrite (m_audio_stream, m_audio_frame_counter++, 1, mp3_buffer, m_mp3_stream_header.cbDstLengthUsed, AVIIF_KEYFRAME, NULL, &frame_bytes_written);
		m_bytes_written += frame_bytes_written;

clean:
		if ((mmr = qacmStreamUnprepareHeader(m_mp3_stream, &m_mp3_stream_header, 0)))
		{
			Con_Printf ("ERROR: Couldn't unprepare header\n");
			free (mp3_buffer);
			return;
		}

		free (mp3_buffer);
	}
	else
	{
		hr = qAVIStreamWrite (m_audio_stream, m_audio_frame_counter++, 1, sample_buffer, sample_bufsize, AVIIF_KEYFRAME, NULL, &frame_bytes_written);
		m_bytes_written += frame_bytes_written;
	}
	if (FAILED(hr))
	{
		Con_Printf ("ERROR: Couldn't write audio stream\n");
		return;
	}
}

LONG Capture_GetNumWrittenBytes (void)
{
	return m_bytes_written;
}
