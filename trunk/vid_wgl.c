/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// vid_wgl.c -- Windows 9x/NT OpenGL driver

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include <commctrl.h>

#define MAX_MODE_LIST	128
#define MAX_BPP_LIST	5
#define MAX_REFRESHRATE_LIST 20
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED	0
#define NO_MODE			(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

qboolean gl_have_stencil = false;

typedef struct
{
	modestate_t	type;
	int		width;
	int		height;
	int		modenum;
	int		dib;
	int		fullscreen;
	int		bpp[MAX_BPP_LIST];
	int		numbpps;
	int		refreshrate[MAX_REFRESHRATE_LIST];
	int		numrefreshrates;
	int		halfscreen;
	char	modedesc[20];
} vmode_t;

typedef struct
{
	int		width;
	int		height;
} lmode_t;

lmode_t	lowresmodes[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 400, 300 },
	{ 512, 384 },
};

qboolean	DDActive;
qboolean	scr_skipupdate;

static	vmode_t	modelist[MAX_MODE_LIST];
static	int	nummodes;
static	vmode_t	*pcurrentmode;
static	vmode_t	badmode;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static qboolean windowed_mouse = true;
static qboolean borderless = false;
extern qboolean	mouseactive;	// from in_win.c
static HICON	hIcon;

DWORD	WindowStyle, ExWindowStyle;

HWND	mainwindow, dibwindow;

int		vid_modenum = NO_MODE;
int		vid_default = MODE_WINDOWED;
static int windowed_default;
unsigned char vid_curpal[256*3];
qboolean fullsbardraw = false;

HGLRC	baseRC;
HDC		maindc;

glvert_t glv;

unsigned short	*currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

qboolean	vid_gammaworks = false;
qboolean	vid_hwgamma_enabled = false;
qboolean	customgamma = false;

void RestoreHWGamma (void);

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

modestate_t	modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate (BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
char *VID_GetExtModeDescription(int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

qboolean OnChange_vid_mode(cvar_t *var, char *string);
cvar_t		vid_mode = {"vid_mode", "-1", 0, OnChange_vid_mode };

cvar_t		_windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE};

qboolean OnChange_vid_displayfrequency(cvar_t *var, char *string); 
cvar_t		vid_displayfrequency = {"vid_displayfrequency", "0", 0, OnChange_vid_displayfrequency };
qboolean OnChange_vid_bpp(cvar_t *var, char *string);
cvar_t		vid_bpp = { "vid_bpp", "0", 0, OnChange_vid_bpp };
cvar_t		vid_hwgammacontrol = {"vid_hwgammacontrol", "1"};

typedef BOOL (APIENTRY *SWAPINTERVALFUNCPTR)(int);
SWAPINTERVALFUNCPTR wglSwapIntervalEXT = NULL;
static qboolean update_vsync = false;
qboolean OnChange_vid_vsync (cvar_t *var, char *string);
cvar_t		vid_vsync = {"vid_vsync", "0", 0, OnChange_vid_vsync};

int		window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT	window_rect;

extern qboolean rawinput;

void GL_WGL_CheckExtensions(void)
{
	if (!COM_CheckParm("-noswapctrl") && CheckExtension("WGL_EXT_swap_control"))
	{
		if ((wglSwapIntervalEXT = (void *)wglGetProcAddress("wglSwapIntervalEXT")))
		{
			Con_Printf("Vsync control extensions found\n");
			Cvar_Register (&vid_vsync);
			update_vsync = true;	// force to update vsync after startup
		}
	}
}

qboolean OnChange_vid_vsync (cvar_t *var, char *string)
{
	update_vsync = true;
	return false;
}

void GL_Init_Win(void) 
{
	GL_WGL_CheckExtensions();
}

// direct draw software compatability stuff

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void CenterWindow (HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY * 2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0 : CenterX;
	CenterY = (CenterY < 0) ? 0 : CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

int GetCurrentFreq(void) 
{
	DEVMODE	testMode;

	memset((void*)&testMode, 0, sizeof(testMode));
	testMode.dmSize = sizeof(testMode);
	return EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &testMode) ? testMode.dmDisplayFrequency : 0;
}

qboolean ChangeFreq(int freq) 
{
	DWORD oldFields = gdevmode.dmFields, oldFreq = gdevmode.dmDisplayFrequency; // so we can return old values if we failed

	if (!vid_initialized || !host_initialized)
		return true; // hm, -freq xxx or +set vid_displayfrequency xxx cmdline params? allow then

	if (!ActiveApp || Minimized || !vid_canalttab || vid_wassuspended) 
	{
		Con_Printf("Can't switch display frequency while minimized\n");
		return false;
	}

	if (modestate != MS_FULLDIB) 
	{
		Con_Printf("Can't switch display frequency in non full screen mode\n");
		return false;
	}

	if (freq == 0)
	{
		Con_Printf("Display frequency forcing switched off. Please restart Quake to apply this setting\n");
		return true;
	}

	if (GetCurrentFreq() == freq)
	{
		//Con_Printf("Display frequency %d already set\n", freq);
		return false;
	}

	memset(&gdevmode, 0, sizeof(gdevmode));
	gdevmode.dmSize = sizeof(gdevmode);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &gdevmode);

	gdevmode.dmDisplayFrequency = freq;
	gdevmode.dmFields |= DM_DISPLAYFREQUENCY;

	if (ChangeDisplaySettings(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) 
	{
		Con_Printf("Can't switch display frequency to %d\n", freq);
		gdevmode.dmDisplayFrequency = oldFreq;
		gdevmode.dmFields = oldFields;
		return false;
	}

	Con_Printf("Switching display frequency to %d\n", freq);

	return true;
}

qboolean OnChange_vid_displayfrequency(cvar_t *var, char *string) 
{
	return !ChangeFreq(Q_atoi(string));
}

int GetCurrentBpp(void)
{
	DEVMODE	testMode;

	memset((void*)&testMode, 0, sizeof(testMode));
	testMode.dmSize = sizeof(testMode);
	return EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &testMode) ? testMode.dmBitsPerPel : 0;
}

