#ifndef __CL_PATHTRACER_H
#define __CL_PATHTRACER_H

extern cvar_t scr_printbunnyhop;
extern cvar_t scr_recordbunnyhop;

// called from cl_main.c CL_Init() and CL_Shutdown()
void PathTracer_Init(void);
void PathTracer_Shutdown(void);

// called each frame from gl_screen.c SCR_UpdateScreen()
void PathTracer_Sample(void);

// called from gl_rmain.c R_RenderScene()
void PathTracer_Draw(void);

#endif // __CL_PATHTRACER_H

