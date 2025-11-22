#include "quakedef.h"


static cvar_t	democam_freefly_speed = {"freefly_speed", "800"};
static cvar_t	democam_freefly_show_pos = {"freefly_show_pos", "0"};
static cvar_t	democam_freefly_show_pos_x = { "freefly_show_pos_x", "-5" }; 
static cvar_t	democam_freefly_show_pos_y = { "freefly_show_pos_y", "-3" };
static cvar_t	democam_freefly_show_pos_dp = {"freefly_show_pos_dp", "1"};
static cvar_t	democam_orbit_speed = {"orbit_speed", "200"};

extern kbutton_t	in_freeflymlook, in_forward, in_back, in_moveleft, in_moveright, in_up, in_down, in_jump;



static void DemoCam_SetMode (democam_mode_t mode)
{
	cl.democam_mode = mode;

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
	} else {
		Con_Printf("First person mode enabled.\n");
	}
}


static void DemoCam_Mode_f (void)
{

	democam_mode_t next_mode;

	if (!cls.demoplayback)
	{
		Con_Printf("Must be playing a demo to use demo cam.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf("Camera mode is: %d\n"
					"Usage:\n"
					" %s    : show current mode\n"
					" %s 0  : set first person mode\n"
					" %s 1  : set freefly mode\n"
					" %s 2  : set orbit mode\n",
					" %s +1 : cycle forwards through modes\n",
					" %s -1 : cycle backwards through modes\n",
					cl.democam_mode,
					Cmd_Argv(0), Cmd_Argv(0), Cmd_Argv(0), Cmd_Argv(0));
		return;
	}

	if (!strcmp(Cmd_Argv(1), "+1"))
	{
		if (cl.democam_mode == DEMOCAM_MODE_COUNT - 1)
			next_mode = 0;
		else
			next_mode = cl.democam_mode + 1;
	} else if (!strcmp(Cmd_Argv(1), "-1")) {
		if (cl.democam_mode == 0)
			next_mode = DEMOCAM_MODE_COUNT - 1;
		else
			next_mode = cl.democam_mode - 1;
	} else {
		next_mode = Q_atoi(Cmd_Argv(1));
		if (next_mode >= DEMOCAM_MODE_COUNT || next_mode < 0)
		{
			Con_Printf("Invalid camera mode %d\n", next_mode);
			return;
		}
	}

	DemoCam_SetMode (next_mode);
}


static void DemoCam_FreeFly_Toggle_f (void)
{
	if (!cls.demoplayback)
	{
		Con_Printf("Must be playing a demo to enable freefly.\n");
		return;
	}

	if (cl.democam_mode != DEMOCAM_MODE_FREEFLY)
		DemoCam_SetMode(DEMOCAM_MODE_FREEFLY);
	else
		DemoCam_SetMode(DEMOCAM_MODE_FIRST_PERSON);
}


