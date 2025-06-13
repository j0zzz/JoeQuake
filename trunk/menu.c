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
// menu.c

#include "quakedef.h"
#include "sound.h"
#include "winquake.h"
#include "qcurses/browser.h"

extern int browserscale;
extern qboolean refresh_demlist;
extern qboolean mod_changed;
qboolean vid_windowedmouse = true;
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_namemaker, 
	m_net, m_options, m_keys, m_mouse, m_misc, m_hud, m_crosshair_colorchooser, m_sound, 
#ifdef GLQUAKE
	m_view, m_renderer, m_textures, m_particles, m_decals, m_weapons, m_screenflashes, m_sky_colorchooser,
	m_outline_colorchooser,
#endif
	m_videomodes, m_nehdemos, m_maps, m_demos, m_mods, m_help, m_quit, m_serialconfig, m_modemconfig,
	m_lanconfig, m_gameoptions, m_search, m_servers, m_slist, m_sedit} m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_ServerList_f (void);
			void M_Menu_SEdit_f (void);
		void M_Menu_Setup_f (void);
			void M_Menu_NameMaker_f (void);
		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Mouse_f(void);
		void M_Menu_Misc_f(void);
		void M_Menu_Hud_f(void);
			void M_Menu_Crosshair_ColorChooser_f(void);
		void M_Menu_Sound_f(void);
#ifdef GLQUAKE
		void M_Menu_View_f(void);
		void M_Menu_Renderer_f(void);
			void M_Menu_Sky_ColorChooser_f(void);
		void M_Menu_Textures_f(void);
		void M_Menu_Particles_f(void);
		void M_Menu_Decals_f(void);
		void M_Menu_Weapons_f(void);
		void M_Menu_ScreenFlashes_f(void);
#endif
		void M_Menu_VideoModes_f (void);
	void M_Menu_NehDemos_f (void);
	void M_Menu_Maps_f (void);
	void M_Menu_Demos_f(void);
	void M_Menu_Mods_f (void);
	void M_Menu_Help_f (void);	// not used anymore
	void M_Menu_Quit_f (void);
void M_Menu_SerialConfig_f (void);
	void M_Menu_ModemConfig_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_FoundServers_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_ServerList_Draw (void);
			void M_SEdit_Draw (void);
		void M_Setup_Draw (void);
			void M_NameMaker_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Mouse_Draw(void);
		void M_Misc_Draw(void);
		void M_Hud_Draw(void);
		void M_Sound_Draw(void);
#ifdef GLQUAKE
		void M_View_Draw(void);
		void M_Renderer_Draw(void);
			void M_ScreenFlashes_Draw(void);
		void M_Textures_Draw(void);
		void M_Particles_Draw(void);
		void M_Decals_Draw(void);
		void M_Weapons_Draw(void);
#endif
		void M_VideoModes_Draw (void);
	void M_NehDemos_Draw (void);
	void M_Maps_Draw (void);
	void M_Demos_Draw(void);
	void M_Mods_Draw (void);
	void M_Quit_Draw (void);
void M_SerialConfig_Draw (void);
	void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_FoundServers_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_ServerList_Key (int key);
			void M_SEdit_Key (int key);
		void M_Setup_Key (int key);
			void M_NameMaker_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Mouse_Key(int key);
		void M_Misc_Key(int key);
		void M_Hud_Key(int key);
		void M_Sound_Key(int key);
#ifdef GLQUAKE
		void M_View_Key(int key);
		void M_Renderer_Key(int key);
			void M_ScreenFlashes_Key(int key);
		void M_Textures_Key(int key);
		void M_Particles_Key(int key);
		void M_Decals_Key(int key);
		void M_Weapons_Key(int key);
#endif
		void M_VideoModes_Key (int key);
	void M_NehDemos_Key (int key);
	void M_Maps_Key (int key);
	void M_Demos_Key(int key);
	void M_Mods_Key (int key);
	void M_Quit_Key (int key);
void M_SerialConfig_Key (int key);
	void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_FoundServers_Key (int key);

qboolean	m_entersound;		// play after drawing a frame, so caching
					// won't disrupt the sound
qboolean	m_recursiveDraw;

int		m_return_state;
qboolean	m_return_onerror;
char		m_return_reason[32];

#define StartingGame	(m_multiplayer_cursor == 2)
#define JoiningGame	(m_multiplayer_cursor == 1)
#define SerialConfig	(m_net_cursor == 0)
#define DirectConfig	(m_net_cursor == 1)
#define	IPXConfig	(m_net_cursor == 2)
#define	TCPIPConfig	(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

#ifdef GLQUAKE
int	menuwidth = 320;
int	menuheight = 240;
#else
#define	menuwidth	vid.width
#define	menuheight	vid.height
#endif

cvar_t	scr_centermenu = {"scr_centermenu", "1"};
int	m_yofs = 0;

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character (cx + ((menuwidth - 320) >> 1), line + m_yofs, num, false);
}

void M_DrawCharacter_GetPoint(int cx, int line, int *rx, int *ry, int num)
{
	cx += ((menuwidth - 320) >> 1);
	line += m_yofs;
	*rx = cx;
	*ry = line;
	Draw_Character(cx, line, num, false);
}

void M_Print_GetPoint(int cx, int cy, int *rx, int *ry, char *str, qboolean red) 
{
	cx += ((menuwidth - 320) >> 1);
	cy += m_yofs;
	*rx = cx;
	*ry = cy;
	if (red)
		Draw_Alt_String(cx, cy, str, false);
	else
		Draw_String(cx, cy, str, false);
}

void M_Print (int cx, int cy, char *str)
{
	int rx, ry;
	M_Print_GetPoint(cx, cy, &rx, &ry, str, true);
}

void M_PrintWhite (int cx, int cy, char *str)
{
	Draw_String (cx + ((menuwidth - 320) >> 1), cy + m_yofs, str, false);
}

// replacement of M_DrawTransPic - sometimes we need the real coordinate of the pic
static void M_DrawTransPic_GetPoint(int x, int y, int *rx, int *ry, mpic_t *pic)
{
	*rx = x + ((menuwidth - 320) >> 1);
	*ry = y + m_yofs;
	Draw_TransPic(x + ((menuwidth - 320) >> 1), y + m_yofs, pic, false);
}

void M_DrawTransPic (int x, int y, mpic_t *pic)
{
	int tx, ty;
	M_DrawTransPic_GetPoint(x, y, &tx, &ty, pic);
}

void M_DrawPic (int x, int y, mpic_t *pic)
{
	Draw_Pic (x + ((menuwidth - 320) >> 1), y + m_yofs, pic, false);
}

byte	identityTable[256], translationTable[256];

void M_BuildTranslationTable (int top, int bottom)
{
	int	j;
	byte	*dest, *source;

	for (j=0 ; j<256 ; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backward ranges. sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}


void M_DrawTransPicTranslate (int x, int y, mpic_t *pic)
{
	Draw_TransPicTranslate (x + ((menuwidth - 320) >> 1), y + m_yofs, pic, translationTable);
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	Draw_TextBox (x + ((menuwidth - 320) >> 1), y + m_yofs, width, lines, false);
}

// will apply menu scaling effect for given window region
static void M_Window_Adjust(const menu_window_t *original, menu_window_t *scaled)
{
#ifdef GLQUAKE
	double sc_x, sc_y; // scale factors

	sc_x = (double)vid.width / (double)menuwidth;
	sc_y = (double)vid.height / (double)menuheight;
	scaled->x = original->x * sc_x;
	scaled->y = original->y * sc_y;
	scaled->w = original->w * sc_x;
	scaled->h = original->h * sc_y;
#else
	memcpy(scaled, original, sizeof(menu_window_t));
#endif
}

// this function will look at window borders and current mouse cursor position
// and will change which item in the window should be selected
// we presume that all entries have same height and occupy the whole window
// 1st par: input, window position & size
// 2nd par: input, mouse position
// 3rd par: input, how many entries does the window have
// 4th par: output, newly selected entry, first entry is 0, second 1, ...
// return value: does the cursor belong to this window? yes/no
qboolean M_Mouse_Select(const menu_window_t *uw, const mouse_state_t *m, int entries, int *newentry)
{
	double entryheight;
	double nentry;
	menu_window_t rw;
	menu_window_t *w = &rw; // just a language "shortcut"

	M_Window_Adjust(uw, w);

	// window is invisible
	if (!(w->h > 0) || !(w->w > 0)) return false;

	// check if the pointer is inside of the window
	if (m->x < w->x || m->y < w->y || m->x > w->x + w->w || m->y > w->y + w->h)
		return false; // no, it's not

	entryheight = (double)w->h / (double)entries;
	nentry = (int)((m->y - w->y) / entryheight);

	*newentry = bound(0, nentry, entries - 1);

	return true;
}

qboolean M_Mouse_Select_Column(const menu_window_t *uw, const mouse_state_t *m, int entries, int *newentry)
{
	double entrywidth;
	double nentry;
	menu_window_t rw;
	menu_window_t *w = &rw; // just a language "shortcut"

	M_Window_Adjust(uw, w);

	// window is invisible
	if (!(w->h > 0) || !(w->w > 0)) return false;

	//FIXME - this method is used for menu sliders, where we want to keep the movement 
	//outside the window too, since ideally the user holds MOUSE1 down during movement

	// check if the pointer is inside of the window
	//if (m->x < w->x || m->y < w->y || m->x > w->x + w->w || m->y > w->y + w->h)
	//	return false; // no, it's not

	entrywidth = (double)w->w / (double)entries;
	nentry = (int)((m->x - w->x) / entrywidth);

	*newentry = bound(0, nentry, entries - 1);

	return true;
}

qboolean M_Mouse_Select_RowColumn(const menu_window_t *uw, const mouse_state_t *m, int row_entries, int *newentry_row, int col_entries, int *newentry_col)
{
	double entryheight, entrywidth;
	double nentry;
	menu_window_t rw;
	menu_window_t *w = &rw; // just a language "shortcut"

	M_Window_Adjust(uw, w);

	// window is invisible
	if (!(w->h > 0) || !(w->w > 0)) return false;

	// check if the pointer is inside of the window
	if (m->x < w->x || m->y < w->y || m->x > w->x + w->w || m->y > w->y + w->h)
		return false; // no, it's not

	entryheight = (double)w->h / (double)row_entries;
	nentry = (int)((m->y - w->y) / entryheight);
	*newentry_row = bound(0, nentry, row_entries - 1);

	entrywidth = (double)w->w / (double)col_entries;
	nentry = (int)((m->x - w->x) / entrywidth);
	*newentry_col = bound(0, nentry, col_entries - 1);

	return true;
}

//=============================================================================

int	m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}

//=============================================================================
/* COMMON STUFF FOR MENUS CONTAINING SCROLLABLE LISTS */

// NOTE: 320x200 res can only handle no more than 17 lines +2 for file
// searching. In GL I use 1 more line, though 320x200 is also available
// under GL too, but I force _nobody_ using that, but 320x240 instead!

#ifndef GLQUAKE
#define	MAXLINES	17	// maximum number of files visible on screen
#else
#define	MAXLINES	18
#endif

#define	MAXNEHLINES	20

#define	MAXKEYLINES	17

char	*NehDemos[][2] =
{
{"INTRO",	"Prologue"},
{"GENF",	"The Beginning"},
{"GENLAB",	"A Doomed Project"},
{"NEHCRE",	"The New Recruits"},
{"MAXNEH",	"Breakthrough"},
{"MAXCHAR",	"Renewal and Duty"},
{"CRISIS",	"Worlds Collide"},
{"POSTCRIS",	"Darkening Skies"},
{"HEARING",	"The Hearing"},
{"GETJACK",	"On a Mexican Radio"},
{"PRELUDE",	"Honor and Justice"},
{"ABASE",	"A Message Sent"},
{"EFFECT",	"The Other Side"},
{"UHOH",	"Missing in Action"},
{"PREPARE",	"The Response"},
{"VISION",	"Farsighted Eyes"},
{"MAXTURNS",	"Enter the Immortal"},
{"BACKLOT",	"Separate Ways"},
{"MAXSIDE",	"The Ancient Runes"},
{"COUNTER",	"The New Initiative"},
{"WARPREP",	"Ghosts to the World"},
{"COUNTER1",	"A Fate Worse Than Death"},
{"COUNTER2",	"Friendly Fire"},
{"COUNTER3",	"Minor Setback"},
{"MADMAX",	"Scores to Settle"},
{"QUAKE",	"One Man"},
{"CTHMM",	"Shattered Masks"},
{"SHADES",	"Deal with the Dead"},
{"GOPHIL",	"An Unlikely Hero"},
{"CSTRIKE",	"War in Hell"},
{"SHUBSET",	"The Conspiracy"},
{"SHUBDIE",	"Even Death May Die"},
{"NEWRANKS",	"An Empty Throne"},
{"SEAL",	"The Seal is Broken"}
};

#define	NUMNEHDEMOS	(sizeof(NehDemos)/sizeof(NehDemos[0]))

char	demodir[MAX_OSPATH] = "";
char	prevdir[MAX_OSPATH] = "";
char	searchfile[MAX_FILELENGTH] = "";

int	list_cursor = 0, list_base = 0, num_searchs = 0;
static qboolean	searchbox = false;

extern	int	key_insert;

menu_window_t list_window;

void SaveCursorPos (void)
{
	int	i;

	list_base = list_cursor = 0;

	if (prevdir)
	{
		for (i=0 ; i<num_files ; i++)
		{
			if (!strcmp(filelist[i].name, prevdir))
			{
				list_cursor = i;
				if (list_cursor >= MAXLINES)
				{
					list_base += list_cursor - (MAXLINES - 1);
					list_cursor = MAXLINES - 1;
				}
				*prevdir = 0;
			}
		}
	}
}

char *toYellow (char *s)
{
	static	char	buf[20];

	Q_strncpyz (buf, s, sizeof(buf));
	for (s = buf ; *s ; s++)
		if (*s >= '0' && *s <= '9')
			*s = *s - '0' + 18;

	return buf;
}

void M_List_Draw (char *title, int top)
{
	int		i, y, num_elements, num_lines, lx = 0, ly = 0;
	direntry_t	*d;

	M_Print (140, top - 16, title);
	if (nehahra)
	{
		M_Print (8, top, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
		num_elements = NUMNEHDEMOS;
		num_lines = MAXNEHLINES;
	}
	else
	{
		M_Print (8, top, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f\x1d\x1e\x1e\x1e\x1e\x1e\x1f");
		d = filelist + list_base;
		num_elements = num_files;
		num_lines = MAXLINES;
	}

	for (i = 0, y = top + 8 ; i < num_elements - list_base && i < num_lines ; i++, y += 8)
	{
		if (nehahra)
		{
			M_Print (24, y, NehDemos[list_base+i][1]);
		}
		else
		{
			char	str[29];

			Q_strncpyz (str, d->name, sizeof(str));
			if (d->type)
			{
				if (i == 0)
					M_Print_GetPoint(24, y, &list_window.x, &list_window.y, str, false);
				else
					M_Print_GetPoint(24, y, &lx, &ly, str, false);
			}
			else
			{
				if (i == 0)
					M_Print_GetPoint(24, y, &list_window.x, &list_window.y, str, true);
				else
					M_Print_GetPoint(24, y, &lx, &ly, str, true);
			}

			if (d->type == 1)
				M_PrintWhite (256, y, "folder");
			else if (d->type == 2)
				M_PrintWhite (256, y, "  up  ");
			else if (d->type == 0)
				M_Print (256, y, toYellow(va("%5ik", d->size >> 10)));
			d++;
		}
	}

	list_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	list_window.h = ly - list_window.y + 8;

	M_DrawCharacter (8, top + 8 + list_cursor*8, 12 + ((int)(realtime*4)&1));

	if (searchbox)
	{
		M_PrintWhite (24, top + 24 + 8*MAXLINES, "search: ");
		M_DrawTextBox (80, top + 16 + 8*MAXLINES, 16, 1);
		M_PrintWhite (88, top + 24 + 8*MAXLINES, searchfile);

		M_DrawCharacter (88 + 8*strlen(searchfile), top + 24 + 8*MAXLINES, ((int)(realtime*4)&1) ? 11 + (84*key_insert) : 10);
	}
}

static void KillSearchBox (void)
{
	searchbox = false;
	memset (searchfile, 0, sizeof(searchfile));
	num_searchs = 0;
}

void GotoStartOfList()
{
	list_cursor = 0;
	list_base = 0;
}

void GotoEndOfList(int num_elements, int num_lines)
{
	if (num_elements > num_lines)
	{
		list_cursor = num_lines - 1;
		list_base = num_elements - list_cursor - 1;
	}
	else
	{
		list_base = 0;
		list_cursor = num_elements - 1;
	}
}

void M_List_Key (int k, int num_elements, int num_lines)
{
	switch (k)
	{
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (list_cursor == 0)
		{
			if (list_base > 0)
				list_base--;
			else
				GotoEndOfList(num_elements, num_lines);
		}
		else if (list_cursor > 0)
			list_cursor--;
		else if (list_base > 0)
			list_base--;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (list_cursor + list_base == num_elements - 1)
			GotoStartOfList();
		else if (list_cursor + list_base < num_elements - 1)
		{
			if (list_cursor < num_lines - 1)
				list_cursor++;
			else
				list_base++;
		}
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		GotoStartOfList();
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		GotoEndOfList(num_elements, num_lines);
		break;

	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		list_cursor -= num_lines - 1;
		if (list_cursor < 0)
		{
			list_base += list_cursor;
			if (list_base < 0)
				list_base = 0;
			list_cursor = 0;
		}
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		list_cursor += num_lines - 1;
		if (list_base + list_cursor >= num_elements)
			list_cursor = num_elements - list_base - 1;
		if (list_cursor >= num_lines)
		{
			list_base += list_cursor - (num_lines - 1);
			list_cursor = num_lines - 1;
			if (list_base + list_cursor >= num_elements)
				list_base = num_elements - list_cursor - 1;
		}
		break;

	case K_MWHEELUP:
		if (list_base > 0)
			list_base--;
		break;

	case K_MWHEELDOWN:
		if (list_base + (num_lines - 1) < num_elements - 1)
			list_base++;
		break;
	}
}

//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
int	MAIN_ITEMS = 7;

menu_window_t m_main_window;

void M_Menu_Main_f (void)
{
#ifdef GLQUAKE
	if (nehahra)
	{
		if (NehGameType == TYPE_DEMO)
			MAIN_ITEMS = 4;
		else if (NehGameType == TYPE_GAME)
			MAIN_ITEMS = 5;
		else
			MAIN_ITEMS = 6;
	}
#endif

	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

void M_Main_Draw (void)
{
	int	f;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

#ifdef GLQUAKE
	if (nehahra)
	{
		if (NehGameType == TYPE_BOTH)
			M_DrawTransPic (72, 32, Draw_CachePic("gfx/mainmenu.lmp"));
		else if (NehGameType == TYPE_GAME)
			M_DrawTransPic (72, 32, Draw_CachePic("gfx/gamemenu.lmp"));
		else
			M_DrawTransPic (72, 32, Draw_CachePic("gfx/demomenu.lmp"));
	}
	else
#endif
	{
		extern searchpath_t	*com_base_searchpaths;
		searchpath_t *temp;

		// temporarly exclude gamedir from searchpaths so that the joequake's custom mainmenu is always loaded from its preferred dir
		temp = com_searchpaths;
		com_searchpaths = com_base_searchpaths;
		p = Draw_CachePic("gfx/mainmenu.lmp");
		com_searchpaths = temp;

		m_main_window.w = p->width;
		m_main_window.h = p->height;
		M_DrawTransPic_GetPoint(72, 32, &m_main_window.x, &m_main_window.y, p);

		// menu specific correction, mainmenu.lmp|png have some useless extra space at the bottom
		// that makes the mouse pointer position calculation imperfect
		m_main_window.h *= 0.93;
	}

	f = (int)(host_time*10) % 6;

	M_DrawTransPic (54, 32 + m_main_cursor*20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}

void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
			CL_NextDemo ();
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = MAIN_ITEMS - 1;
		break;
	
	case K_ENTER:
	case K_MOUSE1: 
		m_entersound = true;
#ifdef GLQUAKE
		if (nehahra)	// nehahra menus
		{
			if (NehGameType == TYPE_GAME)
			{
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_SinglePlayer_f ();
					break;

				case 1:
					M_Menu_MultiPlayer_f ();
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("playdemo ENDCRED\n");
					break;

				case 4:
					M_Menu_Quit_f ();
					break;
				}
			}
			else if (NehGameType == TYPE_DEMO)
			{
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_NehDemos_f ();
					break;

				case 1:
					M_Menu_Options_f ();
					break;

				case 2:
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("playdemo ENDCRED\n");
					break;

				case 3:
					M_Menu_Quit_f ();
					break;
				}
			}
			else
			{
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_SinglePlayer_f ();
					break;

				case 1:
					M_Menu_NehDemos_f ();
					break;

				case 2:
					M_Menu_MultiPlayer_f ();
					break;

				case 3:
					M_Menu_Options_f ();
					break;

        	                case 4:
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("playdemo ENDCRED\n");
					break;

				case 5:
					M_Menu_Quit_f ();
					break;
				}
			}
		}
		else	// original quake menu
#endif
		{
			switch (m_main_cursor)
			{
			case 0:
				M_Menu_SinglePlayer_f ();
				break;

			case 1:
				M_Menu_MultiPlayer_f ();
				break;

			case 2:
				M_Menu_Options_f ();
				break;

			case 3:
				M_Menu_Maps_f ();
				break;

			case 4:
				M_Menu_Demos_f ();
				break;

			case 5:
				M_Menu_Mods_f ();
				break;

			case 6:
				M_Menu_Quit_f ();
				break;
			}
		}
	}
}

static qboolean M_Main_Mouse_Event(const mouse_state_t* ms)
{
	M_Mouse_Select(&m_main_window, ms, MAIN_ITEMS, &m_main_cursor);

	if (ms->button_up == 1) M_Main_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Main_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* SINGLE PLAYER MENU */

#define	SINGLEPLAYER_ITEMS	3

int		m_singleplayer_cursor;
qboolean m_singleplayer_confirm;

menu_window_t m_singleplayer_window;

void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
	m_singleplayer_confirm = false;
}

void M_SinglePlayer_Draw (void)
{
	int	f;
	mpic_t	*p;

	if (m_singleplayer_confirm)
	{
		M_PrintWhite (64, 11*8, "Are you sure you want to");
		M_PrintWhite (64, 12*8, "    start a new game?");
		return;
	}

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	
	p = Draw_CachePic("gfx/sp_menu.lmp");
	m_singleplayer_window.w = p->width;
	m_singleplayer_window.h = p->height;
	M_DrawTransPic_GetPoint(72, 32, &m_singleplayer_window.x, &m_singleplayer_window.y, p);

	// menu specific correction, sp_menu.lmp|png have some useless extra space at the bottom
	// that makes the mouse pointer position calculation imperfect
	m_singleplayer_window.h *= 0.90;

	f = (int)(host_time*10) % 6;
	M_DrawTransPic (54, 32 + m_singleplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}

static void StartNewGame (void)
{
	key_dest = key_game;
	if (sv.active)
		Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cvar_SetValue (&teamplay, 0);
	Cvar_SetValue (&coop, 0);
	if (nehahra)
		Cbuf_AddText ("map nehstart\n");
	else
		Cbuf_AddText ("map start\n");
}

void M_SinglePlayer_Key (int key)
{
	if (m_singleplayer_confirm)
	{
		if (key == 'n' || key == K_ESCAPE || key == K_MOUSE2)
		{
			m_singleplayer_confirm = false;
			m_entersound = true;
		}
		else if (key == 'y' || key == K_ENTER || key == K_MOUSE1)
		{
			StartNewGame ();
		}
		return;
	}

	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;
		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				m_singleplayer_confirm = true;
			else
				StartNewGame ();
			break;

		case 1:
			M_Menu_Load_f ();
			break;

		case 2:
			M_Menu_Save_f ();
			break;
		}
	}
}

qboolean M_SinglePlayer_Mouse_Event(const mouse_state_t* ms)
{
	M_Mouse_Select(&m_singleplayer_window, ms, SINGLEPLAYER_ITEMS, &m_singleplayer_cursor);

	if (ms->button_up == 1) M_SinglePlayer_Key(K_MOUSE1);
	if (ms->button_up == 2) M_SinglePlayer_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* LOAD/SAVE MENU */

int	load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES	12
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int	loadable[MAX_SAVEGAMES];
menu_window_t load_window, save_window;

void M_ScanSaves (void)
{
	int	i, j, version;
	char	name[MAX_OSPATH];
	FILE	*f;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf (name, "%s/s%i.sav", com_gamedir, i);
		if (!(f = fopen(name, "r")))
			continue;
		fscanf (f, "%i\n", &version);
		fscanf (f, "%79s\n", name);
		strncpy (m_filenames[i], name, sizeof(m_filenames[i])-1);

	// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose (f);
	}
}

void M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves ();
}

void M_Menu_Save_f (void)
{
	if (!sv.active || cl.intermission || svs.maxclients != 1)
		return;

	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves ();
}

