/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// glquake.h

// disable data conversion warnings

#ifdef _WIN32
#pragma warning (disable : 4244)     // MIPS
#pragma warning (disable : 4136)     // X86
#pragma warning (disable : 4051)     // ALPHA

#include <windows.h>
#endif

// FAKEGL - switch include files
#ifndef USEFAKEGL
#include <GL/gl.h>
#else
#include "fakegl.h"
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

extern	int	texture_extension_number;
extern	float	gldepthmin, gldepthmax;
extern	byte	color_white[4], color_black[4];

#define	TEX_COMPLAIN		1
#define TEX_MIPMAP			2
#define TEX_ALPHA			4
#define TEX_LUMA			8
#define TEX_FULLBRIGHT		16
#define	TEX_BRIGHTEN		32

#define	MAX_GLTEXTURES	2048

void GL_SelectTexture (GLenum target);
void GL_DisableMultitexture (void);
void GL_EnableMultitexture (void);
void GL_EnableTMU (GLenum target);
void GL_DisableTMU (GLenum target);

void GL_Upload32 (unsigned *data, int width, int height, int mode);
void GL_Upload8 (byte *data, int width, int height, int mode);
int GL_LoadTexture (char *identifier, int width, int height, byte *data, int mode, int bytesperpixel);
byte *GL_LoadImagePixels (char* filename, int matchwidth, int matchheight, int mode);
int GL_LoadTextureImage (char *filename, char *identifier, int matchwidth, int matchheight, int mode);
mpic_t *GL_LoadPicImage (char *filename, char *id, int matchwidth, int matchheight, int mode);
int GL_LoadCharsetImage (char *filename, char *identifier);

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern glvert_t	glv;

extern	int	glx, gly, glwidth, glheight;

// normalizing factor so player model works out to about 1 pixel per triangle
#define ALIAS_BASE_SIZE_RATIO	(1.0 / 11.0)

#define	MAX_LBM_HEIGHT		480

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01

void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base);

#define ISTRANSPARENT(ent)	((ent)->istransparent && (ent)->transparency > 0 && (ent)->transparency < 1)

//====================================================

void QMB_InitParticles (void);
void QMB_ClearParticles (void);
void QMB_DrawParticles (void);

void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void QMB_RocketTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, vec3_t oldorigin, trail_type_t type);
void QMB_BlobExplosion (vec3_t org);
void QMB_ParticleExplosion (vec3_t org);
void QMB_LavaSplash (vec3_t org);
void QMB_TeleportSplash (vec3_t org);
void QMB_StaticBubble (entity_t *ent);
void QMB_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength);
void QMB_TorchFlame (vec3_t org, float size, float time);
void QMB_Q3TorchFlame (vec3_t org, float size);
void QMB_MissileFire (vec3_t org);
void QMB_ShamblerCharge (vec3_t org);
void QMB_LightningBeam (vec3_t start, vec3_t end);
void QMB_GenSparks (vec3_t org, byte col[3], float count, float size, float life);
void QMB_MuzzleFlash (vec3_t org, qboolean weapon);

extern	qboolean	qmb_initialized;

void R_GetParticleMode (void);

void R_GetDecalsState (void);

//====================================================

extern	entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int		r_visframecount;	// ??? what difs?
extern	int		r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys, c_md3_polys;

// view origin
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

// screen size info
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int	currenttexture;
extern	int	particletexture;
extern	int	playertextures;
extern	int	skyboxtextures;
extern	int	underwatertexture, detailtexture;
extern	int	damagetexture;
extern	int	gl_max_size_default;

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_fullbrightskins;
extern	cvar_t	r_fastsky;
extern	cvar_t	r_skycolor;
extern	cvar_t	r_drawflame;
extern	cvar_t	r_skybox;
extern	cvar_t	r_farclip;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_ztrick;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_loadlitfiles;
extern	cvar_t	gl_doubleeyes;
extern	cvar_t	gl_interdist;
extern  cvar_t  gl_waterfog;		
extern  cvar_t  gl_waterfog_density;
extern  cvar_t  gl_detail;
extern  cvar_t  gl_caustics;
extern	cvar_t	gl_fb_bmodels;
extern	cvar_t	gl_fb_models;
extern  cvar_t  gl_solidparticles;
extern  cvar_t  gl_vertexlights;
extern  cvar_t  gl_loadq3models;
extern  cvar_t  gl_lerptextures;
extern	cvar_t	gl_conalpha;
extern	cvar_t	gl_ringalpha;
extern	cvar_t	gl_consolefont;
extern	cvar_t	gl_smoothfont;

