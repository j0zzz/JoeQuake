#include "quakedef.h"


cvar_t	freefly_speed = {"freefly_speed", "800"};

extern kbutton_t	in_freeflymove, in_forward, in_back, in_moveleft, in_moveright;

static void FreeFly_Toggle_f (void)
{
	cl.freefly_enabled = !cl.freefly_enabled;

	if (cl.freefly_enabled)
	{
		cl.freefly_reset = true;
		cl.freefly_last_time = Sys_DoubleTime();
	}
}


static void FreeFly_CopyCam_f (void)
{
	trace_t	trace;
	vec3_t	forward, right, up, end;
	char cam_buf[1024];

	AngleVectors (cl.freefly_angles, forward, right, up);
	VectorMA(cl.freefly_origin, 2048, forward, end);

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, cl.freefly_origin, end, &trace);

	if (!cl.freefly_enabled)
	{
		Con_Printf("freefly not enabled\n");
		return;
	}
	snprintf(cam_buf, sizeof(cam_buf),
			 "%.2f move %.1f %.1f %.1f\n"
			 "%.2f pan %.1f %.1f %.1f\n",
			  cl.mtime[0],
			  cl.freefly_origin[0],
			  cl.freefly_origin[1],
			  cl.freefly_origin[2],
			  cl.mtime[0],
			  trace.endpos[0],
			  trace.endpos[1],
			  trace.endpos[2]);
	if (Sys_SetClipboardData(cam_buf))
		Con_Printf("Remaic commands copy to clipboard:\n%s", cam_buf);
}


qboolean FreeFly_Moving (void)
{
	return cl.freefly_enabled && (in_freeflymove.state & 1);
}


void FreeFly_SetRefdef (void)
{
	if (cl.freefly_enabled)
	{
		if (cl.freefly_reset)
		{
			VectorCopy (r_refdef.vieworg, cl.freefly_origin);
			VectorCopy (r_refdef.viewangles, cl.freefly_angles);
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
	vec3_t	forward, right, up, vel;

	if (!FreeFly_Moving())
		return;

	time = Sys_DoubleTime();
	frametime = time - cl.freefly_last_time;
	cl.freefly_last_time = time;

	AngleVectors (cl.freefly_angles, forward, right, up);
	VectorScale(forward,
				CL_KeyState (&in_forward) - CL_KeyState (&in_back),
				vel);
	VectorMA(vel,
			 CL_KeyState (&in_moveright) - CL_KeyState (&in_moveleft),
			 right,
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
}