static void DemoCam_Orbit_Toggle_f (void)
{
	if (!cls.demoplayback)
	{
		Con_Printf("Must be playing a demo to enable orbit.\n");
		return;
	}

	if (cl.democam_mode != DEMOCAM_MODE_ORBIT)
		DemoCam_SetMode(DEMOCAM_MODE_ORBIT);
	else
		DemoCam_SetMode(DEMOCAM_MODE_FIRST_PERSON);
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

	AngleVectors (cl.democam_freefly_angles, forward, right, up);
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


static void DemoCam_PrintEntities_f(void)
{
	typedef struct {
		int index;
		char* name;
		double distance;
	} entity_info_t;

	if (cls.state != ca_connected)
	{
		Con_Printf("ERROR: You must be connected\n");

	}
	else if (cl.democam_mode != DEMOCAM_MODE_FREEFLY)
	{
		Con_Printf("ERROR: freefly not enabled\n");

	}
	else {

		Con_Printf("ID     Dist  Model\n----------------------------\n");

		entity_info_t entity_list[4096];
		int entity_count = 0;

		entity_t* ent;
		int i, j;

		for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++)
		{
			if (!ent->model)
			{
				continue;
			}
			if (strchr(ent->model->name, '*'))
			{
				continue;
			}

			double dx_squared = pow(ent->origin[0] - cl.democam_freefly_origin[0], 2);
			double dy_squared = pow(ent->origin[1] - cl.democam_freefly_origin[1], 2);
			double dz_squared = pow(ent->origin[2] - (cl.democam_freefly_origin[2] - DEFAULT_VIEWHEIGHT), 2);

			double entity_distance = sqrt(dx_squared + dy_squared + dz_squared);

			entity_list[entity_count].index = i;
			entity_list[entity_count].name = ent->model->name;
			entity_list[entity_count].distance = entity_distance;
			entity_count++;
		}

		for (i = 0; i < entity_count - 1; i++)
		{
			for (j = 0; j < entity_count - i - 1; j++)
			{
				if (entity_list[j].distance < entity_list[j + 1].distance)
				{
					entity_info_t temp = entity_list[j];
					entity_list[j] = entity_list[j + 1];
					entity_list[j + 1] = temp;
				}
			}
		}

		int start_index = (entity_count > 6) ? (entity_count - 6) : 0;
		for (i = start_index; i < entity_count; i++)
		{
			Con_Printf("E%-4i %5.1f  %s\n", entity_list[i].index, entity_list[i].distance, entity_list[i].name);
		}
	}
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
	{
		if (cl.democam_mode != DEMOCAM_MODE_FIRST_PERSON)
			DemoCam_SetMode(DEMOCAM_MODE_FIRST_PERSON);
	} else if (cl.democam_mode == DEMOCAM_MODE_FREEFLY)
	{
		if (cl.democam_freefly_reset)
		{
			VectorCopy (r_refdef.vieworg, cl.democam_freefly_origin);
			VectorCopy (r_refdef.viewangles, cl.democam_freefly_angles);
			cl.democam_freefly_angles[ROLL] = 0.;
			cl.democam_freefly_reset = false;
		} else {
			VectorCopy (cl.democam_freefly_origin, r_refdef.vieworg);
			VectorCopy (cl.democam_freefly_angles, r_refdef.viewangles);
		}
	}
	else if (cl.democam_mode == DEMOCAM_MODE_ORBIT)
	{
		AngleVectors (cl.democam_orbit_angles, forward, right, up);
		VectorMA(r_refdef.vieworg, -cl.democam_orbit_distance, forward,
				r_refdef.vieworg);
		VectorCopy (cl.democam_orbit_angles, r_refdef.viewangles);
	}
}


void DemoCam_MouseMove (double x, double y)
{
	float *angles = NULL;

	if (!DemoCam_MLook())
		return;

	if (cl.democam_mode == DEMOCAM_MODE_FREEFLY)
		angles = cl.democam_freefly_angles;
	else if (cl.democam_mode == DEMOCAM_MODE_ORBIT)
		angles = cl.democam_orbit_angles;

	if (angles != NULL)
	{
		angles[YAW] -= m_yaw.value * x;
		angles[PITCH] += m_pitch.value * y;
		angles[PITCH] = bound(-90, angles[PITCH], 90);
	}
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
		AngleVectors (cl.democam_freefly_angles, forward, right, up);
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
		cl.democam_orbit_distance += -democam_orbit_speed.value * frametime
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


void DemoCam_InitClient (void)
{
	// Set any variables that shouldn't be zero.
	cl.democam_orbit_distance = 300.;
}


void DemoCam_Init (void)
{
	Cvar_Register(&democam_freefly_speed);
	Cvar_Register(&democam_freefly_show_pos);
	Cvar_Register(&democam_freefly_show_pos_x);
	Cvar_Register(&democam_freefly_show_pos_y);
	Cvar_Register(&democam_freefly_show_pos_dp);
	Cvar_Register(&democam_orbit_speed);

	Cmd_AddCommand("democam_mode", DemoCam_Mode_f);
	Cmd_AddCommand("freefly", DemoCam_FreeFly_Toggle_f);
	Cmd_AddCommand("orbit", DemoCam_Orbit_Toggle_f);
	Cmd_AddCommand("freefly_copycam", DemoCam_CopyCam_f);
	Cmd_AddCommand("freefly_writecam", DemoCam_WriteCam_f);
	Cmd_AddCommand("freefly_print_entities", DemoCam_PrintEntities_f);
}
