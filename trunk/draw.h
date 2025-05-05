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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

#ifdef GLQUAKE
typedef struct
{
	int	width, height;
	int	texnum;
	float	sl, tl, sh, th;
} mpic_t;
#else
typedef struct
{
	int	width;
	short	height;
	byte	alpha;
	byte	pad;
	byte	data[4];	// variable sized
} mpic_t;
#endif

extern	mpic_t	*draw_disc;	// also used on sbar

typedef int color_t;

color_t RGBA_TO_COLOR(byte r, byte g, byte b, byte a);
byte* COLOR_TO_RGBA(color_t i, byte rgba[4]);
void Draw_SAlphaPic(int x, int y, mpic_t *gl, float alpha, float scale);
void Draw_AlphaLineRGB(int x_start, int y_start, int x_end, int y_end, float thickness, color_t color);

void Draw_Init (void);
void Draw_Character (int x, int y, int num, qboolean scale);
void Draw_DebugChar (char num);
void Draw_SubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_Pic (int x, int y, mpic_t *pic, qboolean scale);
void Draw_TransPic (int x, int y, mpic_t *pic, qboolean scale);
void Draw_TransPicTranslate (int x, int y, mpic_t *pic, byte *translation);
void Draw_ConsoleBackground (int lines);
void Draw_BeginDisc (void);
void Draw_EndDisc (void);
void Draw_TileClear (int x, int y, int w, int h);
void Draw_AlphaFill(int x, int y, int w, int h, int c, float alpha);
void Draw_Fill (int x, int y, int w, int h, int c);
void Draw_FadeScreen (void);
void Draw_String (int x, int y, char *str, qboolean scale);
void Draw_Alt_String (int x, int y, char *str, qboolean scale);
mpic_t *Draw_PicFromWad (char *name);
mpic_t *Draw_CachePic (char *path);
void Draw_Crosshair (qboolean draw_menu);
void Draw_TextBox(int x, int y, int width, int lines, qboolean scale);
void Draw_AdjustConback(void);

#ifdef GLQUAKE
void Draw_AlphaFillRGB(int x, int y, int w, int h, int c, float alpha);
void Draw_BoxScaledOrigin(int x, int y, int w, int h, int c, float alpha);
#endif
