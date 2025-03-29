#include "../quakedef.h"
#include "pathtracer_private.h"
#include "wishdir.h"

extern cvar_t show_speed_x;
extern cvar_t show_speed_y;

static cvar_t scr_printbunnyhop = { "scr_printbunnyhop", "1" };
static cvar_t scr_recordbunnyhop = { "scr_recordbunnyhop", "1" };

pathtracer_movement_t pathtracer_movement_samples[PATHTRACER_MOVEMENT_BUFFER_MAX];
int pathtracer_movement_write_head = 0;
int pathtracer_movement_read_head = 0;

void PathTracer_Draw(void)
{
	if (sv_player == NULL) return;
	if (!sv.active && !cls.demoplayback) return;
	if (cls.demoplayback) return;

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	// Trace path
	glBegin(GL_LINES);
	int fadeCounter = 0;
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

		// If we are in the range of 200 samples before the write head
		qboolean isFadingOut = (pathtracer_movement_write_head == pathtracer_movement_read_head) ? fadeCounter < PATHTRACER_MOVEMENT_BUFFER_MAX - PATHTRACER_MOVEMENT_BUFFER_FADEOUT : fadeCounter < pathtracer_movement_write_head - pathtracer_movement_read_head - PATHTRACER_MOVEMENT_BUFFER_FADEOUT;
		fadeCounter++;

		// then grey
		if (isFadingOut) {
			glColor3f(.1f, .1f, .1f);
		}
		else
		if (pms_cur->onground && pms_prev->onground) {
			glColor3f(.3f, 0.f, 0.f);
		}
		else if (pms_cur->onground || pms_prev->onground) {
			glColor3f(.1f, .1f, .1f);
		}
		else {
			glColor3f(1.f, 1.f, 1.f);
		}

		GLfloat startPos[3];
		startPos[0] = pms_prev->pos[0];
		startPos[1] = pms_prev->pos[1];
		startPos[2] = pms_prev->pos[2] - 20.f; // on the ground
		glVertex3fv(startPos);

		GLfloat endPos[3];
		endPos[0] = pms_cur->pos[0];
		endPos[1] = pms_cur->pos[1];
		endPos[2] = pms_cur->pos[2] - 20.f; // on the ground
		glVertex3fv(endPos);
	}
	glEnd();

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

		GLfloat pos_on_path[3];
		pos_on_path[0] = pms_cur->pos[0];
		pos_on_path[1] = pms_cur->pos[1];
		pos_on_path[2] = pms_cur->pos[2] - 20.f; // on the ground

		// Movement key triangle
		vec3_t	t_forward, t_right, t_up;

		// Velocity as normal vector movement key triangle
		vec3_t	v_forward, v_right, v_up;
		VectorCopy(pms_cur->velocity, v_forward);

		float length = VectorNormalize(v_forward);
		if (length > 0.f) {

			// Prepare vectors to offset movement triangle from path
			VectorVectors(v_forward, v_right, v_up);
			
			// Triangle height/2
			VectorScale(v_forward, 2.f, t_forward);
			VectorScale(v_right, 2.f, t_right);
			VectorScale(v_up, 2.f, t_up);
			
			// Offset from path position
			VectorScale(v_forward, 5.f, v_forward);
			VectorScale(v_right, 5.f, v_right);
			VectorScale(v_up, 5.f, v_up);

			if (pms_cur->movekeys[mk_forward] & 1)
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
			if (pms_cur->movekeys[mk_back] & 1)
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
			if (pms_cur->movekeys[mk_moveleft] & 1)
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
				glColor3f(1.f, 1.f, 1.f);
				glVertex3fv(pos_triangle_up);
				glVertex3fv(pos_triangle_down);
				glVertex3fv(pos_triangle_down);
				glVertex3fv(pos_triangle_left);
				glVertex3fv(pos_triangle_left);
				glVertex3fv(pos_triangle_up);
				glEnd();
			}
			if (pms_cur->movekeys[mk_moveright] & 1)
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
				glColor3f(1.f, 1.f, 1.f);
				glVertex3fv(pos_triangle_up);
				glVertex3fv(pos_triangle_down);
				glVertex3fv(pos_triangle_down);
				glVertex3fv(pos_triangle_right);
				glVertex3fv(pos_triangle_right);
				glVertex3fv(pos_triangle_up);
				glEnd();
			}
			if (pms_cur->movekeys[mk_jump] & 1)
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


	// Back to normal rendering
	glColor3f(1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void PathTracer_Sample_Each_Frame(void) {

	if (scr_printbunnyhop.value != 1.f) return;
	if (!sv.active && !cls.demoplayback) return;
	if (!sv.active) return;
	if (cls.demoplayback) return;
	if (sv_player == NULL) return;

	extern usercmd_t cmd;
	extern entity_t ghost_entity;

	static double prevcltime = -1;

	// demo playback only has cl.onground which is different from onground in normal play
	qboolean onground0;
	if (sv.active) {
		extern qboolean onground;
		onground0 = onground;
	}
	else {
		onground0 = cl.onground;
	}

	// Restart tracer
	if (cl.time < prevcltime) {
		memset(pathtracer_movement_samples, 0, sizeof(pathtracer_movement_samples));
		pathtracer_movement_write_head = 0;

		prevcltime = cl.time;
	}
	else {
		prevcltime = cl.time;
	}

	float speed = sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);

	// Determine if we should sample
	boolean track = false;
	vec3_t *origin = &sv_player->v.origin;
	if (ghost_entity.model != NULL)
		origin = &ghost_entity.origin;
			
	pathtracer_movement_t* pms_prev;
	if (pathtracer_movement_write_head > 0)
		pms_prev = &pathtracer_movement_samples[pathtracer_movement_write_head - 1];
	else
		// just in case we hit that zero
		pms_prev = &pathtracer_movement_samples[PATHTRACER_BUNNHOP_BUFFER_MAX - 1];
		
	if(pms_prev->holdsData) {
		float dx = fabs(pms_prev->pos[0] - *origin[0]);
		float dy = fabs(pms_prev->pos[1] - *origin[1]);
		float dz = fabs(pms_prev->pos[2] - *origin[2]);
		if (dx > .1f || dy > .1f || dz > .1f) {
			track = true;
		}
	}
	else {
		track = true;
	}

	// Sample movement
	if (track && scr_recordbunnyhop.value == 1.f) {
		pathtracer_movement_t* pms_new = &pathtracer_movement_samples[pathtracer_movement_write_head];
		// Array element has data, ring buffer wrapped around
		if (pms_new->holdsData) {
			pathtracer_movement_read_head = pathtracer_movement_write_head + 1;
			if (pathtracer_movement_read_head >= PATHTRACER_MOVEMENT_BUFFER_MAX) {
				pathtracer_movement_read_head = 0;
			}
		}
		pms_new->holdsData = true;

		pms_new->pos[0] = sv_player->v.origin[0];
		pms_new->pos[1] = sv_player->v.origin[1];
		pms_new->pos[2] = sv_player->v.origin[2];
		pms_new->velocity[0] = sv_player->v.velocity[0];
		pms_new->velocity[1] = sv_player->v.velocity[1];
		pms_new->velocity[2] = sv_player->v.velocity[2];
		pms_new->angle = sv_player->v.angles[1];

		if (ghost_entity.model != NULL) {
			extern int ghost_movekeys_states[NUM_MOVEMENT_KEYS];
			memcpy(pms_new->movekeys, ghost_movekeys_states, sizeof(int[NUM_MOVEMENT_KEYS]));
			pms_new->pos[0] = ghost_entity.origin[0];
			pms_new->pos[1] = ghost_entity.origin[1];
			pms_new->pos[2] = ghost_entity.origin[2];
			pms_new->velocity[0] = ghost_entity.origin[0] - ghost_entity.previousorigin[0];
			pms_new->velocity[1] = ghost_entity.origin[1] - ghost_entity.previousorigin[1];
			pms_new->velocity[2] = ghost_entity.origin[2] - ghost_entity.previousorigin[2];
			pms_new->velocity[0] *= 200.f;
			pms_new->velocity[1] *= 200.f;
			pms_new->velocity[2] *= 200.f;
			pms_new->angle = ghost_entity.angles[1];
		}

		pms_new->speed = speed;
		pms_new->onground = onground0;
		pathtracer_movement_write_head++;
		if (pathtracer_movement_write_head >= PATHTRACER_MOVEMENT_BUFFER_MAX) {
			pathtracer_movement_write_head = 0;
		}
	}
}

static void PathTracer_Debug_f (void)
{
    if (cmd_source != src_command) {
        return;
    }

	Con_Printf("pathtracker_debug : Just a test\n");
}

void PathTracer_Init (void)
{
    Cmd_AddCommand ("pathtracer_debug", PathTracer_Debug_f);

    Cvar_Register (&scr_printbunnyhop);
    Cvar_Register (&scr_recordbunnyhop);
}


void PathTracer_Shutdown (void)
{
}
