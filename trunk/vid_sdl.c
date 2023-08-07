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

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <SDL2/SDL.h>

#include <dlfcn.h>

#include "quakedef.h"

#include "quake.ico.h"


#define MAX_MODE_LIST 600
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
	qboolean	windowed;
} vmode_t;

static SDL_Window* draw_context = NULL;
static SDL_GLContext* gl_context = NULL;
vmode_t	modelist[MAX_MODE_LIST];
int		nummodes;

static qboolean	vid_initialized = false;
double mouse_x, mouse_y;
static double	mx, my, old_mouse_x, old_mouse_y;
cvar_t	m_filter = {"m_filter", "0"};

// Stubs that are used externally.
qboolean vid_hwgamma_enabled = false;
cvar_t	_windowed_mouse = {"_windowed_mouse", "1", 0};
qboolean fullsbardraw = false;
cvar_t vid_mode;
qboolean gl_have_stencil = false;

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

void signal_handler (int sig)
{
	printf ("Received signal %d, exiting...\n", sig);
	Sys_Quit ();
	exit (0);
}

void InitSig (void)
{
	signal (SIGHUP, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGQUIT, signal_handler);
	signal (SIGILL, signal_handler);
	signal (SIGTRAP, signal_handler);
	signal (SIGIOT, signal_handler);
	signal (SIGBUS, signal_handler);
	signal (SIGFPE, signal_handler);
	signal (SIGSEGV, signal_handler);
	signal (SIGTERM, signal_handler);
}

void VID_ShiftPalette (unsigned char *p)
{
}

void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
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

extern void GL_Init (void);
extern GLuint r_gamma_texture;
static void SetMode (vmode_t *mode)
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
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
		
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		draw_context = SDL_CreateWindow (caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode->width, mode->height, flags);
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
	SDL_SetWindowSize (draw_context, mode->width, mode->height);
	if (previous_display >= 0)
		SDL_SetWindowPosition (draw_context, SDL_WINDOWPOS_CENTERED_DISPLAY(previous_display), SDL_WINDOWPOS_CENTERED_DISPLAY(previous_display));
	else
		SDL_SetWindowPosition(draw_context, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SetWindowDisplayMode (draw_context, VID_SDL2_GetDisplayMode(mode->width, mode->height, mode->refreshrate));
	SDL_SetWindowBordered (draw_context, SDL_TRUE);

	/* Make window fullscreen if needed, and show the window */

	if (!mode->windowed) {
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

	vid.width = VID_GetCurrentWidth();
	vid.height = VID_GetCurrentHeight();
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;
	vid.recalc_refdef = 1;
	r_gamma_texture = 0;

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

static void VID_Mode_f (void)
{
	vmode_t new_mode;

	if (Cmd_Argc() != 6)
	{
		Con_Printf("Usage: vid_mode <width> <height> <bpp> <refreshrate> [fullscreen|windowed]\n");
	}
	else
	{
		new_mode.width = Q_atoi(Cmd_Argv(1));
		new_mode.height = Q_atoi(Cmd_Argv(2));
		new_mode.bpp = Q_atoi(Cmd_Argv(3));
		new_mode.refreshrate = Q_atoi(Cmd_Argv(4));
		new_mode.windowed = (strcmp(Cmd_Argv(5), "windowed") == 0);

		SetMode(&new_mode);
	}
}

static void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_Printf ("\n");
			Con_Printf ("% 3d: %4i x %4i x %i : %i", i, modelist[i].width, modelist[i].height, modelist[i].bpp, modelist[i].refreshrate);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);
}

static void VID_InitModelist (void)
{
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

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
			modelist[nummodes].windowed = false;
			nummodes++;
		}
	}
}

void VID_Init (unsigned char *palette)
{
	vmode_t new_mode;
	int		i;

	Cvar_Register (&m_filter);

	Cmd_AddCommand ("vid_mode", VID_Mode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error("Couldn't init SDL video: %s", SDL_GetError());
	VID_InitModelist();
	if (nummodes == 0)
		Sys_Error("No valid modes");
	// TODO: store mode in hidden cvar
	memcpy(&new_mode, &modelist[0], sizeof(new_mode));

	if (COM_CheckParm("-fullscreen"))
		new_mode.windowed = false;
	if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed"))
		new_mode.windowed = true;

	if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
		new_mode.width = Q_atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
		new_mode.height = Q_atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-bpp")) && i + 1 < com_argc)
		new_mode.bpp = Q_atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-refreshrate")) && i + 1 < com_argc)
		new_mode.refreshrate = Q_atoi(com_argv[i+1]);

	SetMode(&new_mode);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	InitSig ();	// trap evil signals

	VID_SetPalette (palette);

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

	case SDL_SCANCODE_LALT: return K_ALT;
	case SDL_SCANCODE_RALT: return K_ALT;
	case SDL_SCANCODE_LCTRL: return K_CTRL;
	case SDL_SCANCODE_RCTRL: return K_CTRL;
	case SDL_SCANCODE_LSHIFT: return K_SHIFT;
	case SDL_SCANCODE_RSHIFT: return K_SHIFT;

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

qboolean M_Video_Mouse_Event(const mouse_state_t *ms)
{
	// We do not have the menu for switching video modes on Linux yet, so we
	// do not need to implement this function.
	return true;
}
