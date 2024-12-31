#include "quakedef.h"


static qboolean	DemoCam_CameraModeChange (struct cvar_s *var, char *value);
static cvar_t	democam_mode = {"democam_mode", "0", 0, DemoCam_CameraModeChange};
static cvar_t	democam_freefly_speed = {"freefly_speed", "800"};
static cvar_t	democam_freefly_show_pos = {"freefly_show_pos", "0"};
static cvar_t	democam_freefly_show_pos_x = { "freefly_show_pos_x", "-5" }; 
static cvar_t	democam_freefly_show_pos_y = { "freefly_show_pos_y", "-3" };
static cvar_t	democam_freefly_show_pos_dp = {"freefly_show_pos_dp", "1"};

extern kbutton_t	in_freeflymlook, in_forward, in_back, in_moveleft, in_moveright, in_up, in_down, in_jump;


static qboolean DemoCam_CameraModeChange (struct cvar_s *var, char *value)
{
	float float_val;
	int int_val;

	float_val = Q_atof(value);
	int_val = (int)float_val;

	if (int_val != float_val || int_val >= DEMOCAM_MODE_COUNT || int_val < 0)
	{
		Con_Printf("Invalid camera mode %f\n", float_val);
		return false;
	}

	cl.democam_mode = int_val;

	if (cl.democam_mode == DEMOCAM_MODE_FREEFLY)
	{
		Con_Printf("Freefly enabled.");
		if (cl_demoui.value != 0)
			Con_Printf(" Hold mouse2 or bind +freeflymlook to change view.");
		Con_Printf("\n");
		cl.democam_freefly_reset = true;
		cl.democam_last_time = Sys_DoubleTime();
	} else if (cl.democam_mode == DEMOCAM_MODE_ORBIT) {
		Con_Printf("Orbit enabled.");
		if (cl_demoui.value != 0)
			Con_Printf(" Hold mouse2 or bind +freeflymlook to change view.");
		Con_Printf("\n");
		cl.democam_last_time = Sys_DoubleTime();
		cl.democam_orbit_distance = 300;
	} else {
		Con_Printf("Freefly disabled.\n");
	}

	return true;
}


static void DemoCam_Toggle_f (void)
{
	if (!cls.demoplayback)
	{
		Con_Printf("Must be playing a demo to enable freefly.\n");
		return;
	}

	if (cl.democam_mode != DEMOCAM_MODE_FREEFLY)
		Cvar_SetValue(&democam_mode, DEMOCAM_MODE_FREEFLY);
	else
		Cvar_SetValue(&democam_mode, DEMOCAM_MODE_FIRST_PERSON);
}


static void DemoCam_Orbit_Toggle_f (void)
{
	if (!cls.demoplayback)
	{
		Con_Printf("Must be playing a demo to enable orbit.\n");
		return;
	}

	if (cl.democam_mode != DEMOCAM_MODE_ORBIT)
		Cvar_SetValue(&democam_mode, DEMOCAM_MODE_ORBIT);
	else
		Cvar_SetValue(&democam_mode, DEMOCAM_MODE_FIRST_PERSON);
}


