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
// menu.h

extern	char	demodir[MAX_OSPATH];

typedef struct menu_window_s
{
	int x;
	int y;
	int w;
	int h;
} menu_window_t;

// menus
void M_Init (void);
void M_Keydown (int key);
void M_Draw (void);
void M_ToggleMenu_f (void);
void M_Menu_Main_f (void);
void M_Menu_Quit_f (void);

void M_DrawTextBox(int x, int y, int width, int lines);
void M_Menu_Options_f(void);
void M_Print(int cx, int cy, char *str);
void M_Print_GetPoint(int cx, int cy, int *rx, int *ry, char *str, qboolean red);
void M_PrintWhite(int cx, int cy, char *str);
void M_DrawCharacter(int cx, int line, int num);
void M_DrawTransPic(int x, int y, mpic_t *pic);
void M_DrawPic(int x, int y, mpic_t *pic);
void M_DrawCheckbox(int x, int y, int on);
void M_DrawSliderFloat2(int x, int y, float range, float value);

qboolean M_Mouse_Select(const menu_window_t *uw, const mouse_state_t *m, int entries, int *newentry);
qboolean Menu_Mouse_Event(const mouse_state_t* ms);
qboolean M_Video_Mouse_Event(const mouse_state_t *ms);
