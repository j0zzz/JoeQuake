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
// vid_sdl.c -- SDL 2 driver

#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <SDL.h>

#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#endif


#define MAX_MODE_LIST 600
#define MAX_MENU_MODES  45
#define MAX_REFRESH_RATES   8
#define	WARP_WIDTH	320
#define	WARP_HEIGHT	200
#define WINDOW_TITLE_STRING "JoeQuake"
#define DEFAULT_REFRESHRATE	60


typedef struct
{
	int			width;
	int			height;
	int			bpp;
	int			refreshrate;
} sdlmode_t;

typedef struct
{
    int         width;
    int         height;
    int         refreshrates[MAX_REFRESH_RATES];
    int         numrefreshrates;
} menumode_t;


static SDL_Window* draw_context = NULL;
static SDL_GLContext* gl_context = NULL;
sdlmode_t	modelist[MAX_MODE_LIST];  // Modes for showing in `vid_describemodes`
int		nummodes;
menumode_t menumodelist[MAX_MENU_MODES];
int     nummenumodes;

static qboolean	vid_initialized = false;
double mouse_x, mouse_y;
static double	mx, my, old_mouse_x, old_mouse_y;
cvar_t	m_filter = {"m_filter", "0"};
cvar_t	vid_width = {"vid_width", "", CVAR_ARCHIVE};
cvar_t	vid_height = {"vid_height", "", CVAR_ARCHIVE};
cvar_t	vid_refreshrate = {"vid_refreshrate", "", CVAR_ARCHIVE};
cvar_t	vid_fullscreen = {"vid_fullscreen", "", CVAR_ARCHIVE};

// Stubs that are used externally.
qboolean vid_hwgamma_enabled = false;
cvar_t	_windowed_mouse = {"_windowed_mouse", "1", 0};
qboolean fullsbardraw = false;
cvar_t vid_mode;
qboolean gl_have_stencil = false;
#ifdef _WIN32
modestate_t   modestate;
#endif
qboolean    DDActive = false;
qboolean	scr_skipupdate;

static int buttonremap[] =
{
	K_MOUSE1,
	K_MOUSE3,	/* right button		*/
	K_MOUSE2,	/* middle button	*/
	K_MOUSE4,
	K_MOUSE5
};

// Sphere --- TODO: Only copied these from in_win.c for now to get a first
// compiling Linux version. Has to be implemented properly to support these
// features.
qboolean use_m_smooth;
cvar_t m_rate = {"m_rate", "60"};
int GetCurrentBpp(void)
{
	return 32;
}
int GetCurrentFreq(void)
{
	return 60;
}
int menu_bpp, menu_display_freq;
cvar_t vid_vsync;
float menu_vsync;

//========================================================
// Video menu stuff
//========================================================

#define VID_ROW_SIZE		3  // number of columns for modes
#define VID_MENU_SPACING	2  // rows between video options and modes

int	video_cursor_row = 0;
int	video_cursor_column = 0;
int video_items = 0;
int video_mode_rows = 0;

menu_window_t video_window;
void VID_MenuDraw (void);
void VID_MenuKey (int key);

//===========================================

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}


void VID_ShiftPalette (unsigned char *p)
{
}

void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
}

void IN_Accumulate(void)
{
	// Should something go here?
}

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	GLenum glerr;
	const char *sdlerr;

	SDL_GL_SwapWindow(draw_context);

	glerr = glGetError();
	if (glerr)
		Con_Printf("open gl error:%d\n", glerr);

	sdlerr = SDL_GetError();
	if (sdlerr[0])
		Con_Printf("SDL error:%d\n", sdlerr);
}

void VID_SetDefaultMode(void)
{
	// Do we need to do anything here?
}

