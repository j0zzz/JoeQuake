#include "../quakedef.h"
#include "pathtracer_private.h"
#include "wishdir.h"

extern cvar_t show_speed_x;
extern cvar_t show_speed_y;

static cvar_t scr_printbunnyhop = { "scr_printbunnyhop", "1" };
static cvar_t scr_recordbunnyhop = { "scr_recordbunnyhop", "1" };

pathtracer_bunnyhop_t pathtracer_bunnyhop_samples[PATHTRACER_BUNNHOP_BUFFER_MAX];
pathtracer_movement_t pathtracer_movement_samples[PATHTRACER_MOVEMENT_BUFFER_MAX];

float drawbestangle = 0.f;

void PathTracer_Draw(void)
{
	if (sv_player == NULL) return;
	if (!sv.active && !cls.demoplayback) return;
	if (cls.demoplayback) return;

	extern pathtracer_bunnyhop_t pathtracer_bunnyhop_samples[PATHTRACER_BUNNHOP_BUFFER_MAX];
	extern pathtracer_movement_t pathtracer_movement_samples[PATHTRACER_MOVEMENT_BUFFER_MAX];
	extern float drawbestangle;

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	glBegin(GL_LINES);
	for (int i = 1;i < PATHTRACER_MOVEMENT_BUFFER_MAX;i++) {
		pathtracer_movement_t* pms_prev = &pathtracer_movement_samples[i - 1];
		pathtracer_movement_t* pms_cur = &pathtracer_movement_samples[i];

		if (pms_prev->pos[0] == 0.f && pms_prev->pos[1] == 0.f && pms_prev->pos[2] == 0.f) {
			continue;
		}
		if (pms_cur->pos[0] == 0.f && pms_cur->pos[1] == 0.f && pms_cur->pos[2] == 0.f) {
			continue;
		}

		if (pms_cur->ongound && pms_prev->ongound) {
			glColor3f(.3f, 0.f, 0.f);
		}
		else if (pms_cur->ongound || pms_prev->ongound) {
			glColor3f(.1f, .1f, .1f);
		}
		else {
			glColor3f(1.f, 1.f, 1.f);
		}

		GLfloat startPos[3];
		startPos[0] = pms_prev->pos[0];
		startPos[1] = pms_prev->pos[1];
		startPos[2] = pms_prev->pos[2];// -20.f;
		glVertex3fv(startPos);

		GLfloat startPos2[3];
		startPos2[0] = pms_cur->pos[0];
		startPos2[1] = pms_cur->pos[1];
		startPos2[2] = pms_cur->pos[2];// -20.f;
		glVertex3fv(startPos2);
	}
	glEnd();

	for (int i = 1;i < PATHTRACER_MOVEMENT_BUFFER_MAX;i++) {
		pathtracer_movement_t* pms_prev = &pathtracer_movement_samples[i - 1];
		pathtracer_movement_t* pms_cur = &pathtracer_movement_samples[i];

		if (pms_prev->pos[0] == 0.f && pms_prev->pos[1] == 0.f && pms_prev->pos[2] == 0.f) {
			continue;
		}
		if (pms_cur->pos[0] == 0.f && pms_cur->pos[1] == 0.f && pms_cur->pos[2] == 0.f) {
			continue;
		}

		vec3_t playerforward, right, up;
		vec3_t playerangles = { 0.f,pms_cur->angle ,0.f };

		AngleVectors(playerangles, playerforward, right, up);

		float playerSpeed = (pms_cur->speed - pms_prev->speed) * 2.f;
		float bestSpeed = 0; // pms_cur->bestspeed * 10.f;

		float oldspeed = sqrtf(pms_prev->velocity[0] * pms_prev->velocity[0] + pms_prev->velocity[1] * pms_prev->velocity[1]);
		float newspeed = sqrtf(pms_cur->velocity[0] * pms_cur->velocity[0] + pms_cur->velocity[1] * pms_cur->velocity[1]);
		float dspeed = newspeed - oldspeed;

		// velocity change, green faster, red slower
		glBegin(GL_LINES);
		if (dspeed > 0)
			glColor3f(0.f, 1.f, 0.f);
		else if (dspeed == 0)
			glColor3f(1.f, 1.f, 1.f);
		else
			glColor3f(1.f, 0.f, 0.f);
		GLfloat startPos[3];
		startPos[0] = pms_cur->pos[0];
		startPos[1] = pms_cur->pos[1];
		startPos[2] = pms_cur->pos[2];// -20.f;
		glVertex3fv(startPos);

		GLfloat anglePos[3];
		anglePos[0] = startPos[0] + (pms_cur->velocity[0] - pms_prev->velocity[0]); //playerforward[0] * playerSpeed;
		anglePos[1] = startPos[1] + (pms_cur->velocity[1] - pms_prev->velocity[1]); //playerforward[1] * playerSpeed;
		anglePos[2] = startPos[2];
		glVertex3fv(anglePos);
		glEnd();

		// speed line in blue
		glBegin(GL_LINES);
		glColor3f(0.f, 0.f, 1.f);
		startPos[0] = pms_prev->pos[0] + up[0] * (oldspeed - 300) / 3.f;
		startPos[1] = pms_prev->pos[1] + up[1] * (oldspeed - 300) / 3.f;
		startPos[2] = 0.f + up[2] * (oldspeed - 300) / 3.f;
		glVertex3fv(startPos);

		anglePos[0] = pms_cur->pos[0] + up[0] * (newspeed - 300) / 3.f;
		anglePos[1] = pms_cur->pos[1] + up[1] * (newspeed - 300) / 3.f;
		anglePos[2] = 0.f + up[2] * (newspeed - 300) / 3.f;
		glVertex3fv(anglePos);
		glEnd();

	}

	glColor3f(1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void PathTracer_Sample(void) {

	if (scr_printbunnyhop.value != 1.f) return;

	// draw speed info for each jump
	if (!sv.active && !cls.demoplayback) return;
	if (!sv.active) return;
	if (cls.demoplayback) return;
	if (sv_player == NULL) return;

	// original code: https://github.com/shalrathy/quakespasm-shalrathy
	// moved this to the bunny hop ring buffer
	const char* fmt = "%c%c %3.0f %c %3.0f = %3.0f %c%3.0f %2.0f%% %6s";
	char str[100];
	int fmtlen = 1 + sprintf(str, fmt, ' ', ' ', 0.0, '+', 0.0, 0.0, '+', 0.0, 0.0, "");
	int rows = (int)10;
	if (rows <= 0) return;
	if (rows > 100) rows = 100;

	float s = fmin(20, fmax(1, 1));
	int w = fmtlen * 8;
	int h = rows * 8;

	extern usercmd_t cmd;

	static qboolean prevonground;
	static qboolean prevprevonground;
	static float prevspeed = 0;
	static float airspeedsum = 0;
	static float airspeedsummax = 0;
	static double prevcltime = -1;
	static int pathtracer_bunnyhop_index = 0;
	static int pathtracer_movement_index = 0;
	static int skipframe = 0;
	static int skipframemod = 10;
	static float currentclosestbunnyhopDistance = INFINITY;
	static int currentclosestbunnyhopIndex = -1;
	extern entity_t ghost_entity;


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
		pathtracer_movement_index = 0;

		memset(pathtracer_bunnyhop_samples, 0, sizeof(pathtracer_bunnyhop_samples));
		pathtracer_bunnyhop_index = 0;

		currentclosestbunnyhopIndex = -1;
		currentclosestbunnyhopDistance = INFINITY;
		drawbestangle = 0.f;

		prevcltime = cl.time;
	}
	else {
		prevcltime = cl.time;
	}

		// Find best wishdir vector (largest speed increase in the plane, ígnores z)
	int angles = 90;
	float bestspeed = 0.f;
	float bestangle = 0;
	bestangle = sv_player->v.angles[1];
	bestspeed = Get_Wishdir_Speed_Delta(bestangle);
	float playerspeed = Get_Wishdir_Speed_Delta(bestangle);
	for (int i = 1; i < angles; i++) {
		for (int neg = -1; neg <= 1; neg += 2) {
			float curangle = 0.0f;
			curangle = sv_player->v.angles[1] + i * neg;
			float curspeed = Get_Wishdir_Speed_Delta(curangle);
			if (curspeed > bestspeed) {
				bestspeed = curspeed;
				bestangle = curangle;
			}
		}
	}
	// 100th of a degree
	for (int i = 1; i < 100; i++) {
		for (int neg = -1; neg <= 1; neg += 2) {
			float curangle = bestangle + neg * i / 100.0f;
			float curspeed = Get_Wishdir_Speed_Delta(curangle);
			if (curspeed > (bestspeed)) {
				bestspeed = curspeed;
				bestangle = curangle;
			}
		}
	}
	if (!onground0) {
		airspeedsum += Get_Wishdir_Speed_Delta(sv_player->v.angles[1]);
		airspeedsummax += bestspeed;
	}

	if (bestangle < -180) bestangle += 360;
	if (bestangle > 180) bestangle -= 360;


	float speed = sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);

	boolean track = false;
	if (pathtracer_movement_index > 0) {
		pathtracer_movement_t* pms_prev = &pathtracer_movement_samples[pathtracer_movement_index - 1];
		float dx = fabs(pms_prev->pos[0] - sv_player->v.origin[0]);
		float dy = fabs(pms_prev->pos[1] - sv_player->v.origin[1]);
		float dz = fabs(pms_prev->pos[2] - sv_player->v.origin[2]);
		if (dx > .1f || dy > .1f || dz > .1f) {
			track = true;
		}
	}
	else {
		track = true;
	}
	// Sample movement
	if (track && scr_recordbunnyhop.value == 1.f) {
		pathtracer_movement_t* pms_new = &pathtracer_movement_samples[pathtracer_movement_index];
		pms_new->pos[0] = sv_player->v.origin[0];
		pms_new->pos[1] = sv_player->v.origin[1];
		pms_new->pos[2] = sv_player->v.origin[2];
		pms_new->velocity[0] = sv_player->v.velocity[0];
		pms_new->velocity[1] = sv_player->v.velocity[1];
		pms_new->velocity[2] = sv_player->v.velocity[2];
		pms_new->angle = sv_player->v.angles[1];

		if (ghost_entity.model != NULL) {
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

		pms_new->bestangle = bestangle;
		pms_new->speed = speed;
		pms_new->bestspeed = bestspeed;
		pms_new->ongound = onground0;
		pathtracer_movement_index++;
		if (pathtracer_movement_index >= PATHTRACER_MOVEMENT_BUFFER_MAX) {
			pathtracer_movement_index = 0;
		}

		// generate bunny hop hint
		if ((onground0 && !prevonground) ||  // landed
			(!onground0 && prevonground && prevprevonground)) { // take-off
			char strextra[10];
			float addspeed;
			float addspeedpct;
			strextra[0] = 0;
			if (sv.active) {
				addspeed = Get_Wishdir_Speed_Delta(sv_player->v.angles[1]);
				addspeedpct = 100.0 * (airspeedsum + addspeed) / (airspeedsummax + bestspeed);
				sprintf(strextra, "%4.0f %1s", fmin(999, fmax(-999, bestangle - sv_player->v.angles[1])), (onground0 && !prevonground) ? "l" : "t");
			}
			else { // demo
				addspeed = 0.0;
				addspeedpct = 0.0;
				airspeedsum = 0.0;
				addspeedpct = 0.0;
			}
			sprintf(pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].str, fmt,
				cmd.sidemove < 0 ? 127 : cmd.sidemove > 0 ? 141 : ' ',
				cmd.forwardmove > 0 ? '^' : cmd.forwardmove < 0 ? 'v' : ' ',
				airspeedsum,
				addspeed >= 0 ? '+' : '-',
				fabs(addspeed),
				fmin(999.0, speed),
				speed - prevspeed >= 0 ? '+' : '-',
				fmin(999.0, fabs(speed - prevspeed)),
				fmin(99, fmax(0, addspeedpct)),
				strextra);

			prevspeed = speed;
			airspeedsum = 0;
			airspeedsummax = 0;

			if (onground0 && !prevonground && scr_recordbunnyhop.value == 1.f) {
				drawbestangle = bestangle;
				pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].playerangle = sv_player->v.angles[1];
				pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].bestangle = bestangle;
				pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].pos[0] = sv_player->v.origin[0];
				pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].pos[1] = sv_player->v.origin[1];
				pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].pos[2] = sv_player->v.origin[2];
				if (onground0 && !prevonground) {
					pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].contact = just_landed;
				}
				else {
					pathtracer_bunnyhop_samples[pathtracer_bunnyhop_index].contact = just_jumped;
				}
				pathtracer_bunnyhop_index++;
				if (pathtracer_bunnyhop_index >= PATHTRACER_BUNNHOP_BUFFER_MAX)
					pathtracer_bunnyhop_index = 0;
			}
		}
		if (speed < 1) prevspeed = 0;

		prevprevonground = prevonground;
		prevonground = onground0;
	}

	// Find closest bunny hop hint
	currentclosestbunnyhopDistance = 1000000000;
	currentclosestbunnyhopIndex = -1;
	for (int i = 0;i < PATHTRACER_BUNNHOP_BUFFER_MAX;i++) {
		pathtracer_bunnyhop_t* pbs = &pathtracer_bunnyhop_samples[i];
		pbs->selected = false;
		if (pbs->pos[0] == 0.f && pbs->pos[1] == 0.f && pbs->pos[2] == 0.f) {
			break;
		}
		GLfloat startPos[3];
		startPos[0] = pbs->pos[0];
		startPos[1] = pbs->pos[1];
		startPos[2] = pbs->pos[2] - 20.f;

		int dx = sv_player->v.origin[0] - startPos[0];
		int dy = sv_player->v.origin[1] - startPos[1];
		int dz = sv_player->v.origin[2] - startPos[2];
		float distance = dx * dx + dy * dy + dz * dz;
		if (distance < currentclosestbunnyhopDistance) {
			currentclosestbunnyhopDistance = distance;
			currentclosestbunnyhopIndex = i;
		}
	}

	// Draw closest bunny hop hint
	if (currentclosestbunnyhopIndex >= 0 && currentclosestbunnyhopDistance < 20000.f) {
		pathtracer_bunnyhop_samples[currentclosestbunnyhopIndex].selected = true;

		int scale = Sbar_GetScaleAmount();
		int size = Sbar_GetScaledCharacterSize();
		int x = ((vid.width - (int)(160 * scale)) / 2) + (show_speed_x.value * size);
		int y = ELEMENT_Y_COORD(show_speed) + 10.f * scale;

		Draw_String(x, y, pathtracer_bunnyhop_samples[currentclosestbunnyhopIndex].str, true);
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