void M_Load_Draw (void)
{
	int	i;
	mpic_t	*p;
	int lx = 0, ly = 0;	// lower bounds of the window 

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		if (i == 0)
			M_Print_GetPoint(16, 32 + 8 * i, &load_window.x, &load_window.y, m_filenames[i], load_cursor == 0);
		else
			M_Print_GetPoint(16, 32 + 8 * i, &lx, &ly, m_filenames[i], load_cursor == i);
	}

	load_window.w = SAVEGAME_COMMENT_LENGTH * 8; // presume 8 pixels for each letter
	load_window.h = ly - load_window.y + 8;

	// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Save_Draw (void)
{
	int	i;
	mpic_t	*p;
	int lx = 0, ly = 0;	// lower bounds of the window

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		if (i == 0)
			M_Print_GetPoint(16, 32 + 8 * i, &save_window.x, &save_window.y, m_filenames[i], load_cursor == 0);
		else
			M_Print_GetPoint(16, 32 + 8 * i, &lx, &ly, m_filenames[i], load_cursor == i);
	}

	save_window.w = SAVEGAME_COMMENT_LENGTH * 8; // presume 8 pixels for each letter
	save_window.h = ly - save_window.y + 8;

	// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Load_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

	// Host_Loadgame_f can't bring up the loading plaque because too much
	// stack space has been used, so do it now
		SCR_BeginLoadingPlaque ();

	// issue the load command
		Cbuf_AddText (va("load s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = MAX_SAVEGAMES - 1;
		break;
	}
}

void M_Save_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
	case K_MOUSE1:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = MAX_SAVEGAMES - 1;
		break;
	}
}

qboolean M_Save_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&save_window, ms, MAX_SAVEGAMES, &load_cursor);

	if (ms->button_up == 1) M_Save_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Save_Key(K_MOUSE2);

	return true;
}

qboolean M_Load_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&load_window, ms, MAX_SAVEGAMES, &load_cursor);

	if (ms->button_up == 1) M_Load_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Load_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	4

menu_window_t multiplayer_window;

void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}

void M_MultiPlayer_Draw (void)
{
	int	f;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	
	p = Draw_CachePic("gfx/mp_menu.lmp");
	multiplayer_window.w = p->width;
	multiplayer_window.h = p->height;
	M_DrawTransPic_GetPoint(72, 32, &multiplayer_window.x, &multiplayer_window.y, p);

	// menu specific correction, mp_menu.lmp|png have some useless extra space at the bottom
	// that makes the mouse pointer position calculation imperfect
	multiplayer_window.h *= 0.92;

	f = (int)(host_time * 10) % 6;
	M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));

	if (serialAvailable || ipxAvailable || tcpipAvailable)
		return;

	M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available");
}

void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_multiplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			M_Menu_ServerList_f ();
			break;

		case 1:
		case 2:
			if (serialAvailable || ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 3:
			M_Menu_Setup_f ();
			break;
		}
	}
}

qboolean M_MultiPlayer_Mouse_Event(const mouse_state_t* ms)
{
	M_Mouse_Select(&multiplayer_window, ms, MULTIPLAYER_ITEMS, &m_multiplayer_cursor);

	if (ms->button_up == 1) M_MultiPlayer_Key(K_MOUSE1);
	if (ms->button_up == 2) M_MultiPlayer_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* SETUP MENU */

int	setup_cursor = 14;
int	setup_cursor_table[] = {40, 0, 56, 0, 72, 0, 0, 96, 0, 0, 120, 0, 0, 0, 152};

char	setup_hostname[16], setup_myname[16];
int	setup_oldtop, setup_oldbottom, setup_top, setup_bottom;

qboolean from_namemaker = false;

#define	NUM_SETUP_CMDS	15

menu_window_t mpsetup_window;

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	if (from_namemaker)
		from_namemaker = !from_namemaker;
	else
		Q_strncpyz (setup_myname, cl_name.string, sizeof(setup_myname));
	Q_strncpyz (setup_hostname, hostname.string, sizeof(setup_hostname));
	setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
	setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;
}

void M_Setup_Draw (void)
{
	mpic_t	*p;
	int		lx = 0, ly = 0;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(64, 40, &mpsetup_window.x, &mpsetup_window.y, "Hostname", setup_cursor == 0);
	M_DrawTextBox (160, 32, 16, 1);
	M_PrintWhite (168, 40, setup_hostname);

	M_Print_GetPoint(64, 56, &lx, &ly, "Your name", setup_cursor == 2);
	M_DrawTextBox (160, 48, 16, 1);
	M_PrintWhite (168, 56, setup_myname);

	M_Print_GetPoint(64, 72, &lx, &ly, "Name maker", setup_cursor == 4);

	M_Print_GetPoint(64, 96, &lx, &ly, "Shirt color", setup_cursor == 7);
	M_Print_GetPoint(64, 120, &lx, &ly, "Pants color", setup_cursor == 10);

	M_DrawTextBox (64, 144, 14, 1);
	M_Print_GetPoint(72, 152, &lx, &ly, "Accept Changes", setup_cursor == 14);

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTransPic (160, 80, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable (setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 88, p);

	mpsetup_window.w = 30 * 8; // presume 8 pixels for each letter
	mpsetup_window.h = ly - mpsetup_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (setup_cursor == 1 || setup_cursor == 3 || setup_cursor == 5 || setup_cursor == 6 || 
		setup_cursor == 8 || setup_cursor == 9 || setup_cursor == 11 || setup_cursor == 12 || setup_cursor == 13)
		return;

	M_DrawCharacter (56, setup_cursor_table[setup_cursor], 12 + ((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table[setup_cursor], 10 + ((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table[setup_cursor], 10 + ((int)(realtime*4)&1));
}

void M_Setup_Key (int k)
{
	int	l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor == 13)
			setup_cursor--;
		if (setup_cursor == 12 || setup_cursor == 9 || setup_cursor == 6)
			setup_cursor--;
		if (setup_cursor == 1 || setup_cursor == 3 || setup_cursor == 5 || setup_cursor == 8 || setup_cursor == 11)
			setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor == 1 || setup_cursor == 3 || setup_cursor == 5 || setup_cursor == 8 || setup_cursor == 11)
			setup_cursor++;
		if (setup_cursor == 12 || setup_cursor == 9 || setup_cursor == 6)
			setup_cursor++;
		if (setup_cursor == 13)
			setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_LEFTARROW:
	case K_MOUSE2:
		if (setup_cursor < 7)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 7)
		{
			setup_top -= 1;
			if (setup_top < 0)
				setup_top = 13;
		}
		else if (setup_cursor == 10)
		{
			setup_bottom -= 1;
			if (setup_bottom < 0)
				setup_bottom = 13;
		}
		break;

	case K_RIGHTARROW:
		if (setup_cursor < 7)
			return;
forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 7)
		{
			setup_top += 1;
			if (setup_top > 13)
				setup_top = 0;
		}
		else if (setup_cursor == 10)
		{
			setup_bottom += 1;
			if (setup_bottom > 13)
				setup_bottom = 0;
		}
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (setup_cursor == 0 || setup_cursor == 2)
			return;

		if (setup_cursor == 7 || setup_cursor == 10)
			goto forward;

		if (setup_cursor == 4)
		{
			m_entersound = true;
			M_Menu_NameMaker_f ();
			break;
		}

		// setup_cursor == 5 (OK)
		Cbuf_AddText (va("name \"%s\"\n", setup_myname));
		Cvar_Set (&hostname, setup_hostname);
		Cbuf_AddText (va("color %i %i\n", setup_top, setup_bottom));
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}
		else if (setup_cursor == 2)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;

		Key_Extra (&k);

		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l] = k;
				setup_hostname[l+1] = 0;
			}
		}
		else if (setup_cursor == 2)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l] = k;
				setup_myname[l+1] = 0;
			}
		}
		break;
	}
}

qboolean M_Setup_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&mpsetup_window, ms, NUM_SETUP_CMDS, &setup_cursor);

	if (ms->button_up == 1) M_Setup_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Setup_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* NAME MAKER MENU */

int	namemaker_cursor_x, namemaker_cursor_y;
#define	NAMEMAKER_TABLE_SIZE	16
#define	NAMEMAKER_ITEMS			NAMEMAKER_TABLE_SIZE + 2

char	namemaker_name[16];

menu_window_t namemaker_window;

void M_Menu_NameMaker_f (void)
{
	key_dest = key_menu;
	m_state = m_namemaker;
	m_entersound = true;
	Q_strncpyz (namemaker_name, cl_name.string, sizeof(namemaker_name));
}

void M_NameMaker_Draw (void)
{
	int	x, y, lx = 0, ly = 0;

	M_Print (48, 16, "Your name");
	M_DrawTextBox (120, 8, 16, 1);
	M_PrintWhite (128, 16, namemaker_name);

	for (y=0 ; y<NAMEMAKER_TABLE_SIZE ; y++)
		for (x=0 ; x<NAMEMAKER_TABLE_SIZE ; x++)
			if (!y && !x)
			{
				M_DrawCharacter_GetPoint(32 + (16 * x), 40 + (8 * y), &namemaker_window.x, &namemaker_window.y, NAMEMAKER_TABLE_SIZE * y + x);
				namemaker_window.x -= 8;	// adjust it slightly to the left
			}
			else
				M_DrawCharacter_GetPoint(32 + (16 * x), 40 + (8 * y), &lx, &ly, NAMEMAKER_TABLE_SIZE * y + x);

	M_DrawTextBox (136, 176, 2, 1);
	M_Print_GetPoint (144, 184, &lx, &ly, "OK", namemaker_cursor_y == NAMEMAKER_ITEMS);

	namemaker_window.w = 32 * 8; // presume 8 pixels for each letter
	namemaker_window.h = ly - namemaker_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (namemaker_cursor_y == NAMEMAKER_ITEMS - 2 || namemaker_cursor_y == NAMEMAKER_ITEMS - 1)
		return;

	// cursor
	if (namemaker_cursor_y == NAMEMAKER_ITEMS)
		M_DrawCharacter(128, 184, 12 + ((int)(realtime * 4) & 1));
	else
		M_DrawCharacter(24 + 16 * namemaker_cursor_x, 40 + 8 * namemaker_cursor_y, 12 + ((int)(realtime * 4) & 1));
}

void M_NameMaker_Key (int k)
{
	int	l;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Setup_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y--;
		if (namemaker_cursor_y < 0)
			namemaker_cursor_y = NAMEMAKER_ITEMS;
		else if (namemaker_cursor_y >= NAMEMAKER_ITEMS - 2 && namemaker_cursor_y <= NAMEMAKER_ITEMS - 1)
			namemaker_cursor_y -= 2;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y++;
		if (namemaker_cursor_y > NAMEMAKER_ITEMS)
			namemaker_cursor_y = 0;
		else if (namemaker_cursor_y >= NAMEMAKER_ITEMS - 2 && namemaker_cursor_y <= NAMEMAKER_ITEMS - 1)
			namemaker_cursor_y += 2;
		break;

	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y = 0;
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y = NAMEMAKER_ITEMS;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x--;
		if (namemaker_cursor_x < 0)
			namemaker_cursor_x = NAMEMAKER_TABLE_SIZE - 1;
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x++;
		if (namemaker_cursor_x >= NAMEMAKER_TABLE_SIZE)
			namemaker_cursor_x = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x = NAMEMAKER_TABLE_SIZE - 1;
		break;

	case K_BACKSPACE:
		if ((l = strlen(namemaker_name)))
			namemaker_name[l-1] = 0;
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (namemaker_cursor_y == NAMEMAKER_ITEMS)
		{
			Q_strncpyz (setup_myname, namemaker_name, sizeof(setup_myname));
			from_namemaker = true;
			M_Menu_Setup_f ();
		}
		else if (namemaker_cursor_y > 0 && namemaker_cursor_y < NAMEMAKER_TABLE_SIZE)
		{
			l = strlen(namemaker_name);
			if (l < 15)
			{
				namemaker_name[l] = NAMEMAKER_TABLE_SIZE * namemaker_cursor_y + namemaker_cursor_x;
				namemaker_name[l+1] = 0;
			}
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;

		Key_Extra (&k);

		l = strlen(namemaker_name);
		if (l < 15)
		{
			namemaker_name[l] = k;
			namemaker_name[l+1] = 0;
		}
		break;
	}
}

qboolean M_NameMaker_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select_RowColumn(&namemaker_window, ms, NAMEMAKER_ITEMS + 1, &namemaker_cursor_y, NAMEMAKER_TABLE_SIZE, &namemaker_cursor_x);

	if (ms->button_up == 1) M_NameMaker_Key(K_MOUSE1);
	if (ms->button_up == 2) M_NameMaker_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* NET MENU */

int	m_net_cursor, m_net_items, m_net_saveHeight;

char *net_helpMessage[] =
{
/* .........1.........2.... */
  "                        ",
  " Two computers connected",
  "   through two modems.  ",
  "                        ",

  "                        ",
  " Two computers connected",
  " by a null-modem cable. ",
  "                        ",

  " Novell network LANs    ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}

void M_Net_Draw (void)
{
	int	f;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	f = 32;
	if (serialAvailable)
	{
		p = Draw_CachePic ("gfx/netmen1.lmp");
	}
	else
	{
#ifdef _WIN32
		p = NULL;
#else
		p = Draw_CachePic ("gfx/dim_modm.lmp");
#endif
	}

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;
	if (serialAvailable)
	{
		p = Draw_CachePic ("gfx/netmen2.lmp");
	}
	else
	{
#ifdef _WIN32
		p = NULL;
#else
		p = Draw_CachePic ("gfx/dim_drct.lmp");
#endif
	}

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;
	if (ipxAvailable)
		p = Draw_CachePic ("gfx/netmen3.lmp");
	else
		p = Draw_CachePic ("gfx/dim_ipx.lmp");
	M_DrawTransPic (72, f, p);

	f += 19;
	if (tcpipAvailable)
		p = Draw_CachePic ("gfx/netmen4.lmp");
	else
		p = Draw_CachePic ("gfx/dim_tcp.lmp");
	M_DrawTransPic (72, f, p);

	if (m_net_items == 5)	// JDC, could just be removed
	{
		f += 19;
		p = Draw_CachePic ("gfx/netmen5.lmp");
		M_DrawTransPic (72, f, p);
	}

	f = (320 - 26 * 8) / 2;
	M_DrawTextBox (f, 134, 24, 4);
	f += 8;
	M_Print (f, 142, net_helpMessage[m_net_cursor*4+0]);
	M_Print (f, 150, net_helpMessage[m_net_cursor*4+1]);
	M_Print (f, 158, net_helpMessage[m_net_cursor*4+2]);
	M_Print (f, 166, net_helpMessage[m_net_cursor*4+3]);

	f = (int)(host_time * 10) % 6;
	M_DrawTransPic (54, 32 + m_net_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}

void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_MultiPlayer_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_net_cursor)
		{
		case 0:
		case 1:
			M_Menu_SerialConfig_f ();
			break;

		case 2:
		case 3:
			M_Menu_LanConfig_f ();
			break;

		case 4:	// multiprotocol
			break;
		}
	}

	if ((SerialConfig || DirectConfig) && !serialAvailable)
		goto again;
	if (IPXConfig && !ipxAvailable)
		goto again;
	if (TCPIPConfig && !tcpipAvailable)
		goto again;
}

qboolean M_Net_Mouse_Event(const mouse_state_t *ms)
{
	if (ms->button_up == 1) M_Net_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Net_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* OPTIONS MENU */

#ifdef GLQUAKE
#define	OPTIONS_ITEMS	16
#else
#define	OPTIONS_ITEMS	9
#endif

#define	SLIDER_RANGE	10

int	options_cursor;
int	mp_optimized;

menu_window_t options_window;

extern	void IN_MLookDown (void);
extern	void IN_MLookUp (void);

extern	cvar_t	v_kickroll, v_kickpitch, v_kicktime;
extern	cvar_t	cl_bob, cl_rollangle, scr_conspeed;

void CheckMPOptimized (void)
{
	if (!lookstrafe.value && !lookspring.value && !v_kickroll.value && !v_kickpitch.value && 
	    !v_kicktime.value && !cl_bob.value && !cl_rollangle.value && scr_conspeed.value == 99999)
		mp_optimized = 1;
	else
		mp_optimized = 0;
}

void SetMPOptimized (int val)
{
	mp_optimized = val;

	if (mp_optimized)
	{
		Cvar_SetValue (&lookstrafe, 0);
		Cvar_SetValue (&lookspring, 0);
		Cvar_SetValue (&v_kickroll, 0);
		Cvar_SetValue (&v_kickpitch, 0);
		Cvar_SetValue (&v_kicktime, 0);
		Cvar_SetValue (&cl_bob, 0);
		Cvar_SetValue (&cl_rollangle, 0);
		Cvar_SetValue (&scr_conspeed, 99999);
	}
	else
	{
		Cvar_ResetVar (&lookstrafe);
		Cvar_ResetVar (&lookspring);
		Cvar_ResetVar (&v_kickroll);
		Cvar_ResetVar (&v_kickpitch);
		Cvar_ResetVar (&v_kicktime);
		Cvar_ResetVar (&cl_bob);
		Cvar_ResetVar (&cl_rollangle);
		Cvar_ResetVar (&scr_conspeed);
	}
}

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;

	CheckMPOptimized ();
}

void M_DebugWindow(menu_window_t *w)
{
#ifdef _DEBUG
	{
		color_t c = RGBA_TO_COLOR(255, 0, 0, 255);
		Draw_AlphaLineRGB(w->x, w->y, w->x + w->w, w->y, 2, c);
		Draw_AlphaLineRGB(w->x, w->y, w->x, w->y + w->h, 2, c);
		Draw_AlphaLineRGB(w->x, w->y + w->h, w->x + w->w, w->y + w->h, 2, c);
		Draw_AlphaLineRGB(w->x + w->w, w->y, w->x + w->w, w->y + w->h, 2, c);
	}
#endif
}

void M_DrawSlider(int x, int y, float range, menu_window_t *w)
{
	int	i;

	range = bound(0, range, 1);
	M_DrawCharacter(x - 8, y, 128);
	for (i = 0; i<SLIDER_RANGE; i++)
		M_DrawCharacter(x + i * 8, y, 129);
	M_DrawCharacter(x + i * 8, y, 130);
	M_DrawCharacter(x + (SLIDER_RANGE - 1) * 8 * range, y, 131);

	w->x = x + ((menuwidth - 320) >> 1);
	w->y = y + m_yofs;
	w->w = SLIDER_RANGE * 8;
	w->h = 8;

	//M_DebugWindow(w);
}

void M_DrawSliderInt(int x, int y, float range, int value, menu_window_t *w)
{
	char val_as_str[10];

	M_DrawSlider(x, y, range, w);

	sprintf(val_as_str, "%i", value);
	M_Print(x + SLIDER_RANGE * 8 + 16, y, val_as_str);
}

void M_DrawSliderFloat (int x, int y, float range, float value, menu_window_t *w)
{
	char val_as_str[10];

	M_DrawSlider(x, y, range, w);

	sprintf(val_as_str, "%.1f", value);
	M_Print(x + SLIDER_RANGE * 8 + 16, y, val_as_str);
}

void M_DrawSliderFloat2(int x, int y, float range, float value, menu_window_t *w)
{
	char val_as_str[10];

	M_DrawSlider(x, y, range, w);

	sprintf(val_as_str, "%.2f", value);
	M_Print(x + SLIDER_RANGE * 8 + 16, y, val_as_str);
}

int FindSliderItemIndex(float *values, int max_items_count, cvar_t *cvar)
{
	int i, current_index;

	if (cvar->value < values[0])
		current_index = 0;
	else if (cvar->value >= values[max_items_count - 1])
		current_index = max_items_count - 1;
	else
		for (i = 0; i < max_items_count - 1; i++)
		{
			if (cvar->value >= values[i] && cvar->value < values[i + 1])
			{
				current_index = i;
				break;
			}
		}

	return current_index;
}

void AdjustSliderBasedOnArrayOfValues(int dir, float *values, int max_items_count, cvar_t *cvar)
{
	int current_index;

	current_index = FindSliderItemIndex(values, max_items_count, cvar);

	if (dir < 0 && current_index > 0)
		Cvar_SetValue(cvar, values[current_index - 1]);
	else if (dir > 0 && current_index < (max_items_count - 1))
		Cvar_SetValue(cvar, values[current_index + 1]);
}

void M_DrawCheckbox (int x, int y, int on)
{
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	mpic_t	*p;
	int		lx = 0, ly = 0;	// lower bounds of the window 

	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &options_window.x, &options_window.y, "    Customize controls", options_cursor == 0);
	M_Print_GetPoint(16, 40, &lx, &ly, "         Mouse options", options_cursor == 1);

	M_Print_GetPoint(16, 56, &lx, &ly, "           Hud options", options_cursor == 3);
	M_Print_GetPoint(16, 64, &lx, &ly, "         Sound options", options_cursor == 4);
#ifdef GLQUAKE
	M_Print_GetPoint(16, 72, &lx, &ly, "          View Options", options_cursor == 5);
	M_Print_GetPoint(16, 80, &lx, &ly, "      Renderer options", options_cursor == 6);
	M_Print_GetPoint(16, 88, &lx, &ly, "       Texture options", options_cursor == 7);
	M_Print_GetPoint(16, 96, &lx, &ly, "      Particle options", options_cursor == 8);
	M_Print_GetPoint(16, 104, &lx, &ly, "         Decal options", options_cursor == 9);
	M_Print_GetPoint(16, 112, &lx, &ly, "        Weapon options", options_cursor == 10);
	M_Print_GetPoint(16, 120, &lx, &ly, "        Screen flashes", options_cursor == 11);
	M_Print_GetPoint(16, 128, &lx, &ly, " Miscellaneous options", options_cursor == 12);
#else
	M_PrintWhite(16, 72, " Miscellaneous options");
#endif

	if (vid_menudrawfn)
	{
#ifndef GLQUAKE
		M_PrintWhite(16, 80, "           Video modes");

		M_PrintWhite(16, 96, "         Go to console");
#else
		M_Print_GetPoint(16, 136, &lx, &ly, "        Set video mode", options_cursor == 13);

		M_Print_GetPoint(16, 152, &lx, &ly, "         Go to console", options_cursor == 15);
#endif
	}
	else
	{
#ifndef GLQUAKE
		M_PrintWhite(16, 88, "         Go to console");
#else
		M_Print_GetPoint(16, 144, &lx, &ly, "         Go to console", options_cursor == 14);
#endif
	}

	options_window.w = 28 * 8; // presume 8 pixels for each letter
	options_window.h = ly - options_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (options_cursor == 2 || (!vid_menudrawfn && options_cursor == (OPTIONS_ITEMS - 3)) || options_cursor == (OPTIONS_ITEMS - 2))
		return;

	// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f();
			break;

		case 1:
			M_Menu_Mouse_f();
			break;

		case 3:
			M_Menu_Hud_f();
			break;

		case 4:
			M_Menu_Sound_f();
			break;

#ifndef GLQUAKE
		case 5:
			M_Menu_Misc_f();
			break;

		case 7:
			if (vid_menudrawfn)
				M_Menu_VideoModes_f();
			break;

		case 9:
			Con_ToggleConsole_f();
			break;
#else
		case 5:
			M_Menu_View_f();
			break;

		case 6:
			M_Menu_Renderer_f();
			break;

		case 7:
			M_Menu_Textures_f();
			break;

		case 8:
			M_Menu_Particles_f();
			break;

		case 9:
			M_Menu_Decals_f();
			break;

		case 10:
			M_Menu_Weapons_f();
			break;

		case 11:
			M_Menu_ScreenFlashes_f();
			break;

		case 12:
			M_Menu_Misc_f();
			break;

		case 13:
			if (vid_menudrawfn)
				M_Menu_VideoModes_f();
			break;

		case 15:
			Con_ToggleConsole_f();
			break;
#endif

		default:
			break;
		}
		return;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_LEFTARROW:
		break;

	case K_RIGHTARROW:
		break;
	}

	if (k == K_UPARROW || k == K_END || k == K_PGDN)
	{
		if (options_cursor == OPTIONS_ITEMS - 3 && !vid_menudrawfn)
			options_cursor = OPTIONS_ITEMS - 4;

		if (k == K_UPARROW && (options_cursor == 2 || options_cursor == (OPTIONS_ITEMS - 2)))
			options_cursor--;
	}
	else
	{
		if (options_cursor == OPTIONS_ITEMS - 3 && !vid_menudrawfn)
			options_cursor = OPTIONS_ITEMS - 2;

		if (k == K_DOWNARROW && (options_cursor == 2 || options_cursor == (OPTIONS_ITEMS - 2)))
			options_cursor++;
	}
}

qboolean M_Options_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&options_window, ms, OPTIONS_ITEMS, &options_cursor);

	if (ms->button_up == 1) M_Options_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Options_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"+jump", 			"jump"},
{"+forward", 		"move forward"},
{"+back", 			"move back"},
{"+moveleft", 		"move left"},
{"+moveright", 		"move right"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"},
{"impulse 12", 		"previous weapon"},
{"impulse 10", 		"next weapon"},
{"+speed", 			"run"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
// RuneQuake specific
{"+hook",			"grappling hook"},
{"rune-use",		"rune-use"},
{"rune-delete",		"rune-delete"},
{"rune-tell",		"rune-tell"},
{"+sattack6",		"quick grenade"},
{"messagemode2",	"team talk"}
};

#define	NUMCOMMANDS	17
#define	NUMRUNECOMMANDS	23

int	num_commands;
int	bind_grab;

menu_window_t keys_window;

void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;

	num_commands = runequake ? NUMRUNECOMMANDS : NUMCOMMANDS;
	list_base = list_cursor = 0;
}

void M_FindKeysForCommand (char *command, int *twokeys)
{
	int	count, j, l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		if (!(b = keybindings[j]))
			continue;
		if (!strncmp(b, command, l))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int	j, l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		if (!(b = keybindings[j]))
			continue;
		if (!strncmp(b, command, l))
			Key_Unbind (j);
	}
}