extern  cvar_t	gl_part_explosions;
extern  cvar_t	gl_part_trails;
extern  cvar_t	gl_part_spikes;
extern  cvar_t	gl_part_gunshots;
extern  cvar_t	gl_part_blood;
extern  cvar_t	gl_part_telesplash;
extern  cvar_t	gl_part_blobs;
extern  cvar_t	gl_part_lavasplash;
extern	cvar_t	gl_part_flames;
extern	cvar_t	gl_part_lightning;
extern	cvar_t	gl_part_damagesplash;
extern	cvar_t	gl_part_muzzleflash;

extern	cvar_t	gl_bounceparticles;

extern	cvar_t	gl_decaltime;
extern	cvar_t	gl_decal_viewdistance;
extern	cvar_t	gl_decal_blood;
extern	cvar_t	gl_decal_bullets;
extern	cvar_t	gl_decal_sparks;
extern	cvar_t	gl_decal_explosions;

extern	cvar_t	gl_externaltextures_world;
extern	cvar_t	gl_externaltextures_bmodels;
extern	cvar_t	gl_externaltextures_models;

extern qboolean draw_no24bit;

extern	int	lightmode;

extern	int	gl_lightmap_format;
extern	int	gl_solid_format;
extern	int	gl_alpha_format;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;
extern	cvar_t	gl_picmip;

extern	int		mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;

extern	float		r_world_matrix[16];

extern	const	char	*gl_vendor;
extern	const	char	*gl_renderer;
extern	const	char	*gl_version;
extern	const	char	*gl_extensions;

void GL_Bind (int texnum);

// Multitexture
#define	GL_TEXTURE0_ARB 		0x84C0
#define	GL_TEXTURE1_ARB 		0x84C1
#define	GL_TEXTURE2_ARB 		0x84C2
#define	GL_TEXTURE3_ARB 		0x84C3
#define GL_MAX_TEXTURE_UNITS_ARB 0x84E2

typedef void (APIENTRY *lpMTexFUNC)(GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC)(GLenum);

extern lpMTexFUNC qglMultiTexCoord2f;
extern lpSelTexFUNC qglActiveTexture;

extern qboolean gl_mtexable;
extern int gl_textureunits;

void GL_DisableMultitexture (void);
void GL_EnableMultitexture (void);

// vid_common_gl.c
void Check_Gamma (unsigned char *pal);
void GL_Init (void);
qboolean CheckExtension (const char *extension);
extern qboolean	gl_add_ext, gl_allow_ztrick;

// gl_warp.c
void GL_SubdivideSurface (msurface_t *fa);
void EmitTurbulentPolys (msurface_t *fa);
void EmitSkyPolys (msurface_t *fa, qboolean mtex);
void EmitCausticsPolys (void);
void R_DrawSkyChain (void);
void R_AddSkyBoxSurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
extern qboolean	r_skyboxloaded;
int R_SetSky (char *skyname);

// gl_draw.c
extern	cvar_t	gl_texturemode;
void GL_Set2D (void);
byte *StringToRGB(char *s);

// gl_rmain.c
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
qboolean R_CullSphere (vec3_t centre, float radius);
void R_PolyBlend (void);
void R_BrightenScreen (void);
void R_Q3DamageDraw (void);
#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];

// gl_rlight.c
void R_MarkLights (dlight_t *light, int lnum, mnode_t *node);
void R_AnimateLight (void);
void R_RenderDlights (void);
int R_LightPoint (vec3_t p);
float R_GetVertexLightValue (byte ppitch, byte pyaw, float apitch, float ayaw);
float R_LerpVertexLight (byte ppitch1, byte pyaw1, byte ppitch2, byte pyaw2, float ilerp, float apitch, float ayaw);
void R_InitVertexLights (void);
extern	float	bubblecolor[NUM_DLIGHTTYPES][4];
extern	vec3_t	lightspot, lightcolor;
extern	float	vlight_pitch, vlight_yaw;
extern	byte	anorm_pitch[NUMVERTEXNORMALS], anorm_yaw[NUMVERTEXNORMALS];

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

// gl_rsurf.c
void EmitDetailPolys (void);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void GL_BuildLightmaps (void);

// gl_rmisc.c
void R_InitOtherTextures (void);

// gl_rpart.c
typedef	enum
{
	pm_classic, pm_qmb, pm_quake3, pm_mixed
} part_mode_t;

void R_SetParticleMode (part_mode_t val);
char *R_NameForParticleMode (void);
void R_ToggleParticles_f (void);
extern	part_mode_t	particle_mode;

// gl_decals.c
void R_InitDecals (void);
void R_ClearDecals (void);
void R_DrawDecals (void);
void R_SpawnDecal (vec3_t center, vec3_t normal, vec3_t tangent, int tex, int size);
void R_SpawnDecalStatic (vec3_t org, int tex, int size);
extern	int		decal_blood1, decal_blood2, decal_blood3, decal_q3blood, decal_burn, decal_mark, decal_glow;
