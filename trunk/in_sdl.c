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

// joystick defines and variables
SDL_Joystick* joystick;
SDL_GameController* controller;
int joyIndex = -1;
int joyCount = -1;
int joyNumAxes = 0;
int joyNumButtons = 0;
int joyGyroSensor = SDL_SENSOR_INVALID;
#define	JOY_MAX_BUTTONS		32			// 32 internal buttons are used (4 more defined but unused)
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6			// X, Y, Z, R, U, V
#define JOY_AXIS_X		0
#define JOY_AXIS_Y		1
#define JOY_AXIS_Z		2
#define JOY_AXIS_R		3
#define JOY_AXIS_U		4
#define JOY_AXIS_V		5
#define	GYRO_MAX_AXES		3
#define JOY_GYRO_PITCH		0	// gyro
#define JOY_GYRO_YAW		1
#define JOY_GYRO_ROLL		2


enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisDebug = 7
};

qboolean	joy_avail, joy_advancedinit;
unsigned long		joy_oldbuttonstate, joy_numbuttons;

unsigned long	dwAxisMap[JOY_MAX_AXES];
unsigned long	dwControlMap[JOY_MAX_AXES];
unsigned long	gyroAxisMap[GYRO_MAX_AXES];
float 			gyroNoiseOffset[GYRO_MAX_AXES];
float 			gyroData[GYRO_MAX_AXES];
float 			gyroBank[GYRO_MAX_AXES] = {0};
int 			gyroSamples = 0;

cvar_t	in_joystick = {"joystick", "0", CVAR_ARCHIVE};
cvar_t	joy_name = {"joyname", "joystick"};
cvar_t	joy_advanced = {"joyadvanced", "0"};
cvar_t	joy_advaxisx = {"joyadvaxisx", "0"};
cvar_t	joy_advaxisy = {"joyadvaxisy", "0"};
cvar_t	joy_advaxisz = {"joyadvaxisz", "0"};
cvar_t	joy_advaxisr = {"joyadvaxisr", "0"};
cvar_t	joy_advaxisu = {"joyadvaxisu", "0"};
cvar_t	joy_advaxisv = {"joyadvaxisv", "0"};

cvar_t	gyro_axisp = {"gyroaxisp", "0"};
cvar_t	gyro_axisr = {"gyroaxisr", "0"};
cvar_t	gyro_axisy = {"gyroaxisy", "0"};

cvar_t	joy_analogdigitalthreshold = {"joyanalogdigitalthreshold", "0.3"};
cvar_t	joy_forwardthreshold = {"joyforwardthreshold", "0.15"};
cvar_t	joy_sidethreshold = {"joysidethreshold", "0.15"};
cvar_t	joy_pitchthreshold = {"joypitchthreshold", "0.15"};
cvar_t	joy_yawthreshold = {"joyyawthreshold", "0.15"};
cvar_t	joy_forwardsensitivity = {"joyforwardsensitivity", "-1.0"};
cvar_t	joy_sidesensitivity = {"joysidesensitivity", "-1.0"};
cvar_t	joy_pitchsensitivity = {"joypitchsensitivity", "1.0"};
cvar_t	joy_yawsensitivity = {"joyyawsensitivity", "-1.0"};
cvar_t	gyro_enable = {"gyro_enable", "0"};
cvar_t	gyro_accel = {"gyro_accel", "0"};
cvar_t	gyro_threshold = {"gyro_threshold", ".01"};
cvar_t	gyro_forwardsensitivity = {"gyroforwardsensitivity", "-1.0"};
cvar_t	gyro_sidesensitivity = {"gyrosidesensitivity", "-1.0"};
cvar_t	gyro_pitchsensitivity = {"gyropitchsensitivity", "1.0"};
cvar_t	gyro_yawsensitivity = {"gyroyawsensitivity", "-1.0"};

void Joy_AdvancedUpdate_f (void) {

	int	i;
	unsigned long	dwTemp;

	// initialize all the maps
	for (i=0 ; i<JOY_MAX_AXES ; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = 0;
	}

	if (joy_advanced.value == 0.0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (unsigned long) joy_advaxisx.value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (unsigned long) joy_advaxisy.value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (unsigned long) joy_advaxisz.value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (unsigned long) joy_advaxisr.value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (unsigned long) joy_advaxisu.value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (unsigned long) joy_advaxisv.value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (unsigned long) gyro_axisp.value;
		gyroAxisMap[JOY_GYRO_PITCH] = dwTemp & 0x0000000f;
		dwTemp = (unsigned long) gyro_axisr.value;
		gyroAxisMap[JOY_GYRO_ROLL] = dwTemp & 0x0000000f;
		dwTemp = (unsigned long) gyro_axisy.value;
		gyroAxisMap[JOY_GYRO_YAW] = dwTemp & 0x0000000f;
	}
};

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