void VID_Shutdown (void)
{
	if (vid_initialized)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static SDL_DisplayMode *VID_SDL2_GetDisplayMode(int width, int height, int refreshrate)
{
	static SDL_DisplayMode mode;
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

	for (i = 0; i < sdlmodes; i++)
	{
		if (SDL_GetDisplayMode(0, i, &mode) != 0)
			continue;

		if (mode.w == width && mode.h == height
			&& SDL_BITSPERPIXEL(mode.format) >= 24
			&& mode.refresh_rate == refreshrate)
		{
			return &mode;
		}
	}
	return NULL;
}

static int VID_GetCurrentWidth (void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return w;
}

static int VID_GetCurrentHeight (void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return h;
}

static int VID_GetCurrentBPP (void)
{
	const Uint32 pixelFormat = SDL_GetWindowPixelFormat(draw_context);
	return SDL_BITSPERPIXEL(pixelFormat);
}

static int VID_GetCurrentRefreshRate (void)
{
	SDL_DisplayMode mode;
	int current_display;

	current_display = SDL_GetWindowDisplayIndex(draw_context);

	if (0 != SDL_GetCurrentDisplayMode(current_display, &mode))
		return DEFAULT_REFRESHRATE;

	return mode.refresh_rate;
}

static qboolean VID_GetFullscreen (void)
{
	return (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN) != 0;
}

static void ClearAllStates (void)
{
	int	i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
		Key_Event (i, false);

	Key_ClearStates ();
	mx = my = old_mouse_x = old_mouse_y = 0.0;
}

static qboolean VID_ValidMode (int width, int height, int refreshrate, qboolean fullscreen)
{
	if (width < 320)
		return false;

	if (height < 200)
		return false;

	if (fullscreen && VID_SDL2_GetDisplayMode(width, height, refreshrate) == NULL)
		return false;

	return true;
}

extern void GL_Init (void);
extern GLuint r_gamma_texture;
static void SetMode (int width, int height, int refreshrate, qboolean fullscreen)
{
	int		temp;
	Uint32	flags;
	char		caption[50];
	int		depthbits;
	int		previous_display;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();
	// TODO: Stop non-CD audio

	/* z-buffer depth */
	depthbits = 24;
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthbits);

	snprintf(caption, sizeof(caption), "%s", WINDOW_TITLE_STRING);

	/* Create the window if needed, hidden */
	if (!draw_context)
	{
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		draw_context = SDL_CreateWindow (caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		if (!draw_context)
			Sys_Error ("Couldn't create window");

		SDL_SetWindowMinimumSize (draw_context, 320, 240);

		previous_display = -1;
	}
	else
	{
		previous_display = SDL_GetWindowDisplayIndex(draw_context);
	}

	/* Ensure the window is not fullscreen */
	if (VID_GetFullscreen ())
	{
		if (SDL_SetWindowFullscreen (draw_context, 0) != 0)
			Sys_Error("Couldn't set fullscreen state mode");
	}

	/* Set window size and display mode */
	SDL_SetWindowSize (draw_context, width, height);
	if (previous_display >= 0)
		SDL_SetWindowPosition (draw_context, SDL_WINDOWPOS_CENTERED_DISPLAY(previous_display), SDL_WINDOWPOS_CENTERED_DISPLAY(previous_display));
	else
		SDL_SetWindowPosition(draw_context, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SetWindowDisplayMode (draw_context, VID_SDL2_GetDisplayMode(width, height, refreshrate));
	SDL_SetWindowBordered (draw_context, SDL_TRUE);

	/* Make window fullscreen if needed, and show the window */

	if (fullscreen) {
		if (SDL_SetWindowFullscreen (draw_context, SDL_WINDOW_FULLSCREEN) != 0)
			Sys_Error ("Couldn't set fullscreen state mode");
	}

	SDL_ShowWindow (draw_context);
	SDL_RaiseWindow (draw_context);

	/* Create GL context if needed */
	if (!gl_context) {
		gl_context = SDL_GL_CreateContext(draw_context);
		if (!gl_context)
		{
			SDL_GL_ResetAttributes();
			Sys_Error("Couldn't create GL context");
		}
		GL_Init ();
	}

	vid.width = vid.conwidth = VID_GetCurrentWidth();
	vid.height = vid.conheight = VID_GetCurrentHeight();
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;
	vid.recalc_refdef = 1;
	r_gamma_texture = 0;
	Draw_AdjustConback();

// read the obtained z-buffer depth
	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthbits) == -1)
		depthbits = 0;

	CDAudio_Resume ();
	// TODO: Start non-CD audio
	scr_disabled_for_loading = temp;

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Con_Printf ("Video mode: %dx%dx%d Z%d %dHz\n",
				VID_GetCurrentWidth(),
				VID_GetCurrentHeight(),
				VID_GetCurrentBPP(),
				depthbits,
				VID_GetCurrentRefreshRate());
}

