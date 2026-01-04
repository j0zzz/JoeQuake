#include "../quakedef.h"

static cvar_t pathtracer_record_player = { "pathtracer_record_player", "0" };
static cvar_t pathtracer_record_samplerate = { "pathtracer_record_samplerate", "0" };
static cvar_t pathtracer_show_player = { "pathtracer_show_player", "0" };
static cvar_t pathtracer_show_demo = { "pathtracer_show_demo", "0" };
static cvar_t pathtracer_show_ghost = { "pathtracer_show_ghost", "0" };
static cvar_t pathtracer_movekeys_player = { "pathtracer_movekeys_player", "1" };
static cvar_t pathtracer_movekeys_demo = { "pathtracer_movekeys_demo", "1" };
static cvar_t pathtracer_movekeys_ghost = { "pathtracer_movekeys_ghost", "1" };
static cvar_t pathtracer_fadeout_ghost = { "pathtracer_fadeout_ghost", "0" };
static cvar_t pathtracer_fadeout_demo = { "pathtracer_fadeout_demo", "1" };
static cvar_t pathtracer_fadeout_seconds = { "pathtracer_fadeout_seconds", "3" };
static cvar_t pathtracer_line_smooth = { "pathtracer_line_smooth", "0" };
static cvar_t pathtracer_line_skip_threshold = { "pathtracer_line_skip_threshold", "160" };
static ghost_level_t* demo_current_level = NULL;

static double prev_cltime = -1;
extern qboolean physframe;


ghost_info_t player_record_info;
ghost_level_t* player_record_current_level = NULL;

// replace VectorVectors, but now with up always pointing up
void VectorVectorsAlwaysUp(vec3_t forward, vec3_t right, vec3_t up)
{
	VectorSet(right, -forward[1], forward[0], 0);
	VectorNormalizeFast(right);
	CrossProduct(right, forward, up);
}

#define RECORD_REALLOC_LOG2     9   // 512 records per chunk, approx 7 seconds

static void PathTracer_Append(ghost_level_t* level, ghostrec_t* rec)
{
	int num_chunks;

	if ((level->num_records & ((1 << RECORD_REALLOC_LOG2) - 1)) == 0)
	{
		num_chunks = (level->num_records >> RECORD_REALLOC_LOG2) + 1;
		level->records = Q_realloc(level->records,
			sizeof(ghostrec_t)
			* (num_chunks << RECORD_REALLOC_LOG2));
	}
	level->records[level->num_records] = *rec;
	level->num_records++;
}

void Pathtracer_Draw_MoveKeys_Triangle(float r, float g, float b, vec3_t edge1, vec3_t edge2, vec3_t edge3)
{
	glColor3f(r, g, b);
	glVertex3fv(edge1);
	glVertex3fv(edge2);
	glVertex3fv(edge2);
	glVertex3fv(edge3);
	glVertex3fv(edge3);
	glVertex3fv(edge1);
}