static void Joy_Info_f(void)
{
	if (joystick == NULL)
	{
		Con_Printf("No Joystick Active\n");
		return;
	}
	Con_Printf("Using Joystick %i: %s\n", joyIndex, SDL_JoystickName(joystick));
	Con_Printf("\tAxes: %i", joyNumAxes);
	if (joyNumAxes > JOY_MAX_AXES) Con_Printf(" (%i usable)", JOY_MAX_AXES);
	Con_Printf("\n");

	int axisButtons = 2*min(joyNumAxes, JOY_MAX_AXES);
	Con_Printf("\tButtons: %i", axisButtons + joyNumButtons);
	if (axisButtons != 0 || joyNumButtons != 0)
		Con_Printf(" (%i digital + %i analog)", axisButtons, joyNumButtons);
	if (axisButtons + joyNumButtons > JOY_MAX_BUTTONS)
		Con_Printf("\n\t\t%i usable", JOY_MAX_BUTTONS);
	Con_Printf("\n");

	if (controller != NULL)
	{
		Con_Printf("\tGyroscope: ", joyGyroSensor);
		switch (joyGyroSensor)
		{
			case SDL_SENSOR_GYRO: Con_Printf("SDL_SENSOR_GYRO\n"); break;
			case SDL_SENSOR_GYRO_R: Con_Printf("SDL_SENSOR_GYRO_R\n"); break;
			case SDL_SENSOR_GYRO_L: Con_Printf("SDL_SENSOR_GYRO_L\n"); break;
			default: Con_Printf("Other\n"); break;
		}
	}
}

static void Next_Joystick_f(void)
{
	//	(Re-) Init Joystick Systems
	if (joyCount < 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
		{
			Sys_Error("Couldn't init SDL joystick: %s", SDL_GetError());
			return;
		}
	}

	SDL_JoystickEventState(1);
	joyCount = SDL_NumJoysticks();
	if (joyCount < 0)
	{
		Con_Printf ("Error finding Joysticks\n");
		return;
	}
	if (joyCount == 0)
	{
		//	TODO: detect joystick disconnection and do all of this there instead
		Con_Printf ("No joysticks found\n");
		joyIndex = -1;
		joy_avail = 0;
		joystick = NULL;
		controller = NULL;
		return;
	}
	Con_Printf ("%i joystick(s) found\n", joyCount);

	int i = joyIndex;
	SDL_Joystick* newJoy = NULL;
	do
	{
		i = (i+1)%joyCount;
		newJoy = SDL_JoystickOpen(i);
		if (i == joyIndex)
		{
			if (newJoy == NULL)
			{
				Con_Printf("No valid joysticks found\n");
				return;
			}
			break;
		}
	} while (newJoy == NULL);

	joystick = newJoy;
	joyIndex = i;
	joy_avail = true;
	joy_advancedinit = false;
	joyNumAxes = SDL_JoystickNumAxes(joystick);
	joyNumButtons = SDL_JoystickNumButtons(joystick);
	if (i == joyIndex) Con_Printf("Reinitialised %i: %s\n", joyIndex, SDL_JoystickName(joystick));
	else Con_Printf("Joystick set to %i: %s\n", joyIndex, SDL_JoystickName(joystick));

	// Gyroscope Init
	controller = NULL;
	joyGyroSensor = SDL_SENSOR_INVALID;
	if (SDL_IsGameController(joyIndex))
	{
		controller = SDL_GameControllerOpen(joyIndex);

		/*	TODO:	Switch JoyCons support seperate left/right gyro readings
		 *	-	Code below needs to be actually tested with joycons: made with the assumption that SDL_SENSOR_GYRO and SDL_SENSOR_GYRO_R are independent
		 *	-	instead of using whichever sensor is found, implement extra axes to allow seperate mappings for each joycon?
		 */
		if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO))
			joyGyroSensor = SDL_SENSOR_GYRO;
		else if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO_R))
			joyGyroSensor = SDL_SENSOR_GYRO_R;
		else if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO_L))
			joyGyroSensor = SDL_SENSOR_GYRO_L;

		if (joyGyroSensor == SDL_SENSOR_INVALID)
		{
			//	No Gyro Sensor: Game Controller interface is useless; forget it
			SDL_GameControllerClose(controller);
			controller = NULL;
		}
		else
		{
			if (!SDL_GameControllerIsSensorEnabled(controller, joyGyroSensor))
				SDL_GameControllerSetSensorEnabled(controller, joyGyroSensor, 1);
		}
	}
}