qboolean ChangeBpp(int bpp)
{
	DWORD oldFields = gdevmode.dmFields, oldBpp = gdevmode.dmBitsPerPel; // so we can return old values if we failed

	if (!vid_initialized || !host_initialized)
		return true; // hm, -bpp xxx or +set vid_bpp xxx cmdline params? allow then

	if (!ActiveApp || Minimized || !vid_canalttab || vid_wassuspended)
	{
		Con_Printf("Can't switch color depth while minimized\n");
		return false;
	}

	if (modestate != MS_FULLDIB)
	{
		Con_Printf("Can't switch color depth in non full screen mode\n");
		return false;
	}

	if (bpp == 0)
	{
		Con_Printf("Color depth forcing switched off. Please restart Quake to apply this setting\n");
		return true;
	}

	if (GetCurrentBpp() == bpp)
	{
		//Con_Printf("Color depth %d already set\n", bpp);
		return false;
	}

	memset(&gdevmode, 0, sizeof(gdevmode));
	gdevmode.dmSize = sizeof(gdevmode);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &gdevmode);

	gdevmode.dmBitsPerPel = bpp;
	gdevmode.dmFields |= DM_BITSPERPEL;

	if (ChangeDisplaySettings(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
	{
		Con_Printf("Can't switch color depth to %d\n", bpp);
		gdevmode.dmDisplayFrequency = oldBpp;
		gdevmode.dmFields = oldFields;
		return false;
	}

	Con_Printf("Switching color depth to %d\n", bpp);

	return true;
}

qboolean OnChange_vid_bpp(cvar_t *var, char *string)
{
	return !ChangeBpp(Q_atoi(string));
}

qboolean VID_SetWindowedMode (int modenum)
{
	HDC	hdc;
	int	lastmodestate, width, height;
	RECT	rect;

	lastmodestate = modestate;

	rect.top = rect.left = 0;
	rect.right = modelist[modenum].width;
	rect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	if (borderless)
		WindowStyle = WS_POPUP;
	else
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	ExWindowStyle = 0;

	AdjustWindowRectEx (&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	if (!dibwindow) // create window, if not exist yet 
		dibwindow = CreateWindowEx (
			 ExWindowStyle,
			 "JoeQuake",
			 "JoeQuake",
			 WindowStyle,
			 rect.left, rect.top,
			 width,
			 height,
			 NULL,
			 NULL,
			 global_hInstance,
			 NULL);
	else // just update size
		SetWindowPos(dibwindow, NULL, 0, 0, width, height, 0);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow (dibwindow, modelist[modenum].width, modelist[modenum].height, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC (dibwindow);
	PatBlt (hdc, 0, 0, modelist[modenum].width, modelist[modenum].height, BLACKNESS);
	ReleaseDC (dibwindow, hdc);

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;

	vid.numpages = 2;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}

qboolean VID_SetFullDIBMode (int modenum)
{
	int	lastmodestate, width, height;
	HDC	hdc;
	RECT	rect;

	if (!leavecurrentmode)
	{
		memset(&gdevmode, 0, sizeof(gdevmode));
		gdevmode.dmSize = sizeof(gdevmode);
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &gdevmode);

		gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;

		if (vid_bpp.value) // bpp was somehow specified, use it
		{
			gdevmode.dmBitsPerPel = vid_bpp.value;
			gdevmode.dmFields |= DM_BITSPERPEL;
		}

		if (vid_displayfrequency.value) // freq was somehow specified, use it
		{
			gdevmode.dmDisplayFrequency = vid_displayfrequency.value;
			gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
		}

		if (ChangeDisplaySettings(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) 
		{
			gdevmode.dmFields &= ~(DM_BITSPERPEL | DM_DISPLAYFREQUENCY); // okay trying default bpp (16?) and default refresh rate (60hz?) then
			if (ChangeDisplaySettings(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
				Sys_Error("Couldn't set fullscreen DIB mode"); // failed for default refresh rate too, bad luck :E
		}

		gdevmode.dmBitsPerPel = GetCurrentBpp();
		Cvar_SetValue(&vid_bpp, (float)(int)gdevmode.dmBitsPerPel);
		gdevmode.dmDisplayFrequency = GetCurrentFreq();
		Cvar_SetValue(&vid_displayfrequency, (float)(int)gdevmode.dmDisplayFrequency); // so variable will we set to actual value (sometimes this fail, but doesn't cause any damage)
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	rect.top = rect.left = 0;
	rect.right = modelist[modenum].width;
	rect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	AdjustWindowRectEx (&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	if (!dibwindow) // create window, if not exist yet 
		dibwindow = CreateWindowEx (
			 ExWindowStyle,
			 "JoeQuake",
			 "JoeQuake",
			 WindowStyle,
			 rect.left, rect.top,
			 width,
			 height,
			 NULL,
			 NULL,
			 global_hInstance,
			 NULL);
	else // just update size
		SetWindowPos(dibwindow, NULL, 0, 0, width, height, 0);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC (dibwindow);
	PatBlt (hdc, 0, 0, modelist[modenum].width, modelist[modenum].height, BLACKNESS);
	ReleaseDC (dibwindow, hdc);

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}

int VID_SetMode (int modenum, unsigned char *palette)
{
	int			temp;
	qboolean	stat;

	//if ((windowed && modenum) || (!windowed && (modenum < 1)) || (!windowed && (modenum >= nummodes)))
	if (modenum < 0 || modenum >= nummodes)
		Sys_Error ("Bad video mode");

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value)
		{
			stat = VID_SetWindowedMode (modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode (modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
		Sys_Error ("Couldn't set video mode");

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't. We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	//VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue (&vid_mode, (float)vid_modenum);

	Draw_AdjustConback(); // need this even vid_conwidth have callback which leads to call this

	//while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	//{
	//	TranslateMessage (&msg);
	//	DispatchMessage (&msg);
	//}

	//Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

//joe	if (!msg_suppress_1)
	Con_Printf ("Video mode %s initialized\n", VID_GetExtModeDescription(vid_modenum));

	//VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}

/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

//=================================================================

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = window_width;
	*height = window_height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	static qboolean	old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks && ActiveApp && !Minimized;
	vid_hwgamma_enabled = vid_hwgamma_enabled && (modestate == MS_FULLDIB || vid_hwgammacontrol.value == 2);
	if (vid_hwgamma_enabled != old_hwgamma_enabled)
	{
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}

	if (!scr_skipupdate || block_drawing)
	{
		if (wglSwapIntervalEXT && update_vsync && vid_vsync.string[0])
			wglSwapIntervalEXT (vid_vsync.value ? 1 : 0);
		update_vsync = false;

#ifdef USEFAKEGL
		FakeSwapBuffers ();
#else
		SwapBuffers (maindc);
#endif
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse.value)
		{
			if (windowed_mouse)
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		}
		else
		{
			windowed_mouse = true;
			if (!mouseactive && ActiveApp)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
		}
	}

	if (fullsbardraw)
		Sbar_Changed ();
}

void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}

void VID_ShiftPalette (unsigned char *palette)
{
}

/*
======================
VID_SetDeviceGammaRamp

Note: ramps must point to a static array
======================
*/
void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
	if (vid_gammaworks)
	{
		currentgammaramp = ramps;
		if (vid_hwgamma_enabled)
		{
			SetDeviceGammaRamp (maindc, ramps);
			customgamma = true;
		}
	}
}

void InitHWGamma (void)
{
	if (gl_glsl_gamma_able || COM_CheckParm("-nohwgamma"))
		return;

	vid_gammaworks = GetDeviceGammaRamp (maindc, systemgammaramp);
	if (vid_gammaworks && !COM_CheckParm("-nogammareset"))
	{
		int i, j;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 256; j++)
				systemgammaramp[i][j] = (j << 8);
	}
}

void RestoreHWGamma (void)
{
	if (vid_gammaworks && customgamma)
	{
		customgamma = false;
		SetDeviceGammaRamp (maindc, systemgammaramp);
	}
}

//=================================================================

void VID_Shutdown (void)
{
   	HGLRC	hRC;
   	HDC		hDC;

	if (vid_initialized)
	{
		RestoreHWGamma ();

		vid_canalttab = false;
		hRC = wglGetCurrentContext ();
		hDC = wglGetCurrentDC ();

		wglMakeCurrent (NULL, NULL);

		if (hRC)
			wglDeleteContext (hRC);

		if (hDC && dibwindow)
			ReleaseDC (dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate (false, false);
	}
}

int bChosePixelFormat(HDC hDC, PIXELFORMATDESCRIPTOR *pfd, PIXELFORMATDESCRIPTOR *retpfd)
{
	int	pixelformat;

	if (!(pixelformat = ChoosePixelFormat(hDC, pfd)))
	{
		MessageBox (NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	if (!(DescribePixelFormat(hDC, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), retpfd)))
	{
		MessageBox(NULL, "DescribePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	return pixelformat;
}

qboolean OnChange_vid_mode(cvar_t *var, char *string)
{
	int modenum;

	if (!vid_initialized || !host_initialized)
		return false; // set from cmd line or from VID_Init(), allow change but do not issue callback

	if (!ActiveApp || Minimized || !vid_canalttab || vid_wassuspended) 
	{
		Con_Printf("Can't switch vid mode while minimized\n");
		return true;
	}

	modenum = Q_atoi(string);

	if (host_initialized) // exec or cfg_load or may be from console, prevent set same mode again, no hurt but less annoying
	{ 
		if (modenum == vid_mode.value) 
		{
			Con_Printf("Vid mode %d already set\n", modenum);
			return true;
		}
	}

	if (modenum == -1)
	{
		Con_Printf("Video mode forcing switched off. Please restart Quake to apply this setting\n");
		return false;
	}

	if (modenum < 0 || modenum >= nummodes
		|| (windowed && modelist[modenum].type != MS_WINDOWED)
		|| (!windowed && modelist[modenum].type != MS_FULLDIB))
	{
		Con_Printf("Invalid vid mode %d\n", modenum);
		return true;
	}

	// we call a few Cvar_SetValues in VID_SetMode and in deeper functions but their callbacks will not be triggered
	VID_SetMode(modenum, host_basepal);

	//Cbuf_AddText("v_cshift 0 0 0 1\n");	//FIXME

	return true;
}

BOOL bSetupPixelFormat (HDC hDC)
{
	int	pixelformat;
	PIXELFORMATDESCRIPTOR retpfd, pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,						// version number
		PFD_DRAW_TO_WINDOW 		// support window
		| PFD_SUPPORT_OPENGL 	// support OpenGL
		| PFD_DOUBLEBUFFER,		// double buffered
		PFD_TYPE_RGBA,			// RGBA type
		24,						// 24-bit color depth
		0, 0, 0, 0, 0, 0,		// color bits ignored
		0,						// no alpha buffer
		0,						// shift bit ignored
		0,						// no accumulation buffer
		0, 0, 0, 0, 			// accum bits ignored
		24,						// 24-bit z-buffer	
		8,						// 8-bit stencil buffer
		0,						// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,						// reserved
		0, 0, 0					// layer masks ignored
	};

	if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
		return FALSE;

	if (retpfd.cDepthBits < 24)
	{
		pfd.cDepthBits = 24;
		if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
			return FALSE;
	}

	if (!SetPixelFormat(hDC, pixelformat, &retpfd))
	{
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (retpfd.cDepthBits < 24)
		gl_allow_ztrick = false;

	gl_have_stencil = true;
	return TRUE;
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int	i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void AppActivate (BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();

			if (vid_canalttab && vid_wassuspended)
			{
				vid_wassuspended = false;
				if (ChangeDisplaySettings(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
					Sys_Error ("Couldn't set fullscreen DIB mode");

				ShowWindow (mainwindow, SW_SHOWNORMAL);
				
				// scr_fullupdate = 0;
				Sbar_Changed ();
			}
		}
		else if (modestate == MS_WINDOWED && Minimized)
			ShowWindow (mainwindow, SW_RESTORE);
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && key_dest == key_game)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}

		if ((vid_canalttab && !Minimized) && currentgammaramp) 
			VID_SetDeviceGammaRamp(currentgammaramp);
	}
	else
	{
		RestoreHWGamma();
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) 
			{ 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int IN_MapKey (int key);
void MW_Hook_Message (long buttons);

#define WM_MWHOOK (WM_USER + 1)

/*
=============
Main Window procedure
=============
*/
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int	fActive, fMinimized, temp;
	LONG	lRet = 1;
	extern	unsigned int	uiWheelMessage;
	extern	cvar_t		cl_confirmquit;

	if (uMsg == uiWheelMessage)
	{
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

	switch (uMsg)
	{
	case WM_KILLFOCUS:
		if (modestate == MS_FULLDIB)
			ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
		break;

	case WM_CREATE:
		break;

	case WM_MOVE:
		window_x = (int) LOWORD(lParam);
		window_y = (int) HIWORD(lParam);
		VID_UpdateWindowStatus ();
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		Key_Event (IN_MapKey(lParam), true);
		break;
			
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Key_Event (IN_MapKey(lParam), false);
		break;

	case WM_SYSCHAR:
	// keep Alt-Space from happening
		break;

	/*
	CRASH FORT
	*/
	case WM_LBUTTONDOWN:
	{
		/*
		Trick to simulate window moving in borderless windows by dragging anywhere,
		but only if the mouse is free and not playing
		*/
		if (borderless && !mouseactive && key_dest != key_game)
		{
			/*
			Equivalent of GET_X_LPARAM and GET_Y_LPARAM from Windowsx.h,
			but no need to drag in that huge header
			*/
			int posx = ((int)(short)LOWORD(lParam));
			int posy = ((int)(short)HIWORD(lParam));

			SendMessageA(mainwindow, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(posx, posy));
		}

		break;
	} 

	/*
	CRASH FORT
	*/
	case WM_INPUT:
	{
		RAWINPUT buf;
		UINT bufsize = sizeof(buf);

		UINT size = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &buf, &bufsize, sizeof(RAWINPUTHEADER));

		if (size <= sizeof(buf))
		{
			if (buf.header.dwType == RIM_TYPEMOUSE)
			{
				IN_RawMouseEvent(&buf.data.mouse);
			}
		}

		break;
	}

// this is complicated because Win32 seems to pack multiple mouse events into
// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		if (!rawinput)
		{
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent(temp);
		}
		break;

	// JACK: This is the mouse wheel with the Intellimouse
	// Its delta is either positive or neg, and we generate the proper
	// Event.
	case WM_MOUSEWHEEL:
		if (!rawinput)
		{
			if ((short)HIWORD(wParam) > 0)
			{
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			}
			else
			{
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
		}
		break;

	case WM_MWHOOK:
		MW_Hook_Message (lParam);
		break;

	case WM_SIZE:
		break;

	case WM_CLOSE:
		if (!cl_confirmquit.value || 
		    MessageBox(mainwindow, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			Sys_Quit ();
	        break;

	case WM_ACTIVATE:
		fActive = LOWORD(wParam);
		fMinimized = (BOOL)HIWORD(wParam);
		AppActivate (!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
		ClearAllStates ();

		break;

	case WM_DESTROY:
		if (dibwindow)
			DestroyWindow (dibwindow);
		PostQuitMessage (0);
		break;

	case MM_MCINOTIFY:
		lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
		break;

	default:
	// pass all unhandled messages to DefWindowProc
		lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}

	// return 1 if handled message, 0 if not
	return lRet;
}

/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}
	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];

	return &badmode;
}

/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;

	return pinfo;
}

// KJB: Added this to return the mode driver name in description for console
char *VID_GetExtModeDescription (int mode)
{
	static char pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		sprintf(pinfo, "%sx%d %dHz fullscreen", pv->modedesc, (int)vid_bpp.value, (int)vid_displayfrequency.value);
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf (pinfo, "windowed");
	}

	return pinfo;
}

void VID_ModeList_f(void) 
{
	int i, j, lnummodes, t, width = -1, height = -1, bpp = -1;
	qboolean foundbpp;
	char *pinfo;
	vmode_t *pv;

	if ((i = Cmd_CheckParm("-w")) && i + 1 < Cmd_Argc())
		width = Q_atoi(Cmd_Argv(i + 1));

	if ((i = Cmd_CheckParm("-h")) && i + 1 < Cmd_Argc())
		height = Q_atoi(Cmd_Argv(i + 1));

	if ((i = Cmd_CheckParm("-b")) && i + 1 < Cmd_Argc())
		bpp = Q_atoi(Cmd_Argv(i + 1));

	if ((i = Cmd_CheckParm("b32")))
		bpp = 32;

	if ((i = Cmd_CheckParm("b16")))
		bpp = 16;

	lnummodes = VID_NumModes();

	t = leavecurrentmode;
	leavecurrentmode = false;

	for (i = 1; i < lnummodes; i++) 
	{
		if (width != -1 && modelist[i].width != width)
			continue;

		if (height != -1 && modelist[i].height != height)
			continue;

		if (bpp != -1)
		{
			for (j = 0, foundbpp = false; j < modelist[i].numbpps; j++)
			{
				if (modelist[i].bpp[j] == bpp)
				{
					foundbpp = true;
					break;
				}
			}
			if (!foundbpp)
				continue;
		}

		pv = VID_GetModePtr(i);
		pinfo = VID_GetExtModeDescription(i);
		Con_Printf("%3d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription(vid_modenum));
}

/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{
	Con_Printf ("Number of available video modes: %d\n", nummodes);
}

/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int	t, modenum;

	modenum = Q_atoi(Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = false;

	Con_Printf ("%s\n", VID_GetExtModeDescription(modenum));

	leavecurrentmode = t;
}

/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int	i, lnummodes, t;
	char	*pinfo;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = false;

	for (i = 1 ; i < lnummodes ; i++)
	{
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

void VID_InitDIB (HINSTANCE hInstance)
{
	int			i;
	WNDCLASS	wc;
	HDC			hdc;

	memset(&wc, 0, sizeof(wc));

	/* Register the frame class */
	wc.style		 = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "JoeQuake";

	if (!RegisterClass(&wc))
		Sys_Error("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
	{
		modelist[0].width = Q_atoi(com_argv[i+1]);
		if (modelist[0].width < 320)
		{
			modelist[0].width = 320;
		}
	}
	else
	{
		modelist[0].width = GetSystemMetrics(SM_CXSCREEN);//use current desktop resolution
	}

	if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
	{
		modelist[0].height = Q_atoi(com_argv[i+1]);
		if (modelist[0].height < 200)
		{
			modelist[0].height = 200;
		}
	}
	else
	{
		modelist[0].height = GetSystemMetrics(SM_CYSCREEN);//use current desktop resolution
	}

	if ((i = COM_CheckParm("-freq")) && i + 1 < com_argc)
	{
		Cvar_SetValue(&vid_displayfrequency, Q_atoi(com_argv[i+1]));
		modelist[0].refreshrate[0] = vid_displayfrequency.value;
	}
	else
	{
		hdc = GetDC(NULL);
		modelist[0].refreshrate[0] = GetDeviceCaps(hdc, VREFRESH);
		Cvar_SetValue(&vid_displayfrequency, modelist[0].refreshrate[0]);
		ReleaseDC(NULL, hdc);
	}

	if ((i = COM_CheckParm("-freq")) && i + 1 < com_argc)
	{
		Cvar_SetValue(&vid_bpp, Q_atoi(com_argv[i+1]));
		modelist[0].bpp[0] = vid_bpp.value;
	}
	else
	{
		hdc = GetDC(NULL);
		modelist[0].bpp[0] = GetDeviceCaps(hdc, BITSPIXEL);
		Cvar_SetValue(&vid_bpp, modelist[0].bpp[0]);
		ReleaseDC(NULL, hdc);
	}

	sprintf(modelist[0].modedesc, "%dx%d", modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].numbpps = 1;
	modelist[0].numrefreshrates = 1;
	modelist[0].halfscreen = 0;

	nummodes = 1;
}

/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode; 
	int		i, j, modenum, bpp, done, originalnummodes, existingmode, existingmodenum, numlowresmodes, existingbpp, existingrefreshrate;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings(NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) && (devmode.dmPelsWidth <= MAXWIDTH) && (devmode.dmPelsHeight <= MAXHEIGHT) && (nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

			//R00k: this takes WAY too long; 14 seconds for all modes (windows 8++ bug?!!)
			//if (ChangeDisplaySettings(&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			if (stat)
			{
				for (i = originalnummodes, existingmode = 0, existingmodenum = 0, existingbpp = 0, existingrefreshrate = 0; i < nummodes; i++)
				{
					if ((devmode.dmPelsWidth == modelist[i].width) &&
						(devmode.dmPelsHeight == modelist[i].height))
					{
						existingmode = 1;
						existingmodenum = i;
						for (j = 0; j < modelist[i].numbpps; j++)
						{
							if (devmode.dmBitsPerPel == modelist[i].bpp[j])
							{
								existingbpp = 1;
								break;
							}
						}
						for (j = 0; j < modelist[i].numrefreshrates; j++)
						{
							if (devmode.dmDisplayFrequency == modelist[i].refreshrate[j])
							{
								existingrefreshrate = 1;
								break;
							}
						}
						break;
					}
				}

				if (!existingmode)
				{
					modelist[nummodes].type = (windowed ? MS_WINDOWED : MS_FULLDIB);
					modelist[nummodes].width = devmode.dmPelsWidth;
					modelist[nummodes].height = devmode.dmPelsHeight;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp[modelist[nummodes].numbpps] = devmode.dmBitsPerPel;
					modelist[nummodes].refreshrate[modelist[nummodes].numrefreshrates] = devmode.dmDisplayFrequency;

					sprintf(modelist[nummodes].modedesc, "%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight);

					// if the width is more than twice the height, reduce it by half because this
					// is probably a dual-screen monitor
					if (!COM_CheckParm("-noadjustaspect"))
					{
						if (modelist[nummodes].width > (modelist[nummodes].height << 1))
						{
							modelist[nummodes].width >>= 1;
							modelist[nummodes].halfscreen = 1;
							sprintf(modelist[nummodes].modedesc, "%dx%d", modelist[nummodes].width, modelist[nummodes].height);
						}
					}

					modelist[nummodes].numbpps++;
					modelist[nummodes].numrefreshrates++;
					nummodes++;
				}
				else if (!existingbpp)
				{
					modelist[existingmodenum].bpp[modelist[existingmodenum].numbpps++] = devmode.dmBitsPerPel;
				}
				else if (!existingrefreshrate)
				{
					modelist[existingmodenum].refreshrate[modelist[existingmodenum].numrefreshrates++] = devmode.dmDisplayFrequency;
				}
			}
		}

		modenum++;
	} while (stat);

	// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do
	{
		for (j = 0; j < numlowresmodes && nummodes < MAX_MODE_LIST; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

			if (ChangeDisplaySettings(&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp[modelist[nummodes].numbpps++] = devmode.dmBitsPerPel;
				modelist[nummodes].refreshrate[modelist[nummodes].numrefreshrates++] = devmode.dmDisplayFrequency;

				for (i = originalnummodes, existingmode = 0; i < nummodes; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width) && 
						(modelist[nummodes].height == modelist[i].height))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
					nummodes++;
			}
		}
		switch (bpp)
		{
		case 16:
			bpp = 32;
			break;

		case 32:
			bpp = 24;
			break;

		case 24:
			done = 1;
			break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_Printf ("No fullscreen DIB modes found\n");
}

/*
===================
VID_Init
===================
*/
void VID_Init (unsigned char *palette)
{
	int		i, temp, basenummodes, width, height, bpp, freq, findbpp, done;
	HDC		hdc;
	DEVMODE	devmode;

	if (COM_CheckParm("-window"))
		windowed = true;
	if (COM_CheckParm("-borderless"))
	{
		windowed = true;	//joe: borderless mode only makes sense if windowed too, so the user don't have to use -window also all the time
		borderless = true;
	}

	memset (&devmode, 0, sizeof(devmode));

	Cvar_Register(&vid_mode);
	Cvar_Register(&vid_bpp);
	Cvar_Register(&vid_displayfrequency);
	Cvar_Register(&_windowed_mouse);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	Cmd_AddCommand("vid_modelist", VID_ModeList_f); 

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes;

	VID_InitFullDIB (global_hInstance);

	if (windowed)
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			Sys_Error ("Can't run in non-RGB mode");

		ReleaseDC (NULL, hdc);

		if ((temp = COM_CheckParm("-mode")) && temp + 1 < com_argc)
			vid_default = Q_atoi(com_argv[temp+1]);
		else if (vid_mode.value != NO_MODE) // serve +set vid_mode
			vid_default = vid_mode.value;
		else 
		{
			vid_default = NO_MODE;

			width = modelist[0].width;
			height = modelist[0].height;

			for (i = 1; i < nummodes; i++)
			{
				if (modelist[i].width == width && (!height || modelist[i].height == height))
				{
					vid_default = i;
					break;
				}
			}

			vid_default = (vid_default == NO_MODE ? MODE_WINDOWED : vid_default);
		}
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if ((i = COM_CheckParm("-mode")) && i + 1 < com_argc)
			vid_default = Q_atoi(com_argv[i+1]);
		else if (vid_mode.value != NO_MODE) // serve +set vid_mode
			vid_default = vid_mode.value;
		else
		{
			if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
				width = Q_atoi(com_argv[i+1]);
			else
				width = GetSystemMetrics(SM_CXSCREEN);

			if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
				height = Q_atoi(com_argv[i+1]);
			else
				height = GetSystemMetrics(SM_CYSCREEN);

			if ((i = COM_CheckParm("-bpp")) && i + 1 < com_argc)
			{
				Cvar_Set(&vid_bpp, com_argv[i+1]);
				bpp = vid_bpp.value;
				findbpp = 0;
			}
			else
			{
				hdc = GetDC(NULL);
				bpp = GetDeviceCaps(hdc, BITSPIXEL);
				Cvar_SetValue(&vid_bpp, bpp);
				ReleaseDC(NULL, hdc);
				findbpp = 1;
			}

			if ((i = COM_CheckParm("-freq")) && i + 1 < com_argc)
			{
				Cvar_Set(&vid_displayfrequency, com_argv[i+1]);
				freq = vid_displayfrequency.value;
			}
			else
			{
				hdc = GetDC(NULL);
				freq = GetDeviceCaps(hdc, VREFRESH);
				Cvar_SetValue(&vid_displayfrequency, freq);
				ReleaseDC(NULL, hdc);
			}

			// if they want to force it, add the specified mode to the list
			if (COM_CheckParm("-force") && nummodes < MAX_MODE_LIST)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = width;
				modelist[nummodes].height = height;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp[modelist[nummodes].numbpps++] = bpp;
				modelist[nummodes].refreshrate[modelist[nummodes].numrefreshrates++] = freq;
				
				sprintf(modelist[nummodes].modedesc, "%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight);

				for (i = nummodes ; i < nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width) && 
					    (modelist[nummodes].height == modelist[i].height))
						break;
				}

				if (i == nummodes)
					nummodes++;
			}

			done = 0;

			do {

				for (i = 1, vid_default = 0 ; i < nummodes ; i++)
				{
					if (modelist[i].width == width && 
						modelist[i].height == height)
					{
						vid_default = i;
						done = 1;
						break;
					}
				}

				if (findbpp && !done)
				{
					switch (bpp)
					{
					case 15:
						bpp = 16;
						break;

					case 16:
						bpp = 32;
						break;

					case 32:
						bpp = 24;
						break;

					case 24:
						done = 1;
						break;
					}
				}
				else
				{
					done = 1;
				}
			} while (!done);

			if (!vid_default)
				Sys_Error ("Specified video mode not available");
		}
	}

	// sort the refreshrate arrays for all vid_modes in ascending order
	for (i = 1; i < nummodes; i++)
	{
		SortIntArrayAscending(modelist[i].refreshrate, modelist[i].numrefreshrates);
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	Check_Gamma (palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

	maindc = GetDC (mainwindow);
	if (!bSetupPixelFormat(maindc))
		Sys_Error ("bSetupPixelFormat failed");

	if (!(baseRC = wglCreateContext(maindc)))
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");

	if (!wglMakeCurrent(maindc, baseRC))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();
	GL_Init_Win();
	Fog_SetupState();

	InitHWGamma();
	if (vid_gammaworks)
		Cvar_Register(&vid_hwgammacontrol);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}

//========================================================
// Video menu stuff
//========================================================

#define	VIDEO_ITEMS	8

int	video_cursor_row = 0;
int	video_cursor_column = 0;
int video_mode_rows = 0;

menu_window_t video_window;
menu_window_t video_slider_resscale_window;

static	int	vid_line, vid_wmodes;

int menu_bpp, menu_display_freq;
float menu_vsync;

qboolean m_videomode_change_confirm;
extern qboolean	m_entersound;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;

#define VID_ROW_SIZE		3
#define MAX_COLUMN_SIZE		12
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE * VID_ROW_SIZE)

static	modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	mpic_t	*p;
	char	*ptr, bpp[10], display_freq[10];
	int		lnummodes, i, k, column, row, lx, ly;
	float	r;
	vmode_t	*pv;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();
	
	for (i = 1; i < lnummodes && vid_wmodes < MAX_MODEDESCS; i++)
	{
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;
	}

	M_Print_GetPoint(16, 32, &video_window.x, &video_window.y, "        Fullscreen", video_cursor_row == 0);
	video_window.x -= 16;	// adjust it slightly to the left due to the larger, 3 columns vid modes list
	M_DrawCheckbox(188, 32, !windowed);

	M_Print_GetPoint(16, 40, &lx, &ly, "       Color depth", video_cursor_row == 1);
	sprintf(bpp, menu_bpp == 0 ? "desktop" : "%i", menu_bpp);
	M_Print(188, 40, bpp);

	M_Print_GetPoint(16, 48, &lx, &ly, "      Refresh rate", video_cursor_row == 2);
	sprintf(display_freq, menu_display_freq == 0 ? "desktop" : "%i Hz", menu_display_freq);
	M_Print(188, 48, display_freq);

	M_Print_GetPoint(16, 56, &lx, &ly, "     Vertical sync", video_cursor_row == 3);
	M_DrawCheckbox(188, 56, menu_vsync);

	M_Print_GetPoint(16, 64, &lx, &ly, "  Resolution scale", video_cursor_row == 4);
	r = (r_scale.value - 0.25) / 0.75;
	M_DrawSliderFloat2(188, 64, r, r_scale.value, &video_slider_resscale_window);

	M_Print_GetPoint(16, 80, &lx, &ly, "     Apply changes", video_cursor_row == 6);

	column = 0;
	row = 32 + VIDEO_ITEMS * 8;

	video_mode_rows = 0;
	for (i = 0 ; i < vid_wmodes ; i++)
	{
		//if (modedescs[i].iscur)
		//	M_PrintWhite (column, row, modedescs[i].desc);
		//else
		M_Print_GetPoint(column, row, &lx, &ly, modedescs[i].desc, video_cursor_row == ((row - 32) / 8) && video_cursor_column == (column / (14 * 8)));

		column += 14 * 8;

		if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
		{
			column = 0;
			row += 8;
		}
		
		if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 2))
		{
			video_mode_rows++;
		}
	}

	video_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	video_window.h = ly - video_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (video_cursor_row == 5 || video_cursor_row == 7)
		return;

	// cursor
	if (video_cursor_row < VIDEO_ITEMS)
		M_DrawCharacter(168, 32 + video_cursor_row * 8, 12 + ((int)(realtime * 4) & 1));
	else // we are in the resolutions region
		M_DrawCharacter(-8 + video_cursor_column * 14 * 8, 32 + video_cursor_row * 8, 12 + ((int)(realtime * 4) & 1));

	M_Print(8 * 8, row + 8, "Press enter to set mode");

	if (video_cursor_row == 0 && modestate == MS_FULLDIB)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Windowed mode must be set from the");
		M_Print(2 * 8, 176 + 8 * 4, "command line with -window");
	}
	else if ((video_cursor_row == 1 || video_cursor_row == 2) && modestate == MS_WINDOWED)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Refresh rate and color depth can only");
		M_Print(2 * 8, 176 + 8 * 4, "be changed in fullscreen mode");
	}

	if (m_videomode_change_confirm)
	{
		M_DrawTextBox(40, 10 * 8, 27, 2);
		M_PrintWhite(48, 11 * 8, "Would you like to keep this");
		M_PrintWhite(48, 12 * 8, "        video mode?");
	}
}

void M_Video_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (video_cursor_row)
	{
	case 4:	// resolution scale
		r_scale.value += dir * 0.05;
		r_scale.value = bound(0.25, r_scale.value, 1);
		Cvar_SetValue(&r_scale, r_scale.value);
		break;

	default:
		break;
	}
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	int i, selected_modenum, num_bpp_modes, num_display_freq_modes;
	static int oldmodenum;

	if (m_videomode_change_confirm)
	{
		if (key == 'y' || key == K_ENTER || key == K_MOUSE1)
		{
			m_videomode_change_confirm = false;
			m_entersound = true;
		}
		else if (key == 'n' || key == K_ESCAPE || key == K_MOUSE2)
		{
			Cvar_SetValue(&vid_mode, (float)oldmodenum);
			m_videomode_change_confirm = false;
			m_entersound = true;
		}
		return;
	}

	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row--;
		if (video_cursor_row < 0)
		{
			video_cursor_row = (VIDEO_ITEMS + video_mode_rows) - 1;
			// if we cycle from the top to the bottom row, check if we have an item in the appropriate column
			if (vid_wmodes % VID_ROW_SIZE == 1 || (vid_wmodes % VID_ROW_SIZE == 2 && video_cursor_column == 2))
				video_cursor_column = 0;
		}
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row++;
		if (video_cursor_row >= (VIDEO_ITEMS + video_mode_rows))
			video_cursor_row = 0;
		else if (video_cursor_row >= ((VIDEO_ITEMS + video_mode_rows) - 1)) // if we step down to the last row, check if we have an item below in the appropriate column
		{
			if (vid_wmodes % VID_ROW_SIZE == 1 || (vid_wmodes % VID_ROW_SIZE == 2 && video_cursor_column == 2))
				video_cursor_column = 0;
		}
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row = (VIDEO_ITEMS + video_mode_rows) - 1;
		break;

	case K_LEFTARROW:
		if (video_cursor_row >= VIDEO_ITEMS)
		{ 
			video_cursor_column--;
			if (video_cursor_column < 0)
			{
				if (video_cursor_row >= ((VIDEO_ITEMS + video_mode_rows) - 1)) // if we stand on the last row, check how many items we have
				{
					if (vid_wmodes % VID_ROW_SIZE == 1)
						video_cursor_column = 0;
					else if (vid_wmodes % VID_ROW_SIZE == 2)
						video_cursor_column = 1;
					else
						video_cursor_column = 2;
				}
				else
				{
					video_cursor_column = VID_ROW_SIZE - 1;
				}
			}
		}
		else
			M_Video_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		if (video_cursor_row >= VIDEO_ITEMS)
		{
			video_cursor_column++;
			if (video_cursor_column >= VID_ROW_SIZE || ((video_cursor_row - VIDEO_ITEMS) * VID_ROW_SIZE + (video_cursor_column + 1)) > vid_wmodes)
				video_cursor_column = 0;
		}
		else
			M_Video_KeyboardSlider(1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (video_cursor_row)
		{
		case 0:
			//TODO when switching windowed/fullscreen mode is supported
			break;

		case 1:
			if (modestate == MS_WINDOWED)
				break;

			num_bpp_modes = modelist[vid_modenum].numbpps;
			for (i = 0; i < num_bpp_modes; i++)
				if (modelist[vid_modenum].bpp[i] == menu_bpp)
					break;

			if (i >= (num_bpp_modes - 1))
				i = -1;

			menu_bpp = modelist[vid_modenum].bpp[i + 1];
			break;

		case 2:
			if (modestate == MS_WINDOWED)
				break;

			num_display_freq_modes = modelist[vid_modenum].numrefreshrates;
			for (i = 0; i < num_display_freq_modes; i++)
				if (modelist[vid_modenum].refreshrate[i] == menu_display_freq)
					break;

			if (i >= (num_display_freq_modes - 1))
				i = -1;

			menu_display_freq = modelist[vid_modenum].refreshrate[i+1];
			break;

		case 3:
			menu_vsync = !menu_vsync;
			break;

		case 6:
			//TODO add support to change between windowed/fullscreen modes
			Cvar_SetValue(&vid_bpp, menu_bpp);
			Cvar_SetValue(&vid_displayfrequency, menu_display_freq);
			Cvar_SetValue(&vid_vsync, menu_vsync);
			break;

		default:
			if (video_cursor_row >= VIDEO_ITEMS)
			{
				int mode_index = (video_cursor_row - VIDEO_ITEMS) * VID_ROW_SIZE + video_cursor_column;
				selected_modenum = modedescs[mode_index].modenum;
			}
			else
				selected_modenum = vid_modenum;

			if (selected_modenum != vid_modenum)
			{
				oldmodenum = vid_modenum;
				Cvar_SetValue(&vid_mode, (float)selected_modenum);
				m_videomode_change_confirm = true;
			}
			break;
		}
		break;
	}

	if (key == K_UPARROW && (video_cursor_row == 5 || video_cursor_row == 7))
		video_cursor_row--;
	else if (key == K_DOWNARROW && (video_cursor_row == 5 || video_cursor_row == 7))
		video_cursor_row++;
}

extern qboolean M_Mouse_Select_Column(const menu_window_t *uw, const mouse_state_t *m, int entries, int *newentry);
extern qboolean M_Mouse_Select_RowColumn(const menu_window_t *uw, const mouse_state_t *m, int row_entries, int *newentry_row, int col_entries, int *newentry_col);

void M_Video_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (video_cursor_row)
		{
		case 4:	// resolution scale
			M_Mouse_Select_Column(&video_slider_resscale_window, ms, 16, &slider_pos);
			r_scale.value = bound(0.25, 0.25 + (slider_pos * 0.05), 1);
			Cvar_SetValue(&r_scale, r_scale.value);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Video_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select_RowColumn(&video_window, ms, VIDEO_ITEMS + video_mode_rows, &video_cursor_row, VID_ROW_SIZE, &video_cursor_column);

	if (ms->button_up == 1) VID_MenuKey(K_MOUSE1);
	if (ms->button_up == 2) VID_MenuKey(K_MOUSE2);

	if (ms->buttons[1]) M_Video_MouseSlider(K_MOUSE1, ms);

	return true;
}