void PathTracer_Draw_MoveKeys(GLfloat pos_on_path[3], vec3_t v_forward, movekeytype_t* movekeys_states)
{
	// Movement key triangle
	vec3_t	t_forward, t_right, t_up;

	// Delta between previous point as normal vector movement key triangle
	vec3_t	v_right, v_up;

	float length = VectorNormalize(v_forward);
	if (length > 0.f &&
		(
			movekeys_states[mk_forward] & 1 ||
			movekeys_states[mk_back] & 1 ||
			movekeys_states[mk_moveleft] & 1 ||
			movekeys_states[mk_moveright] & 1 ||
			movekeys_states[mk_jump] & 1

			)) { // should always be true, given that we only record movement

		// Prepare vectors to offset movement triangle from path
		VectorVectorsAlwaysUp(v_forward, v_right, v_up);

		// Triangle height/2
		VectorScale(v_forward, 2.f, t_forward);
		VectorScale(v_right, 2.f, t_right);
		VectorScale(v_up, 2.f, t_up);

		// Offset from path position
		VectorScale(v_forward, 5.f, v_forward);
		VectorScale(v_right, 5.f, v_right);
		VectorScale(v_up, 5.f, v_up);

		if (movekeys_states[mk_jump] & 1) // Draw a small letter j
		{
			vec3_t pos_offset_path;
			vec3_t pos_jump_down_left, pos_jump_center, pos_jump_up;

			// Move up and right
			VectorSubtract(pos_on_path, v_up, pos_offset_path);
			VectorSubtract(pos_offset_path, v_right, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorAdd(pos_offset_path, t_right, pos_jump_down_left);
			VectorAdd(pos_jump_down_left, t_up, pos_jump_down_left);
			VectorSubtract(pos_offset_path, t_up, pos_jump_up);
			VectorCopy(pos_offset_path, pos_jump_center);

			// Draw
			glColor3f(.1f, .1f, 1.f);
			glVertex3fv(pos_jump_down_left);
			glVertex3fv(pos_jump_center);
			glVertex3fv(pos_jump_center);
			glVertex3fv(pos_jump_up);
		}
		vec3_t pos_triangle_first, pos_triangle_third, pos_triangle_second;
		if (movekeys_states[mk_forward] & 1)
		{
			vec3_t pos_offset_path;

			// Move up
			VectorSubtract(pos_on_path, v_up, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorSubtract(pos_offset_path, t_up, pos_triangle_first);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_second);
			VectorAdd(pos_triangle_second, t_up, pos_triangle_second);
			VectorAdd(pos_offset_path, t_right, pos_triangle_third);
			VectorAdd(pos_triangle_third, t_up, pos_triangle_third);

			Pathtracer_Draw_MoveKeys_Triangle(1.f, 1.f, 1.f, pos_triangle_first, pos_triangle_second, pos_triangle_third);
		}
		if (movekeys_states[mk_back] & 1)
		{
			vec3_t pos_offset_path;

			// Move down
			VectorAdd(pos_on_path, v_up, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorAdd(pos_offset_path, t_up, pos_triangle_first);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_second);
			VectorSubtract(pos_triangle_second, t_up, pos_triangle_second);
			VectorAdd(pos_offset_path, t_right, pos_triangle_third);
			VectorSubtract(pos_triangle_third, t_up, pos_triangle_third);

			Pathtracer_Draw_MoveKeys_Triangle(1.f, 1.f, 1.f, pos_triangle_first, pos_triangle_second, pos_triangle_third);
		}
		if (movekeys_states[mk_moveleft] & 1)
		{
			vec3_t pos_offset_path;

			// Move left
			VectorAdd(pos_on_path, v_right, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorSubtract(pos_offset_path, t_right, pos_triangle_first);
			VectorSubtract(pos_triangle_first, t_up, pos_triangle_first);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_second);
			VectorAdd(pos_triangle_second, t_up, pos_triangle_second);
			VectorAdd(pos_offset_path, t_right, pos_triangle_third);

			Pathtracer_Draw_MoveKeys_Triangle(1.f, .1f, .1f, pos_triangle_first, pos_triangle_second, pos_triangle_third);
		}
		if (movekeys_states[mk_moveright] & 1)
		{
			vec3_t pos_offset_path;

			// Move right
			VectorSubtract(pos_on_path, v_right, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorAdd(pos_offset_path, t_right, pos_triangle_first);
			VectorSubtract(pos_triangle_first, t_up, pos_triangle_first);
			VectorAdd(pos_offset_path, t_right, pos_triangle_second);
			VectorAdd(pos_triangle_second, t_up, pos_triangle_second);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_third);

			Pathtracer_Draw_MoveKeys_Triangle(.1f, 1.f, .1f, pos_triangle_first, pos_triangle_second, pos_triangle_third);
		}
	}
}

static void PathTracer_Draw_Level (ghost_level_t* level, qboolean fadeout_enable, float fadeout_seconds, float skip_line_threshold, qboolean show_movekeys)
{
	if (level->num_records <= 1)
		return;

	const vec3_t to_ground = { 0.f, 0.f, -20.f }; // Slightly above ground, so that the down arrow of the movement keys is visible

	// Trace path
	glBegin(GL_LINES);
	for (int i = 1; i < level->num_records; i++) {
		ghostrec_t cur_record = level->records[i];
		ghostrec_t prev_record = level->records[i - 1];

		if (fadeout_enable == false || (cur_record.time > cl.time - fadeout_seconds && cur_record.time < cl.time + fadeout_seconds)) {
			if (i % 2) {
				glColor3f(1.f, 1.f, 1.f);
			}
			else {
				glColor3f(0.f, 0.f, 0.f);
			}
		}
		else {
			glColor3f(.2f, .2f, .2f);
		}
		vec3_t startPos, endPos, delta;
		VectorAdd(prev_record.origin, to_ground, startPos);
		VectorAdd(cur_record.origin, to_ground, endPos);
		VectorSubtract(startPos, endPos, delta);
		if (VectorLength(delta) > skip_line_threshold) {
			continue;
		}

		glVertex3fv(startPos);
		glVertex3fv(endPos);

		if (fadeout_enable == false || (cur_record.time > cl.time - fadeout_seconds && cur_record.time < cl.time + fadeout_seconds)) {
			// Angle vector
			vec3_t a_forward, a_right, a_up;
			vec3_t view_angles = { -cur_record.angle[0],cur_record.angle[1],cur_record.angle[2] }; // I don't know why, but had to flip the first angle
			AngleVectors(view_angles, a_forward, a_right, a_up);
			VectorScale(a_forward, 20.f, a_forward);
			VectorCopy(cur_record.origin, startPos);
			VectorAdd(cur_record.origin, a_forward, endPos);
			VectorAdd(startPos, to_ground, startPos);
			VectorAdd(endPos, to_ground, endPos);
			glColor3f(.2f, .2f, .2f);
			glVertex3fv(startPos);
			glVertex3fv(endPos);
			if (show_movekeys) {
				vec3_t	v_forward;
				GLfloat pos_on_path[3];
				movekeytype_t* movekeys_states = cur_record.movekeys_states;
				VectorAdd(cur_record.origin, to_ground, pos_on_path);
				VectorSubtract(cur_record.origin, prev_record.origin, v_forward);

				PathTracer_Draw_MoveKeys(pos_on_path, v_forward, movekeys_states);
			}
		}
	}
	glEnd();
}

void PathTracer_Draw(void)
{
	if (!sv.active && !cls.demoplayback) return;

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	qboolean gl_line_smooth_was_enabled = glIsEnabled(GL_LINE_SMOOTH);
	if (pathtracer_line_smooth.value == 1.f)
		glEnable(GL_LINE_SMOOTH);

	if (pathtracer_show_player.value == 1.f && player_record_current_level != NULL) {
		PathTracer_Draw_Level(player_record_current_level, false, pathtracer_fadeout_seconds.value, pathtracer_line_skip_threshold.value, (pathtracer_movekeys_player.value == 1.f));
	}

	extern ghost_info_t* demo_info;
	if (pathtracer_show_demo.value == 1.f && demo_info != NULL && demo_current_level != NULL) {
		PathTracer_Draw_Level(demo_current_level, (pathtracer_fadeout_demo.value == 1.f), pathtracer_fadeout_seconds.value, pathtracer_line_skip_threshold.value, (pathtracer_movekeys_demo.value == 1.f));
	}

	extern ghost_level_t* ghost_current_level;
	if (pathtracer_show_ghost.value == 1.f && ghost_current_level != NULL) {
		PathTracer_Draw_Level(ghost_current_level, (pathtracer_fadeout_ghost.value == 1.f), pathtracer_fadeout_seconds.value, pathtracer_line_skip_threshold.value, (pathtracer_movekeys_ghost.value == 1.f));
	}
	
	// Back to normal rendering
	if(pathtracer_line_smooth.value == 1.f && !gl_line_smooth_was_enabled) // line smoothing was enabled, so do not disable it.
		glDisable(GL_LINE_SMOOTH);
	glColor3f(1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void PathTracer_Sample_Each_Frame(void) {

	if (pathtracer_record_player.value != 1.f) return;

	// Not playing locally and not in demo playback, e.g. multiplayer then return
	if (!sv.active && !cls.demoplayback) return;

	// Demo handleded separately in draw method, no recording for that
	if (cls.demoplayback) {
		return;
	}

	// Determine if we should sample
	qboolean track = false;
	if (pathtracer_record_samplerate.value == 0.f) {
		// sample each physics frame
		if (physframe)
			track = true;
		else
			track = false;
	} else {
		// sample each cl_maxfps frame
		if (cl.time > prev_cltime + 1.f / cl_maxfps.value) {
			track = true;
			prev_cltime = cl.time;
		}
		else {
			track = false;
		}
	}

	// Sample movement
	if (track) {
		ghostrec_t cur_record;
		
		extern int show_movekeys_states[NUM_MOVEMENT_KEYS];
		memcpy(cur_record.movekeys_states, show_movekeys_states, sizeof(int[NUM_MOVEMENT_KEYS]));

		cur_record.time = cl.time;
		cur_record.frame = 0;
		cur_record.model = 0;
			
		VectorCopy(cl_entities[cl.viewentity].origin, cur_record.origin);
		VectorCopy(cl_entities[cl.viewentity].angles, cur_record.angle);

		PathTracer_Append (player_record_current_level, &cur_record);
	}
}

// called after Ghost_Load
void PathTracer_Load(void) 
{
	// Remove records from player
	free(player_record_info.levels[0].records);
	player_record_info.levels[0].records = NULL;
	player_record_info.levels[0].num_records = 0;
	prev_cltime = 0.f;

	// Find demo level
	extern ghost_info_t* demo_info;
	if (demo_info == NULL) {
		return;
	}
	char* map_name = CL_MapName();
	if (map_name == NULL || map_name[0] == '\0') {
		return;
	}

	demo_current_level = NULL;

	ghost_level_t* level;
	int i;
	for (i = 0; i < demo_info->num_levels; i++) {
		level = &demo_info->levels[i];
		if (strcmp(map_name, level->map_name) == 0) {
			break;
		}
	}
	if (i >= demo_info->num_levels) {
		demo_current_level = NULL;
		Con_Printf("Map %s not found in demo, searched for index %d\n", map_name, i);
	}
	else {
		demo_current_level = level;
	}
}

void PathTracer_Init (void)
{
	Cvar_Register (&pathtracer_record_player);
	Cvar_Register (&pathtracer_record_samplerate);
	Cvar_Register (&pathtracer_show_player);
	Cvar_Register (&pathtracer_show_ghost);
	Cvar_Register (&pathtracer_show_demo);
	Cvar_Register (&pathtracer_movekeys_player);
	Cvar_Register (&pathtracer_movekeys_ghost);
	Cvar_Register (&pathtracer_movekeys_demo);
	Cvar_Register (&pathtracer_fadeout_seconds);
	Cvar_Register (&pathtracer_fadeout_ghost);
	Cvar_Register (&pathtracer_fadeout_demo);
	Cvar_Register (&pathtracer_line_smooth);
	Cvar_Register (&pathtracer_line_skip_threshold);

	// Initiate player record
	player_record_info.levels[0].map_name[0] = '\0';
	player_record_info.levels[0].num_records = 0;
	player_record_info.levels[0].records = NULL;
	player_record_info.num_levels = 1;
	player_record_current_level = &player_record_info.levels[0];
}


void PathTracer_Shutdown (void)
{
}
