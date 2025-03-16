#include "../quakedef.h"
#include "wishdir.h"

// code from: https://github.com/shalrathy/quakespasm-shalrathy
// Double checked the code, it is the same as in JoeQuake
float Get_Wishdir_Speed_Delta(float angle) {
	extern cvar_t sv_maxspeed;
	extern cvar_t sv_friction;
	extern cvar_t sv_edgefriction;
	extern cvar_t sv_stopspeed;
	extern cvar_t sv_accelerate;
	extern usercmd_t cmd;
	extern vec3_t velocitybeforethink;
	extern qboolean onground;

	int		i;
	vec3_t		wishvel, wishdir, velocity, angles, origin, forward, right, up;
	float		wishspeed, fmove, smove;

	if (angle < -180) angle += 360;
	if (angle > 180) angle -= 360;

	for (i = 0;i < 3;i++) {
		if (sv_player != NULL) {
			angles[i] = sv_player->v.angles[i];
		}
		else {
			angles[i] = 0;
		}
	}
	angles[1] = angle;

	AngleVectors(angles, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

	// hack to not let you back into teleporter
	if (sv_player != NULL && sv.time < sv_player->v.teleport_time && fmove < 0)
		fmove = 0;

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	if (sv_player != NULL && (int)sv_player->v.movetype != MOVETYPE_WALK)
		wishvel[2] = cmd.upmove;
	else
		wishvel[2] = 0;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		VectorScale(wishvel, sv_maxspeed.value / wishspeed, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	for (i = 0;i < 3;i++) velocity[i] = velocitybeforethink[i];
	for (i = 0;i < 3;i++) {
		if (sv_player != NULL) {
			origin[i] = sv_player->v.origin[i];
		}
		else {
			origin[i] = 0;
		}
	}

	float oldspeed = sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]);

	if (sv_player != NULL && sv_player->v.movetype == MOVETYPE_NOCLIP)
	{	// noclip
		//VectorCopy (wishvel, velocity);
	}
	else if (onground)
	{
		while (1) { // SV_UserFriction ();
			float* vel;
			float	speed, newspeed, control;
			vec3_t	start, stop;
			float	friction;
			trace_t	trace;

			vel = velocity;

			speed = sqrt(vel[0] * vel[0] + vel[1] * vel[1]);
			if (!speed)
				break;

			// if the leading edge is over a dropoff, increase friction
			start[0] = stop[0] = origin[0] + vel[0] / speed * 16;
			start[1] = stop[1] = origin[1] + vel[1] / speed * 16;
			start[2] = origin[2] + sv_player->v.mins[2];
			stop[2] = start[2] - 34;

			trace = SV_Move(start, vec3_origin, vec3_origin, stop, true, sv_player);

			if (trace.fraction == 1.0)
				friction = sv_friction.value * sv_edgefriction.value;
			else
				friction = sv_friction.value;

			// apply friction
			control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
			newspeed = speed - host_frametime * control * friction;

			if (newspeed < 0)
				newspeed = 0;
			newspeed /= speed;

			vel[0] = vel[0] * newspeed;
			vel[1] = vel[1] * newspeed;
			vel[2] = vel[2] * newspeed;
			break;
		}

		while (1) { // SV_Accelerate (wishspeed, wishdir);
			int			i;
			float		addspeed, accelspeed, currentspeed;

			currentspeed = DotProduct(velocity, wishdir);
			addspeed = wishspeed - currentspeed;
			if (addspeed <= 0)
				break;
			accelspeed = sv_accelerate.value * host_frametime * wishspeed;
			if (accelspeed > addspeed)
				accelspeed = addspeed;

			for (i = 0; i < 3; i++)
				velocity[i] += accelspeed * wishdir[i];
			break;
		}
	}
	else
	{	// not on ground, so little effect on velocity
		while (1) { // SV_AirAccelerate (wishspeed, wishvel);
			int			i;
			float		addspeed, wishspd, accelspeed, currentspeed;

			wishspd = VectorNormalize(wishvel);
			if (wishspd > 30)
				wishspd = 30;
			currentspeed = DotProduct(velocity, wishvel);
			addspeed = wishspd - currentspeed;
			if (addspeed <= 0)
				break;
			// accelspeed = sv_accelerate.value * host_frametime;
			accelspeed = sv_accelerate.value * wishspeed * host_frametime;
			if (accelspeed > addspeed)
				accelspeed = addspeed;

			for (i = 0; i < 3; i++)
				velocity[i] += accelspeed * wishvel[i];
			break;
		}
	}
	float newspeed = sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]);
	return newspeed - oldspeed;
}