static char *DemoCam_GetRemaicCommand (const char *arg)
{
	trace_t	trace;
	vec3_t	forward, right, up, end;
	static char cam_buf[1024];

	if (cls.state != ca_connected)
	{
		Con_Printf("ERROR: You must be connected\n");
		return NULL;
	}
	if (cl.democam_mode != DEMOCAM_MODE_FREEFLY)
	{
		Con_Printf("ERROR: freefly not enabled\n");
		return NULL;
	}

	AngleVectors (cl.democam_angles, forward, right, up);
	VectorMA(cl.democam_freefly_origin, 2048, forward, end);

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, cl.democam_freefly_origin, end, &trace);

	if (!strcmp(arg, "move"))
	{
		snprintf(cam_buf, sizeof(cam_buf),
				 "move %.1f %.1f %.1f\n"
				 "pan %.1f %.1f %.1f\n"
				 "%.2f\n",
				 cl.democam_freefly_origin[0],
				 cl.democam_freefly_origin[1],
				 cl.democam_freefly_origin[2] - DEFAULT_VIEWHEIGHT,
				 trace.endpos[0],
				 trace.endpos[1],
				 trace.endpos[2],
				 cl.mtime[0]);
	} else if (!strcmp(arg, "stay")) {
		snprintf(cam_buf, sizeof(cam_buf),
				 "%.2f stay %.1f %.1f %.1f\n",
				 cl.mtime[0],
				 cl.democam_freefly_origin[0],
				 cl.democam_freefly_origin[1],
				 cl.democam_freefly_origin[2] - DEFAULT_VIEWHEIGHT);
	} else if (!strcmp(arg, "view")) {
		snprintf(cam_buf, sizeof(cam_buf),
				 "%.2f view %.1f %.1f %.1f\n",
				 cl.mtime[0],
				 trace.endpos[0],
				 trace.endpos[1],
				 trace.endpos[2]);
	} else if (!strcmp(arg, "endmove")) {
		snprintf(cam_buf, sizeof(cam_buf), "%.2f end move\n", cl.mtime[0]);
	} else if (!strcmp(arg, "endpan")) {
		snprintf(cam_buf, sizeof(cam_buf), "%.2f end pan\n", cl.mtime[0]);
	} else if (!strcmp(arg, "time")) {
		snprintf(cam_buf, sizeof(cam_buf), "%.2f\n", cl.mtime[0]);
	} else {
		Con_Printf("ERROR: invalid argument \"%s\"\n", arg);
		return NULL;
	}

	return cam_buf;
}


