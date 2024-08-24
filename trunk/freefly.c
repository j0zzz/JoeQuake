#include "quakedef.h"


cvar_t	freefly_speed = {"freefly_speed", "800"};

extern kbutton_t	in_freeflymove, in_forward, in_back, in_moveleft, in_moveright, in_up, in_down, in_jump;

static void FreeFly_Toggle_f (void)
{
	cl.freefly_enabled = !cl.freefly_enabled;

	if (cl.freefly_enabled)
	{
		cl.freefly_reset = true;
		cl.freefly_last_time = Sys_DoubleTime();
	}
}


static char *FreeFly_GetRemaicCommand (void)
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

	return cam_buf;
}


static void FreeFly_WriteCam_f (void)
{
	char path[MAX_OSPATH];
	char *cmd;
	FILE *f;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("Usage: %s [filename]\n", Cmd_Argv(0));
		return;
	}

	cmd = FreeFly_GetRemaicCommand();
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


static void FreeFly_CopyCam_f (void)
{
	char *cmd = FreeFly_GetRemaicCommand();

	if (cmd != NULL && Sys_SetClipboardData(cmd))
		Con_Printf("Remaic commands copy to clipboard:\n%s", cmd);
}


qboolean FreeFly_Moving (void)
{
	return cl.freefly_enabled
		&& (cl_demoui.value == 0 || (in_freeflymove.state & 1) || demoui_freefly_move);
}


void FreeFly_SetRefdef (void)
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


void FreeFly_MouseMove (double x, double y)
{
	if (!FreeFly_Moving())
		return;

	cl.freefly_angles[YAW] -= m_yaw.value * x;
	cl.freefly_angles[PITCH] += m_pitch.value * y;
	cl.freefly_angles[PITCH] = bound(-90, cl.freefly_angles[PITCH], 90);
}


void FreeFly_UpdateOrigin (void)
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
	VectorScale(vel, freefly_speed.value, vel);
	VectorMA(cl.freefly_origin, frametime, vel, cl.freefly_origin);
}


void FreeFly_Init (void)
{
	Cvar_Register(&freefly_speed);

	Cmd_AddCommand("freefly", FreeFly_Toggle_f);
	Cmd_AddCommand("freefly_copycam", FreeFly_CopyCam_f);
	Cmd_AddCommand("freefly_writecam", FreeFly_WriteCam_f);
}