void M_Keys_Draw (void)
{
	int		i, l, keys[2], x, y, lx = 0, ly = 0;
	char	*name;
	mpic_t	*p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	if (bind_grab)
		M_Print(12, 32, "Press a key or button for this action");
	else
		M_Print(18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i = 0, y = 48 ; i < num_commands - list_base && i < MAXKEYLINES ; i++, y += 8)
	{
		if (i == 0)
			M_Print_GetPoint (16, y, &keys_window.x, &keys_window.y, bindnames[list_base+i][1], list_cursor == i);
		else
			M_Print_GetPoint(16, y, &lx, &ly, bindnames[list_base + i][1], list_cursor == i);

		l = strlen (bindnames[list_base+i][0]);

		M_FindKeysForCommand (bindnames[list_base+i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (156, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (156, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (156 + x + 8, y, "or");
				M_Print (156 + x + 32, y, Key_KeynumToString(keys[1]));
			}
		}
	}

	keys_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	keys_window.h = ly - keys_window.y + 8;

	if (bind_grab)
		M_DrawCharacter (142, 48 + list_cursor*8, '=');
	else
		M_DrawCharacter (142, 48 + list_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Keys_Key (int k)
{
	int	keys[2];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
			bind_grab = false;
		else if (k != '`')
			Key_SetBinding (k, bindnames[list_cursor][0]);

		bind_grab = false;
		return;
	}

	M_List_Key (k, num_commands, MAXKEYLINES);

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f ();
		break;

	case K_ENTER:		// go into bind mode
	case K_MOUSE1:
		M_FindKeysForCommand (bindnames[list_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[list_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:	// delete bindings
	case K_DEL:
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[list_cursor][0]);
		break;
	}
}

qboolean M_Keys_Mouse_Event(const mouse_state_t *ms)
{
	if (bind_grab)
		return false;

	M_Mouse_Select(&keys_window, ms, MAXKEYLINES, &list_cursor);

	if (ms->button_up == 1) M_Keys_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Keys_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* MOUSE OPTIONS MENU */

#define	MOUSE_ITEMS	11

int	mouse_cursor = 0;

menu_window_t mouse_window;
menu_window_t mouse_slider_sensitivity_window;
menu_window_t mouse_slider_cursor_sensitivity_window;
menu_window_t mouse_slider_cursor_scale_window;
menu_window_t mouse_slider_rate_window;

extern qboolean use_m_smooth;
extern cvar_t m_filter;
extern cvar_t m_rate;
extern cvar_t scr_cursor_scale;

#define MOUSE_RATE_ITEMS 8
float mouse_rate_values[MOUSE_RATE_ITEMS] = { 60, 125, 250, 500, 800, 1000, 1500, 2000 };

void M_Menu_Mouse_f(void)
{
	key_dest = key_menu;
	m_state = m_mouse;
	m_entersound = true;
}

void M_Mouse_Draw(void)
{
	float	r;
	mpic_t	*p;
	int		lx = 0, ly = 0;	// lower bounds of the window 

	//M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &mouse_window.x, &mouse_window.y, "   In-game sensitivity", mouse_cursor == 0);
	r = (sensitivity.value - 0.1) / (3 - 0.1);
	M_DrawSliderFloat(220, 32, r, sensitivity.value, &mouse_slider_sensitivity_window);

	M_Print_GetPoint(16, 40, &lx, &ly, "          Mouse filter", mouse_cursor == 1);
	M_DrawCheckbox(220, 40, m_filter.value);

	M_Print_GetPoint(16, 48, &lx, &ly, "          Invert mouse", mouse_cursor == 2);
	M_DrawCheckbox(220, 48, m_pitch.value < 0);

	M_Print_GetPoint(16, 64, &lx, &ly, "    Cursor sensitivity", mouse_cursor == 4);
	r = (cursor_sensitivity.value - 0.1) / (3 - 0.1);
	M_DrawSliderFloat(220, 64, r, cursor_sensitivity.value, &mouse_slider_cursor_sensitivity_window);

	M_Print_GetPoint(16, 72, &lx, &ly, "          Cursor scale", mouse_cursor == 5);
	r = (scr_cursor_scale.value - 0.1) / (2 - 0.1);
	M_DrawSliderFloat(220, 72, r, scr_cursor_scale.value, &mouse_slider_cursor_scale_window);

	M_Print_GetPoint(16, 88, &lx, &ly, "   Use mouse smoothing", mouse_cursor == 7);
	M_DrawCheckbox(220, 88, use_m_smooth);

	M_Print_GetPoint(16, 96, &lx, &ly, "            Mouse rate", mouse_cursor == 8);
	if (use_m_smooth)
	{
		//r = (m_rate.value - mouse_rate_values[0]) / (mouse_rate_values[MOUSE_RATE_ITEMS-1] - mouse_rate_values[0]);
		r = (float)FindSliderItemIndex(mouse_rate_values, MOUSE_RATE_ITEMS, &m_rate) / (MOUSE_RATE_ITEMS - 1);
		M_DrawSliderInt(220, 96, r, m_rate.value, &mouse_slider_rate_window);
	}
	else
	{
		M_Print(220, 96, "-");
	}

#ifdef _WIN32
	if (modestate == MS_WINDOWED)
#else
	if (vid_windowedmouse)
#endif
	{
		M_Print_GetPoint(-16, 112, &lx, &ly, "Use mouse in windowed mode", mouse_cursor == 10);
		M_DrawCheckbox(220, 112, _windowed_mouse.value);
	}

	mouse_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	mouse_window.h = ly - mouse_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (mouse_cursor == 3 || mouse_cursor == 6 || mouse_cursor == 9)
		return;

	// cursor
	M_DrawCharacter(200, 32 + mouse_cursor * 8, 12 + ((int)(realtime * 4) & 1));

	if (mouse_cursor == 7)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Mouse smoothing must be set from the");
		M_Print(2 * 8, 176 + 8 * 4, "command line with -dinput and -m_smooth");
	}
}

void M_Mouse_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (mouse_cursor)
	{
	case 0:	// in-game sensitivity
		sensitivity.value += dir * 0.1;
		sensitivity.value = bound(0.1, sensitivity.value, 3);
		Cvar_SetValue(&sensitivity, sensitivity.value);
		break;

	case 4:	// menu cursor sensitivity
		cursor_sensitivity.value += dir * 0.1;
		cursor_sensitivity.value = bound(0.1, cursor_sensitivity.value, 3);
		Cvar_SetValue(&cursor_sensitivity, cursor_sensitivity.value);
		break;

	case 5:	// menu cursor scale
		scr_cursor_scale.value += dir * 0.1;
		scr_cursor_scale.value = bound(0.1, scr_cursor_scale.value, 2);
		Cvar_SetValue(&scr_cursor_scale, scr_cursor_scale.value);
		break;

	case 8:	// mouse rate
		if (use_m_smooth)
			AdjustSliderBasedOnArrayOfValues(dir, mouse_rate_values, MOUSE_RATE_ITEMS, &m_rate);
		break;
	}
}

void M_Mouse_Key(int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (mouse_cursor)
		{
		case 1:	// mouse filter
			Cvar_SetValue(&m_filter, !m_filter.value);
			break;

		case 2:	// invert mouse
			Cvar_SetValue(&m_pitch, -m_pitch.value);
			break;

		case 7:	// mouse smoothing
			break;

		case 10:// _windowed_mouse
			Cvar_SetValue(&_windowed_mouse, !_windowed_mouse.value);
			break;

		default:
			break;
		}
		return;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		mouse_cursor--;
		if (mouse_cursor < 0)
			mouse_cursor = MOUSE_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		mouse_cursor++;
		if (mouse_cursor >= MOUSE_ITEMS)
			mouse_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		mouse_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		mouse_cursor = MOUSE_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Mouse_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Mouse_KeyboardSlider(1);
		break;
	}

	if (k == K_UPARROW || k == K_END || k == K_PGDN)
	{
		if (mouse_cursor == MOUSE_ITEMS - 1
#ifdef _WIN32
			&& modestate != MS_WINDOWED
#else
			&& !vid_windowedmouse
#endif
			)
		{
			mouse_cursor = MOUSE_ITEMS - 3;
		}

		if (k == K_UPARROW && (mouse_cursor == 3 || mouse_cursor == 6 || mouse_cursor == 9))
			mouse_cursor--;
	}
	else
	{
		if (k == K_DOWNARROW && (mouse_cursor == 3 || mouse_cursor == 6 || mouse_cursor == 9))
			mouse_cursor++;

		if (mouse_cursor == MOUSE_ITEMS - 1
#ifdef _WIN32
			&& modestate != MS_WINDOWED
#else
			&& !vid_windowedmouse
#endif
			)
		{
			mouse_cursor = 0;
		}
	}
}

void M_Mouse_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;
	
	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (mouse_cursor)
		{
		case 0:	// in-game sensitivity
			M_Mouse_Select_Column(&mouse_slider_sensitivity_window, ms, 30, &slider_pos);
			sensitivity.value = bound(0.1, (slider_pos * 0.1) + 0.1, 3);
			Cvar_SetValue(&sensitivity, sensitivity.value);
			break;

		case 4:	// menu cursor sensitivity
			M_Mouse_Select_Column(&mouse_slider_cursor_sensitivity_window, ms, 30, &slider_pos);
			cursor_sensitivity.value = bound(0.1, (slider_pos * 0.1) + 0.1, 3);
			Cvar_SetValue(&cursor_sensitivity, cursor_sensitivity.value);
			break;

		case 5:	// menu cursor scale
			M_Mouse_Select_Column(&mouse_slider_cursor_scale_window, ms, 20, &slider_pos);
			scr_cursor_scale.value = bound(0.1, (slider_pos * 0.1) + 0.1, 2);
			Cvar_SetValue(&scr_cursor_scale, scr_cursor_scale.value);
			break;

		case 6:	// mouse rate
			if (use_m_smooth)
			{
				M_Mouse_Select_Column(&mouse_slider_rate_window, ms, MOUSE_RATE_ITEMS, &slider_pos);
				Cvar_SetValue(&m_rate, mouse_rate_values[slider_pos]);
			}
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Mouse_Mouse_Event(const mouse_state_t *ms)
{
	int mouse_items = MOUSE_ITEMS;

#ifdef _WIN32
	if (modestate != MS_WINDOWED)
#else
	if (!vid_windowedmouse)
#endif
		mouse_items = MOUSE_ITEMS - 2;

	M_Mouse_Select(&mouse_window, ms, mouse_items, &mouse_cursor);

	if (ms->button_up == 1) M_Mouse_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Mouse_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Mouse_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* MISC OPTIONS MENU */

#define	MISC_ITEMS	11

int	misc_cursor = 0;

menu_window_t misc_window;
menu_window_t misc_slider_demospeed_window;

#define DEMO_SPEED_ITEMS 6
float demo_speed_values[DEMO_SPEED_ITEMS] = { 0.125, 0.25, 0.5, 1, 2, 4 };

void M_Menu_Misc_f(void)
{
	key_dest = key_menu;
	m_state = m_misc;
	m_entersound = true;
}

void M_Misc_Draw(void)
{
	int		lx, ly;
	float	r;
	mpic_t *p;

	//M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &misc_window.x, &misc_window.y, "            Always Run", misc_cursor == 0);
	M_DrawCheckbox (220, 32, cl_forwardspeed.value > 200);

	M_Print_GetPoint(16, 40, &lx, &ly, "    Power Bunnyhopping", misc_cursor == 1);
	M_DrawCheckbox(220, 40, cl_forwardspeed.value == 200 && (in_speed.state & 1));

	M_Print_GetPoint(16, 48, &lx, &ly, "        Demo player UI", misc_cursor == 2);
	M_DrawCheckbox(220, 48, cl_demoui.value);

	M_Print_GetPoint(16, 56, &lx, &ly, "   Demo playback speed", misc_cursor == 3);
	//r = (cl_demospeed.value - demo_speed_values[0]) / (demo_speed_values[DEMO_SPEED_ITEMS-1] - demo_speed_values[0]);
	r = (float)FindSliderItemIndex(demo_speed_values, DEMO_SPEED_ITEMS, &cl_demospeed) / (DEMO_SPEED_ITEMS - 1);
	M_DrawSliderFloat2(220, 56, r, cl_demospeed.value, &misc_slider_demospeed_window);

	M_Print_GetPoint(0, 64, &lx, &ly, "Automatic demo recording", misc_cursor == 4);
	M_DrawCheckbox(220, 64, cl_autodemo.value);

	M_Print_GetPoint(-8, 72, &lx, &ly, "Precise intermission time", misc_cursor == 5);
	M_DrawCheckbox(220, 72, scr_precisetime.value);

	M_Print_GetPoint(16, 88, &lx, &ly, "            Aim assist", misc_cursor == 7);
	M_DrawCheckbox(220, 88, sv_aim.value < 1);

	M_Print_GetPoint(-24, 96, &lx, &ly, "Advanced command completion", misc_cursor == 8);
	M_DrawCheckbox(220, 96, cl_advancedcompletion.value);

	M_Print_GetPoint(16, 104, &lx, &ly, "      Cvar saving mode", misc_cursor == 9);
	M_Print(220, 104, !cvar_savevars.value ? "default" : cvar_savevars.value == 2 ? "save all" : "save modified");

	M_Print_GetPoint(16, 112, &lx, &ly, "     Confirm when quit", misc_cursor == 10);
	M_DrawCheckbox(220, 112, cl_confirmquit.value);

	misc_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	misc_window.h = ly - misc_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (misc_cursor == 6)
		return;

	// cursor
	M_DrawCharacter(200, 32 + misc_cursor * 8, 12 + ((int)(realtime * 4) & 1));

	if (misc_cursor == 8)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Shows a list of relevant commands when");
		M_Print(2 * 8, 176 + 8 * 4, "pressing the TAB key for completion");
	}
	else if (misc_cursor == 9)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Defines which console variables");
		M_Print(2 * 8, 176 + 8 * 4, "are saved when exiting the game");
	}
	else if (misc_cursor == 10)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Shows a confirmation screen");
		M_Print(2 * 8, 176 + 8 * 4, "when exiting the game");
	}
}

void M_Misc_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (misc_cursor)
	{
	case 3:	// demo speed
		AdjustSliderBasedOnArrayOfValues(dir, demo_speed_values, DEMO_SPEED_ITEMS, &cl_demospeed);
		break;

	default:
		break;
	}
}

void M_Misc_Key(int k)
{
	float newvalue;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (misc_cursor)
		{
		case 0:	// always run
			if (cl_forwardspeed.value > 200)
			{
				Cvar_SetValue (&cl_forwardspeed, 200);
				Cvar_SetValue (&cl_backspeed, 200);
			}
			else
			{
				Cvar_SetValue (&cl_forwardspeed, 400);
				Cvar_SetValue (&cl_backspeed, 400);
			}
			break;

		case 1:	// power bunnyhopping
			if (!(cl_forwardspeed.value == 200 && (in_speed.state & 1)))
			{
				Cvar_SetValue(&cl_forwardspeed, 200);
				Cvar_SetValue(&cl_backspeed, 200);
				Cmd_ExecuteString("+speed", src_command);
			}
			else
			{
				Cmd_ExecuteString("-speed", src_command);
			}
			break;

		case 2:
			Cvar_SetValue(&cl_demoui, !cl_demoui.value);
			break;

		case 4:
			Cvar_SetValue(&cl_autodemo, !cl_autodemo.value);
			break;

		case 5:
			Cvar_SetValue(&scr_precisetime, !scr_precisetime.value);
			break;

		case 7:
			if (sv_aim.value == 1)
				Cvar_SetValue (&sv_aim, 0.93);
			else
				Cvar_SetValue (&sv_aim, 1);
			break;

		case 8:
			Cvar_SetValue(&cl_advancedcompletion, !cl_advancedcompletion.value);
			break;

		case 9:
			newvalue = cvar_savevars.value + 1;
			if (newvalue > 2)
				newvalue = 0;
			Cvar_SetValue(&cvar_savevars, newvalue);
			break;

		case 10:
			Cvar_SetValue(&cl_confirmquit, !cl_confirmquit.value);
			break;

		default:
			break;
		}
		return;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		misc_cursor--;
		if (misc_cursor < 0)
			misc_cursor = MISC_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		misc_cursor++;
		if (misc_cursor >= MISC_ITEMS)
			misc_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		misc_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		misc_cursor = MISC_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Misc_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Misc_KeyboardSlider(1);
		break;
	}

	if (k == K_UPARROW && misc_cursor == 6)
		misc_cursor--;
	else if (k == K_DOWNARROW && misc_cursor == 6)
		misc_cursor++;
}

void M_Misc_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (misc_cursor)
		{
		case 3:	// demo speed
			M_Mouse_Select_Column(&misc_slider_demospeed_window, ms, DEMO_SPEED_ITEMS, &slider_pos);
			Cvar_SetValue(&cl_demospeed, demo_speed_values[slider_pos]);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Misc_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&misc_window, ms, MISC_ITEMS, &misc_cursor);

	if (ms->button_up == 1) M_Misc_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Misc_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Misc_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* HUD OPTIONS MENU */

#define	HUD_ITEMS	22

int	hud_cursor = 0;

menu_window_t hud_window;
menu_window_t hud_slider_sbarscale_window;
menu_window_t hud_slider_crosshairsize_window;
menu_window_t hud_slider_crosshairalpha_window;
menu_window_t hud_slider_consize_window;
menu_window_t hud_slider_conspeed_window;
menu_window_t hud_slider_conalpha_window;

const float scr_sbarscale_amount_max = 6.0f;

#define CONSOLE_SPEED_ITEMS 5
float console_speed_values[CONSOLE_SPEED_ITEMS] = { 200, 500, 1000, 5000, 99999 };

void DrawHudType(int x, int y)
{
	char *str;

	switch ((int)(cl_sbar.value))
	{
	case 0:
		str = "transparent";
		break;

	case 1:
		str = "original";
		break;

	case 2:
		str = "alternative";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawCrosshairType(int x, int y)
{
	char *str;

	switch ((int)(crosshair.value))
	{
	case 0:
		str = "off";
		break;

	case 2:
		str = "type 1";
		break;

	case 3:
		str = "type 2";
		break;

	case 4:
		str = "type 3";
		break;

	case 5:
		str = "type 4";
		break;

	case 6:
		str = "type 5";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawClockType(int x, int y)
{
	char *str;

	switch ((int)(scr_clock.value))
	{
	case 0:
		str = "off";
		break;

	case 1:
		str = "type 1";
		break;

	case 2:
		str = "type 2";
		break;

	case 3:
		str = "type 3";
		break;

	case 4:
		str = "type 4";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawStatsType(int x, int y)
{
	char *str;

	switch ((int)(show_stats.value))
	{
	case 0:
		str = "off";
		break;

	case 1:
		str = "time only";
		break;

	case 2:
		str = "full";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void SearchForCrosshairs(void)
{
	searchpath_t	*search;

	EraseDirEntries();
	pak_files = 0;

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!search->pack)
		{
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);
			ReadDir(va("%s/crosshairs", search->filename), "*.tga");
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);
			ReadDir(va("%s/crosshairs", search->filename), "*.png");
		}
	}
	FindFilesInPak("crosshairs/*.tga");
	FindFilesInPak("crosshairs/*.png");
}

void M_Menu_Hud_f(void)
{
	key_dest = key_menu;
	m_state = m_hud;
	m_entersound = true;

	SearchForCrosshairs();
}

void M_Hud_Draw(void)
{
	int x, y, lx = 0, ly = 0;
	float r;
	mpic_t *p;
#ifdef GLQUAKE
	byte *col;
#endif

	//M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &hud_window.x, &hud_window.y, "     Hud/Console scale", hud_cursor == 0);
	r = (scr_sbarscale_amount.value - 1) / (scr_sbarscale_amount_max - 1.0f);
	M_DrawSliderFloat(220, 32, r, scr_sbarscale_amount.value, &hud_slider_sbarscale_window);

	M_Print_GetPoint(16, 40, &lx, &ly, "             Hud style", hud_cursor == 1);
	DrawHudType(220, 40);

	M_Print_GetPoint(16, 56, &lx, &ly, "        Crosshair type", hud_cursor == 3);
#ifdef GLQUAKE
	if (draw_no24bit)
#endif
		DrawCrosshairType(220, 56);
#ifdef GLQUAKE
	else
		M_Print(220, 56, !strcmp(gl_crosshairimage.string, "") ? "off" : gl_crosshairimage.string);
#endif

	M_Print_GetPoint(16, 64, &lx, &ly, "       Crosshair color", hud_cursor == 4);
	x = 220 + ((menuwidth - 320) >> 1);
	y = 64 + m_yofs;
#ifdef GLQUAKE
	col = StringToRGB(crosshaircolor.string);
	glDisable(GL_TEXTURE_2D);
	glColor3ubv(col);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + 32, y);
	glVertex2f(x + 32, y + 8);
	glVertex2f(x, y + 8);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);
#endif

	M_Print_GetPoint(16, 72, &lx, &ly, "       Crosshair scale", hud_cursor == 5);
	r = crosshairsize.value / 3;
	M_DrawSliderFloat(220, 72, r, crosshairsize.value, &hud_slider_crosshairsize_window);

	M_Print_GetPoint(16, 80, &lx, &ly, "Crosshair transparency", hud_cursor == 6);
#ifdef GLQUAKE
	M_DrawSliderFloat(220, 80, gl_crosshairalpha.value, gl_crosshairalpha.value, &hud_slider_crosshairalpha_window);
#endif

	M_DrawTextBox(180, 88, 3, 3);
	Draw_Crosshair(true);

	M_Print_GetPoint(16, 128, &lx, &ly, "              Show FPS", hud_cursor == 12);
	M_DrawCheckbox(220, 128, show_fps.value);

	M_Print_GetPoint(16, 136, &lx, &ly, "            Show clock", hud_cursor == 13);
	DrawClockType(220, 136);

	M_Print_GetPoint(16, 144, &lx, &ly, "            Show speed", hud_cursor == 14);
	M_DrawCheckbox(220, 144, show_speed.value);

	M_Print_GetPoint(8, 152, &lx, &ly, "Show time/kills/secrets", hud_cursor == 15);
	DrawStatsType(220, 152);

	M_Print_GetPoint(16, 160, &lx, &ly, "   Show stats in small", hud_cursor == 16);
	M_DrawCheckbox(220, 160, show_stats_small.value);

	M_Print_GetPoint(16, 168, &lx, &ly, "    Show movement keys", hud_cursor == 17);
	M_DrawCheckbox(220, 168, show_movekeys.value);

	M_Print_GetPoint(16, 184, &lx, &ly, "        Console height", hud_cursor == 19);
	M_DrawSliderFloat(220, 184, scr_consize.value, scr_consize.value, &hud_slider_consize_window);

	M_Print_GetPoint(16, 192, &lx, &ly, "         Console speed", hud_cursor == 20);
	r = (float)FindSliderItemIndex(console_speed_values, CONSOLE_SPEED_ITEMS, &scr_conspeed) / (CONSOLE_SPEED_ITEMS - 1);
	M_DrawSliderInt(220, 192, r, scr_conspeed.value, &hud_slider_conspeed_window);

	M_Print_GetPoint(16, 200, &lx, &ly, "  Console transparency", hud_cursor == 21);
#ifdef GLQUAKE
	M_DrawSliderFloat(220, 200, gl_conalpha.value, gl_conalpha.value, &hud_slider_conalpha_window);
#endif

	hud_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	hud_window.h = ly - hud_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (hud_cursor == 2 || hud_cursor == 18 || (hud_cursor >= 7 && hud_cursor <= 11))
		return;

	// cursor
	M_DrawCharacter(200, 32 + hud_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Hud_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (hud_cursor)
	{
	case 0:	// sbar scale
		scr_sbarscale_amount.value += dir * 0.5;
		scr_sbarscale_amount.value = bound(1, scr_sbarscale_amount.value, scr_sbarscale_amount_max);
		Cvar_SetValue(&scr_sbarscale_amount, scr_sbarscale_amount.value);
		break;

	case 5:	// crosshair scale
		crosshairsize.value += dir * 0.5;
		crosshairsize.value = bound(0, crosshairsize.value, 3);
		Cvar_SetValue(&crosshairsize, crosshairsize.value);
		break;

	case 6:	// crosshair alpha
#ifdef GLQUAKE
		gl_crosshairalpha.value += dir * 0.1;
		gl_crosshairalpha.value = bound(0, gl_crosshairalpha.value, 1);
		Cvar_SetValue(&gl_crosshairalpha, gl_crosshairalpha.value);
#endif
		break;

	case 19:// console height
		scr_consize.value += dir * 0.1;
		scr_consize.value = bound(0, scr_consize.value, 1);
		Cvar_SetValue(&scr_consize, scr_consize.value);
		break;

	case 20: // console speed
		AdjustSliderBasedOnArrayOfValues(dir, console_speed_values, CONSOLE_SPEED_ITEMS, &scr_conspeed);
		break;

	case 21:// console alpha
#ifdef GLQUAKE
		gl_conalpha.value += dir * 0.1;
		gl_conalpha.value = bound(0, gl_conalpha.value, 1);
		Cvar_SetValue(&gl_conalpha, gl_conalpha.value);
#endif
		break;

	default:
		break;
	}
}

void M_Hud_Key(int k)
{
	float newvalue;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (hud_cursor)
		{
		case 1:	// cl_sbar
			newvalue = cl_sbar.value + 1;
			if (newvalue > 2)
				newvalue = 0;
			Cvar_SetValue(&cl_sbar, newvalue);
			break;

		case 3: // crosshair
#ifdef GLQUAKE
			if (draw_no24bit)
#endif
			{
				newvalue = crosshair.value + 1;
				if (newvalue == 1)	// skip the good old '+' character crosshair
					newvalue++;
				if (newvalue > 6)
					newvalue = 0;
				Cvar_SetValue (&crosshair, newvalue);
			}
#ifdef GLQUAKE
			else
			{
				if (num_files > 0)
				{
					int i;
					char *crosshairimage;
					direntry_t *crosshairfile;

					for (i = 0, crosshairfile = filelist; i < num_files; i++, crosshairfile++)
					{
						if (!strcmp(crosshairfile->name, gl_crosshairimage.string))
						{
							if (i == num_files - 1)
								crosshairimage = "";
							else
							{
								crosshairfile++;
								crosshairimage = crosshairfile->name;
							}
							break;
						}
					}
					if (i == num_files)
						crosshairimage = filelist->name;
					Cvar_Set(&gl_crosshairimage, crosshairimage);
					if (!strcmp(crosshairimage, ""))
						Cvar_SetValue(&crosshair, 0);
				}
			}
#endif
			break;
		
		case 4:
			M_Menu_Crosshair_ColorChooser_f();
			break;

		case 12: // fps
			Cvar_SetValue(&show_fps, !show_fps.value);
			break;

		case 13: // clock
			newvalue = scr_clock.value + 1;
			if (newvalue > 4)
				newvalue = 0;
			Cvar_SetValue(&scr_clock, newvalue);
			break;

		case 14: // speed
			Cvar_SetValue(&show_speed, !show_speed.value);
			break;

		case 15: // time, kills, secrets
			newvalue = show_stats.value + 1;
			if (newvalue > 2)
				newvalue = 0;
			Cvar_SetValue(&show_stats, newvalue);
			break;

		case 16: // time, kills, secrets in small
			Cvar_SetValue(&show_stats_small, !show_stats_small.value);
			break;

		case 17: // movement keys
			Cvar_SetValue(&show_movekeys, !show_movekeys.value);
			break;

		default:
			break;
		}
		return;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		hud_cursor--;
		if (hud_cursor < 0)
			hud_cursor = HUD_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		hud_cursor++;
		if (hud_cursor >= HUD_ITEMS)
			hud_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		hud_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		hud_cursor = HUD_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Hud_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Hud_KeyboardSlider(1);
		break;
	}

	if (k == K_UPARROW)
	{
		if (hud_cursor == 2 || hud_cursor == 18)
			hud_cursor--;
		else if (hud_cursor >= 7 && hud_cursor <= 11)
			hud_cursor -= 5;
	}
	else if (k == K_DOWNARROW)
	{
		if (hud_cursor == 2 || hud_cursor == 18)
			hud_cursor++;
		else if (hud_cursor >= 7 && hud_cursor <= 11)
			hud_cursor += 5;
	}
}

void M_Hud_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (hud_cursor)
		{
		case 0:	// sbar scale
			M_Mouse_Select_Column(&hud_slider_sbarscale_window, ms, 11, &slider_pos);
			scr_sbarscale_amount.value = bound(1, (slider_pos * 0.5) + 1, scr_sbarscale_amount_max);
			Cvar_SetValue(&scr_sbarscale_amount, scr_sbarscale_amount.value);
			break;

		case 5:	// crosshair scale
			M_Mouse_Select_Column(&hud_slider_crosshairsize_window, ms, 7, &slider_pos);
			crosshairsize.value = bound(0, slider_pos * 0.5, 3);
			Cvar_SetValue(&crosshairsize, crosshairsize.value);
			break;

		case 6:	// crosshair alpha
#ifdef GLQUAKE
			M_Mouse_Select_Column(&hud_slider_crosshairalpha_window, ms, 11, &slider_pos);
			gl_crosshairalpha.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&gl_crosshairalpha, gl_crosshairalpha.value);
#endif
			break;

		case 19:// console height
			M_Mouse_Select_Column(&hud_slider_consize_window, ms, 11, &slider_pos);
			scr_consize.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&scr_consize, scr_consize.value);
			break;

		case 20: // console speed
			M_Mouse_Select_Column(&hud_slider_conspeed_window, ms, CONSOLE_SPEED_ITEMS, &slider_pos);
			Cvar_SetValue(&scr_conspeed, console_speed_values[slider_pos]);
			break;

		case 21:// console alpha
#ifdef GLQUAKE
			M_Mouse_Select_Column(&hud_slider_conalpha_window, ms, 11, &slider_pos);
			gl_conalpha.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&gl_conalpha, gl_conalpha.value);