void IN_Init (void)
{
	Cvar_Register (&m_filter);

	Cvar_Register (&in_joystick);
	Cvar_Register (&joy_name);
	Cvar_Register (&joy_advanced);
	Cvar_Register (&joy_advaxisx);
	Cvar_Register (&joy_advaxisy);
	Cvar_Register (&joy_advaxisz);
	Cvar_Register (&joy_advaxisr);
	Cvar_Register (&joy_advaxisu);
	Cvar_Register (&joy_advaxisv);

	Cvar_Register (&gyro_axisp);
	Cvar_Register (&gyro_axisr);
	Cvar_Register (&gyro_axisy);

	Cvar_Register (&joy_analogdigitalthreshold);
	Cvar_Register (&joy_forwardthreshold);
	Cvar_Register (&joy_sidethreshold);
	Cvar_Register (&joy_pitchthreshold);
	Cvar_Register (&joy_yawthreshold);
	Cvar_Register (&joy_forwardsensitivity);
	Cvar_Register (&joy_sidesensitivity);
	Cvar_Register (&joy_pitchsensitivity);
	Cvar_Register (&joy_yawsensitivity);

	Cvar_Register (&gyro_enable);
	Cvar_Register (&gyro_accel);
	Cvar_Register (&gyro_threshold);
	Cvar_Register (&gyro_forwardsensitivity);
	Cvar_Register (&gyro_sidesensitivity);
	Cvar_Register (&gyro_pitchsensitivity);
	Cvar_Register (&gyro_yawsensitivity);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	Cmd_AddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);
	Cmd_AddCommand ("joyinfo", Joy_Info_f);
	Cmd_AddCommand ("joynext", Next_Joystick_f);

 	// assume no joystick
	joy_avail = false;
	joystick = NULL;
	controller = NULL;

	Next_Joystick_f();
}

void IN_Shutdown (void)
{
	if (joystick) SDL_JoystickClose(joystick);
	joystick = NULL;
	if (controller) SDL_GameControllerClose(controller);
	controller = NULL;
}

void IN_Commands (void)
{
	if (!joy_avail)
		return;

	int	i, key_index;
	unsigned long	buttonstate = 0;
	float fAxisValue;

	for (i = 0; i < JOY_MAX_BUTTONS && i < joyNumButtons + (2*joyNumAxes); i++)
	{
		if (i < joyNumButtons)
		{
			buttonstate |= (1<<i) * SDL_JoystickGetButton(joystick, i);
		}
		else
		{
			//	convert axes to digital inputs

			//	only allow presses for axes not already bound to movement/aiming
			//if (dwAxisMap[(i-joyNumButtons)/2] != 0) continue;

			if ((i-joyNumButtons)%2 == 0)
			{
				fAxisValue = SDL_JoystickGetAxis(joystick, (i-joyNumButtons)/2);
				fAxisValue /= 32768.0;
				buttonstate |= (1<<i) * (fAxisValue < -joy_analogdigitalthreshold.value);
			}
			else // check inverse: uses previous fAxisValue
				buttonstate |= (1<<i) * (fAxisValue > joy_analogdigitalthreshold.value);
		}

		// AUX1-4 are unused due to how the key_index is calculated
		// fix below. leaving uncompiled for now
#ifndef AUXFIX
		if ((buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)))
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, true);
		}

		if (!(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)))
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, false);
		}
#else	//	fix
		//	TODO: increase JOY_MAX_BUTTONS to 36(?), and remove key_index variable
		if ((buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)))
			Key_Event (K_JOY1 + i, true);
		else if (!(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)))
			Key_Event (K_JOY1 + i, false);
#endif
	}
	joy_oldbuttonstate = buttonstate;
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

