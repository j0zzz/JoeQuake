/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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

#include <SDL.h>

#include "quakedef.h"

double mouse_x, mouse_y;
static double	mx, my, old_mouse_x, old_mouse_y;
cvar_t	m_filter = {"m_filter", "0"};

extern cvar_t cl_maxpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_minpitch; //johnfitz -- variable pitch clamping

// Stubs that are used externally.
qboolean use_m_smooth;
cvar_t m_rate = {"m_rate", "60"};


static int buttonremap[] =
{
	K_MOUSE1,
	K_MOUSE3,	/* right button		*/
	K_MOUSE2,	/* middle button	*/
	K_MOUSE4,
	K_MOUSE5
};


void IN_Accumulate(void)
{
	// Should something go here?
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
	case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;

	case SDL_SCANCODE_NUMLOCKCLEAR: return KP_NUMLOCK;
	case SDL_SCANCODE_KP_DIVIDE: return KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY: return KP_STAR;
	case SDL_SCANCODE_KP_7: return KP_HOME;
	case SDL_SCANCODE_KP_8: return KP_UPARROW;
	case SDL_SCANCODE_KP_9: return KP_PGUP;
	case SDL_SCANCODE_KP_MINUS: return KP_MINUS;
	case SDL_SCANCODE_KP_4: return KP_LEFTARROW;
	case SDL_SCANCODE_KP_5: return KP_5;
	case SDL_SCANCODE_KP_6: return KP_RIGHTARROW;
	case SDL_SCANCODE_KP_PLUS: return KP_PLUS;
	case SDL_SCANCODE_KP_1: return KP_END;
	case SDL_SCANCODE_KP_2: return KP_DOWNARROW;
	case SDL_SCANCODE_KP_3: return KP_PGDN;
	case SDL_SCANCODE_KP_0: return KP_INS;
	case SDL_SCANCODE_KP_PERIOD: return KP_DEL;
	case SDL_SCANCODE_KP_ENTER: return KP_ENTER;

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
	Cvar_Register (&m_filter);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

static void IN_MouseMove (usercmd_t *cmd)
{
	float	sens;
	
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

		if (key_dest == key_menu || key_dest == key_console || (CL_DemoUIOpen() && !DemoCam_MLook()))
		{
			mouse_x *= ((mousespeed * m_accel_factor) + cursor_sensitivity.value);
			mouse_y *= ((mousespeed * m_accel_factor) + cursor_sensitivity.value);
		}
		else
		{
			sens = tan(DEG2RAD(r_refdef.basefov) * 0.5f) / tan(DEG2RAD(scr_fov.value) * 0.5f);
			sens *= ((mousespeed * m_accel_factor) + sensitivity.value);

			mouse_x *= sens;
			mouse_y *= sens;
		}
	}
	else
	{
		if (key_dest == key_menu || key_dest == key_console || (CL_DemoUIOpen() && !DemoCam_MLook()))
		{
			mouse_x *= cursor_sensitivity.value;
			mouse_y *= cursor_sensitivity.value;
		}
		else
		{
			sens = tan(DEG2RAD(r_refdef.basefov) * 0.5f) / tan(DEG2RAD(scr_fov.value) * 0.5f);
			sens *= sensitivity.value;

			mouse_x *= sens;
			mouse_y *= sens;
		}
	}

	if (key_dest != key_menu && key_dest != key_console)
		DemoCam_MouseMove(mouse_x, mouse_y);

	//
	// Do not move the player if we're in menu mode.
	// And don't apply ingame sensitivity, since that will make movements jerky.
	//
	if (key_dest != key_menu && key_dest != key_console && !CL_DemoUIOpen())
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
			cl.viewangles[PITCH] = bound(cl_minpitch.value, cl.viewangles[PITCH], cl_maxpitch.value);
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

void IN_ClearStates (void)
{
	mx = my = old_mouse_x = old_mouse_y = 0.0;
}