#endif
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Hud_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&hud_window, ms, HUD_ITEMS, &hud_cursor);

	if (ms->button_up == 1) M_Hud_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Hud_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Hud_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* COLOR CHOOSER MENU */

typedef enum 
{ 
	cs_crosshair, cs_sky, cs_outline 
} colorchooser_t;

#define	COLORCHOOSER_ITEMS			5
#define	COLORCHOOSER_PALETTE_SIZE	16

int red, green, blue;
int	colorchooser_cursor = 0;
int	colorchooser_palette_row, colorchooser_palette_column;
qboolean is_palette_selected;

menu_window_t colorchooser_window;
menu_window_t colorchooser_slider_red_window;
menu_window_t colorchooser_slider_green_window;
menu_window_t colorchooser_slider_blue_window;
menu_window_t colorchooser_palette_window;

void M_Menu_Crosshair_ColorChooser_f(void)
{
#ifdef GLQUAKE
	byte *col;

	key_dest = key_menu;
	m_state = m_crosshair_colorchooser;
	m_entersound = true;
	colorchooser_cursor = 0;

	col = StringToRGB(crosshaircolor.string);
	red = col[0];
	green = col[1];
	blue = col[2];
#endif
}

void M_Menu_Sky_ColorChooser_f(void)
{
#ifdef GLQUAKE
	byte *col;

	key_dest = key_menu;
	m_state = m_sky_colorchooser;
	m_entersound = true;
	colorchooser_cursor = 0;

	col = StringToRGB(r_skycolor.string);
	red = col[0];
	green = col[1];
	blue = col[2];
#endif
}

void M_Menu_Outline_ColorChooser_f(void)
{
	byte *col;

	key_dest = key_menu;
	m_state = m_outline_colorchooser;
	m_entersound = true;
	colorchooser_cursor = 0;

	col = StringToRGB(r_outline_color.string);
	red = col[0];
	green = col[1];
	blue = col[2];
}

void M_ColorChooser_Draw(colorchooser_t cstype)
{
	mpic_t *p;
	float r;
	char title[MAX_QPATH];
	int x, y, square_size = 48, lx = 0, ly = 0;
#ifdef GLQUAKE
	byte *col, *palcol;
#endif

	switch (cstype)
	{
	case cs_crosshair:
		sprintf(title, "Choose crosshair color");
#ifdef GLQUAKE
		col = StringToRGB(crosshaircolor.string);
#endif
		break;

	case cs_sky:
		sprintf(title, "Choose sky color");
#ifdef GLQUAKE
		col = StringToRGB(r_skycolor.string);
#endif
		break;

	case cs_outline:
		sprintf(title, "Choose outline color");
		col = StringToRGB(r_outline_color.string);
		break;

	default:
		break;
	}

	//M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print(16, 32, title);

	M_Print_GetPoint(16, 48, &colorchooser_window.x, &colorchooser_window.y, "           Red", colorchooser_cursor == 0);
	r = red / 255.0f;
	M_DrawSliderInt(156, 48, r, red, &colorchooser_slider_red_window);

	M_Print_GetPoint(16, 56, &lx, &ly, "         Green", colorchooser_cursor == 1);
	r = green / 255.0f;
	M_DrawSliderInt(156, 56, r, green, &colorchooser_slider_green_window);

	M_Print_GetPoint(16, 64, &lx, &ly, "          Blue", colorchooser_cursor == 2);
	r = blue / 255.0f;
	M_DrawSliderInt(156, 64, r, blue, &colorchooser_slider_blue_window);

	M_Print_GetPoint(16, 80, &lx, &ly, "Undo changes", colorchooser_cursor == 4);

	colorchooser_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	colorchooser_window.h = ly - colorchooser_window.y + 8;

	glDisable(GL_TEXTURE_2D);
	y = 96 + m_yofs;
	for (int i = 0; i < COLORCHOOSER_PALETTE_SIZE; i++, y += 8)
	{
		x = 16 + ((menuwidth - 320) >> 1);
		for (int j = 0; j < COLORCHOOSER_PALETTE_SIZE; j++, x += 8)
		{
			palcol = (byte *)&d_8to24table[j+(i*COLORCHOOSER_PALETTE_SIZE)];
			glColor3ub(palcol[0], palcol[1], palcol[2]);
			palcol += 3;
			glBegin(GL_QUADS);
			glVertex2f(x, y);
			glVertex2f(x + 8, y);
			glVertex2f(x + 8, y + 8);
			glVertex2f(x, y + 8);
			glEnd();
		}
	}
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);
	colorchooser_palette_window.x = 16 + ((menuwidth - 320) >> 1);
	colorchooser_palette_window.y = 96 + m_yofs;
	colorchooser_palette_window.w = 
	colorchooser_palette_window.h = COLORCHOOSER_PALETTE_SIZE * 8;

	M_Print(176, 96, "Old");
	x = 176 + ((menuwidth - 320) >> 1);
	y = 104 + m_yofs;
	glDisable(GL_TEXTURE_2D);
	glColor3ubv(col);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + square_size, y);
	glVertex2f(x + square_size, y + square_size);
	glVertex2f(x, y + square_size);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);

	M_Print(176 + square_size, 96, "New");
	x = 176 + square_size + ((menuwidth - 320) >> 1);
	y = 104 + m_yofs;
	glDisable(GL_TEXTURE_2D);
	glColor3ub(red, green, blue);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + square_size, y);
	glVertex2f(x + square_size, y + square_size);
	glVertex2f(x, y + square_size);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);

	//M_DebugWindow(&colorchooser_palette_window);

	// don't draw cursor if we're on a spacing line
	if (colorchooser_cursor == 3)
		return;

	// cursor
	M_DrawCharacter(136, 48 + colorchooser_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_ColorChooser_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (colorchooser_cursor)
	{
	case 0:
		red += dir * 15;
		red = bound(0, red, 255);
		break;

	case 1:
		green += dir * 15;
		green = bound(0, green, 255);
		break;

	case 2:
		blue += dir * 15;
		blue = bound(0, blue, 255);
		break;

	default:
		break;
	}
}

void M_ColorChooser_Key(int k, colorchooser_t cstype)
{
	char color[MAX_QPATH];
	byte *col, *palcol;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		sprintf(color, "%i %i %i", red, green, blue);
		switch (cstype)
		{
		case cs_crosshair:
			Cvar_Set(&crosshaircolor, color);
			M_Menu_Hud_f();
			break;

		case cs_sky:
#ifdef GLQUAKE
			Cvar_Set(&r_skycolor, color);
			M_Menu_View_f();
#endif
			break;

		case cs_outline:
			Cvar_Set(&r_outline_color, color);
			M_Menu_View_f();
			break;

		default:
			break;
		}
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		if (is_palette_selected)
		{
			palcol = (byte *)&d_8to24table[colorchooser_palette_column+(colorchooser_palette_row*COLORCHOOSER_PALETTE_SIZE)];
			red = palcol[0];
			green = palcol[1];
			blue = palcol[2];
		}
		else
		{
			switch (colorchooser_cursor)
			{
			case 4:
				sprintf(color, "%i %i %i", red, green, blue);
				switch (cstype)
				{
				case cs_crosshair:
					col = StringToRGB(crosshaircolor.string);
					red = col[0];
					green = col[1];
					blue = col[2];
					break;

				case cs_sky:
#ifdef GLQUAKE
					col = StringToRGB(r_skycolor.string);
					red = col[0];
					green = col[1];
					blue = col[2];
#endif
					break;

				case cs_outline:
					col = StringToRGB(r_outline_color.string);
					red = col[0];
					green = col[1];
					blue = col[2];
					break;

				default:
					break;
				}
				break;

			default:
				break;
			}
		}
		return;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		colorchooser_cursor--;
		if (colorchooser_cursor < 0)
			colorchooser_cursor = COLORCHOOSER_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		colorchooser_cursor++;
		if (colorchooser_cursor >= COLORCHOOSER_ITEMS)
			colorchooser_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		colorchooser_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		colorchooser_cursor = COLORCHOOSER_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_ColorChooser_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_ColorChooser_KeyboardSlider(1);
		break;
	}

	if (k == K_UPARROW && colorchooser_cursor == 3)
		colorchooser_cursor--;
	else if (k == K_DOWNARROW && colorchooser_cursor == 3)
		colorchooser_cursor++;
}

void M_ColorChooser_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (colorchooser_cursor)
		{
		case 0:
			M_Mouse_Select_Column(&colorchooser_slider_red_window, ms, 18, &slider_pos);
			red = bound(0, slider_pos * 15, 255);
			break;

		case 1:
			M_Mouse_Select_Column(&colorchooser_slider_green_window, ms, 18, &slider_pos);
			green = bound(0, slider_pos * 15, 255);
			break;

		case 2:
			M_Mouse_Select_Column(&colorchooser_slider_blue_window, ms, 18, &slider_pos);
			blue = bound(0, slider_pos * 15, 255);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_ColorChooser_Mouse_Event(const mouse_state_t *ms, colorchooser_t cstype)
{
	is_palette_selected = M_Mouse_Select_RowColumn(&colorchooser_palette_window, ms, COLORCHOOSER_PALETTE_SIZE, &colorchooser_palette_row, COLORCHOOSER_PALETTE_SIZE, &colorchooser_palette_column);
	if (!is_palette_selected)
		M_Mouse_Select(&colorchooser_window, ms, COLORCHOOSER_ITEMS, &colorchooser_cursor);

	if (ms->button_up == 1) M_ColorChooser_Key(K_MOUSE1, cstype);
	if (ms->button_up == 2) M_ColorChooser_Key(K_MOUSE2, cstype);

	if (ms->buttons[1]) M_ColorChooser_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* SOUND OPTIONS MENU */

#define	SOUND_ITEMS	3

int	sound_cursor = 0;

menu_window_t sound_window;
menu_window_t sound_slider_volume_window;
menu_window_t sound_slider_bgmvolume_window;

void M_Menu_Sound_f(void)
{
	key_dest = key_menu;
	m_state = m_sound;
	m_entersound = true;
}

void M_Sound_Draw(void)
{
	mpic_t	*p;
	char	sound_quality[10];
	int		lx = 0, ly = 0;

	//M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &sound_window.x, &sound_window.y, "          Sound volume", sound_cursor == 0);
	M_DrawSliderFloat(220, 32, s_volume.value, s_volume.value, &sound_slider_volume_window);

	M_Print_GetPoint(16, 40, &lx, &ly, "          Music volume", sound_cursor == 1);
	M_DrawSliderFloat(220, 40, bgmvolume.value, bgmvolume.value, &sound_slider_bgmvolume_window);

	M_Print_GetPoint(16, 48, &lx, &ly, "         Sound quality", sound_cursor == 2);
	sprintf(sound_quality, "%s KHz", s_khz.string);
	M_Print(220, 48, sound_quality);

	sound_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	sound_window.h = ly - sound_window.y + 8;

	// cursor
	M_DrawCharacter(200, 32 + sound_cursor * 8, 12 + ((int)(realtime * 4) & 1));

	if (sound_cursor == 2)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Sound quality must be set from the");
		M_Print(2 * 8, 176 + 8 * 4, "command line with +set s_khz <22 or 44>");
	}
}

void M_Sound_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (sound_cursor)
	{
	case 0:	// sfx volume
		s_volume.value += dir * 0.1;
		s_volume.value = bound(0, s_volume.value, 1);
		Cvar_SetValue(&s_volume, s_volume.value);
		break;

	case 1:	// bgm volume
		bgmvolume.value += dir * 0.1;
		bgmvolume.value = bound(0.0f, bgmvolume.value, 1.0f);
		Cvar_SetValue(&bgmvolume, bgmvolume.value);
		break;
	}
}

void M_Sound_Key(int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (sound_cursor)
		{
		case 2:
			break;

		default:
			break;
		}
		return;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		sound_cursor--;
		if (sound_cursor < 0)
			sound_cursor = SOUND_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		sound_cursor++;
		if (sound_cursor >= SOUND_ITEMS)
			sound_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		sound_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		sound_cursor = SOUND_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Sound_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Sound_KeyboardSlider(1);
		break;
	}
}

void M_Sound_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (sound_cursor)
		{
		case 0:	// sfx volume
			M_Mouse_Select_Column(&sound_slider_volume_window, ms, 11, &slider_pos);
			s_volume.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&s_volume, s_volume.value);
			break;

		case 1:	// bgm volume
			M_Mouse_Select_Column(&sound_slider_bgmvolume_window, ms, 11, &slider_pos);
			bgmvolume.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&bgmvolume, bgmvolume.value);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Sound_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&sound_window, ms, SOUND_ITEMS, &sound_cursor);

	if (ms->button_up == 1) M_Sound_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Sound_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Sound_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* VIEW OPTIONS MENU */

#ifdef GLQUAKE

#define	VIEW_ITEMS	19

int	view_cursor = 0;

menu_window_t view_window;
menu_window_t view_slider_viewsize_window;
menu_window_t view_slider_gamma_window;
menu_window_t view_slider_contrast_window;
menu_window_t view_slider_fov_window;
menu_window_t view_slider_outline_players_window;
menu_window_t view_slider_outline_monsters_window;

void M_Menu_View_f (void)
{
	key_dest = key_menu;
	m_state = m_view;
	m_entersound = true;
}

void M_View_Draw (void)
{
	int x, y, lx, ly;
	float r;
	mpic_t *p;
	byte *col;
	
	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	
	M_Print_GetPoint(16, 32, &view_window.x, &view_window.y, "           Screen size", view_cursor == 0);
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSliderInt(220, 32, r, (int)(scr_viewsize.value / 10), &view_slider_viewsize_window);

	M_Print_GetPoint(16, 40, &lx, &ly, "                 Gamma", view_cursor == 1);
	r = (1.0 - v_gamma.value) / 0.5;
	M_DrawSliderFloat2(220, 40, r, v_gamma.value, &view_slider_gamma_window);

	M_Print_GetPoint(16, 48, &lx, &ly, "              Contrast", view_cursor == 2);
	r = v_contrast.value - 1.0;
	M_DrawSliderFloat(220, 48, r, v_contrast.value, &view_slider_contrast_window);

	M_Print_GetPoint(16, 64, &lx, &ly, "         Field of view", view_cursor == 4);
	r = (scr_fov.value - 90) / (130 - 90);
	M_DrawSliderInt(220, 64, r, scr_fov.value, &view_slider_fov_window);

	M_Print_GetPoint(16, 72, &lx, &ly, "        Widescreen fov", view_cursor == 5);
	M_DrawCheckbox(220, 72, scr_widescreen_fov.value);

	M_Print_GetPoint(16, 80, &lx, &ly, "             Solid sky", view_cursor == 6);
	M_DrawCheckbox(220, 80, r_fastsky.value);

	M_Print_GetPoint(16, 88, &lx, &ly, "       Solid sky color", view_cursor == 7);
	x = 220 + ((menuwidth - 320) >> 1);
	y = 88 + m_yofs;
	col = StringToRGB(r_skycolor.string);
	glDisable(GL_TEXTURE_2D);
	glColor3ubv(col);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + 32, y);
	glVertex2f(x + 32, y + 8);
	glVertex2f(x, y + 8);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);

	M_Print_GetPoint(16, 96, &lx, &ly, "          Powerup glow", view_cursor == 8);
	M_DrawCheckbox(220, 96, r_powerupglow.value);

	M_Print_GetPoint(16, 112, &lx, &ly, "        Show player id", view_cursor == 10);
	M_DrawCheckbox(220, 112, scr_autoid.value);

	M_Print_GetPoint(16, 120, &lx, &ly, "  Show player outlines", view_cursor == 11);
	r = r_outline_players.value / 3.0f;
	M_DrawSliderInt(220, 120, r, r_outline_players.value, &view_slider_outline_players_window);

	M_Print_GetPoint(16, 128, &lx, &ly, " Show monster outlines", view_cursor == 12);
	r = r_outline_monsters.value / 3.0f;
	M_DrawSliderInt(220, 128, r, r_outline_monsters.value, &view_slider_outline_monsters_window);

	M_Print_GetPoint(16, 136, &lx, &ly, "         Outline color", view_cursor == 13);
	x = 220 + ((menuwidth - 320) >> 1);
	y = 136 + m_yofs;
	col = StringToRGB(r_outline_color.string);
	glDisable(GL_TEXTURE_2D);
	glColor3ubv(col);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + 32, y);
	glVertex2f(x + 32, y + 8);
	glVertex2f(x, y + 8);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);

	M_Print_GetPoint(16, 144, &lx, &ly, "   Show bounding boxes", view_cursor == 14);
	M_Print(220, 144, cl_bbox.value == CL_BBOX_MODE_ON
						? "on" : cl_bbox.value == CL_BBOX_MODE_DEMO
						? "demo only" : cl_bbox.value == CL_BBOX_MODE_LIVE
						? "live only" : "off");

	M_Print_GetPoint(16, 152, &lx, &ly, "      Fullbright skins", view_cursor == 15);
	M_Print(220, 152, !r_fullbrightskins.value ? "off" : r_fullbrightskins.value == 2 ? "players + monsters" : "players");

	M_Print_GetPoint(16, 160, &lx, &ly, "Rotating items bobbing", view_cursor == 16);
	M_DrawCheckbox(220, 160, cl_bobbing.value);

	M_Print_GetPoint(16, 168, &lx, &ly, "      Hide dead bodies", view_cursor == 17);
	M_DrawCheckbox(220, 168, cl_deadbodyfilter.value);

	M_Print_GetPoint(16, 176, &lx, &ly, "             Hide gibs", view_cursor == 18);
	M_DrawCheckbox(220, 176, cl_gibfilter.value);

	view_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	view_window.h = ly - view_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (view_cursor == 3 || view_cursor == 9)
		return;

	// cursor
	M_DrawCharacter (200, 32 + view_cursor * 8, 12+((int)(realtime*4)&1));

	if (view_cursor == 10)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Shows the player's name on top");
		M_Print(2 * 8, 176 + 8 * 4, "of him when watching from outside");
	}
	else if (view_cursor == 11)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Shows outlines of coop teammates");
		M_Print(2 * 8, 176 + 8 * 4, "through walls");
	}
	else if (view_cursor == 12)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Shows outlines of monsters through");
		M_Print(2 * 8, 176 + 8 * 4, "walls (disabled for demo recording)");
	}
}

void M_View_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (view_cursor)
	{
	case 0:	// screen size
		scr_viewsize.value += dir * 10;
		scr_viewsize.value = bound(30, scr_viewsize.value, 120);
		Cvar_SetValue(&scr_viewsize, scr_viewsize.value);
		break;

	case 1:	// gamma
		v_gamma.value -= dir * 0.05;
		v_gamma.value = bound(0.5, v_gamma.value, 1);
		Cvar_SetValue(&v_gamma, v_gamma.value);
		break;

	case 2:	// contrast
		v_contrast.value += dir * 0.1;
		v_contrast.value = bound(1, v_contrast.value, 2);
		Cvar_SetValue(&v_contrast, v_contrast.value);
		break;

	case 4:// fov
		scr_fov.value += dir * 5;
		scr_fov.value = bound(90, scr_fov.value, 130);
		Cvar_SetValue(&scr_fov, scr_fov.value);
		break;

	case 11:// player outlines
		r_outline_players.value += dir * 1;
		r_outline_players.value = bound(0, r_outline_players.value, 3);
		Cvar_SetValue(&r_outline_players, r_outline_players.value);
		break;

	case 12:// monster outlines
		r_outline_monsters.value += dir * 1;
		r_outline_monsters.value = bound(0, r_outline_monsters.value, 3);
		Cvar_SetValue(&r_outline_monsters, r_outline_monsters.value);
		break;
	}
}

void M_View_Key (int k)
{
	float newvalue;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		view_cursor--;
		if (view_cursor < 0)
			view_cursor = VIEW_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		view_cursor++;
		if (view_cursor >= VIEW_ITEMS)
			view_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		view_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		view_cursor = VIEW_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_View_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_View_KeyboardSlider(1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		switch (view_cursor)
		{
		case 5:
			Cvar_SetValue(&scr_widescreen_fov, !scr_widescreen_fov.value);
			break;

		case 6:
			Cvar_SetValue(&r_fastsky, !r_fastsky.value);
			break;

		case 7:
			M_Menu_Sky_ColorChooser_f();
			break;

		case 8:
			Cvar_SetValue(&r_powerupglow, !r_powerupglow.value);
			break;

		case 10:
			Cvar_SetValue(&scr_autoid, !scr_autoid.value);
			break;

		case 13:
			M_Menu_Outline_ColorChooser_f();
			break;

		case 14:
			Cvar_SetValue(&cl_bbox, ((int)cl_bbox.value + 1) % NUM_BBOX_MODE);
			break;

		case 15:
			newvalue = r_fullbrightskins.value + 1;
			if (newvalue > 2)
				newvalue = 0;
			Cvar_SetValue(&r_fullbrightskins, newvalue);
			break;

		case 16:
			Cvar_SetValue(&cl_bobbing, !cl_bobbing.value);
			break;

		case 17:
			Cvar_SetValue(&cl_deadbodyfilter, !cl_deadbodyfilter.value);
			break;

		case 18:
			Cvar_SetValue(&cl_gibfilter, !cl_gibfilter.value);
			break;

		default:
			break;
		}
	}

	if (k == K_UPARROW && (view_cursor == 3 || view_cursor == 9))
		view_cursor--;
	else if (k == K_DOWNARROW && (view_cursor == 3 || view_cursor == 9))
		view_cursor++;
}