static void IN_JoyMove (usercmd_t *cmd)
{
	// adapted from in_win

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick.value)
		return;

	if (joy_advancedinit != true)
	{
		Joy_AdvancedUpdate_f ();
		joy_advancedinit = true;
	}

	int	i;
	float	speed, aspeed, fAxisValue;

	speed = (in_speed.state & 1) ? cl_movespeedkey.value : 1;
	aspeed = speed * host_frametime;

	// loop through the axes
	for (i=0 ; i<JOY_MAX_AXES && i < joyNumAxes ; i++)
	{
		fAxisValue = SDL_JoystickGetAxis(joystick, i);
		fAxisValue /= 32768.0; // convert range from -32768..32767 to -1..1 

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced.value == 0.0) && mlook_active)
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{		
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch.value < 0.0)
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					else
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					V_StopPitchDrift ();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if (lookspring.value == 0.0)
						V_StopPitchDrift ();
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold.value)
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold.value)
				cmd->sidemove += (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold.value)
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold.value)
				{
					if (dwControlMap[i] == JOY_ABSOLUTE_AXIS)
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					else
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * speed * 180.0;
				}
			}
			break;

		case AxisLook:
			if (mlook_active)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// pitch movement detected and pitch movement desired by user
					if (dwControlMap[i] == JOY_ABSOLUTE_AXIS)
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					else
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * speed * 180.0;
					V_StopPitchDrift ();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if (lookspring.value == 0.0)
						V_StopPitchDrift ();
				}
			}
			break;
		case AxisDebug:
			//	TODO: print this to screen instead of console (i can't get it working - i assume the world gets drawn after this)
			Con_Printf ("Axis %i: %f (%f)\n", i, fAxisValue, fAxisValue*32768);
			break;
		default:
			break;
		}
	}

	cl.viewangles[PITCH] = bound(cl_minpitch.value, cl.viewangles[PITCH], cl_maxpitch.value);
}
static void IN_GyroMove (usercmd_t *cmd)
{
	if (!joy_avail || !in_joystick.value || controller == NULL || !gyro_enable.value)
		return;
	// TODO: nothing ^^^here accounts for if the joystick is disconnected, so this function will still continue,
	// though gyroData is cleared after being read once, so it's just about fine?

	//	Prepare Gyro Data
	if (SDL_GameControllerGetSensorData(controller, joyGyroSensor, gyroData, GYRO_MAX_AXES) != 0)
		return;

	int	i;

	float accel_factor = 0;
	if (gyro_accel.value != 0)
	{
		//	a quick and dirty implementation copied from m_accel: don't use accel personally, so might need to be tested and changed by someone who knows what they're doing
		//	would per-axis accel work better? especially if more axes were implemented
		for (i=0 ; i<GYRO_MAX_AXES; i++)
			if (gyroAxisMap[i] == AxisTurn || gyroAxisMap[i] == AxisLook) // only for aiming
				accel_factor += gyroData[i]*gyroData[i];

		accel_factor = sqrt(accel_factor);
		accel_factor *= gyro_accel.value * 0.1;
	}

	// loop through the axes
	for (i=0 ; i<GYRO_MAX_AXES; i++)
	{
		gyroBank[i] += gyroData[i];

		if (fabs(gyroBank[i]) < gyro_threshold.value)
		{
			// decay banked data
			if (4*gyroBank[i] < gyro_threshold.value && -4*gyroBank[i] < gyro_threshold.value)
			if (4*fabs(gyroBank[i]) < gyro_threshold.value)
				gyroBank[i] = 0;
			else gyroBank[i] /= 1.1;
			continue;
		}

		switch (gyroAxisMap[i])
		{
		case AxisForward:
			cmd->forwardmove += (gyroBank[i] * (accel_factor + gyro_forwardsensitivity.value)) * cl_forwardspeed.value;
			break;

		case AxisSide:
			cmd->sidemove += (gyroBank[i] * (accel_factor + gyro_sidesensitivity.value)) * cl_sidespeed.value;
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
			{
				// user wants turn control to become side control
				cmd->sidemove -= (gyroBank[i] * (accel_factor + gyro_sidesensitivity.value)) * cl_sidespeed.value;
			}
			else
			{
				// user wants turn control to be turn control
				if (dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					cl.viewangles[YAW] += (gyroBank[i] * (accel_factor + gyro_yawsensitivity.value)) * host_frametime * cl_yawspeed.value;
				else
					cl.viewangles[YAW] += (gyroBank[i] * (accel_factor + gyro_yawsensitivity.value)) * 180.0;
			}
			break;

		case AxisLook:
			if (dwControlMap[i] == JOY_ABSOLUTE_AXIS)
				cl.viewangles[PITCH] += (gyroBank[i] * (accel_factor + gyro_pitchsensitivity.value)) * host_frametime * cl_pitchspeed.value;
			else
				cl.viewangles[PITCH] += (gyroBank[i] * (accel_factor + gyro_pitchsensitivity.value)) * 180.0;
			V_StopPitchDrift ();
			break;

		case AxisDebug:
			Con_Printf ("Axis %i: %f (%f)\n", i, gyroBank[i], gyroBank[i]*32768);
			break;
		default:
			break;
		}

		// gyro data was parsed: clear bank
		gyroBank[i] = 0;
	}

	cl.viewangles[PITCH] = bound(cl_minpitch.value, cl.viewangles[PITCH], cl_maxpitch.value);
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove (cmd);
	IN_JoyMove (cmd);
	IN_GyroMove (cmd);
}

void IN_ClearStates (void)
{
	mx = my = old_mouse_x = old_mouse_y = 0.0;
}
