#include "../quakedef.h"
#include "pathtracer_private.h"
#include "wishdir.h"

extern ghost_info_t* demo_info;

static cvar_t pathtracer_show_player = { "pathtracer_show_player", "1" };
static cvar_t pathtracer_show_demo = { "pathtracer_show_demo", "1" };
static cvar_t pathtracer_record_player = { "pathtracer_record_player", "1" };
static cvar_t pathtracer_fadeout_ghost = { "pathtracer_fadeout_ghost", "0" };
static cvar_t pathtracer_fadeout_demo = { "pathtracer_fadeout_demo", "1" };
static cvar_t pathtracer_fadeout_seconds = { "pathtracer_fadeout_seconds", "3" };
static ghost_level_t* demo_current_level = NULL;

pathtracer_movement_t pathtracer_movement_samples[PATHTRACER_MOVEMENT_BUFFER_MAX];
int pathtracer_movement_write_head = 0;
int pathtracer_movement_read_head = 0;

static double prev_cltime = -1;

// replace VectorVectors, but now with up always pointing up
void VectorVectorsAlwaysUp(vec3_t forward, vec3_t right, vec3_t up)
{
	VectorSet(right, -forward[1], forward[0], 0);
	VectorNormalizeFast(right);
	CrossProduct(right, forward, up);
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

		if (movekeys_states[mk_forward] & 1)
		{
			vec3_t pos_offset_path;
			vec3_t pos_triangle_up, pos_triangle_left, pos_triangle_right;

			// Move up
			VectorSubtract(pos_on_path, v_up, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorSubtract(pos_offset_path, t_up, pos_triangle_up);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_right);
			VectorAdd(pos_triangle_right, t_up, pos_triangle_right);
			VectorAdd(pos_offset_path, t_right, pos_triangle_left);
			VectorAdd(pos_triangle_left, t_up, pos_triangle_left);

			// Draw
			glBegin(GL_LINES);
			glLineWidth(2.0f);
			glColor3f(1.f, 1.f, 1.f);
			glVertex3fv(pos_triangle_up);
			glVertex3fv(pos_triangle_right);
			glVertex3fv(pos_triangle_right);
			glVertex3fv(pos_triangle_left);
			glVertex3fv(pos_triangle_left);
			glVertex3fv(pos_triangle_up);
			glEnd();
		}
		if (movekeys_states[mk_back] & 1)
		{
			vec3_t pos_offset_path;
			vec3_t pos_triangle_down, pos_triangle_left, pos_triangle_right;

			// Move down
			VectorAdd(pos_on_path, v_up, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorAdd(pos_offset_path, t_up, pos_triangle_down);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_right);
			VectorSubtract(pos_triangle_right, t_up, pos_triangle_right);
			VectorAdd(pos_offset_path, t_right, pos_triangle_left);
			VectorSubtract(pos_triangle_left, t_up, pos_triangle_left);

			// Draw
			glBegin(GL_LINES);
			glLineWidth(2.0f);
			glColor3f(1.f, 1.f, 1.f);
			glVertex3fv(pos_triangle_down);
			glVertex3fv(pos_triangle_right);
			glVertex3fv(pos_triangle_right);
			glVertex3fv(pos_triangle_left);
			glVertex3fv(pos_triangle_left);
			glVertex3fv(pos_triangle_down);
			glEnd();
		}
		if (movekeys_states[mk_moveleft] & 1)
		{
			vec3_t pos_offset_path;
			vec3_t pos_triangle_up, pos_triangle_left, pos_triangle_down;

			// Move left
			VectorAdd(pos_on_path, v_right, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorSubtract(pos_offset_path, t_right, pos_triangle_up);
			VectorSubtract(pos_triangle_up, t_up, pos_triangle_up);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_down);
			VectorAdd(pos_triangle_down, t_up, pos_triangle_down);
			VectorAdd(pos_offset_path, t_right, pos_triangle_left);

			// Draw
			glBegin(GL_LINES);
			glLineWidth(2.0f);
			glColor3f(1.f, .1f, .1f);
			glVertex3fv(pos_triangle_up);
			glVertex3fv(pos_triangle_down);
			glVertex3fv(pos_triangle_down);
			glVertex3fv(pos_triangle_left);
			glVertex3fv(pos_triangle_left);
			glVertex3fv(pos_triangle_up);
			glEnd();
		}
		if (movekeys_states[mk_moveright] & 1)
		{
			vec3_t pos_offset_path;
			vec3_t pos_triangle_up, pos_triangle_right, pos_triangle_down;

			// Move right
			VectorSubtract(pos_on_path, v_right, pos_offset_path);

			// Calculate edges of the movement triangle
			VectorAdd(pos_offset_path, t_right, pos_triangle_up);
			VectorSubtract(pos_triangle_up, t_up, pos_triangle_up);
			VectorAdd(pos_offset_path, t_right, pos_triangle_down);
			VectorAdd(pos_triangle_down, t_up, pos_triangle_down);
			VectorSubtract(pos_offset_path, t_right, pos_triangle_right);

			// Draw
			glBegin(GL_LINES);
			glLineWidth(2.0f);
			glColor3f(.1f, 1.f, .1f);
			glVertex3fv(pos_triangle_up);
			glVertex3fv(pos_triangle_down);
			glVertex3fv(pos_triangle_down);
			glVertex3fv(pos_triangle_right);
			glVertex3fv(pos_triangle_right);
			glVertex3fv(pos_triangle_up);
			glEnd();
		}
		if (movekeys_states[mk_jump] & 1)
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
			glBegin(GL_LINES);
			glLineWidth(2.0f);
			glColor3f(.1f, .1f, 1.f);
			glVertex3fv(pos_jump_down_left);
			glVertex3fv(pos_jump_center);
			glVertex3fv(pos_jump_center);
			glVertex3fv(pos_jump_up);
			glEnd();
		}
	}
}