static void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	Con_Printf("List of fullscreen modes:\n");
	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_Printf ("\n");
			Con_Printf ("%4i x %4i x %i bpp : %i Hz", modelist[i].width, modelist[i].height, modelist[i].bpp, modelist[i].refreshrate);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes.\n", count);
}

static void VID_TestMode_f (void)
{
	Con_Printf("vid_testmode not supported, use vid_forcemode instead.\n");
}

static void VID_ForceMode_f (void)
{
	int		i;
	int		width, height, refreshrate;
	qboolean	fullscreen;

	// If this is the `vid_forcemode` that is made after config.cfg is executed
	// then read overrides from command line.
	if (Cmd_Argc() == 2 && strcmp(Cmd_Argv(1), "post-config") == 0)
	{
		if (COM_CheckParm("-fullscreen"))
			Cvar_SetValue(&vid_fullscreen, 1);

		if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed"))
			Cvar_SetValue(&vid_fullscreen, 0);

		if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
			Cvar_SetValue(&vid_width, Q_atoi(com_argv[i+1]));

		if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
			Cvar_SetValue(&vid_height, Q_atoi(com_argv[i+1]));

		if ((i = COM_CheckParm("-refreshrate")) && i + 1 < com_argc)
			Cvar_SetValue(&vid_refreshrate, Q_atoi(com_argv[i+1]));
	}

	// Decide what mode to set based on cvars, using defaults if not set.
	if (vid_width.string[0] == '\0')
		Cvar_SetValue(&vid_width, 800);
	width = (int)vid_width.value;

	if (vid_height.string[0] == '\0')
		Cvar_SetValue(&vid_height, 600);
	height = (int)vid_height.value;

	if (vid_refreshrate.string[0] == '\0')
		Cvar_SetValue(&vid_refreshrate, VID_GetCurrentRefreshRate());
	refreshrate = (int)vid_refreshrate.value;

	if (vid_fullscreen.string[0] == '\0')
		Cvar_SetValue(&vid_fullscreen, 0);
	fullscreen = (int)vid_fullscreen.value;

	// Check the mode set above is valid.
	if (!VID_ValidMode(width, height, refreshrate, fullscreen))
	{
		Con_Printf("Invalid mode %dx%d %d Hz %s\n",
					width, height, refreshrate,
					fullscreen ? "fullscreen" : "windowed");
	} else {
		SetMode(width, height, refreshrate, fullscreen);
	}
}

static int CmpModes(sdlmode_t *m1, sdlmode_t *m2)
{
	// Sort lexicographically by (width, height, refreshrate, bpp)
	if (m1->width < m2->width)
		return -1;
	if (m1->width > m2->width)
		return 1;
	if (m1->height < m2->height)
		return -1;
	if (m1->height > m2->height)
		return 1;
	if (m1->refreshrate < m2->refreshrate)
		return -1;
	if (m1->refreshrate > m2->refreshrate)
		return 1;
	if (m1->bpp < m2->bpp)
		return -1;
	if (m1->bpp > m2->bpp)
		return 1;
	return 0;
}

static void SwapModes(sdlmode_t *m1, sdlmode_t *m2)
{
	static sdlmode_t temp_mode;

	temp_mode = *m1;
	*m1 = *m2;
	*m2 = temp_mode;
}

