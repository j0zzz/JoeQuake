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
#include <GL/glext.h>
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

#define	MAX_GLTEXTURES	4096	//joe: was 2048

//MHGLQFIX: polygons are not native to GPUs so use triangle fans instead
#undef GL_POLYGON
#define GL_POLYGON GL_TRIANGLE_FAN

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

#define ISTRANSPARENT(ent)	(((ent)->transparency > 0 && (ent)->transparency < 1) || \
							 (ent)->model && (ent)->model->type == mod_md3 && ((ent)->model->flags & EF_Q3TRANS))

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
extern	texture_t	*r_notexture_mip2;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int	currenttexture;
extern	int	particletexture;
extern	int	particletexture2;
extern	int	playertextures;
extern	int	ghosttextures;
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
extern	cvar_t	r_litwater;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_fullbrightskins;
extern	cvar_t	r_fastsky;
extern	cvar_t	r_skycolor;
extern	cvar_t	r_drawflame;
extern	cvar_t	r_skybox;
extern	cvar_t	r_farclip;
extern	cvar_t	r_outline;
extern	cvar_t	r_outline_surf;
extern	cvar_t	r_scale;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_loadlitfiles;
extern	cvar_t	gl_doubleeyes;
extern	cvar_t	gl_interdist;
extern	cvar_t	gl_interpolate_anims;
extern	cvar_t	gl_interpolate_moves;
extern  cvar_t  gl_waterfog;		
extern  cvar_t  gl_waterfog_density;
extern  cvar_t  gl_detail;
extern  cvar_t  gl_caustics;
extern	cvar_t	gl_fb_bmodels;
extern	cvar_t	gl_fb_models;
extern	cvar_t	gl_overbright;
extern	cvar_t	gl_overbright_models;
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
extern	cvar_t	gl_externaltextures_gfx;

extern qboolean draw_no24bit;

extern	int	gl_lightmap_format;
extern	int	gl_solid_format;
extern	int	gl_alpha_format;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;
extern	cvar_t	gl_picmip;

extern	int		mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;

extern	const	char	*gl_vendor;
extern	const	char	*gl_renderer;
extern	const	char	*gl_version;
extern	const	char	*gl_extensions;

void GL_Bind (int texnum);

// Generate mipmaps
typedef void (APIENTRY *lpGenerateMipmapFUNC)(GLenum);

// Multitexture
typedef void (APIENTRY *lpMTexFUNC)(GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC)(GLenum);

//johnfitz -- anisotropic filtering
#define	GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define	GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF
extern	float		gl_max_anisotropy;
extern	qboolean	gl_anisotropy_able;

// VBO
typedef void (APIENTRY *lpBindBufFUNC)(GLenum, GLuint);
typedef void (APIENTRY *lpBufferDataFUNC) (GLenum, GLsizeiptrARB, const GLvoid *, GLenum);
typedef void (APIENTRY *lpBufferSubDataFUNC) (GLenum, GLintptrARB, GLsizeiptrARB, const GLvoid *);
typedef void (APIENTRY *lpDeleteBuffersFUNC) (GLsizei, const GLuint *);
typedef void (APIENTRY *lpGenBuffersFUNC) (GLsizei, GLuint *);

