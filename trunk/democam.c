#include "quakedef.h"


cvar_t	democam_freefly_speed = {"democam_freefly_speed", "800"};
cvar_t	democam_freefly_show_pos = {"freefly_show_pos", "0"};
cvar_t  democam_freefly_show_pos_x = { "freefly_show_pos_x", "-5" }; 
cvar_t  democam_freefly_show_pos_y = { "freefly_show_pos_y", "-3" };
cvar_t  democam_freefly_show_pos_dp = {"freefly_show_pos_dp", "1"};

extern kbutton_t	in_freeflymlook, in_forward, in_back, in_moveleft, in_moveright, in_up, in_down, in_jump;

static void DemoCam_Toggle_f (void)
{
	if (!cls.demoplayback)
	{
		Con_Printf("Must be playing a demo to enable freefly.\n");
		cl.freefly_enabled = false;
		return;
	}

	cl.freefly_enabled = !cl.freefly_enabled;

	if (cl.freefly_enabled)
	{
		Con_Printf("Freefly enabled.");
		if (cl_demoui.value != 0)
			Con_Printf(" Hold mouse2 or bind +freeflymlook to change view.");
		Con_Printf("\n");
		cl.freefly_reset = true;
		cl.freefly_last_time = Sys_DoubleTime();
	} else {
		Con_Printf("Freefly disabled.\n");
	}
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
	if (!cl.freefly_enabled)
	{
		Con_Printf("ERROR: freefly not enabled\n");
		return NULL;
	}

	AngleVectors (cl.freefly_angles, forward, right, up);
	VectorMA(cl.freefly_origin, 2048, forward, end);

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, cl.freefly_origin, end, &trace);

	if (!strcmp(arg, "move"))
	{
		snprintf(cam_buf, sizeof(cam_buf),
				 "move %.1f %.1f %.1f\n"
				 "pan %.1f %.1f %.1f\n"
				 "%.2f\n",
				 cl.freefly_origin[0],
				 cl.freefly_origin[1],
				 cl.freefly_origin[2] - DEFAULT_VIEWHEIGHT,
				 trace.endpos[0],
				 trace.endpos[1],
				 trace.endpos[2],
				 cl.mtime[0]);
	} else if (!strcmp(arg, "stay")) {
		snprintf(cam_buf, sizeof(cam_buf),
				 "%.2f stay %.1f %.1f %.1f\n",
				 cl.mtime[0],
				 cl.freefly_origin[0],
				 cl.freefly_origin[1],
				 cl.freefly_origin[2] - DEFAULT_VIEWHEIGHT);
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
	return cl.freefly_enabled
		&& (cl_demoui.value == 0 || (in_freeflymlook.state & 1) || demoui_freefly_mlook);
}


void DemoCam_SetRefdef (void)
{
    if (!cls.demoplayback)
        cl.freefly_enabled = false;

	if (cl.freefly_enabled)
	{
		if (cl.freefly_reset)
		{
			VectorCopy (r_refdef.vieworg, cl.freefly_origin);
			VectorCopy (r_refdef.viewangles, cl.freefly_angles);
			cl.freefly_angles[ROLL] = 0.;
			cl.freefly_reset = false;
		} else {
			VectorCopy (cl.freefly_origin, r_refdef.vieworg);
			VectorCopy (cl.freefly_angles, r_refdef.viewangles);
		}
	}
}


void DemoCam_MouseMove (double x, double y)
{
	if (!DemoCam_MLook())
		return;

	cl.freefly_angles[YAW] -= m_yaw.value * x;
	cl.freefly_angles[PITCH] += m_pitch.value * y;
	cl.freefly_angles[PITCH] = bound(-90, cl.freefly_angles[PITCH], 90);
}


void DemoCam_UpdateOrigin (void)
{
	double time, frametime;
	vec3_t forward, right, up, vel;
	vec3_t world_up = {0, 0, 1};

	time = Sys_DoubleTime();
	frametime = time - cl.freefly_last_time;
	cl.freefly_last_time = time;

	if (!cl.freefly_enabled)
		return;

	AngleVectors (cl.freefly_angles, forward, right, up);
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
	VectorMA(cl.freefly_origin, frametime, vel, cl.freefly_origin);
}


void DemoCam_DrawPos (void)
{
	int x, y;
	int dp;
	char str[128];
	vec3_t pos;

	if (cls.state != ca_connected || !cl.freefly_enabled
			|| !democam_freefly_show_pos.value)
		return;

	dp = (int)democam_freefly_show_pos_dp.value;
	VectorCopy(cl.freefly_origin, pos);
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

	Cmd_AddCommand("freefly", DemoCam_Toggle_f);
	Cmd_AddCommand("freefly_copycam", DemoCam_CopyCam_f);
	Cmd_AddCommand("freefly_writecam", DemoCam_WriteCam_f);
}
