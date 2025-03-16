#ifndef __PATHTRACER_PRIVATE
#define __PATHTRACER_PRIVATE

#ifndef QUAKE_GAME
#include "../quakedef.h"
#endif

typedef struct {
	float bestangle;
	float playerangle;
	float pos[3];
	char str[100];
	contacttype_t contact;
	qboolean selected;
} pathtracer_bunnyhop_t;

typedef struct {
	float pos[3];
	float velocity[3];
	float angle;
	float speed;
	float bestangle;
	float bestspeed;
	qboolean ongound;
} pathtracer_movement_t;

extern pathtracer_movement_t pathtracer_movement_samples[100000];
extern pathtracer_bunnyhop_t pathtracer_bunnyhop_samples[100];
extern float drawbestangle;

#endif // __PATHTRACER_PRIVATE