// GLSL
typedef GLuint(APIENTRY *lpCreateShaderFUNC) (GLenum type);
typedef void (APIENTRY *lpDeleteShaderFUNC) (GLuint shader);
typedef void (APIENTRY *lpDeleteProgramFUNC) (GLuint program);
typedef void (APIENTRY *lpShaderSourceFUNC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRY *lpCompileShaderFUNC) (GLuint shader);
typedef void (APIENTRY *lpGetShaderivFUNC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *lpGetShaderInfoLogFUNC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *lpGetProgramivFUNC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *lpGetProgramInfoLogFUNC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLuint(APIENTRY *lpCreateProgramFUNC) (void);
typedef void (APIENTRY *lpAttachShaderFUNC) (GLuint program, GLuint shader);
typedef void (APIENTRY *lpLinkProgramFUNC) (GLuint program);
typedef void (APIENTRY *lpBindAttribLocationFUNC) (GLuint program, GLuint index, const GLchar *name);
typedef void (APIENTRY *lpUseProgramFUNC) (GLuint program);
typedef GLint(APIENTRY *lpGetAttribLocationFUNC) (GLuint program, const GLchar *name);
typedef void (APIENTRY *lpVertexAttribPointerFUNC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRY *lpEnableVertexAttribArrayFUNC) (GLuint index);
typedef void (APIENTRY *lpDisableVertexAttribArrayFUNC) (GLuint index);
typedef GLint(APIENTRY *lpGetUniformLocationFUNC) (GLuint program, const GLchar *name);
typedef void (APIENTRY *lpUniform1iFUNC) (GLint location, GLint v0);
typedef void (APIENTRY *lpUniform1ivFUNC) (GLint location, GLsizei count, const GLint *v);
typedef void (APIENTRY *lpUniform1fFUNC) (GLint location, GLfloat v0);
typedef void (APIENTRY *lpUniform3fFUNC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY *lpUniform4fFUNC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY *lpUniformMatrix4fvFUNC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRY *lpTexBufferFUNC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRY *lpBindBufferBaseFUNC) (GLenum target, GLuint index, GLuint buffer);
typedef GLuint(APIENTRY *lpGetUniformBlockIndexFUNC) (GLuint program, const GLchar *uniformBlockName);
typedef void (APIENTRY *lpUniformBlockBindingFUNC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

extern lpGenerateMipmapFUNC qglGenerateMipmap;

extern lpMTexFUNC qglMultiTexCoord2f;
extern lpSelTexFUNC qglActiveTexture;
extern lpBindBufFUNC qglBindBuffer;
extern lpBufferDataFUNC qglBufferData;
extern lpBufferSubDataFUNC qglBufferSubData;
extern lpDeleteBuffersFUNC qglDeleteBuffers;
extern lpGenBuffersFUNC qglGenBuffers;

extern lpCreateShaderFUNC qglCreateShader;
extern lpDeleteShaderFUNC qglDeleteShader;
extern lpDeleteProgramFUNC qglDeleteProgram;
extern lpShaderSourceFUNC qglShaderSource;
extern lpCompileShaderFUNC qglCompileShader;
extern lpGetShaderivFUNC qglGetShaderiv;
extern lpGetShaderInfoLogFUNC qglGetShaderInfoLog;
extern lpGetProgramivFUNC qglGetProgramiv;
extern lpGetProgramInfoLogFUNC qglGetProgramInfoLog;
extern lpCreateProgramFUNC qglCreateProgram;
extern lpAttachShaderFUNC qglAttachShader;
extern lpLinkProgramFUNC qglLinkProgram;
extern lpBindAttribLocationFUNC qglBindAttribLocation;
extern lpUseProgramFUNC qglUseProgram;
extern lpGetAttribLocationFUNC qglGetAttribLocation;
extern lpVertexAttribPointerFUNC qglVertexAttribPointer;
extern lpEnableVertexAttribArrayFUNC qglEnableVertexAttribArray;
extern lpDisableVertexAttribArrayFUNC qglDisableVertexAttribArray;
extern lpGetUniformLocationFUNC qglGetUniformLocation;
extern lpUniform1iFUNC qglUniform1i;
extern lpUniform1ivFUNC qglUniform1iv;
extern lpUniform1fFUNC qglUniform1f;
extern lpUniform3fFUNC qglUniform3f;
extern lpUniform4fFUNC qglUniform4f;
extern lpUniformMatrix4fvFUNC qglUniformMatrix4fv;
extern lpTexBufferFUNC qglTexBuffer;
extern lpBindBufferBaseFUNC qglBindBufferBase;
extern lpGetUniformBlockIndexFUNC qglGetUniformBlockIndex;
extern lpUniformBlockBindingFUNC qglUniformBlockBinding;

extern qboolean gl_mtexable;
extern int gl_textureunits;
extern qboolean	gl_vbo_able;
extern qboolean	gl_glsl_able;
extern qboolean gl_glsl_gamma_able;
extern qboolean gl_glsl_alias_able;
extern qboolean gl_packed_pixels;

typedef struct glsl_attrib_binding_s {
	const char *name;
	GLuint attrib;
} glsl_attrib_binding_t;

void GL_DisableMultitexture (void);
void GL_EnableMultitexture (void);

// vid_common_gl.c
void Check_Gamma (unsigned char *pal);
void GL_Init (void);
void GL_SetupState(void);
qboolean CheckExtension (const char *extension);
extern qboolean	gl_add_ext;

// gl_warp.c
void GL_SubdivideSurface (msurface_t *fa);
void EmitTurbulentPolys (msurface_t *fa);
void EmitCausticsPolys (void);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
extern qboolean	r_skyboxloaded;
int R_SetSky (char *skyname);
void Sky_NewMap(void);
void R_DrawSky(void);
void R_InitSky(texture_t *mt);		// called at level load
void R_UpdateWarpTextures(void);
extern int gl_warpimagesize;

// gl_draw.c
extern	cvar_t	gl_texturemode;
extern	cvar_t	gl_texturemode_hud;
extern	cvar_t	gl_texturemode_sky;
void GL_Set2D (void);
byte *StringToRGB(char *s);
void Draw_LoadPics(void);
void GL_SetCanvas(int newcanvas);

//johnfitz -- stuff for 2d drawing control
typedef enum {
	CANVAS_NONE,
	CANVAS_DEFAULT,
	CANVAS_WARPIMAGE,
	CANVAS_INVALID = -1
} canvastype;

// gl_rmain.c
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
qboolean R_CullSphere (vec3_t centre, float radius);
void R_PolyBlend (void);
void R_BrightenScreen (void);
void R_Q3DamageDraw (void);
void GLAlias_CreateShaders(void);
void GLSLGamma_GammaCorrect(void);
qboolean R_CullModelForEntity(entity_t *ent);

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];

//johnfitz -- fog functions called from outside gl_fog.c
void Fog_ParseServerMessage(void);
float *Fog_GetColor(void);
float Fog_GetDensity(void);
void Fog_EnableGFog(void);
void Fog_DisableGFog(void);
void Fog_StartAdditive(void);
void Fog_StopAdditive(void);
void Fog_SetupFrame(void);
void Fog_NewMap(void);
void Fog_Init(void);
void Fog_SetupState(void);
qboolean Fog_IsWaterFog(void);

typedef struct
{
	int useWaterFog;
	float density;
	float start;
	float end;
	float color[4];
} fog_data_t;

extern fog_data_t fog_data;

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
extern	int		anorm_pitch[NUMVERTEXNORMALS], anorm_yaw[NUMVERTEXNORMALS];

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

// gl_rsurf.c
void DrawGLPoly(glpoly_t *p);
void EmitDetailPolys (void);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void GL_BuildLightmaps (void);
void GL_DeleteBModelVertexBuffer(void);
void GL_BuildBModelVertexBuffer(void);
void GLWorld_CreateShaders(void);

// gl_rmisc.c
void R_InitOtherTextures (void);
void GL_BindBuffer(GLenum target, GLuint buffer);
void GL_ClearBufferBindings();
GLint GL_GetUniformLocation(GLuint *programPtr, const char *name);
GLuint GL_CreateProgram(const GLchar *vertSource, const GLchar *fragSource, int numbindings, const glsl_attrib_binding_t *bindings);

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

// gl_screen.c
void SCR_LoadPics(void);