static void VID_InitModelist (void)
{
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i, j;
	sdlmode_t *m1, *m2, *mode;
	menumode_t *menumode;

	// Fetch modes from SDL which have a valid pixel format.
	nummodes = 0;
	for (i = 0; i < sdlmodes; i++)
	{
		SDL_DisplayMode mode;

		if (nummodes >= MAX_MODE_LIST)
			break;
		if (SDL_GetDisplayMode(0, i, &mode) == 0 && SDL_BITSPERPIXEL (mode.format) >= 24)
		{
			modelist[nummodes].width = mode.w;
			modelist[nummodes].height = mode.h;
			modelist[nummodes].bpp = SDL_BITSPERPIXEL(mode.format);
			modelist[nummodes].refreshrate = mode.refresh_rate;
			nummodes++;
		}
	}

	if (nummodes == 0)
		Sys_Error("No valid modes");

	// Sort this array, putting the larger resolutions first.
	for (i = 0; i < nummodes - 1; i++)
	{
		for (j = i + 1; j < nummodes; j++)
		{
			m1 = &modelist[i];
			m2 = &modelist[j];
			if (CmpModes(m1, m2) < 0)
				SwapModes(m1, m2);
		}
	}

	// Build the menu mode list.
	nummenumodes = 0;
	menumode = NULL;		// invariant:  menumode == menumodelist[nummenumodes - 1]
	for (i = 0; i < nummodes && nummenumodes < MAX_MENU_MODES; i++)
	{
		mode = &modelist[i];

		if (menumode == NULL || menumode->width != mode->width || menumode->height != mode->height)
		{
			// This is a resolution we haven't seen, so add a new menu mode.
			menumode = &menumodelist[nummenumodes++];

			menumode->width = mode->width;
			menumode->height = mode->height;
			menumode->refreshrates[0] = mode->refreshrate;
			menumode->numrefreshrates = 1;
		}
		else if (menumode->refreshrates[menumode->numrefreshrates - 1] != mode->refreshrate
					&& menumode->numrefreshrates < MAX_REFRESH_RATES)
		{
			// This is a refresh rate we haven't seen, add to the current menu mode.
			menumode->refreshrates[menumode->numrefreshrates++] = mode->refreshrate;
		}
	}
}

void VID_Init (unsigned char *palette)
{
	Cvar_Register (&m_filter);
	Cvar_Register (&vid_width);
	Cvar_Register (&vid_height);
	Cvar_Register (&vid_refreshrate);
	Cvar_Register (&vid_fullscreen);

	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f);
	Cmd_AddCommand ("vid_testmode", VID_TestMode_f);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error("Couldn't init SDL video: %s", SDL_GetError());
	VID_InitModelist();

	// Set a mode for maximum compatibility.  The real mode will be set once
	// cvars have been set, via an automatic call to `vid_forcemode`.  This
	// initial mode will only be visible fleetingly, as long as there's no
	// problem setting the configured mode.
	SetMode(800, 600, VID_GetCurrentRefreshRate(), false);

	SDL_SetRelativeMouseMode(SDL_TRUE);
	if (SDL_GL_SetSwapInterval (0) != 0)
		Sys_Error("Couldn't disable vsync: %s", SDL_GetError());

	VID_SetPalette (palette);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	vid_initialized = true;
}

