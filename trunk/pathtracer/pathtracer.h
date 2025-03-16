#ifndef __CL_PATHTRACER_H
#define __CL_PATHTRACER_H

extern cvar_t scr_printbunnyhop;
extern cvar_t scr_recordbunnyhop;

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

void PathTracer_Init(void);
void PathTracer_Shutdown(void);
void R_DrawPathTracer(void);

#endif // __CL_PATHTRACER_H