void M_View_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (view_cursor)
		{
		case 0:	// screen size
			M_Mouse_Select_Column(&view_slider_viewsize_window, ms, 10, &slider_pos);
			scr_viewsize.value = bound(30, (slider_pos * 10) + 30, 120);
			Cvar_SetValue(&scr_viewsize, scr_viewsize.value);
			break;

		case 1:	// gamma
			M_Mouse_Select_Column(&view_slider_gamma_window, ms, 11, &slider_pos);
			v_gamma.value = bound(0.5, 1 - (slider_pos * 0.05), 1);
			Cvar_SetValue(&v_gamma, v_gamma.value);
			break;

		case 2:	// contrast
			M_Mouse_Select_Column(&view_slider_contrast_window, ms, 11, &slider_pos);
			v_contrast.value = bound(1, (slider_pos * 0.1) + 1, 2);
			Cvar_SetValue(&v_contrast, v_contrast.value);
			break;

		case 4:// fov
			M_Mouse_Select_Column(&view_slider_fov_window, ms, 9, &slider_pos);
			scr_fov.value = bound(90, (slider_pos * 5) + 90, 130);
			Cvar_SetValue(&scr_fov, scr_fov.value);
			break;

		case 11:// player outlines
			M_Mouse_Select_Column(&view_slider_outline_players_window, ms, 4, &slider_pos);
			r_outline_players.value = bound(0, slider_pos, 3);
			Cvar_SetValue(&r_outline_players, r_outline_players.value);
			break;

		case 12:// monster outlines
			M_Mouse_Select_Column(&view_slider_outline_monsters_window, ms, 4, &slider_pos);
			r_outline_monsters.value = bound(0, slider_pos, 3);
			Cvar_SetValue(&r_outline_monsters, r_outline_monsters.value);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_View_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&view_window, ms, VIEW_ITEMS, &view_cursor);

	if (ms->button_up == 1) M_View_Key(K_MOUSE1);
	if (ms->button_up == 2) M_View_Key(K_MOUSE2);

	if (ms->buttons[1]) M_View_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* OPENGL OPTIONS MENU */

#define	OPENGL_ITEMS	19

int opengl_cursor = 0;

menu_window_t opengl_window;
menu_window_t opengl_slider_wateralpha_window;
menu_window_t opengl_slider_waterfog_density_window;
menu_window_t opengl_slider_ringalpha_window;

void SearchForCharsets(void)
{
	searchpath_t	*search;

	EraseDirEntries();
	pak_files = 0;

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!search->pack)
		{
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);
			ReadDir(va("%s/textures/charsets", search->filename), "*.tga");
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);
			ReadDir(va("%s/textures/charsets", search->filename), "*.png");
		}
	}
	FindFilesInPak("textures/charsets/*.tga");
	FindFilesInPak("textures/charsets/*.png");
}

void M_Menu_Renderer_f(void)
{
	key_dest = key_menu;
	m_state = m_renderer;
	m_entersound = true;

	SearchForCharsets();
}

void M_Renderer_Draw(void)
{
	int		lx, ly;
	mpic_t	*p;

	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &opengl_window.x, &opengl_window.y, "Static coloured lights", opengl_cursor == 0);
	M_DrawCheckbox(220, 32, gl_loadlitfiles.value);

	M_Print_GetPoint(16, 40, &lx, &ly, "        Dynamic lights", opengl_cursor == 1);
	M_DrawCheckbox(220, 40, r_dynamic.value);

	M_Print_GetPoint(16, 48, &lx, &ly, " Dynamic lighting mode", opengl_cursor == 2);
	M_Print(220, 48, !gl_flashblend.value ? "lightmap" : "sphere");

	M_Print_GetPoint(16, 56, &lx, &ly, "       Vertex lighting", opengl_cursor == 3);
	M_DrawCheckbox(220, 56, gl_vertexlights.value);

	M_Print_GetPoint(16, 64, &lx, &ly, "               Shadows", opengl_cursor == 4);
	M_DrawCheckbox(220, 64, r_shadows.value);

	M_Print_GetPoint(16, 80, &lx, &ly, "       Load md3 models", opengl_cursor == 6);
	M_DrawCheckbox(220, 80, gl_loadq3models.value);

	M_Print_GetPoint(16, 96, &lx, &ly, "    Water transparency", opengl_cursor == 8);
	M_DrawSliderFloat(220, 96, r_wateralpha.value, r_wateralpha.value, &opengl_slider_wateralpha_window);

	M_Print_GetPoint(16, 104, &lx, &ly, "        Water caustics", opengl_cursor == 9);
	M_DrawCheckbox(220, 104, gl_caustics.value);

	M_Print_GetPoint(16, 112, &lx, &ly, "        Underwater fog", opengl_cursor == 10);
	M_Print(220, 112, !gl_waterfog.value ? "off" : gl_waterfog.value == 2 ? "extra" : "normal");

	M_Print_GetPoint(16, 120, &lx, &ly, "      Waterfog density", opengl_cursor == 11);
	M_DrawSliderFloat(220, 120, gl_waterfog_density.value, gl_waterfog_density.value, &opengl_slider_waterfog_density_window);

	M_Print_GetPoint(-8, 136, &lx, &ly, "Invisibility transparency", opengl_cursor == 13);
	M_DrawSliderFloat(220, 136, gl_ringalpha.value, gl_ringalpha.value, &opengl_slider_ringalpha_window);

	M_Print_GetPoint(16, 144, &lx, &ly, "     Console font type", opengl_cursor == 14);
	M_Print(220, 144, gl_consolefont.string);

	M_Print_GetPoint(16, 152, &lx, &ly, "   Smooth console font", opengl_cursor == 15);
	M_DrawCheckbox(220, 152, gl_smoothfont.value);

	M_Print_GetPoint(16, 168, &lx, &ly, "  Set high performance", opengl_cursor == 17);

	M_Print_GetPoint(16, 176, &lx, &ly, "      Set high quality", opengl_cursor == 18);

	opengl_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	opengl_window.h = ly - opengl_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (opengl_cursor == 5 || opengl_cursor == 7 || opengl_cursor == 12 || opengl_cursor == 16)
		return;

	// cursor
	M_DrawCharacter(200, 32 + opengl_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Renderer_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (opengl_cursor)
	{
	case 8:
		r_wateralpha.value += dir * 0.1;
		r_wateralpha.value = bound(0, r_wateralpha.value, 1);
		Cvar_SetValue(&r_wateralpha, r_wateralpha.value);
		break;

	case 11:
		gl_waterfog_density.value += dir * 0.1;
		gl_waterfog_density.value = bound(0, gl_waterfog_density.value, 1);
		Cvar_SetValue(&gl_waterfog_density, gl_waterfog_density.value);
		break;

	case 13:
		gl_ringalpha.value += dir * 0.1;
		gl_ringalpha.value = bound(0, gl_ringalpha.value, 1);
		Cvar_SetValue(&gl_ringalpha, gl_ringalpha.value);
		break;
	}
}

void M_Renderer_Key(int k)
{
	int i;
	float newwaterfog;
	direntry_t *charsetfile;
	
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		opengl_cursor--;
		if (opengl_cursor < 0)
			opengl_cursor = OPENGL_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		opengl_cursor++;
		if (opengl_cursor >= OPENGL_ITEMS)
			opengl_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		opengl_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		opengl_cursor = OPENGL_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Renderer_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Renderer_KeyboardSlider(1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (opengl_cursor)
		{
		case 0:
			Cvar_SetValue(&gl_loadlitfiles, !gl_loadlitfiles.value);
			break;

		case 1:
			Cvar_SetValue(&r_dynamic, !r_dynamic.value);
			break;

		case 2:
			Cvar_SetValue(&gl_flashblend, !gl_flashblend.value);
			break;

		case 3:
			Cvar_SetValue(&gl_vertexlights, !gl_vertexlights.value);
			break;

		case 4:
			Cvar_SetValue(&r_shadows, !r_shadows.value);
			break;

		case 6:
			Cvar_SetValue(&gl_loadq3models, !gl_loadq3models.value);
			break;

		case 9:
			Cvar_SetValue(&gl_caustics, !gl_caustics.value);
			break;

		case 10:
			newwaterfog = gl_waterfog.value + 1;
			if (newwaterfog > 2)
				newwaterfog = 0;
			Cvar_SetValue(&gl_waterfog, newwaterfog);
			break;

		case 14:
			if (num_files > 0)
			{
				char *charset;

				for (i = 0, charsetfile = filelist; i < num_files; i++, charsetfile++)
				{
					if (!strcmp(charsetfile->name, gl_consolefont.string))
					{
						if (i == num_files - 1)
							charset = "original";
						else
						{
							charsetfile++;
							charset = charsetfile->name;
						}
						break;
					}
				}
				if (i == num_files)
					charset = filelist->name;
				Cvar_Set(&gl_consolefont, charset);
			}
			break;

		case 15:
			Cvar_SetValue(&gl_smoothfont, !gl_smoothfont.value);
			break;

		case 17:
			Cvar_Set(&gl_texturemode, "GL_LINEAR_MIPMAP_NEAREST");
			Cvar_SetValue(&gl_picmip, 3);
			//Cvar_SetValue(&gl_detail, 0);
			Cvar_SetValue(&gl_externaltextures_world, 0);
			Cvar_SetValue(&gl_externaltextures_bmodels, 0);
			Cvar_SetValue(&gl_externaltextures_models, 0);
			R_SetParticleMode(pm_classic);
			Cvar_SetValue(&gl_decal_explosions, 0);
			Cvar_SetValue(&gl_decal_blood, 0);
			Cvar_SetValue(&gl_decal_bullets, 0);
			Cvar_SetValue(&gl_decal_sparks, 0);
			Cvar_SetValue(&gl_loadlitfiles, 0);
			Cvar_SetValue(&r_dynamic, 0);
			Cvar_SetValue(&gl_flashblend, 1);
			Cvar_SetValue(&gl_vertexlights, 0);
			Cvar_SetValue(&r_shadows, 0);
			Cvar_SetValue(&r_wateralpha, 1.0);
			Cvar_SetValue(&gl_caustics, 0);
			Cvar_SetValue(&gl_waterfog, 0);
			break;

		case 18:
			Cvar_Set(&gl_texturemode, "GL_LINEAR_MIPMAP_LINEAR");
			Cvar_SetValue(&gl_picmip, 0);
			//Cvar_SetValue(&gl_detail, 1);
			Cvar_SetValue(&gl_externaltextures_world, 1);
			Cvar_SetValue(&gl_externaltextures_bmodels, 1);
			Cvar_SetValue(&gl_externaltextures_models, 1);
			R_SetParticleMode(pm_qmb);
			Cvar_SetValue(&gl_decal_explosions, 1);
			Cvar_SetValue(&gl_decal_blood, 1);
			Cvar_SetValue(&gl_decal_bullets, 1);
			Cvar_SetValue(&gl_decal_sparks, 1);
			Cvar_SetValue(&gl_loadlitfiles, 1);
			Cvar_SetValue(&r_dynamic, 1);
			Cvar_SetValue(&gl_flashblend, 0);
			Cvar_SetValue(&gl_vertexlights, 1);
			Cvar_SetValue(&r_shadows, 2);
			Cvar_SetValue(&r_wateralpha, 0.4);
			Cvar_SetValue(&gl_caustics, 1);
			Cvar_SetValue(&gl_waterfog, 1);
			break;

		default:
			break;
		}
	}

	if (k == K_UPARROW && (opengl_cursor == 5 || opengl_cursor == 7 || opengl_cursor == 12 || opengl_cursor == 16))
		opengl_cursor--;
	else if (k == K_DOWNARROW && (opengl_cursor == 5 || opengl_cursor == 7 || opengl_cursor == 12 || opengl_cursor == 16))
		opengl_cursor++;
}

void M_Renderer_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (opengl_cursor)
		{
		case 8:
			M_Mouse_Select_Column(&opengl_slider_wateralpha_window, ms, 11, &slider_pos);
			r_wateralpha.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&r_wateralpha, r_wateralpha.value);
			break;

		case 11:
			M_Mouse_Select_Column(&opengl_slider_waterfog_density_window, ms, 11, &slider_pos);
			gl_waterfog_density.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&gl_waterfog_density, gl_waterfog_density.value);
			break;

		case 13:
			M_Mouse_Select_Column(&opengl_slider_ringalpha_window, ms, 11, &slider_pos);
			gl_ringalpha.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&gl_ringalpha, gl_ringalpha.value);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Renderer_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&opengl_window, ms, OPENGL_ITEMS, &opengl_cursor);

	if (ms->button_up == 1) M_Renderer_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Renderer_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Renderer_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* TEXTURES MENU */

#define	TEXTURES_ITEMS	13

int textures_cursor = 0;

menu_window_t textures_window;
menu_window_t textures_slider_anisotropy_window;
menu_window_t textures_slider_picmip_window;
menu_window_t textures_slider_maxsize_window;

char *texture_filters[] = {
	"GL_NEAREST_MIPMAP_NEAREST",
	"GL_NEAREST_MIPMAP_LINEAR",
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_LINEAR_MIPMAP_LINEAR"
};

char *sky_hud_texture_filters[] = {
	"GL_NEAREST",
	"GL_LINEAR"
};

int num_max_anisotropy_values;
float max_anisotropy_values[10];	//joe: I hope this would be enough

void CollectAvailableAnisotropyValues()
{
	float current_anisotropy = 1.0;

	num_max_anisotropy_values = 0;
	while (current_anisotropy <= gl_max_anisotropy)
	{
		max_anisotropy_values[num_max_anisotropy_values++] = current_anisotropy;
		current_anisotropy *= 2;
	}
}

void M_Menu_Textures_f(void)
{
	key_dest = key_menu;
	m_state = m_textures;
	m_entersound = true;

	CollectAvailableAnisotropyValues();
}

#define MAX_SIZE_ITEMS 7
float max_size_values[MAX_SIZE_ITEMS] = { 256, 512, 1024, 2048, 4096, 8192, 16384 };

void M_Textures_Draw(void)
{
	int		lx, ly;
	float	r;
	mpic_t	*p;

	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &textures_window.x, &textures_window.y, "  World texture filter", textures_cursor == 0);
	M_Print(220, 32, !Q_strcasecmp(gl_texturemode.string, "GL_LINEAR_MIPMAP_NEAREST") ? "linear (mipmap off)" :
					 !Q_strcasecmp(gl_texturemode.string, "GL_LINEAR_MIPMAP_LINEAR") ? "linear (mipmap on)" :
					 !Q_strcasecmp(gl_texturemode.string, "GL_NEAREST_MIPMAP_NEAREST") ? "nearest (mipmap off)" : 
					 !Q_strcasecmp(gl_texturemode.string, "GL_NEAREST_MIPMAP_LINEAR") ? "nearest (mipmap on)" : gl_texturemode.string);

	M_Print_GetPoint(8, 40, &lx, &ly, "Menu/Hud texture filter", textures_cursor == 1);
	M_Print(220, 40, !Q_strcasecmp(gl_texturemode_hud.string, "GL_LINEAR") ? "linear" :
					 !Q_strcasecmp(gl_texturemode_hud.string, "GL_NEAREST") ? "nearest" : gl_texturemode_hud.string);

	M_Print_GetPoint(16, 48, &lx, &ly, "    Sky texture filter", textures_cursor == 2);
	M_Print(220, 48, !Q_strcasecmp(gl_texturemode_sky.string, "GL_LINEAR") ? "linear" :
					 !Q_strcasecmp(gl_texturemode_sky.string, "GL_NEAREST") ? "nearest" : gl_texturemode_sky.string);

	M_Print_GetPoint(16, 56, &lx, &ly, " Anisotropic filtering", textures_cursor == 3);
	r = (float)FindSliderItemIndex(max_anisotropy_values, num_max_anisotropy_values, &gl_texture_anisotropy) / (num_max_anisotropy_values - 1);
	M_DrawSliderInt(220, 56, r, gl_texture_anisotropy.value, &textures_slider_anisotropy_window);

	M_Print_GetPoint(16, 64, &lx, &ly, "       Texture quality", textures_cursor == 4);
	r = (4 - gl_picmip.value) * 0.25;
	M_DrawSlider(220, 64, r, &textures_slider_picmip_window);

	M_Print_GetPoint(16, 72, &lx, &ly, "     Detailed textures", textures_cursor == 5);
	M_DrawCheckbox(220, 72, gl_detail.value);

	M_Print_GetPoint(16, 80, &lx, &ly, "      Max texture size", textures_cursor == 6);
	//r = (gl_max_size.value - max_size_values[0]) / (max_size_values[MAX_SIZE_ITEMS-1] - max_size_values[0]);
	r = (float)FindSliderItemIndex(max_size_values, MAX_SIZE_ITEMS, &gl_max_size) / (MAX_SIZE_ITEMS - 1);
	M_DrawSliderInt(220, 80, r, gl_max_size.value, &textures_slider_maxsize_window);

	M_Print_GetPoint(-40, 96, &lx, &ly, "Enable external textures for:", false);

	M_Print_GetPoint(16, 104, &lx, &ly, "                 World", textures_cursor == 9);
	M_DrawCheckbox(220, 104, gl_externaltextures_world.value);

	M_Print_GetPoint(16, 112, &lx, &ly, "        Static objects", textures_cursor == 10);
	M_DrawCheckbox(220, 112, gl_externaltextures_bmodels.value);

	M_Print_GetPoint(16, 120, &lx, &ly, "       Dynamic objects", textures_cursor == 11);
	M_DrawCheckbox(220, 120, gl_externaltextures_models.value);

	M_Print_GetPoint(16, 128, &lx, &ly, "              Menu/Hud", textures_cursor == 12);
	M_DrawCheckbox(220, 128, gl_externaltextures_gfx.value);

	textures_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	textures_window.h = ly - textures_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (textures_cursor == 7 || textures_cursor == 8)
		return;

	// cursor
	M_DrawCharacter(200, 32 + textures_cursor * 8, 12 + ((int)(realtime * 4) & 1));

	if (textures_cursor == 10)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Static objects are health boxes,");
		M_Print(2 * 8, 176 + 8 * 4, "ammo boxes and explosion barrels");
	}
	else if (textures_cursor == 11)
	{
		M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
		M_Print(2 * 8, 176 + 8 * 3, "Dynamic objects are players, monsters,");
		M_Print(2 * 8, 176 + 8 * 4, "weapons, armors, keys, gibs, backpack,");
		M_Print(2 * 8, 176 + 8 * 5, "rocket, grenade and torches");
	}
}

void M_Textures_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (textures_cursor)
	{
	case 3:
		AdjustSliderBasedOnArrayOfValues(dir, max_anisotropy_values, num_max_anisotropy_values, &gl_texture_anisotropy);
		break;
	
	case 4:
		gl_picmip.value -= dir;
		gl_picmip.value = bound(0, gl_picmip.value, 4);
		Cvar_SetValue(&gl_picmip, gl_picmip.value);
		break;

	case 6:
		AdjustSliderBasedOnArrayOfValues(dir, max_size_values, MAX_SIZE_ITEMS, &gl_max_size);
		break;
	}
}

void M_Textures_Key(int k)
{
	int	i;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		textures_cursor--;
		if (textures_cursor < 0)
			textures_cursor = TEXTURES_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		textures_cursor++;
		if (textures_cursor >= TEXTURES_ITEMS)
			textures_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		textures_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		textures_cursor = TEXTURES_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Textures_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Textures_KeyboardSlider(1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (textures_cursor)
		{
		case 0:
			for (i = 0; i < 4; i++)
				if (!Q_strcasecmp(texture_filters[i], gl_texturemode.string))
					break;
			if (i >= 3)
				i = -1;
			Cvar_Set(&gl_texturemode, texture_filters[i + 1]);
			break;

		case 1:
			for (i = 0; i < 2; i++)
				if (!Q_strcasecmp(sky_hud_texture_filters[i], gl_texturemode_hud.string))
					break;
			if (i >= 1)
				i = -1;
			Cvar_Set(&gl_texturemode_hud, sky_hud_texture_filters[i + 1]);
			break;

		case 2:
			for (i = 0; i < 2; i++)
				if (!Q_strcasecmp(sky_hud_texture_filters[i], gl_texturemode_sky.string))
					break;
			if (i >= 1)
				i = -1;
			Cvar_Set(&gl_texturemode_sky, sky_hud_texture_filters[i + 1]);
			break;

		case 5:
			Cvar_SetValue(&gl_detail, !gl_detail.value);
			break;

		case 9:
			Cvar_SetValue(&gl_externaltextures_world, !gl_externaltextures_world.value);
			break;

		case 10:
			Cvar_SetValue(&gl_externaltextures_bmodels, !gl_externaltextures_bmodels.value);
			break;

		case 11:
			Cvar_SetValue(&gl_externaltextures_models, !gl_externaltextures_models.value);
			break;

		case 12:
			Cvar_SetValue(&gl_externaltextures_gfx, !gl_externaltextures_gfx.value);
			break;

		default:
			break;
		}
	}

	if (k == K_UPARROW && (textures_cursor == 7 || textures_cursor == 8))
		textures_cursor -= 2;
	else if (k == K_DOWNARROW && (textures_cursor == 7 || textures_cursor == 8))
		textures_cursor += 2;
}

void M_Textures_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (textures_cursor)
		{
		case 3:
			M_Mouse_Select_Column(&textures_slider_anisotropy_window, ms, num_max_anisotropy_values, &slider_pos);
			Cvar_SetValue(&gl_texture_anisotropy, max_anisotropy_values[slider_pos]);
			break;
		
		case 4:
			M_Mouse_Select_Column(&textures_slider_picmip_window, ms, 5, &slider_pos);
			gl_picmip.value = bound(0, 4 - slider_pos, 4);
			Cvar_SetValue(&gl_picmip, gl_picmip.value);
			break;

		case 6:
			M_Mouse_Select_Column(&textures_slider_maxsize_window, ms, MAX_SIZE_ITEMS, &slider_pos);
			Cvar_SetValue(&gl_max_size, max_size_values[slider_pos]);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Textures_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&textures_window, ms, TEXTURES_ITEMS, &textures_cursor);

	if (ms->button_up == 1) M_Textures_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Textures_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Textures_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* PARTICLES MENU */

#define	PART_ITEMS	11

int	part_cursor = 0;

menu_window_t part_window;

void M_Menu_Particles_f (void)
{
	key_dest = key_menu;
	m_state = m_particles;
	m_entersound = true;
}