static inline int IN_SDL2_ScancodeToQuakeKey(SDL_Scancode scancode)
{
	switch (scancode)
	{
	case SDL_SCANCODE_TAB: return K_TAB;
	case SDL_SCANCODE_RETURN: return K_ENTER;
	case SDL_SCANCODE_RETURN2: return K_ENTER;
	case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
	case SDL_SCANCODE_SPACE: return K_SPACE;

	case SDL_SCANCODE_A: return 'a';
	case SDL_SCANCODE_B: return 'b';
	case SDL_SCANCODE_C: return 'c';
	case SDL_SCANCODE_D: return 'd';
	case SDL_SCANCODE_E: return 'e';
	case SDL_SCANCODE_F: return 'f';
	case SDL_SCANCODE_G: return 'g';
	case SDL_SCANCODE_H: return 'h';
	case SDL_SCANCODE_I: return 'i';
	case SDL_SCANCODE_J: return 'j';
	case SDL_SCANCODE_K: return 'k';
	case SDL_SCANCODE_L: return 'l';
	case SDL_SCANCODE_M: return 'm';
	case SDL_SCANCODE_N: return 'n';
	case SDL_SCANCODE_O: return 'o';
	case SDL_SCANCODE_P: return 'p';
	case SDL_SCANCODE_Q: return 'q';
	case SDL_SCANCODE_R: return 'r';
	case SDL_SCANCODE_S: return 's';
	case SDL_SCANCODE_T: return 't';
	case SDL_SCANCODE_U: return 'u';
	case SDL_SCANCODE_V: return 'v';
	case SDL_SCANCODE_W: return 'w';
	case SDL_SCANCODE_X: return 'x';
	case SDL_SCANCODE_Y: return 'y';
	case SDL_SCANCODE_Z: return 'z';

	case SDL_SCANCODE_1: return '1';
	case SDL_SCANCODE_2: return '2';
	case SDL_SCANCODE_3: return '3';
	case SDL_SCANCODE_4: return '4';
	case SDL_SCANCODE_5: return '5';
	case SDL_SCANCODE_6: return '6';
	case SDL_SCANCODE_7: return '7';
	case SDL_SCANCODE_8: return '8';
	case SDL_SCANCODE_9: return '9';
	case SDL_SCANCODE_0: return '0';

	case SDL_SCANCODE_MINUS: return '-';
	case SDL_SCANCODE_EQUALS: return '=';
	case SDL_SCANCODE_LEFTBRACKET: return '[';
	case SDL_SCANCODE_RIGHTBRACKET: return ']';
	case SDL_SCANCODE_BACKSLASH: return '\\';
	case SDL_SCANCODE_NONUSHASH: return '#';
	case SDL_SCANCODE_SEMICOLON: return ';';
	case SDL_SCANCODE_APOSTROPHE: return '\'';
	case SDL_SCANCODE_GRAVE: return '`';
	case SDL_SCANCODE_COMMA: return ',';
	case SDL_SCANCODE_PERIOD: return '.';
	case SDL_SCANCODE_SLASH: return '/';
	case SDL_SCANCODE_NONUSBACKSLASH: return '\\';

	case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;
	case SDL_SCANCODE_UP: return K_UPARROW;
	case SDL_SCANCODE_DOWN: return K_DOWNARROW;
	case SDL_SCANCODE_LEFT: return K_LEFTARROW;
	case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;

	case SDL_SCANCODE_LALT: return K_LALT;
	case SDL_SCANCODE_RALT: return K_RALT;
	case SDL_SCANCODE_LCTRL: return K_LCTRL;
	case SDL_SCANCODE_RCTRL: return K_RCTRL;
	case SDL_SCANCODE_LSHIFT: return K_LSHIFT;
	case SDL_SCANCODE_RSHIFT: return K_RSHIFT;

	case SDL_SCANCODE_F1: return K_F1;
	case SDL_SCANCODE_F2: return K_F2;
	case SDL_SCANCODE_F3: return K_F3;
	case SDL_SCANCODE_F4: return K_F4;
	case SDL_SCANCODE_F5: return K_F5;
	case SDL_SCANCODE_F6: return K_F6;
	case SDL_SCANCODE_F7: return K_F7;
	case SDL_SCANCODE_F8: return K_F8;
	case SDL_SCANCODE_F9: return K_F9;
	case SDL_SCANCODE_F10: return K_F10;
	case SDL_SCANCODE_F11: return K_F11;
	case SDL_SCANCODE_F12: return K_F12;
	case SDL_SCANCODE_INSERT: return K_INS;
	case SDL_SCANCODE_DELETE: return K_DEL;
	case SDL_SCANCODE_PAGEDOWN: return K_PGDN;
	case SDL_SCANCODE_PAGEUP: return K_PGUP;
	case SDL_SCANCODE_HOME: return K_HOME;
	case SDL_SCANCODE_END: return K_END;

	case SDL_SCANCODE_PAUSE: return K_PAUSE;

	default: return 0;
	}
}

