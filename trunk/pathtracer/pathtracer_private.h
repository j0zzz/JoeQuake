#ifndef __PATHTRACER_PRIVATE
#define __PATHTRACER_PRIVATE

#ifndef QUAKE_GAME
#include "../quakedef.h"
#endif

typedef struct {
	qboolean holdsData;
	float pos[3];
	float velocity[3];
	float angle;
	float speed;
	qboolean onground;
	movekeytype_t movekeys[NUM_MOVEMENT_KEYS];
} pathtracer_movement_t;

#define PATHTRACER_BUNNHOP_BUFFER_MAX 100
#define PATHTRACER_MOVEMENT_BUFFER_MAX 10000
#define PATHTRACER_MOVEMENT_BUFFER_FADEOUT 500

extern pathtracer_movement_t pathtracer_movement_samples[PATHTRACER_MOVEMENT_BUFFER_MAX];

#endif // __PATHTRACER_PRIVATE