void PathTracer_Draw(void)
{
	if (sv_player == NULL) return;
	if (!sv.active && !cls.demoplayback) return;

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	extern ghost_level_t* ghost_current_level;
	boolean draw_ghost = true;
	boolean draw_demo = false;

	ghost_level_t* level = ghost_current_level;
	if (demo_info != NULL && demo_current_level != NULL) {
		level = demo_current_level;
		draw_ghost = false;
		draw_demo = true;
	}

	boolean pathtracer_fadeout_enable = (draw_ghost && pathtracer_fadeout_ghost.value == 1.f) || (draw_demo && pathtracer_fadeout_demo.value == 1.f);

	const vec3_t to_ground = { 0.f, 0.f, -cl.viewheight };
	if (pathtracer_show_demo.value > 0.f &&
		level != NULL &&
		level->num_records > 1) { // we have client data

		// Trace path
		glBegin(GL_LINES);
		for (int i = 1; i < level->num_records; i++) {
			ghostrec_t cur_record = level->records[i];
			ghostrec_t prev_record = level->records[i - 1];

			if (pathtracer_fadeout_enable == false || (cur_record.time > cl.time - pathtracer_fadeout_seconds.value && cur_record.time < cl.time + pathtracer_fadeout_seconds.value)) {
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
			if (VectorLength(delta) > 50.f) {
				continue;
			}

			glVertex3fv(startPos);
			glVertex3fv(endPos);

			if (pathtracer_fadeout_enable == false || (cur_record.time > cl.time - pathtracer_fadeout_seconds.value && cur_record.time < cl.time + pathtracer_fadeout_seconds.value)) {
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
			}
		}
		glEnd();

		// Draw movement keys
		for (int i = 1; i < level->num_records; i++) {
			ghostrec_t cur_record = level->records[i];
			ghostrec_t prev_record = level->records[i - 1];

			if (pathtracer_fadeout_enable == false || (cur_record.time > cl.time - pathtracer_fadeout_seconds.value && cur_record.time < cl.time + pathtracer_fadeout_seconds.value)) {
				vec3_t	v_forward;
				GLfloat pos_on_path[3];
				movekeytype_t* movekeys_states = cur_record.ghost_movekeys_states;
				VectorAdd(cur_record.origin, to_ground, pos_on_path);
				VectorSubtract(cur_record.origin, prev_record.origin, v_forward);

				PathTracer_Draw_MoveKeys(pos_on_path, v_forward, movekeys_states);
			}
		}
	}

	if(pathtracer_show_player.value > 0.f) {
		// Trace player path
		glBegin(GL_LINES);
		for (int buffer_index = 0;buffer_index < PATHTRACER_MOVEMENT_BUFFER_MAX; buffer_index++) {
			int i = (pathtracer_movement_read_head + buffer_index) % PATHTRACER_MOVEMENT_BUFFER_MAX;
			pathtracer_movement_t* pms_prev = &pathtracer_movement_samples[i++];
			if (i >= PATHTRACER_MOVEMENT_BUFFER_MAX)
				i = 0;
			pathtracer_movement_t* pms_cur = &pathtracer_movement_samples[i];

			// Reached write head?
			if (i == pathtracer_movement_write_head)
				continue;

			// No data in yet?
			if (!(pms_prev->holdsData && pms_cur->holdsData))
				continue;

			if (i % 2) {
				glColor3f(1.f, 1.f, 1.f);
			}
			else {
				glColor3f(0.f, 0.f, 0.f);
			}

			vec3_t startPos, endPos, delta;
			VectorAdd(pms_prev->pos, to_ground, startPos);
			VectorAdd(pms_cur->pos, to_ground, endPos);
			VectorSubtract(startPos, endPos, delta);
			if (VectorLength(delta) > 50.f) {
				continue;
			}

			glVertex3fv(startPos);
			glVertex3fv(endPos);

			// Angle vector
			vec3_t a_forward, a_right, a_up;
			vec3_t view_angles = { -pms_cur->angles[0],pms_cur->angles[1],pms_cur->angles[2] };
			AngleVectors(view_angles, a_forward, a_right, a_up);
			VectorScale(a_forward, 20.f, a_forward);
			VectorCopy(pms_cur->pos, startPos);
			VectorAdd(pms_cur->pos, a_forward, endPos);
			VectorAdd(startPos, to_ground, startPos);
			VectorAdd(endPos, to_ground, endPos);
			glColor3f(.2f, .2f, .2f);
			glVertex3fv(startPos);
			glVertex3fv(endPos);

		}
		glEnd();

		// Draw movement keys
		for (int buffer_index = 0;buffer_index < PATHTRACER_MOVEMENT_BUFFER_MAX; buffer_index++) {
			int i = (pathtracer_movement_read_head + buffer_index) % PATHTRACER_MOVEMENT_BUFFER_MAX;
			pathtracer_movement_t* pms_prev = &pathtracer_movement_samples[i++];
			if (i >= PATHTRACER_MOVEMENT_BUFFER_MAX)
				i = 0;
			pathtracer_movement_t* pms_cur = &pathtracer_movement_samples[i];

			// Reached write head?
			if (i == pathtracer_movement_write_head)
				continue;

			// No data in yet?
			if (!(pms_prev->holdsData && pms_cur->holdsData))
				continue;

			vec3_t pos_on_path;
			vec3_t v_forward;

			VectorAdd(pms_cur->pos, to_ground, pos_on_path);
			VectorSubtract(pms_cur->pos, pms_prev->pos, v_forward);
			movekeytype_t* movekeys_states = pms_cur->movekeys;
			PathTracer_Draw_MoveKeys(pos_on_path, v_forward, movekeys_states);
		}
	}
	// Back to normal rendering
	glColor3f(1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void PathTracer_Sample_Each_Frame(void) {

	if (pathtracer_show_player.value != 1.f) return;

	// Not playing locally and not in demo playback, e.g. multiplayer then return
	if (!sv.active && !cls.demoplayback) return;

	// Demo handleded separately in draw method
	if (cls.demoplayback) {
		return;
	}

	boolean enable_recording = pathtracer_record_player.value == 1.f;

	// Restart tracer
	if (cl.time < prev_cltime) {
		memset(pathtracer_movement_samples, 0, sizeof(pathtracer_movement_samples));
		pathtracer_movement_write_head = 0;
		prev_cltime = cl.time;
	}

	float speed = VectorLength(cl.velocity);

	// Determine if we should sample
	boolean track = false;
	pathtracer_movement_t* pms_prev;
	if (pathtracer_movement_write_head > 0)
		pms_prev = &pathtracer_movement_samples[pathtracer_movement_write_head - 1];
	else
		// just in case we hit that zero
		pms_prev = &pathtracer_movement_samples[PATHTRACER_BUNNHOP_BUFFER_MAX - 1];
		
	if(pms_prev->holdsData) {
		vec3_t traveled_vector;
		VectorSubtract(pms_prev->pos, cl_entities[cl.viewentity].origin, traveled_vector);
		float traveled_distance = VectorLength(traveled_vector);
		if (traveled_distance > .1f) {
			// That's practically every frame
			track = true;
		}
		else {
			track = false;
		}
	}
	else {
		track = true;
	}
	if (cl.time > prev_cltime + 1.f / cl_maxfps.value) {
		track = true;
		prev_cltime = cl.time;
	}
	else {
		track = false;
	}

	// Sample movement
	if (track && enable_recording) {
		pathtracer_movement_t* pms_new = &pathtracer_movement_samples[pathtracer_movement_write_head];
		// Array element has data, ring buffer wrapped around
		if (pms_new->holdsData) {
			pathtracer_movement_read_head = pathtracer_movement_write_head + 1;
			if (pathtracer_movement_read_head >= PATHTRACER_MOVEMENT_BUFFER_MAX) {
				pathtracer_movement_read_head = 0;
			}
		}
		pms_new->holdsData = true;

		extern int show_movekeys_states[NUM_MOVEMENT_KEYS];
		memcpy(pms_new->movekeys, show_movekeys_states, sizeof(int[NUM_MOVEMENT_KEYS]));
			
		VectorCopy(cl_entities[cl.viewentity].origin, pms_new->pos);
		VectorSubtract(cl_entities[cl.viewentity].origin, cl_entities[cl.viewentity].previousorigin, pms_new->velocity);
		VectorCopy(cl_entities[cl.viewentity].angles, pms_new->angles);

		pms_new->speed = speed;
		pms_new->onground = cl.onground;
		pathtracer_movement_write_head++;
		if (pathtracer_movement_write_head >= PATHTRACER_MOVEMENT_BUFFER_MAX) {
			pathtracer_movement_write_head = 0;
		}
	}
}

// called after Ghost_Load
void PathTracer_Load(void) 
{
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
    Cvar_Register (&pathtracer_show_player);
	Cvar_Register (&pathtracer_record_player);
	Cvar_Register (&pathtracer_fadeout_seconds);
	Cvar_Register (&pathtracer_show_demo);
	Cvar_Register (&pathtracer_fadeout_ghost);
	Cvar_Register (&pathtracer_fadeout_demo);
}


void PathTracer_Shutdown (void)
{
}