#define	GET_PARTICLE_VAL(var) (gl_part_##var.value == pm_classic ? "Classic" : gl_part_##var.value == pm_qmb ? "QMB" : "Quake3")
#define	SET_PARTICLE_VAL(var)						\
	if (gl_part_##var.value == pm_classic)			\
		Cvar_SetValue (&gl_part_##var, pm_qmb);		\
	else if (gl_part_##var.value == pm_qmb)			\
		Cvar_SetValue (&gl_part_##var, pm_quake3);	\
	else											\
		Cvar_SetValue (&gl_part_##var, pm_classic);

void M_Particles_Draw (void)
{
	int		lx, ly;
	mpic_t	*p;
	
	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	
	M_Print_GetPoint(16, 32, &part_window.x, &part_window.y, "            Explosions", part_cursor == 0);
	M_Print (220, 32, GET_PARTICLE_VAL(explosions));

	M_Print_GetPoint(16, 40, &lx, &ly, "                Trails", part_cursor == 1);
	M_Print (220, 40, GET_PARTICLE_VAL(trails));

	M_Print_GetPoint(16, 48, &lx, &ly, "                Spikes", part_cursor == 2);
	M_Print (220, 48, GET_PARTICLE_VAL(spikes));

	M_Print_GetPoint(16, 56, &lx, &ly, "              Gunshots", part_cursor == 3);
	M_Print (220, 56, GET_PARTICLE_VAL(gunshots));

	M_Print_GetPoint(16, 64, &lx, &ly, "                 Blood", part_cursor == 4);
	M_Print (220, 64, GET_PARTICLE_VAL(blood));

	M_Print_GetPoint(16, 72, &lx, &ly, "     Teleport splashes", part_cursor == 5);
	M_Print (220, 72, GET_PARTICLE_VAL(telesplash));

	M_Print_GetPoint(16, 80, &lx, &ly, "      Spawn explosions", part_cursor == 6);
	M_Print (220, 80, GET_PARTICLE_VAL(blobs));

	M_Print_GetPoint(16, 88, &lx, &ly, "         Lava splashes", part_cursor == 7);
	M_Print (220, 88, GET_PARTICLE_VAL(lavasplash));

	M_Print_GetPoint(16, 96, &lx, &ly, "                Flames", part_cursor == 8);
	M_Print (220, 96, GET_PARTICLE_VAL(flames));

	M_Print_GetPoint(16, 104, &lx, &ly, "             Lightning", part_cursor == 9);
	M_Print (220, 104, GET_PARTICLE_VAL(lightning));

	M_Print_GetPoint(16, 112, &lx, &ly, "    Bouncing particles", part_cursor == 10);
	M_DrawCheckbox (220, 112, gl_bounceparticles.value);

	part_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	part_window.h = ly - part_window.y + 8;

	// cursor
	M_DrawCharacter (200, 32 + part_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Particles_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		part_cursor--;
		if (part_cursor < 0)
			part_cursor = PART_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		part_cursor++;
		if (part_cursor >= PART_ITEMS)
			part_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		part_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		part_cursor = PART_ITEMS - 1;
		break;

	case K_RIGHTARROW:
	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		switch (part_cursor)
		{
		case 0:
			SET_PARTICLE_VAL(explosions);
			break;

		case 1:
			SET_PARTICLE_VAL(trails);
			break;

		case 2:
			SET_PARTICLE_VAL(spikes);
			break;

		case 3:
			SET_PARTICLE_VAL(gunshots);
			break;

		case 4:
			SET_PARTICLE_VAL(blood);
			break;

		case 5:
			SET_PARTICLE_VAL(telesplash);
			break;

		case 6:
			SET_PARTICLE_VAL(blobs);
			break;

		case 7:
			SET_PARTICLE_VAL(lavasplash);
			break;

		case 8:
			SET_PARTICLE_VAL(flames);
			break;

		case 9:
			SET_PARTICLE_VAL(lightning);
			break;

		case 10:
			Cvar_SetValue (&gl_bounceparticles, !gl_bounceparticles.value);
			break;
		}
	}
}

qboolean M_Particles_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&part_window, ms, PART_ITEMS, &part_cursor);

	if (ms->button_up == 1) M_Particles_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Particles_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* DECALS MENU */

#define	DECALS_ITEMS	7

int	decals_cursor = 0;

menu_window_t decals_window;
menu_window_t decals_slider_decaltime_window;
menu_window_t decals_slider_viewdistance_window;

void M_Menu_Decals_f(void)
{
	key_dest = key_menu;
	m_state = m_decals;
	m_entersound = true;
}

#define	DECAL_TIME_ITEMS	6
float decal_time_values[] = { 15, 30, 60, 120, 300, 600 };

#define	DECAL_VIEWDISTANCE_ITEMS	6
float decal_viewdistance_values[] = { 512, 1024, 2048, 4096, 8192, 16384 };

void M_Decals_Draw(void)
{
	int		lx, ly;
	float	r;
	mpic_t	*p;

	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(-32, 32, &decals_window.x, &decals_window.y, "Lifetime of decals (seconds)", decals_cursor == 0);
	//r = (gl_decaltime.value - decal_time_values[0]) / (decal_time_values[DECAL_TIME_ITEMS-1] - decal_time_values[0]);
	r = (float)FindSliderItemIndex(decal_time_values, DECAL_TIME_ITEMS, &gl_decaltime) / (DECAL_TIME_ITEMS - 1);
	M_DrawSliderInt(220, 32, r, gl_decaltime.value, &decals_slider_decaltime_window);

	M_Print_GetPoint(8, 40, &lx, &ly, "View distance of decals", decals_cursor == 1);
	//r = (gl_decal_viewdistance.value - decal_viewdistance_values[0]) / (decal_viewdistance_values[DECAL_VIEWDISTANCE_ITEMS-1] - decal_viewdistance_values[0]);
	r = (float)FindSliderItemIndex(decal_viewdistance_values, DECAL_VIEWDISTANCE_ITEMS, &gl_decal_viewdistance) / (DECAL_VIEWDISTANCE_ITEMS - 1);
	M_DrawSliderInt(220, 40, r, gl_decal_viewdistance.value, &decals_slider_viewdistance_window);

	M_Print_GetPoint(16, 56, &lx, &ly, "       Blood splatters", decals_cursor == 3);
	M_DrawCheckbox(220, 56, gl_decal_blood.value);

	M_Print_GetPoint(16, 64, &lx, &ly, "          Bullet holes", decals_cursor == 4);
	M_DrawCheckbox(220, 64, gl_decal_bullets.value);

	M_Print_GetPoint(16, 72, &lx, &ly, "          Spark trails", decals_cursor == 5);
	M_DrawCheckbox(220, 72, gl_decal_sparks.value);

	M_Print_GetPoint(16, 80, &lx, &ly, "       Explosion marks", decals_cursor == 6);
	M_DrawCheckbox(220, 80, gl_decal_explosions.value);

	decals_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	decals_window.h = ly - decals_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (decals_cursor == 2)
		return;

	// cursor
	M_DrawCharacter(200, 32 + decals_cursor * 8, 12 + ((int)(realtime * 4) & 1));

	M_PrintWhite(2 * 8, 176 + 8 * 2, "Hint:");
	M_Print(2 * 8, 176 + 8 * 3, "Decals are displayed only when using");
	M_Print(2 * 8, 176 + 8 * 4, "QMB or Quake3 style particle effects");
}

void M_Decals_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (decals_cursor)
	{
	case 0:
		AdjustSliderBasedOnArrayOfValues(dir, decal_time_values, DECAL_TIME_ITEMS, &gl_decaltime);
		break;

	case 1:
		AdjustSliderBasedOnArrayOfValues(dir, decal_viewdistance_values, DECAL_VIEWDISTANCE_ITEMS, &gl_decal_viewdistance);
		break;
	}
}

void M_Decals_Key(int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		decals_cursor--;
		if (decals_cursor < 0)
			decals_cursor = DECALS_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		decals_cursor++;
		if (decals_cursor >= DECALS_ITEMS)
			decals_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		decals_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		decals_cursor = DECALS_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Decals_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Decals_KeyboardSlider(1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (decals_cursor)
		{
		case 3:
			Cvar_SetValue(&gl_decal_blood, !gl_decal_blood.value);
			break;

		case 4:
			Cvar_SetValue(&gl_decal_bullets, !gl_decal_bullets.value);
			break;

		case 5:
			Cvar_SetValue(&gl_decal_sparks, !gl_decal_sparks.value);
			break;

		case 6:
			Cvar_SetValue(&gl_decal_explosions, !gl_decal_explosions.value);
			break;

		default:
			break;
		}
	}

	if (k == K_UPARROW && decals_cursor == 2)
		decals_cursor--;
	else if (k == K_DOWNARROW && decals_cursor == 2)
		decals_cursor++;
}

void M_Decals_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (decals_cursor)
		{
		case 0:
			M_Mouse_Select_Column(&decals_slider_decaltime_window, ms, DECAL_TIME_ITEMS, &slider_pos);
			Cvar_SetValue(&gl_decaltime, decal_time_values[slider_pos]);
			break;

		case 1:
			M_Mouse_Select_Column(&decals_slider_viewdistance_window, ms, DECAL_VIEWDISTANCE_ITEMS, &slider_pos);
			Cvar_SetValue(&gl_decal_viewdistance, decal_viewdistance_values[slider_pos]);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Decals_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&decals_window, ms, DECALS_ITEMS, &decals_cursor);

	if (ms->button_up == 1) M_Decals_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Decals_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Decals_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* WEAPON OPTIONS MENU */

#define	WEAPONS_ITEMS	17

int	weapons_cursor = 0;

menu_window_t weapons_window;
menu_window_t weapons_slider_gun_fovscale_window;

void M_Menu_Weapons_f(void)
{
	key_dest = key_menu;
	m_state = m_weapons;
	m_entersound = true;
}

void DrawViewmodelType(int x, int y)
{
	char *str;

	switch ((int)(r_drawviewmodel.value))
	{
	case 0:
		str = "off";
		break;

	case 1:
		str = "bobbing";
		break;

	case 2:
		str = "fixed";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawHandType(int x, int y)
{
	char *str;

	switch ((int)(cl_hand.value))
	{
	case 0:
		str = "center";
		break;

	case 1:
		str = "right";
		break;

	case 2:
		str = "left";
		break;

	case 3:
		str = "dp smoke (QMB only)";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawRocketTrailType(int x, int y)
{
	char *str;

	switch ((int)(r_rockettrail.value))
	{
	case 0:
		str = "no trail";
		break;

	case 1:
		str = "rocket smoke";
		break;

	case 2:
		str = "grenade smoke";
		break;

	case 3:
		str = "dp smoke (QMB only)";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawGrenadeTrailType(int x, int y)
{
	char *str;

	switch ((int)(r_grenadetrail.value))
	{
	case 0:
		str = "no trail";
		break;

	case 1:
		str = "grenade smoke";
		break;

	case 2:
		str = "rocket smoke";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawColorType(int x, int y, int value)
{
	char *str;

	switch (value)
	{
	case 0:
		str = "original";
		break;

	case 1:
		str = "red";
		break;

	case 2:
		str = "blue";
		break;

	case 3:
		str = "purple";
		break;

	case 4:
		str = "green";
		break;

	case 5:
		str = "random";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void DrawExplosionType(int x, int y)
{
	char *str;

	switch ((int)(r_explosiontype.value))
	{
	case 0:
		str = "sprite and particles";
		break;

	case 1:
		str = "sprite only";
		break;

	case 2:
		str = "particles only";
		break;

	case 3:
		str = "blood";
		break;

	default:
		str = "unknown";
		break;
	}

	M_Print(x, y, str);
}

void M_Weapons_Draw(void)
{
	int		lx, ly;
	mpic_t	*p;

	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &weapons_window.x, &weapons_window.y, "           Show weapon", weapons_cursor == 0);
	DrawViewmodelType(220, 32);

	M_Print_GetPoint(16, 40, &lx, &ly, "      Weapon FOV scale", weapons_cursor == 1);
	M_DrawSliderFloat(220, 40, cl_gun_fovscale.value, cl_gun_fovscale.value, &weapons_slider_gun_fovscale_window);

	M_Print_GetPoint(16, 48, &lx, &ly, "     Weapon handedness", weapons_cursor == 2);
	DrawHandType(220, 48);

	M_Print_GetPoint(16, 56, &lx, &ly, "           Weapon kick", weapons_cursor == 3);
	M_DrawCheckbox(220, 56, v_gunkick.value);

	M_Print_GetPoint(16, 64, &lx, &ly, "     Weapon fire light", weapons_cursor == 4);
	M_DrawCheckbox(220, 64, cl_muzzleflash.value);

	M_Print_GetPoint(16, 72, &lx, &ly, "   Enable view weapons", weapons_cursor == 5);
	M_DrawCheckbox(220, 72, cl_viewweapons.value);

	M_Print_GetPoint(16, 88, &lx, &ly, "        True lightning", weapons_cursor == 7);
	M_DrawCheckbox(220, 88, cl_truelightning.value);

	M_Print_GetPoint(16, 96, &lx, &ly, "     Rocket trail type", weapons_cursor == 8);
	DrawRocketTrailType(220, 96);

	M_Print_GetPoint(16, 104, &lx, &ly, "    Grenade trail type", weapons_cursor == 9);
	DrawGrenadeTrailType(220, 104);

	M_Print_GetPoint(-32, 112, &lx, &ly, "Show grenade model as rocket", weapons_cursor == 10);
	M_DrawCheckbox(220, 112, cl_rocket2grenade.value);

	M_Print_GetPoint(16, 128, &lx, &ly, "          Rocket light", weapons_cursor == 12);
	M_DrawCheckbox(220, 128, r_rocketlight.value);

	M_Print_GetPoint(16, 136, &lx, &ly, "    Rocket light color", weapons_cursor == 13);
	DrawColorType(220, 136, (int)(r_rocketlightcolor.value));

	M_Print_GetPoint(16, 144, &lx, &ly, "        Explosion type", weapons_cursor == 14);
	DrawExplosionType(220, 144);

	M_Print_GetPoint(16, 152, &lx, &ly, "       Explosion light", weapons_cursor == 15);
	M_DrawCheckbox(220, 152, r_explosionlight.value);

	M_Print_GetPoint(16, 160, &lx, &ly, " Explosion light color", weapons_cursor == 16);
	DrawColorType(220, 160, (int)(r_explosionlightcolor.value));

	weapons_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	weapons_window.h = ly - weapons_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (weapons_cursor == 6 || weapons_cursor == 11)
		return;

	// cursor
	M_DrawCharacter(200, 32 + weapons_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Weapons_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (weapons_cursor)
	{
	case 1:
		cl_gun_fovscale.value += dir * 0.1;
		cl_gun_fovscale.value = bound(0, cl_gun_fovscale.value, 1);
		Cvar_SetValue(&cl_gun_fovscale, cl_gun_fovscale.value);
		break;
	}
}

void M_Weapons_Key(int k)
{
	float newvalue;

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		weapons_cursor--;
		if (weapons_cursor < 0)
			weapons_cursor = WEAPONS_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		weapons_cursor++;
		if (weapons_cursor >= WEAPONS_ITEMS)
			weapons_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		weapons_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		weapons_cursor = WEAPONS_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_Weapons_KeyboardSlider(-1);
		break;

	case K_RIGHTARROW:
		M_Weapons_KeyboardSlider(1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (weapons_cursor)
		{
		case 0:
			newvalue = r_drawviewmodel.value + 1;
			if (newvalue > 2)
				newvalue = 0;
			Cvar_SetValue(&r_drawviewmodel, newvalue);
			break;

		case 2:
			newvalue = cl_hand.value + 1;
			if (newvalue > 2)
				newvalue = 0;
			Cvar_SetValue(&cl_hand, newvalue);
			break;

		case 3:
			if (v_gunkick.value != 0)
				Cvar_SetValue(&v_gunkick, 0);
			else
				Cvar_SetValue(&v_gunkick, 2);
			break;

		case 4:
			Cvar_SetValue(&cl_muzzleflash, !cl_muzzleflash.value);
			break;

		case 5:
			Cvar_SetValue(&cl_viewweapons, !cl_viewweapons.value);
			break;

		case 7:
			Cvar_SetValue(&cl_truelightning, !cl_truelightning.value);
			break;

		case 8:
			newvalue = r_rockettrail.value + 1;
			if (newvalue > 3)
				newvalue = 0;
			Cvar_SetValue(&r_rockettrail, newvalue);
			break;

		case 9:
			newvalue = r_grenadetrail.value + 1;
			if (newvalue > 3)
				newvalue = 0;
			Cvar_SetValue(&r_grenadetrail, newvalue);
			break;

		case 10:
			Cvar_SetValue(&cl_rocket2grenade, !cl_rocket2grenade.value);
			break;

		case 12:
			Cvar_SetValue(&r_rocketlight, !r_rocketlight.value);
			break;

		case 13:
			newvalue = r_rocketlightcolor.value + 1;
			if (newvalue > 5)
				newvalue = 0;
			Cvar_SetValue(&r_rocketlightcolor, newvalue);
			break;

		case 14:
			newvalue = r_explosiontype.value + 1;
			if (newvalue > 3)
				newvalue = 0;
			Cvar_SetValue(&r_explosiontype, newvalue);
			break;

		case 15:
			Cvar_SetValue(&r_explosionlight, !r_explosionlight.value);
			break;

		case 16:
			newvalue = r_explosionlightcolor.value + 1;
			if (newvalue > 5)
				newvalue = 0;
			Cvar_SetValue(&r_explosionlightcolor, newvalue);
			break;

		default:
			break;
		}
	}

	if (k == K_UPARROW && (weapons_cursor == 6 || weapons_cursor == 11))
		weapons_cursor--;
	else if (k == K_DOWNARROW && (weapons_cursor == 6 || weapons_cursor == 11))
		weapons_cursor++;
}

void M_Weapons_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (weapons_cursor)
		{
		case 1:
			M_Mouse_Select_Column(&weapons_slider_gun_fovscale_window, ms, 11, &slider_pos);
			cl_gun_fovscale.value = bound(0, slider_pos * 0.1, 1);
			Cvar_SetValue(&cl_gun_fovscale, cl_gun_fovscale.value);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Weapons_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&weapons_window, ms, WEAPONS_ITEMS, &weapons_cursor);

	if (ms->button_up == 1) M_Weapons_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Weapons_Key(K_MOUSE2);

	if (ms->buttons[1]) M_Weapons_MouseSlider(K_MOUSE1, ms);

	return true;
}

//=============================================================================
/* SCREEN FLASHES MENU */

#define	SCREENFLASHES_ITEMS	8

int	screenflashes_cursor = 0;

menu_window_t screenflashes_window;

void M_Menu_ScreenFlashes_f(void)
{
	key_dest = key_menu;
	m_state = m_screenflashes;
	m_entersound = true;
}

void M_ScreenFlashes_Draw(void)
{
	int		lx, ly;
	mpic_t	*p;

	//M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) >> 1, 4, p);

	M_Print_GetPoint(16, 32, &screenflashes_window.x, &screenflashes_window.y, "        Screen flashes", screenflashes_cursor == 0);
	M_DrawCheckbox(220, 32, gl_polyblend.value);

	M_Print_GetPoint(16, 40, &lx, &ly, "      When under water", screenflashes_cursor == 1);
	M_DrawCheckbox(220, 40, v_contentblend.value);

	M_Print_GetPoint(16, 48, &lx, &ly, "      When getting hit", screenflashes_cursor == 2);
	M_DrawCheckbox(220, 48, v_damagecshift.value);

	M_Print_GetPoint(16, 56, &lx, &ly, "   When item picked up", screenflashes_cursor == 3);
	M_DrawCheckbox(220, 56, v_bonusflash.value);

	M_Print_GetPoint(0, 64, &lx, &ly, "When quad damage is used", screenflashes_cursor == 4);
	M_DrawCheckbox(220, 64, v_quadcshift.value);

	M_Print_GetPoint(0, 72, &lx, &ly, "When enviro-suit is used", screenflashes_cursor == 5);
	M_DrawCheckbox(220, 72, v_suitcshift.value);

	M_Print_GetPoint(-8, 80, &lx, &ly, "When invisibility is used", screenflashes_cursor == 6);
	M_DrawCheckbox(220, 80, v_ringcshift.value);

	M_Print_GetPoint(16, 88, &lx, &ly, "When pentagram is used", screenflashes_cursor == 7);
	M_DrawCheckbox(220, 88, v_pentcshift.value);

	screenflashes_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	screenflashes_window.h = ly - screenflashes_window.y + 8;

	// cursor
	M_DrawCharacter(200, 32 + screenflashes_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_ScreenFlashes_Key(int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Options_f();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		screenflashes_cursor--;
		if (screenflashes_cursor < 0)
			screenflashes_cursor = SCREENFLASHES_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		screenflashes_cursor++;
		if (screenflashes_cursor >= SCREENFLASHES_ITEMS)
			screenflashes_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		screenflashes_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		screenflashes_cursor = SCREENFLASHES_ITEMS - 1;
		break;

	case K_LEFTARROW:
		break;

	case K_RIGHTARROW:
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (screenflashes_cursor)
		{
		case 0:
			Cvar_SetValue(&gl_polyblend, !gl_polyblend.value);
			break;

		case 1:
			Cvar_SetValue(&v_contentblend, !v_contentblend.value);
			break;

		case 2:
			Cvar_SetValue(&v_damagecshift, !v_damagecshift.value);
			break;

		case 3:
			Cvar_SetValue(&v_bonusflash, !v_bonusflash.value);
			break;

		case 4:
			Cvar_SetValue(&v_quadcshift, !v_quadcshift.value);
			break;

		case 5:
			Cvar_SetValue(&v_suitcshift, !v_suitcshift.value);
			break;

		case 6:
			Cvar_SetValue(&v_ringcshift, !v_ringcshift.value);
			break;

		case 7:
			Cvar_SetValue(&v_pentcshift, !v_pentcshift.value);
			break;

		default:
			break;
		}
	}
}

qboolean M_ScreenFlashes_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&screenflashes_window, ms, SCREENFLASHES_ITEMS, &screenflashes_cursor);

	if (ms->button_up == 1) M_ScreenFlashes_Key(K_MOUSE1);
	if (ms->button_up == 2) M_ScreenFlashes_Key(K_MOUSE2);

	return true;
}

#endif

//=============================================================================
/* VIDEO MENU */

qboolean m_videomode_change_confirm;

#ifdef GLQUAKE
extern int menu_bpp, menu_display_freq;
extern float menu_vsync;
extern cvar_t vid_vsync;
extern int GetCurrentBpp(void);
extern int GetCurrentFreq(void);
#endif

void M_Menu_VideoModes_f(void)
{
#ifdef GLQUAKE
	menu_bpp = GetCurrentBpp();
	menu_display_freq = GetCurrentFreq();
	menu_vsync = vid_vsync.value;
#endif

	key_dest = key_menu;
	m_state = m_videomodes;
	m_entersound = true;
	m_videomode_change_confirm = false;
}

void M_VideoModes_Draw (void)
{
	(*vid_menudrawfn)();
}

void M_VideoModes_Key (int key)
{
	(*vid_menukeyfn)(key);
}

//=============================================================================
/* MAPS MENU */

void SearchForMaps (void)
{
	searchpath_t	*search;

	EraseDirEntries ();
	pak_files = 0;

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (!search->pack)
		{
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);
			ReadDir (va("%s/maps", search->filename), "*.bsp");
		}
	}
	FindFilesInPak ("maps/*.bsp");

	SaveCursorPos ();
}

void M_Menu_Maps_f (void)
{
	key_dest = key_menu;
	m_state = m_maps;
	m_entersound = true;

	SearchForMaps ();
}

void M_Maps_Draw (void)
{
	M_List_Draw ("MAPS", 24);
}

void M_Maps_Key (int k)
{
	int		i;
	qboolean	worx;

	M_List_Key (k, num_files, MAXLINES);

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		if (searchbox)
		{
			KillSearchBox ();
		}
		else
		{
			Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));
			M_Menu_Main_f ();
		}
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (!num_files || filelist[list_base+list_cursor].type == 3)
			break;

		key_dest = key_game;
		m_state = m_none;
		if (cl.gametype == GAME_DEATHMATCH)
			Cbuf_AddText (va("changelevel %s\n", filelist[list_base+list_cursor].name));
		else
			Cbuf_AddText (va("map %s\n", filelist[list_base+list_cursor].name));
		Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));

		if (searchbox)
			KillSearchBox ();
		break;

	case K_BACKSPACE:
		if (strcmp(searchfile, ""))
			searchfile[--num_searchs] = 0;
		break;

	default:
		if (k < 32 || k > 127)
			break;

		searchbox = true;
		searchfile[num_searchs++] = k;
		worx = false;
		for (i=0 ; i<num_files ; i++)
		{
			if (strstr(filelist[i].name, searchfile) == filelist[i].name)
			{
				worx = true;
				S_LocalSound ("misc/menu1.wav");
				list_base = i - 10;
				if (list_base < 0)
				{
					list_base = 0;
					list_cursor = i;
				}
				else if (list_base > (num_files - MAXLINES))
				{
					list_base = num_files - MAXLINES;
					list_cursor = MAXLINES - (num_files - i);
				}
				else
					list_cursor = 10;
				break;
			}
		}
		if (!worx)
			searchfile[--num_searchs] = 0;
		break;
	}
}

qboolean M_Maps_Mouse_Event(const mouse_state_t *ms)
{
	int entries = min(num_files, MAXLINES);
	M_Mouse_Select(&list_window, ms, entries, &list_cursor);

	if (ms->button_up == 1) M_Maps_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Maps_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* DEMOS MENU */

// Nehahra's Demos Menu

void M_Menu_NehDemos_f (void)
{
	key_dest = key_menu;
	m_state = m_nehdemos;
	m_entersound = true;

	list_base = list_cursor = 0;
}

void M_NehDemos_Draw (void)
{
	M_List_Draw ("DEMOS", 24);
}

void M_NehDemos_Key (int k)
{
	M_List_Key (k, NUMNEHDEMOS, MAXNEHLINES);

	switch (k)
 	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText (va("playdemo %s\n", NehDemos[list_base+list_cursor][0]));
		break;

	default:
		break;
 	}
}

// JoeQuake's Demos Menu

void SearchForDemos (void)
{
	RDFlags |= RD_MENU_DEMOS;
	if (!demodir[0])
	{
		RDFlags |= RD_MENU_DEMOS_MAIN;
		ReadDir (com_basedir, "*");
	}
	else
	{
		ReadDir (va("%s%s", com_basedir, demodir), "*");
	}

	SaveCursorPos ();
}

void M_Menu_Demos_f (void)
{
	key_dest = key_menu;
	m_state = m_demos;
	refresh_demlist = true;
	m_entersound = true;

	SearchForDemos ();
}

void M_Demos_Draw (void)
{
	int ghost_text_y = 8 * (MAXLINES + 6);
	int ghost_text_x;
	int help_text_x = 28;
	int help_text_y = 8 * (MAXLINES + 8);
	char *ghost_demo_path_short;

	// Current directory
	M_Print (16, 16, demodir);

	// The file list itself
	M_List_Draw ("DEMOS", 24);

	// Current ghost
	if (ghost_demo_path[0] != '\0') {
		ghost_demo_path_short = ghost_demo_path;
		if (strncmp(ghost_demo_path, "../", 3) == 0) {
			ghost_demo_path_short += 2;
		}
		ghost_text_x = (320 - 8 * (7 + strlen(ghost_demo_path_short))) / 2;
		M_PrintWhite (ghost_text_x, ghost_text_y, "ghost: ");
		M_Print (ghost_text_x + 8 * 7, ghost_text_y, ghost_demo_path_short);
	}

	// Keyboard shortcut help text
	M_PrintWhite (help_text_x, help_text_y,
			      "enter:       ctrl-enter:          ");
	M_Print(help_text_x, help_text_y,
			      "       play              set ghost");
	M_PrintWhite (help_text_x, help_text_y + 8,
			      "  ctrl-shift-enter:            ");
	M_Print(help_text_x, help_text_y + 8,
			      "                    clear ghost");
}

void M_Demos_Key (int k)
{
	int		i;
	qboolean	worx;

	M_List_Key (k, num_files, MAXLINES);

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		if (searchbox)
		{
			KillSearchBox ();
		}
		else
		{
			Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));
			M_Menu_Main_f ();
		}
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (!num_files || filelist[list_base+list_cursor].type == 3)
			break;

		if (keydown[K_CTRL] && keydown[K_SHIFT])
		{
			Cbuf_AddText ("ghost_remove\n");
		}
		else if (filelist[list_cursor+list_base].type)
		{
			if (filelist[list_base+list_cursor].type == 2)
			{
				char	*p;

				if ((p = strrchr(demodir, '/')))
				{
					Q_strncpyz (prevdir, p + 1, sizeof(prevdir));
					*p = 0;
				}
			}
			else
			{
				strncat (demodir, va("/%s", filelist[list_base+list_cursor].name), sizeof(demodir) - 1);
			}
			SearchForDemos ();
		}
		else
		{
			if (keydown[K_CTRL] && !keydown[K_SHIFT])
			{
				Cbuf_AddText (va("ghost \"..%s/%s\"\n", demodir, filelist[list_base+list_cursor].name));
			}
			else
			{
				key_dest = key_game;
				m_state = m_none;
				Cbuf_AddText (va("playdemo \"..%s/%s\"\n", demodir, filelist[list_base+list_cursor].name));
			}
			Q_strncpyz (prevdir, filelist[list_base+list_cursor].name, sizeof(prevdir));
		}

		if (searchbox)
			KillSearchBox ();
		break;

	case K_BACKSPACE:
		if (strcmp(searchfile, ""))
			searchfile[--num_searchs] = 0;
		break;

	default:
		if (k < 32 || k > 127)
			break;

		searchbox = true;
		searchfile[num_searchs++] = k;
		worx = false;
		for (i=0 ; i<num_files ; i++)
		{
			if (strstr(filelist[i].name, searchfile) == filelist[i].name)
			{
				worx = true;
				S_LocalSound ("misc/menu1.wav");
				list_base = i - 10;
				if (list_base < 0 || num_files < MAXLINES)
				{
					list_base = 0;
					list_cursor = i;
				}
				else if (list_base > (num_files - MAXLINES))
				{
					list_base = num_files - MAXLINES;
					list_cursor = MAXLINES - (num_files - i);
				}
				else
					list_cursor = 10;
				break;
			}
		}
		if (!worx)
			searchfile[--num_searchs] = 0;
		break;
	}
}

//=============================================================================
/* MODS MENU */

void SearchForMods(void)
{
	RDFlags |= (RD_MENU_DEMOS | RD_MENU_DEMOS_MAIN);
	ReadDir(com_basedir, "*");

	SaveCursorPos();
}

void M_Menu_Mods_f(void)
{
	key_dest = key_menu;
	m_state = m_mods;
	m_entersound = true;

	SearchForMods();
}

void M_Mods_Draw(void)
{
	M_List_Draw("MODS", 24);
}

void M_Mods_Key(int k)
{
	int		i;
	qboolean	worx;
	extern void Draw_ReloadPics(void);

	M_List_Key(k, num_files, MAXLINES);

	switch (k)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		if (searchbox)
		{
			KillSearchBox();
		}
		else
		{
			Q_strncpyz(prevdir, filelist[list_base + list_cursor].name, sizeof(prevdir));
			M_Menu_Main_f();
		}
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (!num_files || filelist[list_base + list_cursor].type == 3)
			break;

		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText(va("disconnect\ngamedir %s\nexec quake.rc\n", filelist[list_base + list_cursor].name));
		Cbuf_Execute();
		Draw_ReloadPics();

		mod_changed = true;

		Q_strncpyz(prevdir, filelist[list_base + list_cursor].name, sizeof(prevdir));

		if (searchbox)
			KillSearchBox();
		break;

	case K_BACKSPACE:
		if (strcmp(searchfile, ""))
			searchfile[--num_searchs] = 0;
		break;

	default:
		if (k < 32 || k > 127)
			break;

		searchbox = true;
		searchfile[num_searchs++] = k;
		worx = false;
		for (i = 0; i<num_files; i++)
		{
			if (strstr(filelist[i].name, searchfile) == filelist[i].name)
			{
				worx = true;
				S_LocalSound("misc/menu1.wav");
				list_base = i - 10;
				if (list_base < 0)
				{
					list_base = 0;
					list_cursor = i;
				}
				else if (list_base >(num_files - MAXLINES))
				{
					list_base = num_files - MAXLINES;
					list_cursor = MAXLINES - (num_files - i);
				}
				else
					list_cursor = 10;
				break;
			}
		}
		if (!worx)
			searchfile[--num_searchs] = 0;
		break;
	}
}