static void DemoCam_WriteCam_f (void)
{
	char path[MAX_OSPATH];
	char *cmd;
	const char *arg;
	FILE *f;

	if (Cmd_Argc() < 2 || Cmd_Argc() > 3)
	{
		Con_Printf("Usage: %s [filename] (move|stay|view|endmove|endpan|time)\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 2)
	{
		arg = "move";  // For backwards compatibility assume "move" if not specified
	} else {
		arg = Cmd_Argv(2);
	}

	cmd = DemoCam_GetRemaicCommand(arg);
	if (cmd == NULL)
		return;

	Q_strncpyz(path, Cmd_Argv(1), sizeof(path));
	COM_DefaultExtension (path, ".cam");

	if (!(f = fopen(va("%s/%s", com_gamedir, path), "a")))
	{
		Con_Printf ("Couldn't write %s\n", path);
		return;
	}

	fprintf(f, "%s", cmd);

	fclose (f);

	Con_Printf("Remaic commands written to %s:\n%s", path, cmd);
}


static void DemoCam_CopyCam_f (void)
{
	char *cmd;
	const char *arg;

	if (Cmd_Argc() == 1)
	{
		arg = "move";  // For backwards compatibility assume "move" if not specified
	} else {
		arg = Cmd_Argv(1);
	}

	cmd = DemoCam_GetRemaicCommand(arg);

	if (cmd != NULL && Sys_SetClipboardData(cmd))
		Con_Printf("Remaic commands copy to clipboard:\n%s", cmd);
}


qboolean DemoCam_MLook (void)
{
	return cl.democam_mode != DEMOCAM_MODE_FIRST_PERSON
		&& (cl_demoui.value == 0 || (in_freeflymlook.state & 1) || demoui_freefly_mlook);
}


void DemoCam_SetRefdef (void)
{
	vec3_t forward, right, up;

	if (!cls.demoplayback)
		Cvar_SetValue(&democam_mode, DEMOCAM_MODE_FIRST_PERSON);

	if (cl.democam_mode == DEMOCAM_MODE_FREEFLY)
	{
		if (cl.democam_freefly_reset)
		{
			VectorCopy (r_refdef.vieworg, cl.democam_freefly_origin);
			VectorCopy (r_refdef.viewangles, cl.democam_angles);
			cl.democam_angles[ROLL] = 0.;
			cl.democam_freefly_reset = false;
		} else {
			VectorCopy (cl.democam_freefly_origin, r_refdef.vieworg);
			VectorCopy (cl.democam_angles, r_refdef.viewangles);
		}
	}
	else if (cl.democam_mode == DEMOCAM_MODE_ORBIT)
	{
		AngleVectors (cl.democam_angles, forward, right, up);
		VectorMA(r_refdef.vieworg, -cl.democam_orbit_distance, forward,
				r_refdef.vieworg);
		VectorCopy (cl.democam_angles, r_refdef.viewangles);
	}
}


void DemoCam_MouseMove (double x, double y)
{
	if (!DemoCam_MLook())
		return;

	cl.democam_angles[YAW] -= m_yaw.value * x;
	cl.democam_angles[PITCH] += m_pitch.value * y;
	cl.democam_angles[PITCH] = bound(-90, cl.democam_angles[PITCH], 90);
}


void DemoCam_UpdateOrigin (void)
{
	double time, frametime;
	vec3_t forward, right, up, vel;
	vec3_t world_up = {0, 0, 1};

	time = Sys_DoubleTime();
	frametime = time - cl.democam_last_time;
	cl.democam_last_time = time;

	if (cl.democam_mode == DEMOCAM_MODE_FREEFLY)
	{
		AngleVectors (cl.democam_angles, forward, right, up);
		VectorScale(forward,
					CL_KeyState (&in_forward) - CL_KeyState (&in_back),
					vel);
		VectorMA(vel,
				 CL_KeyState (&in_moveright) - CL_KeyState (&in_moveleft),
				 right,
				 vel);
		VectorMA(vel,
				 CL_KeyState (&in_up)
				 + CL_KeyState (&in_jump)
				 - CL_KeyState (&in_down),
				 world_up,
				 vel);
		VectorNormalize(vel);
		VectorScale(vel, democam_freefly_speed.value, vel);
		VectorMA(cl.democam_freefly_origin, frametime, vel, cl.democam_freefly_origin);
	}
	else if (cl.democam_mode == DEMOCAM_MODE_ORBIT)
	{
		cl.democam_orbit_distance += -democam_freefly_speed.value * frametime
			* (CL_KeyState(&in_forward) - CL_KeyState(&in_back));
		cl.democam_orbit_distance = max(0, cl.democam_orbit_distance);
	}
}


void DemoCam_DrawPos (void)
{
	int x, y;
	int dp;
	char str[128];
	vec3_t pos;

	if (cls.state != ca_connected
			|| cl.democam_mode != DEMOCAM_MODE_FREEFLY
			|| !democam_freefly_show_pos.value)
		return;

	dp = (int)democam_freefly_show_pos_dp.value;
	VectorCopy(cl.democam_freefly_origin, pos);
	pos[2] -= DEFAULT_VIEWHEIGHT;

	snprintf(str, sizeof(str), "\xd8\xba%+*.*f \xd9\xba%+*.*f \xda\xba%+*.*f",
				dp + 6, dp, pos[0],
				dp + 6, dp, pos[1],
				dp + 6, dp, pos[2]);

	x = ELEMENT_X_COORD(democam_freefly_show_pos);
	y = ELEMENT_Y_COORD(democam_freefly_show_pos);
	Draw_String (x, y, str, true);
}


void DemoCam_Init (void)
{
	Cvar_Register(&democam_freefly_speed);
	Cvar_Register(&democam_freefly_show_pos);
	Cvar_Register(&democam_freefly_show_pos_x);
	Cvar_Register(&democam_freefly_show_pos_y);
	Cvar_Register(&democam_freefly_show_pos_dp);
	Cvar_Register(&democam_mode);

	Cmd_AddCommand("freefly", DemoCam_Toggle_f);
	Cmd_AddCommand("orbit", DemoCam_Orbit_Toggle_f);
	Cmd_AddCommand("freefly_copycam", DemoCam_CopyCam_f);
	Cmd_AddCommand("freefly_writecam", DemoCam_WriteCam_f);
}