void Sys_SendKeyEvents (void)
{
	SDL_Event event;
	int key;
	qboolean down;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			down = (event.key.state == SDL_PRESSED);

		// SDL2: we interpret the keyboard as the US layout, so keybindings
		// are based on key position, not the label on the key cap.
			key = IN_SDL2_ScancodeToQuakeKey(event.key.keysym.scancode);

			if (key != 0)
				Key_Event (key, down);

			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (event.button.button < 1 ||
			    event.button.button > sizeof(buttonremap) / sizeof(buttonremap[0]))
			{
				Con_Printf ("Ignored event for mouse button %d\n",
							event.button.button);
				break;
			}
			Key_Event(buttonremap[event.button.button - 1], event.button.state == SDL_PRESSED);
			break;

		case SDL_MOUSEWHEEL:
			if (event.wheel.y > 0)
			{
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			}
			else if (event.wheel.y < 0)
			{
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;

		case SDL_MOUSEMOTION:
			mx += event.motion.xrel;
			my += event.motion.yrel;
			break;

		case SDL_QUIT:
			CL_Disconnect ();
			Sys_Quit ();
			break;

		default:
			break;
		}
	}
}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void IN_Init (void)
{
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_MouseMove (usercmd_t *cmd)
{
	if (m_filter.value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	if (m_accel.value)
	{
		float mousespeed = sqrt(mx * mx + my * my);
		float m_accel_factor = m_accel.value * 0.1;

		if (key_dest == key_menu || key_dest == key_console)
		{
			mouse_x *= ((mousespeed * m_accel_factor) + cursor_sensitivity.value);
			mouse_y *= ((mousespeed * m_accel_factor) + cursor_sensitivity.value);
		}
		else
		{
			mouse_x *= ((mousespeed * m_accel_factor) + sensitivity.value);
			mouse_y *= ((mousespeed * m_accel_factor) + sensitivity.value);
		}
	}
	else
	{
		if (key_dest == key_menu || key_dest == key_console)
		{
			mouse_x *= cursor_sensitivity.value;
			mouse_y *= cursor_sensitivity.value;
		}
		else
		{
			mouse_x *= sensitivity.value;
			mouse_y *= sensitivity.value;
		}
	}

	//
	// Do not move the player if we're in menu mode.
	// And don't apply ingame sensitivity, since that will make movements jerky.
	//
	if (key_dest != key_menu && key_dest != key_console)
	{
		// add mouse X/Y movement to cmd
		if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
			cmd->sidemove += m_side.value * mouse_x;
		else
			cl.viewangles[YAW] -= m_yaw.value * mouse_x;

		if (mlook_active)
			V_StopPitchDrift();

		if (mlook_active && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch.value * mouse_y;
			cl.viewangles[PITCH] = bound(-70, cl.viewangles[PITCH], 80);
		}
		else
		{
			if ((in_strafe.state & 1) && noclip_anglehack)
				cmd->upmove -= m_forward.value * mouse_y;
			else
				cmd->forwardmove -= m_forward.value * mouse_y;
		}
	}
	mx = my = 0.0;
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove (cmd);
}

static menumode_t *GetMenuMode(void)
{
	menumode_t *menumode;
	int i, width, height;

	width = (int)vid_width.value;
	height = (int)vid_height.value;

	for (i = 0; i < nummenumodes; i++)
	{
		menumode = &menumodelist[i];
		if (width == menumode->width && height == menumode->height)
			return menumode;
	}

	return NULL;
}

static void MenuSelectPrevRefreshRate (void)
{
	int i;
	menumode_t *menumode;
	int refreshrate;

	menumode = GetMenuMode();
	if (menumode == NULL)
		return;  // Current video mode not in SDL list (eg. windowed mode)

	refreshrate = (int)vid_refreshrate.value;

	// refresh rates are in descending order
	for (i = 0; i < menumode->numrefreshrates; i ++)
	{
		if (menumode->refreshrates[i] < refreshrate)
			break;
	}
	if (i == menumode->numrefreshrates)
		i = 0;		// wrapped around

	Cvar_SetValue(&vid_refreshrate, menumode->refreshrates[i]);
}

static void MenuSelectNextRefreshRate (void)
{
	int i;
	menumode_t *menumode;
	int refreshrate;

	menumode = GetMenuMode();
	if (menumode == NULL)
		return;  // Current video mode not in SDL list (eg. windowed mode)

	refreshrate = (int)vid_refreshrate.value;

	// refresh rates are in descending order
	for (i = 0; i < menumode->numrefreshrates; i ++)
	{
		if (menumode->refreshrates[i] <= refreshrate)
			break;
	}
	if (i == 0)
		i = menumode->numrefreshrates;
	i--;

	Cvar_SetValue(&vid_refreshrate, menumode->refreshrates[i]);
}

static void MenuSelectNearestRefreshRate (void)
{
	int i;
	menumode_t *menumode;
	int refreshrate;

	menumode = GetMenuMode();
	if (menumode == NULL)
		return;  // Current video mode not in SDL list (eg. windowed mode)

	refreshrate = (int)vid_refreshrate.value;

	// refresh rates are in descending order
	for (i = 0; i < menumode->numrefreshrates; i ++)
	{
		if (menumode->refreshrates[i] <= refreshrate)
			break;
	}

	if (i == menumode->numrefreshrates)
		i = menumode->numrefreshrates - 1;	// smaller than all, pick smallest
	else if (i > 0
			 && abs(menumode->refreshrates[i - 1] - refreshrate)
				< abs(menumode->refreshrates[i] - refreshrate))
		i--;  // either larger than all, or prev value is closer.

	Cvar_SetValue(&vid_refreshrate, menumode->refreshrates[i]);
}

void VID_MenuDraw (void)
{
	int i, row, x, y, lx, ly;
	menumode_t *menumode;
	char mode_desc[14], refresh_rate_desc[8];
	qboolean red;
	mpic_t		*p;

	// title graphic
	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	// general settings
	y = 32;
	row = 0;
	M_Print_GetPoint(16, y, &video_window.x, &video_window.y, "        Fullscreen", video_cursor_row == row);
	video_window.x -= 16;	// adjust it slightly to the left due to the larger, 3 columns vid modes list
	M_DrawCheckbox(188, y, vid_fullscreen.value != 0);

	y += 8; row ++;
	M_Print_GetPoint(16, y, &lx, &ly, "      Refresh rate", video_cursor_row == row);
	snprintf(refresh_rate_desc, sizeof(refresh_rate_desc), "%i Hz", (int)vid_refreshrate.value);
	M_Print(188, y, refresh_rate_desc);

	y += 8; row ++;
	M_Print_GetPoint(16, y, &lx, &ly, "     Apply changes", video_cursor_row == row);

	video_items = row + 1;

	// resolutions
	x = 0;
	y += 8 * (1 + VID_MENU_SPACING);
	video_mode_rows = 0;
	for (i = 0 ; i < nummenumodes ; i++)
	{
		menumode = &menumodelist[i];
		red = (menumode->width == (int)vid_width.value && menumode->height == (int)vid_height.value);
		snprintf(mode_desc, sizeof(mode_desc), "%dx%d", menumode->width, menumode->height);
		M_Print_GetPoint(x, y, &lx, &ly, mode_desc, red);

		x += 14 * 8;

		// if we are at the end of the curent row (last column), prepare for next row
		if (((i + 1) % VID_ROW_SIZE) == 0)
		{
			x = 0;
			y += 8;
		}

		// if we just started a new row, increment row counter
		if (((i + 1) % VID_ROW_SIZE) == 1)
		{
			video_mode_rows++;
		}
	}

	video_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	video_window.h = ly - video_window.y + 8;

	// help text
	switch (video_cursor_row)
	{
		case 0: // fullscreen
			break;
		case 1: // refresh rate
			M_Print(8 * 8, y + 16, "  Choose a refresh rate");
			M_Print(8 * 8, y + 24, "after selecting resolution");
			break;
		case 2: // apply
			M_Print(8 * 8, y + 16, "Apply selected settings");
		default:
			if (video_cursor_row >= video_items + VID_MENU_SPACING)
			{
				M_Print(8 * 8, y + 16, "  Select a resolution");
				M_Print(8 * 8, y + 24, "then apply/test changes");
			}
			break;
	}

	// cursor
	if (video_cursor_row < video_items)
		M_DrawCharacter(168, 32 + video_cursor_row * 8, 12 + ((int)(realtime * 4) & 1));
	else if (video_cursor_row >= video_items + VID_MENU_SPACING
			&& (video_cursor_row - video_items - VID_MENU_SPACING) * VID_ROW_SIZE + video_cursor_column < nummenumodes) // we are in the resolutions region
		M_DrawCharacter(-8 + video_cursor_column * 14 * 8, 32 + video_cursor_row * 8, 12 + ((int)(realtime * 4) & 1));
}

void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row--;
		if (video_cursor_row == video_items + VID_MENU_SPACING - 1)
			video_cursor_row -= VID_MENU_SPACING;
		if (video_cursor_row < 0)
		{
			video_cursor_row = (video_items + video_mode_rows + VID_MENU_SPACING) - 1;
			// if we cycle from the top to the bottom row, check if we have an item in the appropriate column
			if (nummenumodes % VID_ROW_SIZE == 1 || (nummenumodes % VID_ROW_SIZE == 2 && video_cursor_column == 2))
				video_cursor_column = 0;
		}
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row++;
		if (video_cursor_row == video_items)
			video_cursor_row += VID_MENU_SPACING;
		if (video_cursor_row >= (video_items + video_mode_rows + VID_MENU_SPACING))
			video_cursor_row = 0;
		else if (video_cursor_row == ((video_items + video_mode_rows + VID_MENU_SPACING) - 1)) // if we step down to the last row, check if we have an item below in the appropriate column
		{
			if (nummenumodes % VID_ROW_SIZE == 1 || (nummenumodes % VID_ROW_SIZE == 2 && video_cursor_column == 2))
				video_cursor_column = 0;
		}
		break;
	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (video_cursor_row)
		{
			case 0: // fullscreen
				Cvar_SetValue(&vid_fullscreen, vid_fullscreen.value == 0);
				break;
            case 1: // refresh rate
				MenuSelectNextRefreshRate();
                break;
			case 2: // apply
				Cbuf_AddText ("vid_forcemode\n");
				break;
            default:
                if (video_cursor_row >= video_items)
                {
					int i = (video_cursor_row - video_items - VID_MENU_SPACING)
										* VID_ROW_SIZE + video_cursor_column;
					if (i < nummenumodes)
					{
						Cvar_SetValue(&vid_width, menumodelist[i].width);
						Cvar_SetValue(&vid_height, menumodelist[i].height);
						MenuSelectNearestRefreshRate();
					}
                }
                break;
		}
		break;

	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		if (video_cursor_row == 1) {
			// refresh rate
			MenuSelectPrevRefreshRate();
		}
		else if (video_cursor_row >= video_items)
		{ 
			video_cursor_column--;
			if (video_cursor_column < 0)
			{
				if (video_cursor_row >= ((video_items + video_mode_rows + VID_MENU_SPACING) - 1)) // if we stand on the last row, check how many items we have
				{
					if (nummenumodes % VID_ROW_SIZE == 1)
						video_cursor_column = 0;
					else if (nummenumodes % VID_ROW_SIZE == 2)
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
		break;

	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		if (video_cursor_row == 1) {
			// refresh rate
			MenuSelectNextRefreshRate();
		}
		else if (video_cursor_row >= video_items)
		{
			video_cursor_column++;
			if (video_cursor_column >= VID_ROW_SIZE || ((video_cursor_row - video_items - VID_MENU_SPACING) * VID_ROW_SIZE + (video_cursor_column + 1)) > nummenumodes)
				video_cursor_column = 0;
		}
		break;
	}
}

extern qboolean M_Mouse_Select_RowColumn(const menu_window_t *uw, const mouse_state_t *m, int row_entries, int *newentry_row, int col_entries, int *newentry_col);
qboolean M_Video_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select_RowColumn(&video_window, ms, video_items + video_mode_rows + VID_MENU_SPACING, &video_cursor_row, VID_ROW_SIZE, &video_cursor_column);

	if (ms->button_down == 1) VID_MenuKey(K_MOUSE1);
	if (ms->button_down == 2) VID_MenuKey(K_MOUSE2);

	return true;
}