qboolean M_Mods_Mouse_Event(const mouse_state_t *ms)
{
	int entries = min(num_files, MAXLINES);
	M_Mouse_Select(&list_window, ms, entries, &list_cursor);

	if (ms->button_up == 1) M_Mods_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Mods_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* QUIT MENU */

int		m_quit_prevstate;
qboolean	wasInMenus;

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
}

void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case K_ENTER:
	case K_MOUSE1: 
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit ();
		break;

	default:
		break;
	}

}

void M_Quit_Draw (void)
{
	static char *quitmsg[] = {
	"0JoeQuake " JOEQUAKE_VERSION,
	"1",
	"1http://joequake.runecentral.com",
	"1",
	"0Custom Quake engine",
	"0targeted for speedrunning",
	"1",
	"0Programming",
	"1Jozsef Szalontai",
	"1Sphere",
	"1Matthew Earl",
	"1",
	"0Id Software is not responsible for",
	"0providing technical support for",
	"0JoeQuake.",
	"1NOTICE: The copyright and trademark",
	"1 notices appearing  in your copy of",
	"1Quake(r) are not modified by the use",
	"1of JoeQuake and remain in full force.",
	"0Quake(tm) is a trademark of",
	"0Id Software, Inc.",
	NULL};
	char	**p;
	int	x, y;

	M_DrawTextBox (0, 4, 38, 22);
	y = 16;
	for (p = quitmsg ; *p ; p++, y += 8)
	{
		char	*c = *p;

		x = 16 + (36 - (strlen(c + 1))) * 4;
		if (*c == '2')
		{
			c++;
			while (*c != '0' && *c != '1')
			{
				M_DrawCharacter (x, y, *c++ | 128);
				x += 8;
			}
		}

		if (*c == '0')
			M_PrintWhite (x, y, c + 1);
		else
			M_Print (x, y, c + 1);
	}
}

qboolean M_Quit_Mouse_Event(const mouse_state_t *ms)
{
	if (ms->button_up == 1) M_Quit_Key(K_MOUSE1);
	if (ms->button_up == 2) M_Quit_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* SERIAL CONFIG MENU */

int	serialConfig_cursor;
int	serialConfig_cursor_table[] = {48, 64, 80, 96, 112, 132};
#define	NUM_SERIALCONFIG_CMDS	6

static	int	ISA_uarts[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
static	int	ISA_IRQs[] = {4, 3, 4, 3};
int		serialConfig_baudrate[] = {9600, 14400, 19200, 28800, 38400, 57600};

int	serialConfig_comport;
int	serialConfig_irq ;
int	serialConfig_baud;
char	serialConfig_phone[16];

void M_Menu_SerialConfig_f (void)
{
	int		n, port, baudrate;
	qboolean	useModem;

	key_dest = key_menu;
	m_state = m_serialconfig;
	m_entersound = true;
	if (JoiningGame && SerialConfig)
		serialConfig_cursor = 4;
	else
		serialConfig_cursor = 5;

	(*GetComPortConfig)(0, &port, &serialConfig_irq, &baudrate, &useModem);

	// map uart's port to COMx
	for (n=0 ; n<4 ; n++)
		if (ISA_uarts[n] == port)
			break;
	if (n == 4)
	{
		n = 0;
		serialConfig_irq = 4;
	}
	serialConfig_comport = n + 1;

	// map baudrate to index
	for (n=0 ; n<6 ; n++)
		if (serialConfig_baudrate[n] == baudrate)
			break;
	if (n == 6)
		n = 5;
	serialConfig_baud = n;

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

void M_SerialConfig_Draw (void)
{
	mpic_t	*p;
	int	basex;
	char	*startJoin, *directModem;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320 - p->width) >> 1;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (SerialConfig)
		directModem = "Modem";
	else
		directModem = "Direct Connect";
	M_Print (basex, 32, va("%s - %s", startJoin, directModem));
	basex += 8;

	M_Print (basex, serialConfig_cursor_table[0], "Port");
	M_DrawTextBox (160, 40, 4, 1);
	M_Print (168, serialConfig_cursor_table[0], va("COM%u", serialConfig_comport));

	M_Print (basex, serialConfig_cursor_table[1], "IRQ");
	M_DrawTextBox (160, serialConfig_cursor_table[1] - 8, 1, 1);
	M_Print (168, serialConfig_cursor_table[1], va("%u", serialConfig_irq));

	M_Print (basex, serialConfig_cursor_table[2], "Baud");
	M_DrawTextBox (160, serialConfig_cursor_table[2] - 8, 5, 1);
	M_Print (168, serialConfig_cursor_table[2], va("%u", serialConfig_baudrate[serialConfig_baud]));

	if (SerialConfig)
	{
		M_Print (basex, serialConfig_cursor_table[3], "Modem Setup...");
		if (JoiningGame)
		{
			M_Print (basex, serialConfig_cursor_table[4], "Phone number");
			M_DrawTextBox (160, serialConfig_cursor_table[4] - 8, 16, 1);
			M_Print (168, serialConfig_cursor_table[4], serialConfig_phone);
		}
	}

	if (JoiningGame)
	{
		M_DrawTextBox (basex, serialConfig_cursor_table[5] - 8, 7, 1);
		M_Print (basex + 8, serialConfig_cursor_table[5], "Connect");
	}
	else
	{
		M_DrawTextBox (basex, serialConfig_cursor_table[5] - 8, 2, 1);
		M_Print (basex + 8, serialConfig_cursor_table[5], "OK");
	}

	M_DrawCharacter (basex - 8, serialConfig_cursor_table[serialConfig_cursor], 12 + ((int)(realtime*4)&1));

	if (serialConfig_cursor == 4)
		M_DrawCharacter (168 + 8*strlen(serialConfig_phone), serialConfig_cursor_table[serialConfig_cursor], 10 + ((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}

void M_SerialConfig_Key (int key)
{
	int	l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		serialConfig_cursor--;
		if (serialConfig_cursor < 0)
			serialConfig_cursor = NUM_SERIALCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		serialConfig_cursor++;
		if (serialConfig_cursor >= NUM_SERIALCONFIG_CMDS)
			serialConfig_cursor = 0;
		break;

	case K_LEFTARROW:
		if (serialConfig_cursor > 2)
			break;
		S_LocalSound ("misc/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport--;
			if (serialConfig_comport == 0)
				serialConfig_comport = 4;
			serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
		}
		else if (serialConfig_cursor == 1)
		{
			serialConfig_irq--;
			if (serialConfig_irq == 6)
				serialConfig_irq = 5;
			if (serialConfig_irq == 1)
				serialConfig_irq = 7;
		}
		else if (serialConfig_cursor == 2)
		{
			serialConfig_baud--;
			if (serialConfig_baud < 0)
				serialConfig_baud = 5;
		}
		break;

	case K_RIGHTARROW:
		if (serialConfig_cursor > 2)
			break;
forward:
		S_LocalSound ("misc/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport++;
			if (serialConfig_comport > 4)
				serialConfig_comport = 1;
			serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
		}
		else if (serialConfig_cursor == 1)
		{
			serialConfig_irq++;
			if (serialConfig_irq == 6)
				serialConfig_irq = 7;
			if (serialConfig_irq == 8)
				serialConfig_irq = 2;
		}
		else if (serialConfig_cursor == 2)
		{
			serialConfig_baud++;
			if (serialConfig_baud > 5)
				serialConfig_baud = 0;
		}
		break;

	case K_ENTER:
		if (serialConfig_cursor < 3)
			goto forward;

		m_entersound = true;

		if (serialConfig_cursor == 3)
		{
			(*SetComPortConfig)(0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

			M_Menu_ModemConfig_f ();
			break;
		}
		else if (serialConfig_cursor == 4)
		{
			serialConfig_cursor = 5;
			break;
		}

		// serialConfig_cursor == 5 (OK/CONNECT)
		(*SetComPortConfig)(0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

		M_ConfigureNetSubsystem ();

		if (StartingGame)
		{
			M_Menu_GameOptions_f ();
			break;
		}

		m_return_state = m_state;
		m_return_onerror = true;
		key_dest = key_game;
		m_state = m_none;

		if (SerialConfig)
			Cbuf_AddText (va("connect \"%s\"\n", serialConfig_phone));
		else
			Cbuf_AddText ("connect\n");
		break;

	case K_BACKSPACE:
		if (serialConfig_cursor == 4)
		{
			if (strlen(serialConfig_phone))
				serialConfig_phone[strlen(serialConfig_phone)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (serialConfig_cursor == 4)
		{
			l = strlen(serialConfig_phone);
			if (l < 15)
			{
				serialConfig_phone[l] = key;
				serialConfig_phone[l+1] = 0;
			}
		}
		break;
	}

	if (DirectConfig && (serialConfig_cursor == 3 || serialConfig_cursor == 4))
		serialConfig_cursor = (key == K_UPARROW) ? 2 : 5;

	if (SerialConfig && StartingGame && serialConfig_cursor == 4)
		serialConfig_cursor = (key == K_UPARROW) ? 3 : 5;
}

//=============================================================================
/* MODEM CONFIG MENU */

int	modemConfig_cursor;
int	modemConfig_cursor_table[] = {40, 56, 88, 120, 156};
#define NUM_MODEMCONFIG_CMDS	5

char	modemConfig_dialing;
char	modemConfig_clear[16];
char	modemConfig_init[32];
char	modemConfig_hangup[16];

void M_Menu_ModemConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_modemconfig;
	m_entersound = true;
	(*GetModemConfig)(0, &modemConfig_dialing, modemConfig_clear, modemConfig_init, modemConfig_hangup);
}

void M_ModemConfig_Draw (void)
{
	mpic_t	*p;
	int	basex;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320 - p->width) >> 1;
	M_DrawPic (basex, 4, p);
	basex += 8;

	if (modemConfig_dialing == 'P')
		M_Print (basex, modemConfig_cursor_table[0], "Pulse Dialing");
	else
		M_Print (basex, modemConfig_cursor_table[0], "Touch Tone Dialing");

	M_Print (basex, modemConfig_cursor_table[1], "Clear");
	M_DrawTextBox (basex, modemConfig_cursor_table[1] + 4, 16, 1);
	M_Print (basex + 8, modemConfig_cursor_table[1] + 12, modemConfig_clear);
	if (modemConfig_cursor == 1)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_clear), modemConfig_cursor_table[1] + 12, 10 + ((int)(realtime*4)&1));

	M_Print (basex, modemConfig_cursor_table[2], "Init");
	M_DrawTextBox (basex, modemConfig_cursor_table[2] + 4, 30, 1);
	M_Print (basex+8, modemConfig_cursor_table[2] + 12, modemConfig_init);
	if (modemConfig_cursor == 2)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_init), modemConfig_cursor_table[2] + 12, 10 + ((int)(realtime*4)&1));

	M_Print (basex, modemConfig_cursor_table[3], "Hangup");
	M_DrawTextBox (basex, modemConfig_cursor_table[3] + 4, 16, 1);
	M_Print (basex + 8, modemConfig_cursor_table[3] + 12, modemConfig_hangup);
	if (modemConfig_cursor == 3)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_hangup), modemConfig_cursor_table[3] + 12, 10 + ((int)(realtime*4)&1));

	M_DrawTextBox (basex, modemConfig_cursor_table[4] - 8, 2, 1);
	M_Print (basex+8, modemConfig_cursor_table[4], "OK");

	M_DrawCharacter (basex - 8, modemConfig_cursor_table[modemConfig_cursor], 12 + ((int)(realtime*4)&1));
}

void M_ModemConfig_Key (int key)
{
	int	l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_SerialConfig_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		modemConfig_cursor--;
		if (modemConfig_cursor < 0)
			modemConfig_cursor = NUM_MODEMCONFIG_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		modemConfig_cursor++;
		if (modemConfig_cursor >= NUM_MODEMCONFIG_CMDS)
			modemConfig_cursor = 0;
		break;

	case K_LEFTARROW:
	case K_RIGHTARROW:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			S_LocalSound ("misc/menu1.wav");
		}
		break;

	case K_ENTER:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			m_entersound = true;
		}
		else if (modemConfig_cursor == 4)
		{
			(*SetModemConfig)(0, va("%c", modemConfig_dialing), modemConfig_clear, modemConfig_init, modemConfig_hangup);
			m_entersound = true;
			M_Menu_SerialConfig_f ();
		}
		break;

	case K_BACKSPACE:
		if (modemConfig_cursor == 1)
		{
			if (strlen(modemConfig_clear))
				modemConfig_clear[strlen(modemConfig_clear)-1] = 0;
		}
		else if (modemConfig_cursor == 2)
		{
			if (strlen(modemConfig_init))
				modemConfig_init[strlen(modemConfig_init)-1] = 0;
		}
		else if (modemConfig_cursor == 3)
		{
			if (strlen(modemConfig_hangup))
				modemConfig_hangup[strlen(modemConfig_hangup)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (modemConfig_cursor == 1)
		{
			l = strlen (modemConfig_clear);
			if (l < 15)
			{
				modemConfig_clear[l+1] = 0;
				modemConfig_clear[l] = key;
			}
		}
		else if (modemConfig_cursor == 2)
		{
			l = strlen (modemConfig_init);
			if (l < 29)
			{
				modemConfig_init[l+1] = 0;
				modemConfig_init[l] = key;
			}
		}
		else if (modemConfig_cursor == 3)
		{
			l = strlen (modemConfig_hangup);
			if (l < 15)
			{
				modemConfig_hangup[l+1] = 0;
				modemConfig_hangup[l] = key;
			}
		}
		break;
	}
}

//=============================================================================
/* LAN CONFIG MENU */

int	lanConfig_cursor = -1;
int	lanConfig_cursor_table[] = {72, 0, 88, 0, 0, 0, 120};
#define NUM_LANCONFIG_CMDS	7

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

menu_window_t lanConfig_window;

void M_Menu_LanConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 6;
		else
			lanConfig_cursor = 2;
	}
	if (StartingGame && lanConfig_cursor == 6)
		lanConfig_cursor = 2;
	lanConfig_port = DEFAULTnet_hostport;
	sprintf (lanConfig_portname, "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

void M_LanConfig_Draw (void)
{
	int		basex, lx = 0, ly = 0;
	char	*startJoin, *protocol;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320 - p->width) >> 1;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (IPXConfig)
		protocol = "IPX";
	else
		protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	if (IPXConfig)
		M_Print (basex+9*8, 52, my_ipx_address);
	else
		M_Print (basex+9*8, 52, my_tcpip_address);

	M_Print_GetPoint (basex, lanConfig_cursor_table[0], &lanConfig_window.x, &lanConfig_window.y, "Port", lanConfig_cursor == 0);
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print_GetPoint(basex, lanConfig_cursor_table[2], &lx, &ly, "Search for local games...", lanConfig_cursor == 2);
		M_Print (basex, 108, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[6]-8, 22, 1);
		M_Print_GetPoint(basex+16, lanConfig_cursor_table[6], &lx, &ly, lanConfig_joinname, lanConfig_cursor == 6);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[2]-8, 2, 1);
		M_Print_GetPoint(basex+8, lanConfig_cursor_table[2], &lx, &ly, "OK", lanConfig_cursor == 2);
		M_Print_GetPoint(basex+16, lanConfig_cursor_table[6], &lx, &ly, "", lanConfig_cursor == 6);	//dummy invisible line for menu mouse to work properly
	}

	lanConfig_window.w = 26 * 8; // presume 8 pixels for each letter
	lanConfig_window.h = ly - lanConfig_window.y + 8;

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table[0], 10+((int)(realtime*4)&1));
	else if (JoiningGame && lanConfig_cursor == 6)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table[6], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);

	// don't draw cursor if we're on a spacing line
	if (lanConfig_cursor == 1 || lanConfig_cursor == 3 || lanConfig_cursor == 4 || lanConfig_cursor == 5 || 
		(!JoiningGame && lanConfig_cursor == 6))
		return;

	// cursor
	M_DrawCharacter(basex - 8, lanConfig_cursor_table[lanConfig_cursor], 12 + ((int)(realtime * 4) & 1));
}

void M_LanConfig_Key (int key)
{
	int	l;

	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (lanConfig_cursor == 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
		else if (lanConfig_cursor == 2)
			lanConfig_cursor = 0;
		else
			lanConfig_cursor = 2;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (lanConfig_cursor == 0)
			lanConfig_cursor = 2;
		else if (lanConfig_cursor == 2)
			lanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
		else
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 2)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_Search_f ();
			break;
		}
		else if (lanConfig_cursor == 6)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText (va("connect \"%s\"\n", lanConfig_joinname));
			break;
		}
		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}
		else if (lanConfig_cursor == 6)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (lanConfig_cursor == 6)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l] = key;
				lanConfig_joinname[l+1] = 0;
			}
		}

		if (key < '0' || key > '9')
			break;

		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l] = key;
				lanConfig_portname[l+1] = 0;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 6)
		lanConfig_cursor = (key == K_UPARROW) ? 2 : 0;

	l = Q_atoi(lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	sprintf (lanConfig_portname, "%u", lanConfig_port);
}

qboolean M_LanConfig_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&lanConfig_window, ms, NUM_LANCONFIG_CMDS, &lanConfig_cursor);

	if (ms->button_up == 1) M_LanConfig_Key(K_MOUSE1);
	if (ms->button_up == 2) M_LanConfig_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t	levels[] =
{
	{"start", "Entrance"},			// 0

	{"e1m1", "Slipgate Complex"},		// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},		// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},	// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},		// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},	// 31

	{"dm1", "Place of Two Deaths"},		// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t	hipnoticlevels[] =
{
	{"start", "Command HQ"},		// 0

	{"hip1m1", "The Pumping Station"},	// 1
	{"hip1m2", "Storage Facility"},
	{"hip1m3", "The Lost Mine"},
	{"hip1m4", "Research Facility"},
	{"hip1m5", "Military Complex"},

	{"hip2m1", "Ancient Realms"},		// 6
	{"hip2m2", "The Black Cathedral"},
	{"hip2m3", "The Catacombs"},
	{"hip2m4", "The Crypt"},
	{"hip2m5", "Mortum's Keep"},
	{"hip2m6", "The Gremlin's Domain"},

	{"hip3m1", "Tur Torment"},		// 12
	{"hip3m2", "Pandemonium"},
	{"hip3m3", "Limbo"},
	{"hip3m4", "The Gauntlet"},

	{"hipend", "Armagon's Lair"},		// 16

	{"hipdm1", "The Edge of Oblivion"}	// 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t	roguelevels[] =
{
	{"start", "Split Decision"},

	{"r1m1", "Deviant's Domain"},
	{"r1m2", "Dread Portal"},
	{"r1m3", "Judgement Call"},
	{"r1m4", "Cave of Death"},
	{"r1m5", "Towers of Wrath"},
	{"r1m6", "Temple of Pain"},
	{"r1m7", "Tomb of the Overlord"},

	{"r2m1", "Tempus Fugit"},
	{"r2m2", "Elemental Fury I"},
	{"r2m3", "Elemental Fury II"},
	{"r2m4", "Curse of Osiris"},
	{"r2m5", "Wizard's Keep"},
	{"r2m6", "Blood Sacrifice"},
	{"r2m7", "Last Bastion"},
	{"r2m8", "Source of Evil"},

	{"ctf1", "Division of Change"}
};

typedef struct
{
	char	*description;
	int	firstLevel;
	int	levels;
} episode_t;

episode_t episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t hipnoticepisodes[] =
{
	{"Scourge of Armagon", 0, 1},
	{"Fortress of the Dead", 1, 5},
	{"Dominion of Darkness", 6, 6},
	{"The Rift", 12, 4},
	{"Final Level", 16, 1},
	{"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

int	startepisode;
int	startlevel;
int	maxplayers;
qboolean m_serverInfoMessage = false;
double	m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}

int	gameoptions_cursor_table[] = {40, 0, 56, 64, 72, 80, 88, 96, 0, 112, 120};
#define	NUM_GAMEOPTIONS	11
int	gameoptions_cursor;

menu_window_t gameoptions_window;

void M_GameOptions_Draw (void)
{
	mpic_t	*p;
	int		x, lx, ly;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print_GetPoint (160, 40, &gameoptions_window.x, &gameoptions_window.y, "begin game", gameoptions_cursor == 0);

	M_Print (0, 56, "      Max players");
	M_Print_GetPoint(160, 56, &lx, &ly, va("%i", maxplayers), gameoptions_cursor == 2);

	M_Print (0, 64, "        Game Type");
	if (coop.value)
		M_Print_GetPoint(160, 64, &lx, &ly, "Cooperative", gameoptions_cursor == 3);
	else
		M_Print_GetPoint(160, 64, &lx, &ly, "Deathmatch", gameoptions_cursor == 3);

	M_Print (0, 72, "        Teamplay");
	if (rogue)
	{
		char	*msg;

		switch ((int)teamplay.value)
		{
		case 1:
			msg = "No Friendly Fire";
			break;

		case 2:
			msg = "Friendly Fire";
			break;

		case 3:
			msg = "Tag";
			break;

		case 4:
			msg = "Capture the Flag";
			break;

		case 5:
			msg = "One Flag CTF";
			break;

		case 6:
			msg = "Three Team CTF";
			break;

		default:
			msg = "Off";
			break;
		}
		M_Print_GetPoint(160, 72, &lx, &ly, msg, gameoptions_cursor == 4);
	}
	else
	{
		char	*msg;

		switch ((int)teamplay.value)
		{
		case 1:
			msg = "No Friendly Fire";
			break;

		case 2:
			msg = "Friendly Fire";
			break;

		default:
			msg = "Off";
			break;
		}
		M_Print_GetPoint(160, 72, &lx, &ly, msg, gameoptions_cursor == 4);
	}

	M_Print (0, 80, "            Skill");
	if (skill.value == 0)
		M_Print_GetPoint(160, 80, &lx, &ly, "Easy difficulty", gameoptions_cursor == 5);
	else if (skill.value == 1)
		M_Print_GetPoint(160, 80, &lx, &ly, "Normal difficulty", gameoptions_cursor == 5);
	else if (skill.value == 2)
		M_Print_GetPoint(160, 80, &lx, &ly, "Hard difficulty", gameoptions_cursor == 5);
	else
		M_Print_GetPoint(160, 80, &lx, &ly, "Nightmare difficulty", gameoptions_cursor == 5);

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit.value == 0)
		M_Print_GetPoint(160, 88, &lx, &ly, "none", gameoptions_cursor == 6);
	else
		M_Print_GetPoint(160, 88, &lx, &ly, va("%i frags", (int)fraglimit.value), gameoptions_cursor == 6);

	M_Print (0, 96, "       Time Limit");
	if (timelimit.value == 0)
		M_Print_GetPoint(160, 96, &lx, &ly, "none", gameoptions_cursor == 7);
	else
		M_Print_GetPoint(160, 96, &lx, &ly, va("%i minutes", (int)timelimit.value), gameoptions_cursor == 7);

	M_Print (0, 112, "         Episode");

//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
		M_Print_GetPoint(160, 112, &lx, &ly, hipnoticepisodes[startepisode].description, gameoptions_cursor == 9);
//PGM 01/07/97 added rogue episodes
	else if (rogue)
		M_Print_GetPoint(160, 112, &lx, &ly, rogueepisodes[startepisode].description, gameoptions_cursor == 9);
	else
		M_Print_GetPoint(160, 112, &lx, &ly, episodes[startepisode].description, gameoptions_cursor == 9);

	M_Print (0, 120, "           Level");

//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
	{
		M_Print_GetPoint(160, 120, &lx, &ly, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description, gameoptions_cursor == 10);
		M_PrintWhite(160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
	}
//PGM 01/07/97 added rogue episodes
	else if (rogue)
	{
		M_Print_GetPoint(160, 120, &lx, &ly, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description, gameoptions_cursor == 10);
		M_PrintWhite(160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
	}
	else
	{
		M_Print_GetPoint(160, 120, &lx, &ly, levels[episodes[startepisode].firstLevel + startlevel].description, gameoptions_cursor == 10);
		M_PrintWhite(160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
	}

	gameoptions_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	gameoptions_window.h = ly - gameoptions_window.y + 8;

	// don't draw cursor if we're on a spacing line
	if (gameoptions_cursor == 1 || gameoptions_cursor == 8)
		return;

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320 - 26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ");
			M_Print (x, 154, " requires using command ");
			M_Print (x, 162, "line parameters; please ");
			M_Print (x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}

void M_NetStart_Change (int dir)
{
	int	count;

	switch (gameoptions_cursor)
	{
	case 2:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 3:
		Cvar_SetValue (&coop, coop.value ? 0 : 1);
		break;

	case 4:
		count = rogue ? 6 : 2;
		Cvar_SetValue (&teamplay, teamplay.value + dir);
		if (teamplay.value > count)
			Cvar_SetValue (&teamplay, 0);
		else if (teamplay.value < 0)
			Cvar_SetValue (&teamplay, count);
		break;

	case 5:
		Cvar_SetValue (&skill, skill.value + dir);
		if (skill.value > 3)
			Cvar_SetValue (&skill, 0);
		if (skill.value < 0)
			Cvar_SetValue (&skill, 3);
		break;

	case 6:
		Cvar_SetValue (&fraglimit, fraglimit.value + dir*10);
		if (fraglimit.value > 100)
			Cvar_SetValue (&fraglimit, 0);
		if (fraglimit.value < 0)
			Cvar_SetValue (&fraglimit, 100);
		break;

	case 7:
		Cvar_SetValue (&timelimit, timelimit.value + dir*5);
		if (timelimit.value > 60)
			Cvar_SetValue (&timelimit, 0);
		if (timelimit.value < 0)
			Cvar_SetValue (&timelimit, 60);
		break;

	case 9:
		startepisode += dir;
	//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
	//PGM 01/07/97 added rogue count
	//PGM 03/02/97 added 1 for dmatch episode
		else if (rogue)
			count = 4;
		else if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 10:
		startlevel += dir;
	//MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
	//PGM added rogue episodes
		else if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
			count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		if (gameoptions_cursor == 1 || gameoptions_cursor == 8)
			gameoptions_cursor--;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		if (gameoptions_cursor == 1 || gameoptions_cursor == 8)
			gameoptions_cursor++;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText (va("maxplayers %u\n", maxplayers));
			SCR_BeginLoadingPlaque ();

			if (hipnotic)
				Cbuf_AddText (va("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name));
			else if (rogue)
				Cbuf_AddText (va("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name));
			else
				Cbuf_AddText (va("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name));

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

qboolean M_GameOptions_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&gameoptions_window, ms, NUM_GAMEOPTIONS, &gameoptions_cursor);

	if (ms->button_up == 1) M_GameOptions_Key(K_MOUSE1);
	if (ms->button_up == 2) M_GameOptions_Key(K_MOUSE2);

	return true;
}

//=============================================================================
/* SEARCH MENU */

qboolean searchComplete = false;
double	searchCompleteTime;

void M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;

	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f ();
}

void M_Search_Draw (void)
{
	mpic_t	*p;
	int	x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	x = (320/2) - ((12*8)/2) + 4;
	M_DrawTextBox (x-8, 32, 12, 1);
	M_Print (x, 40, "Searching...");

	if (slistInProgress)
	{
		NET_Poll ();
		return;
	}

	if (!searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_FoundServers_f ();
		return;
	}

	M_PrintWhite ((320/2) - ((22*8)/2), 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}

void M_Search_Key (int key)
{
}

int	servers_cursor;
qboolean servers_sorted;

void M_Menu_FoundServers_f (void)
{
	key_dest = key_menu;
	m_state = m_servers;
	m_entersound = true;

	servers_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	servers_sorted = false;
}

void M_FoundServers_Draw (void)
{
	int	n;
	char	string[64];
	mpic_t	*p;

	if (!servers_sorted)
	{
		if (hostCacheCount > 1)
		{
			int		i, j;
			hostcache_t	temp;

			for (i=0 ; i<hostCacheCount ; i++)
			{
				for (j=i+1 ; j<hostCacheCount ; j++)
				{
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						memcpy (&temp, &hostcache[j], sizeof(hostcache_t));
						memcpy (&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						memcpy (&hostcache[i], &temp, sizeof(hostcache_t));
					}
				}
			}
		}
		servers_sorted = true;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	for (n=0 ; n<hostCacheCount ; n++)
	{
		if (hostcache[n].maxusers)
			sprintf (string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			sprintf (string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print (16, 32 + 8*n, string);
	}
	M_DrawCharacter (0, 32 + servers_cursor*8, 12+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (16, 148, m_return_reason);
}

void M_FoundServers_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		servers_cursor--;
		if (servers_cursor < 0)
			servers_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		servers_cursor++;
		if (servers_cursor >= hostCacheCount)
			servers_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		servers_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText (va("connect \"%s\"\n", hostcache[servers_cursor].cname));
		break;

	default:
		break;
	}
}

//=============================================================================
/* SERVER LIST MENU */

#define	MENU_X	50
#define	MENU_Y	21
#define TITLE_Y 4
#define	STAT_Y	166

int	slist_cursor = 0, slist_mins = 0, slist_maxs = 15, slist_state;

void M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;

	slist_state = 0;
}

void M_ServerList_Draw (void)
{
	int	serv, line;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	M_DrawTextBox (MENU_X, TITLE_Y, 23, 1);
	M_PrintWhite (MENU_X + 60, TITLE_Y + 8, "Server List");

	if (!slist[0].server)
	{
		M_PrintWhite (84, MENU_Y + 16 + 16, "Empty server list");
		M_Print (60, MENU_Y + 16 + 32, "Press INS to add a server");
		M_Print (64, MENU_Y + 16 + 40, "Press E to edit a server");
		return;
	}

	M_DrawTextBox (MENU_X, STAT_Y, 23, 1);
	M_DrawTextBox (MENU_X, MENU_Y, 23, slist_maxs - slist_mins + 1);
	for (serv = slist_mins, line = 1 ; serv <= slist_maxs && serv < MAX_SERVER_LIST && slist[serv].server ; serv++, line++)
		M_Print (MENU_X + 18, line * 8 + MENU_Y, va("%1.21s", slist[serv].description));
	M_PrintWhite (MENU_X, STAT_Y - 4, "INS = add server, E = edit");
	M_PrintWhite (MENU_X + 18, STAT_Y + 8, va("%1.22s", slist[slist_cursor].server));
	M_DrawCharacter (MENU_X + 8, (slist_cursor - slist_mins + 1) * 8 + MENU_Y, 12+((int)(realtime*4)&1));
}

void M_ServerList_Key (key)
{
	int	slist_length;

	if (!slist[0].server && key != K_ESCAPE && key != K_INS)
		return;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (slist_cursor > 0)
		{
			if (keydown[K_CTRL])
				SList_Switch (slist_cursor, slist_cursor - 1);
			slist_cursor--;
		}
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (keydown[K_CTRL])
		{
			if (slist_cursor != SList_Length() - 1)
			{
				SList_Switch (slist_cursor, slist_cursor + 1);
				slist_cursor++;
			}
		}
		else if (slist_cursor < MAX_SERVER_LIST - 1 && slist[slist_cursor+1].server)
			slist_cursor++;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor = SList_Length() - 1;
		break;
		
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor -= (slist_maxs - slist_mins);
		if (slist_cursor < 0)
			slist_cursor = 0;
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor += (slist_maxs - slist_mins);
		if (slist_cursor >= MAX_SERVER_LIST)
			slist_cursor = MAX_SERVER_LIST - 1;
		while (!slist[slist_cursor].server)
			slist_cursor--;
		break;

	case K_ENTER:
		if (keydown[K_CTRL])
		{
			M_Menu_SEdit_f ();
			break;
		}
		m_state = m_main;
		M_ToggleMenu_f ();
		Cbuf_AddText (va("connect \"%s\"\n", slist[slist_cursor].server));
		break;

	case 'e':
	case 'E':
		M_Menu_SEdit_f ();
		break;

	case K_INS:
		S_LocalSound ("misc/menu2.wav");
		if ((slist_length = SList_Length()) < MAX_SERVER_LIST)
		{
			if (keydown[K_CTRL] && slist_length > 0)
			{
				if (slist_cursor < slist_length - 1)
					memmove (&slist[slist_cursor+2], &slist[slist_cursor+1], (slist_length - slist_cursor - 1) * sizeof(slist[0]));
				SList_Reset_NoFree (slist_cursor + 1);
				SList_Set (slist_cursor + 1, "127.0.0.1", "<BLANK>");
				if (slist_length)
					slist_cursor++;
			}
			else
			{
				memmove (&slist[slist_cursor+1], &slist[slist_cursor], (slist_length - slist_cursor) * sizeof(slist[0]));
				SList_Reset_NoFree (slist_cursor);
				SList_Set (slist_cursor, "127.0.0.1", "<BLANK>");
			}
		}
		break;

	case K_DEL:
		S_LocalSound("misc/menu2.wav");
		if ((slist_length = SList_Length()) > 0)
		{
			SList_Reset (slist_cursor);
			if (slist_cursor > 0 && slist_length - 1 == slist_cursor)
			{
				slist_cursor--;
			}
			else
			{
				memmove (&slist[slist_cursor], &slist[slist_cursor+1], (slist_length - slist_cursor - 1) * sizeof(slist[0]));
				SList_Reset_NoFree (slist_length - 1);
			}
		}
		break;
	}

	if (slist_cursor < slist_mins)
	{
		slist_maxs -= (slist_mins - slist_cursor);
		slist_mins = slist_cursor;
	}
	if (slist_cursor > slist_maxs)
	{
		slist_mins += (slist_cursor - slist_maxs);
		slist_maxs = slist_cursor;
	}
}

#define	SERV_X	60
#define	SERV_Y	64
#define	DESC_X	60
#define	DESC_Y	40
#define	SERV_L	23
#define	DESC_L	23

#define	SLIST_BUFSIZE	128

static	char	slist_serv[SLIST_BUFSIZE], slist_desc[SLIST_BUFSIZE];
static	int	slist_serv_max, slist_serv_min, slist_desc_max, slist_desc_min, sedit_state;

void M_Menu_SEdit_f (void)
{
	int	size;

	key_dest = key_menu;
	m_state = m_sedit;
	m_entersound = true;

	sedit_state = 0;
	Q_strncpyz (slist_serv, slist[slist_cursor].server, sizeof(slist_serv));
	Q_strncpyz (slist_desc, slist[slist_cursor].description, sizeof(slist_desc));
	slist_serv_max = (size = strlen(slist_serv)) > SERV_L ? size : SERV_L;
	slist_serv_min = slist_serv_max - SERV_L;
	slist_desc_max = (size = strlen(slist_desc)) > DESC_L ? size : DESC_L;
	slist_desc_min = slist_desc_max - DESC_L;
}

void M_SEdit_Draw (void)
{
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	M_DrawTextBox (SERV_X, SERV_Y, 23, 1);
	M_DrawTextBox (DESC_X, DESC_Y, 23, 1);
	M_PrintWhite (SERV_X, SERV_Y - 4, "Hostname/IP:");
	M_PrintWhite (DESC_X, DESC_Y - 4, "Description:");
	M_Print (SERV_X + 9, SERV_Y + 8, va("%1.23s", slist_serv + slist_serv_min));
	M_Print (DESC_X + 9, DESC_Y + 8, va("%1.23s", slist_desc + slist_desc_min));
	if (sedit_state == 0)
		M_DrawCharacter (SERV_X + 9 + 8*(strlen(slist_serv) - slist_serv_min), SERV_Y + 8, 10+((int)(realtime*4)&1));
	else
		M_DrawCharacter (DESC_X + 9 + 8*(strlen(slist_desc) - slist_desc_min), DESC_Y + 8, 10+((int)(realtime*4)&1));
}

void M_SEdit_Key (int key)
{
	int	l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_ServerList_f ();
		break;

	case K_ENTER:
		SList_Set (slist_cursor, slist_serv, slist_desc);
		M_Menu_ServerList_f ();
		break;

	case K_UPARROW:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		sedit_state = !sedit_state;
		break;

	case K_BACKSPACE:
		if (sedit_state == 0)
		{
			if ((l = strlen(slist_serv)))
				slist_serv[--l] = 0;
			if (strlen(slist_serv) - 6 < slist_serv_min && slist_serv_min)
			{
				slist_serv_min--;
				slist_serv_max--;
			}
		}
		else
		{
			if ((l = strlen(slist_desc)))
				slist_desc[--l] = 0;
			if (strlen(slist_desc) - 6 < slist_desc_min && slist_desc_min)
			{
				slist_desc_min--;
				slist_desc_max--;
			}
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (sedit_state == 0)
		{
			l = strlen (slist_serv);
			if (l < SLIST_BUFSIZE - 1)
			{
				slist_serv[l+1] = 0;
				slist_serv[l] = key;
				l++;
			}
			if (l > slist_serv_max)
			{
				slist_serv_min++;
				slist_serv_max++;
			}
		}
		else
		{
			l = strlen (slist_desc);
			if (l < SLIST_BUFSIZE - 1)
			{
				slist_desc[l+1] = 0;
				slist_desc[l] = key;
				l++;
			}
			if (l > slist_desc_max)
			{
				slist_desc_min++;
				slist_desc_max++;
			}
		}
		break;
	}
}

//=============================================================================
/* HELP MENU */	// joe: I decided to left it in, coz svc_sellscreen use it

#define	NUM_HELP_PAGES	6
int	help_page;

void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}

void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic(va("gfx/help%i.lmp", help_page)));
}

void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES - 1;
		break;
	}
}

//=============================================================================
/* Credits menu -- used by the 2021 re-release */

void M_Menu_Credits_f(void)
{
}

//=============================================================================
/* Menu Subsystem */

void M_Init (void)
{
	Cvar_Register (&scr_centermenu);

	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_slist", M_Menu_ServerList_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_namemaker", M_Menu_NameMaker_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_videomodes", M_Menu_VideoModes_f);
	Cmd_AddCommand("menu_mouse", M_Menu_Mouse_f);
	Cmd_AddCommand("menu_miscellaneous", M_Menu_Misc_f);
	Cmd_AddCommand("menu_hud", M_Menu_Hud_f);
	Cmd_AddCommand("menu_crosshair_colorchooser", M_Menu_Crosshair_ColorChooser_f);
	Cmd_AddCommand("menu_sound", M_Menu_Sound_f);
#ifdef GLQUAKE
	Cmd_AddCommand ("menu_view", M_Menu_View_f);
	Cmd_AddCommand ("menu_particles", M_Menu_Particles_f);
	Cmd_AddCommand("menu_renderer", M_Menu_Renderer_f);
	Cmd_AddCommand("menu_textures", M_Menu_Textures_f);
	Cmd_AddCommand("menu_decals", M_Menu_Decals_f);
	Cmd_AddCommand("menu_weapons", M_Menu_Weapons_f);
	Cmd_AddCommand("menu_screenflashes", M_Menu_ScreenFlashes_f);
	Cmd_AddCommand("menu_sky_colorchooser", M_Menu_Sky_ColorChooser_f);
#endif
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_maps", M_Menu_Maps_f);
	Cmd_AddCommand("menu_mods", M_Menu_Mods_f);
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);

	if (machine)
		Cmd_AddCommand("menu_credits", M_Menu_Credits_f); // needed by the 2021 re-release
}

void M_Draw (void)
{
#ifdef GLQUAKE
	double aspect;
#endif

	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current == vid.height)	// console is full screen
		{
			Draw_ConsoleBackground (scr_con_current); // joe: was vid.height
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		}
		else
		{
			Draw_FadeScreen ();
		}

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

#ifdef GLQUAKE
	menuwidth = 480;
	aspect = (double)vid.height / (double)vid.width;
	menuheight = (int)(menuwidth * aspect);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0, menuwidth, menuheight, 0, -99999, 99999);
#endif

	m_yofs = scr_centermenu.value ? (menuheight - 200) / 2 : 0;

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_load:
		M_Load_Draw ();
		break;

	case m_save:
		M_Save_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
		M_Setup_Draw ();
		break;

	case m_namemaker:
		M_NameMaker_Draw ();
		break;

	case m_net:
		M_Net_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_mouse:
		M_Mouse_Draw();
		break;

	case m_misc:
		M_Misc_Draw();
		break;

	case m_hud:
		M_Hud_Draw();
		break;

	case m_crosshair_colorchooser:
		M_ColorChooser_Draw(cs_crosshair);
		break;

	case m_sound:
		M_Sound_Draw();
		break;

#ifdef GLQUAKE
	case m_view:
		M_View_Draw ();
		break;

	case m_renderer:
		M_Renderer_Draw();
		break;

	case m_textures:
		M_Textures_Draw();
		break;

	case m_particles:
		M_Particles_Draw ();
		break;

	case m_decals:
		M_Decals_Draw();
		break;

	case m_weapons:
		M_Weapons_Draw();
		break;

	case m_screenflashes:
		M_ScreenFlashes_Draw();
		break;

	case m_sky_colorchooser:
		M_ColorChooser_Draw(cs_sky);
		break;

	case m_outline_colorchooser:
		M_ColorChooser_Draw(cs_outline);
		break;
#endif

	case m_videomodes:
		M_VideoModes_Draw ();
		break;

	case m_nehdemos:
		M_NehDemos_Draw ();
		break;

	case m_maps:
		M_Maps_Draw ();
		break;

	case m_demos:
#ifdef GLQUAKE
		browserscale = max(vid.width / 8 / 120, 1);
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho (0, vid.width / browserscale, vid.height / browserscale, 0, -99999, 99999);
		M_Demos_Display(vid.width / browserscale, vid.height / browserscale);
#endif
		break;

	case m_mods:
		M_Mods_Draw();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_quit:
		if (!cl_confirmquit.value)
		{
			key_dest = key_console;
			Host_Quit();
			return;
		}
		M_Quit_Draw ();
		break;

	case m_serialconfig:
		M_SerialConfig_Draw ();
		break;

	case m_modemconfig:
		M_ModemConfig_Draw ();
		break;

	case m_lanconfig:
		M_LanConfig_Draw ();
		break;

	case m_gameoptions:
		M_GameOptions_Draw ();
		break;

	case m_search:
		M_Search_Draw ();
		break;

	case m_servers:
		M_FoundServers_Draw ();
		break;

	case m_slist:
		M_ServerList_Draw ();
		break;

	case m_sedit:
		M_SEdit_Draw ();
		break;
	}

#ifdef GLQUAKE
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0, vid.width, vid.height, 0, -99999, 99999);
#endif

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:			return;
	case m_main:			M_Main_Key (key); return;
	case m_singleplayer:	M_SinglePlayer_Key (key); return;
	case m_load:			M_Load_Key (key); return;
	case m_save:			M_Save_Key (key); return;
	case m_multiplayer:		M_MultiPlayer_Key (key); return;
	case m_setup:			M_Setup_Key (key); return;
	case m_namemaker:		M_NameMaker_Key (key); return;
	case m_net:				M_Net_Key (key); return;
	case m_options:			M_Options_Key (key); return;
	case m_keys:			M_Keys_Key (key); return;
	case m_mouse:			M_Mouse_Key(key); return;
	case m_misc:			M_Misc_Key(key); break;
	case m_hud:				M_Hud_Key(key); break;
	case m_crosshair_colorchooser: M_ColorChooser_Key(key, cs_crosshair); break;
	case m_sound:			M_Sound_Key(key); break;
#ifdef GLQUAKE
	case m_view:			M_View_Key (key); return;
	case m_renderer:		M_Renderer_Key(key); return;
	case m_textures:		M_Textures_Key(key); return;
	case m_particles:		M_Particles_Key (key); return;
	case m_decals:			M_Decals_Key(key); return;
	case m_weapons:			M_Weapons_Key(key); return;
	case m_screenflashes:	M_ScreenFlashes_Key(key); return;
	case m_sky_colorchooser: M_ColorChooser_Key(key, cs_sky); break;
	case m_outline_colorchooser: M_ColorChooser_Key(key, cs_outline); break;
#endif
	case m_videomodes:		M_VideoModes_Key (key); return;
	case m_nehdemos:		M_NehDemos_Key (key); return;
	case m_maps:			M_Maps_Key (key); return;
	case m_demos:			M_Demos_KeyHandle (key); return;
	case m_mods:			M_Mods_Key(key); return;
	case m_help:			M_Help_Key (key); return;
	case m_quit:			M_Quit_Key (key); return;
	case m_serialconfig:	M_SerialConfig_Key (key); return;
	case m_modemconfig:		M_ModemConfig_Key (key); return;
	case m_lanconfig:		M_LanConfig_Key (key); return;
	case m_gameoptions:		M_GameOptions_Key (key); return;
	case m_search:			M_Search_Key (key); break;
	case m_servers:			M_FoundServers_Key (key); break;
	case m_slist:			M_ServerList_Key (key); return;
	case m_sedit:			M_SEdit_Key (key); break;
	}
}

qboolean Menu_Mouse_Event(const mouse_state_t* ms)
{
	if (ms->button_down == K_MWHEELDOWN || ms->button_up == K_MWHEELDOWN ||
		ms->button_down == K_MWHEELUP || ms->button_up == K_MWHEELUP)
	{
		// menus do not handle this type of mouse wheel event, they accept it as a key event	
		return false;
	}

	// send the mouse state to appropriate modules here
	// functions should report if they handled the event or not
	switch (m_state) 
	{
	case m_none: default:	return false;
	case m_main:			return M_Main_Mouse_Event(ms);
	case m_singleplayer:	return M_SinglePlayer_Mouse_Event(ms);
	case m_multiplayer:		return M_MultiPlayer_Mouse_Event(ms);
	case m_load:			return M_Load_Mouse_Event(ms);
	case m_save:			return M_Save_Mouse_Event(ms);
	case m_options:			return M_Options_Mouse_Event(ms);
	case m_keys:			return M_Keys_Mouse_Event(ms);
	case m_mouse:			return M_Mouse_Mouse_Event(ms);
	case m_hud:				return M_Hud_Mouse_Event(ms);
	case m_sound:			return M_Sound_Mouse_Event(ms);
	case m_view:			return M_View_Mouse_Event(ms);
	case m_renderer:		return M_Renderer_Mouse_Event(ms);
	case m_textures:		return M_Textures_Mouse_Event(ms);
	case m_particles:		return M_Particles_Mouse_Event(ms);
	case m_decals:			return M_Decals_Mouse_Event(ms);
	case m_weapons:			return M_Weapons_Mouse_Event(ms);
	case m_screenflashes:	return M_ScreenFlashes_Mouse_Event(ms);
	case m_misc:			return M_Misc_Mouse_Event(ms);
	case m_videomodes:		return M_Video_Mouse_Event(ms);
	case m_setup:			return M_Setup_Mouse_Event(ms);
	case m_demos:			return M_Demos_Mouse_Event(ms);
	case m_maps:			return M_Maps_Mouse_Event(ms);
	case m_mods:			return M_Mods_Mouse_Event(ms);
	case m_crosshair_colorchooser: return M_ColorChooser_Mouse_Event(ms, cs_crosshair);
	case m_sky_colorchooser: return M_ColorChooser_Mouse_Event(ms, cs_sky);
	case m_outline_colorchooser: return M_ColorChooser_Mouse_Event(ms, cs_outline);
	case m_net:				return M_Net_Mouse_Event(ms);
	case m_lanconfig:		return M_LanConfig_Mouse_Event(ms);
	case m_quit:			return M_Quit_Mouse_Event(ms);
	case m_namemaker:		return M_NameMaker_Mouse_Event(ms);
	case m_gameoptions:		return M_GameOptions_Mouse_Event(ms);
	}
	return false;
}

void M_ConfigureNetSubsystem (void)
{
// enable/disable net systems to match desired config
	Cbuf_AddText ("stopdemo\n");
	if (SerialConfig || DirectConfig)
		Cbuf_AddText ("com1 enable\n");

	if (IPXConfig || TCPIPConfig)
		net_hostport = lanConfig_port;
}
