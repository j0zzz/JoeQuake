#include "quakedef.h"

#define SPEED_DRAW_CHARS	6
#define MAP_NAME_DRAW_CHARS	12
#define HIDE_TIME	1.5f


typedef struct
{
	int top;
	int char_size;
	int bar_x, bar_y, bar_width, bar_num_chars;
	int skip_prev_x, skip_next_x, skip_y;
	int time_y;
	int speed_prev_x, speed_next_x, speed_y;
	int map_min_num, map_max_num, map_x, map_y, map_width, map_height;
} layout_t;


typedef enum
{
	HOVER_NONE,
	HOVER_SEEK,
	HOVER_SKIP_PREV,
	HOVER_SKIP,
	HOVER_SKIP_NEXT,
	HOVER_SPEED_PREV,
	HOVER_SPEED,
	HOVER_SPEED_NEXT,
} hover_t;


qboolean demoui_dragging_seek;
static double last_event_time = -1.0f;
static qboolean over_ui = false;
static qboolean map_menu_open = false;
static hover_t hover;
static int hover_map_idx = -1;

static void
ChangeSpeed (int change)
{
	static const float levels[] = {0.25, 0.5, 0.8, 1.0, 1.25, 2, 4};
	static const int num_levels = sizeof(levels) / sizeof(float);
	int i, current_level;

	// Find level nearest to cl_demospeed
	current_level = 0;
	for (i = 0; i < num_levels - 1; i++)
		if (cl_demospeed.value > (levels[i] + levels[i + 1]) * 0.5)
			current_level = i + 1;

	current_level = (current_level + change) % num_levels;
	if (current_level < 0)
		current_level += num_levels;
	Cvar_SetValue(&cl_demospeed, levels[current_level]);
}


static qboolean
GetLayout (layout_t *layout, int map_num)
{
	int rows_available;
	float show_frac;

	if (over_ui)
		show_frac = 1;
	else
		show_frac = bound(0, 1 - 2 * (realtime - last_event_time - HIDE_TIME), 1);

	if (show_frac == 0)
		return false;

	layout->char_size = Sbar_GetScaledCharacterSize();
	layout->top = (int)(vid.height - 2 * layout->char_size * show_frac);

	layout->bar_num_chars = (int)(vid.width / layout->char_size);
	layout->bar_width = layout->bar_num_chars * layout->char_size;
	layout->bar_x = (vid.width - layout->bar_width) / 2;
	layout->bar_y = layout->top + layout->char_size;

	layout->skip_next_x = vid.width - 2 * layout->char_size;
	layout->skip_prev_x = vid.width - layout->char_size * (4 + MAP_NAME_DRAW_CHARS);
	layout->skip_y = layout->top;

	layout->speed_prev_x = (vid.width - layout->char_size * (4 + SPEED_DRAW_CHARS)) / 2;
	layout->speed_next_x = (vid.width + layout->char_size * (SPEED_DRAW_CHARS)) / 2;
	layout->speed_y = layout->top;

	layout->time_y = layout->top;

	layout->map_x = (int)(vid.width - show_frac * layout->char_size * MAP_NAME_DRAW_CHARS);
	layout->map_width = (1 + MAP_NAME_DRAW_CHARS) * layout->char_size;
	rows_available = (int)((float)vid.height / layout->char_size - 4);
	if (rows_available >= demo_seek_info.num_maps)
	{
		// Draw all maps at once, centered.
		layout->map_min_num = 0;
		layout->map_max_num = demo_seek_info.num_maps;
	}
	else
	{
		int centre_y, rows_above, rows_below;

		centre_y = (vid.height - layout->char_size * 3) / 2;
		rows_above = centre_y / layout->char_size;
		rows_below = (vid.height - layout->char_size * 3 - centre_y) / layout->char_size;

		if (map_num < rows_above)
		{
			layout->map_min_num = 0;
			layout->map_max_num = rows_below + rows_above;
		}
		else if (demo_seek_info.num_maps - map_num < rows_below)
		{
			layout->map_min_num = demo_seek_info.num_maps - rows_below - rows_above;
			layout->map_max_num = demo_seek_info.num_maps;
		}
		else
		{
			layout->map_min_num = map_num - rows_above;
			layout->map_max_num = map_num + rows_below;
		}
	}

	layout->map_height = (layout->map_max_num - layout->map_min_num) * layout->char_size;
	layout->map_y = (vid.height - layout->char_size * 2 - layout->map_height) / 2;

	return true;
}



static void
UpdateHover (layout_t *layout, const mouse_state_t* ms)
{
	hover = HOVER_NONE;

	if(ms->y >= layout->bar_y
		&& ms->y <= layout->bar_y + layout->char_size
		&& ms->x >= layout->bar_x
		&& ms->x < layout->bar_x + layout->bar_width)
	{
		hover = HOVER_SEEK;
	}
	else if (ms->y >= layout->skip_y
		&& ms->y < layout->skip_y + layout->char_size
		&& ms->x >= layout->skip_next_x
		&& ms->x < layout->skip_next_x + 2 * layout->char_size)
	{
		hover = HOVER_SKIP_NEXT;
	}
	else if (ms->y >= layout->skip_y
		&& ms->y < layout->skip_y + layout->char_size
		&& ms->x >= layout->skip_prev_x
		&& ms->x < layout->skip_prev_x + 2 * layout->char_size)
	{
		hover = HOVER_SKIP_PREV;
	}
	else if (ms->y >= layout->skip_y
		&& ms->y < layout->skip_y + layout->char_size
		&& ms->x >= layout->skip_prev_x + 2 * layout->char_size
		&& ms->x < layout->skip_next_x)
	{
		hover = HOVER_SKIP;
	}
	else if (ms->y >= layout->speed_y
		&& ms->y < layout->speed_y + layout->char_size
		&& ms->x >= layout->speed_next_x
		&& ms->x < layout->speed_next_x + 2 * layout->char_size)
	{
		hover = HOVER_SPEED_NEXT;
	}
	else if (ms->y >= layout->speed_y
		&& ms->y < layout->speed_y + layout->char_size
		&& ms->x >= layout->speed_prev_x
		&& ms->x < layout->speed_prev_x + 2 * layout->char_size)
	{
		hover = HOVER_SPEED_PREV;
	}
	else if (ms->y >= layout->speed_y
		&& ms->y < layout->speed_y + layout->char_size
		&& ms->x >= layout->speed_prev_x + 2 * layout->char_size
		&& ms->x < layout->speed_next_x)
	{
		hover = HOVER_SPEED;
	}

	if (map_menu_open
		&& ms->x >= layout->map_x
		&& ms->y >= layout->map_y
		&& ms->y < layout->map_y + layout->map_height)
	{
		hover_map_idx = layout->map_min_num
					+ (int)((ms->y - layout->map_y) / layout->char_size);
	}
	else
	{
		hover_map_idx = -1;
	}

}


qboolean Demo_MouseEvent(const mouse_state_t* ms)
{
	char command[64];
	float progress;
	qboolean handled = false;
	dseek_map_info_t *dsmi;
	layout_t layout;
	int map_num, clicked_map_num;

	dsmi = CL_DemoGetCurrentMapInfo (&map_num);
	if (dsmi == NULL)
		return handled;

	if (dsmi->min_time == dsmi->max_time)
		return handled;  // only one frame in demo

	last_event_time = realtime;
	over_ui = false;
	if (!GetLayout(&layout, map_num))
		return handled;
	over_ui = ms->y > layout.top || (map_menu_open && ms->x > layout.map_x);

	UpdateHover(&layout, ms);

	if (ms->button_down == 1)
	{
		handled = true;
		switch (hover)
		{
			case HOVER_SEEK:
				demoui_dragging_seek = true; break;
			case HOVER_SKIP_NEXT:
				Cmd_ExecuteString("demoskip +1", src_command); break;
			case HOVER_SKIP:
				map_menu_open = !map_menu_open; break;
			case HOVER_SKIP_PREV:
				Cmd_ExecuteString("demoskip -1", src_command); break;
			case HOVER_SPEED_NEXT:
				ChangeSpeed(1); break;
			case HOVER_SPEED:
			case HOVER_SPEED_PREV:
				ChangeSpeed(-1); break;
			default:
				handled = false; break;
		}

		if (hover_map_idx != -1)
		{
			Q_snprintfz(command, sizeof(command), "demoskip %d", hover_map_idx);
			Cmd_ExecuteString(command, src_command);
			handled = true;
		}
	}

	if (!ms->buttons[1])
	{
		demoui_dragging_seek = false;
	}
	else if (demoui_dragging_seek)
	{
		progress = (ms->x - layout.char_size / 2 - layout.bar_x - layout.char_size)
					/ (float)(layout.bar_width - layout.char_size * 3);

		progress = bound(0, progress, 1);

		Q_snprintfz(command, sizeof(command),
					"demoseek %f",
					(1 - progress) * dsmi->min_time + progress * dsmi->max_time);
		Cmd_ExecuteString(command, src_command);
		handled = true;
	}

	if (ms->button_down == 1 && !handled)
	{
		if (map_menu_open)
			map_menu_open = false;
		else
			Cmd_ExecuteString("pause", src_command);
	}

	return handled;
}


static void
DrawArrows (layout_t *layout, int x, int y, hover_t highlight_hover, qboolean left)
{
	char c;
	c = left ? 0xbc : 0xbe;
	if (hover == highlight_hover)
		c -= 0x80;
	Draw_Character(x, y, c, true);
	Draw_Character(x + layout->char_size, y, c, true);
}


static void
FormatTimeString (float seconds, int buf_size, char *buf)
{
	int minutes = (int)(seconds / 60);
	Q_snprintfz(buf, buf_size, "%d:%05.2f", minutes, seconds - 60 * minutes);
}


void Demo_DrawUI(void)
{
	float sbar_scale = Sbar_GetScaleAmount();
	char current_time_buf[32];
	char max_time_buf[32];
	char buf[64];
	char c;
	float progress;
	int i, x, y, playhead_x;
	dseek_map_info_t *dsmi;
	int map_num;
	layout_t layout;

	dsmi = CL_DemoGetCurrentMapInfo (&map_num);
	if (dsmi == NULL)
		return;

	if (dsmi->min_time == dsmi->max_time)
		return;  // only one frame in demo

	if (!GetLayout(&layout, map_num))
		return;

	// Backdrop
	Draw_AlphaFill(0, layout.top, vid.width / sbar_scale, 16, 0, 0.7);

	// Seek bar
	for (i = 0, x = layout.bar_x; i < layout.bar_num_chars; i++, x += layout.char_size)
	{
		if (i == 0)
			c = 128;
		else if (i == layout.bar_num_chars - 1)
			c = 130;
		else
			c = 129;

		Draw_Character (x, layout.bar_y, c, true);
	}

	progress = (cl.mtime[0] - dsmi->min_time) / (dsmi->max_time - dsmi->min_time);
	playhead_x = (int)((layout.bar_x + layout.char_size) * (1 - progress)
						+ (layout.bar_x + layout.bar_width - layout.char_size * 2) * progress);

	Draw_Character(playhead_x, layout.bar_y, 131, true);

	// Time
	FormatTimeString(cl.mtime[0], sizeof(current_time_buf), current_time_buf);
	FormatTimeString(dsmi->max_time, sizeof(max_time_buf), max_time_buf);
	Q_snprintfz(buf, sizeof(buf), "%s / %s", current_time_buf, max_time_buf);
	Draw_String(0, layout.time_y, buf, true);

	// Speed
	DrawArrows(&layout, layout.speed_prev_x, layout.speed_y, HOVER_SPEED_PREV, true);
	Q_snprintfz(buf, sizeof(buf), "%4.2fx", cl_demospeed.value);
	Draw_String(layout.speed_prev_x + 2 * layout.char_size
					+ layout.char_size * (SPEED_DRAW_CHARS - strlen(buf)) / 2,
				layout.speed_y, buf, true);
	DrawArrows(&layout, layout.speed_next_x, layout.speed_y, HOVER_SPEED_NEXT, false);

	// Current level
	if (map_num > 0)
		DrawArrows(&layout, layout.skip_prev_x, layout.skip_y, HOVER_SKIP_PREV, true);
	Q_snprintfz(buf, min(sizeof(buf), MAP_NAME_DRAW_CHARS + 1),
				"%s", dsmi->name);
	Draw_String(layout.skip_prev_x + 2 * layout.char_size
					+ layout.char_size * (MAP_NAME_DRAW_CHARS - strlen(buf)) / 2,
				layout.skip_y, buf, true);
	if (map_num < demo_seek_info.num_maps - 1)
		DrawArrows(&layout, layout.skip_next_x, layout.skip_y, HOVER_SKIP_NEXT, false);

	// Level selector
	if (map_menu_open)
	{
		Draw_AlphaFill(layout.map_x, layout.map_y,
						layout.map_width / sbar_scale,
						layout.map_height / sbar_scale,
						0, 0.7);
		for (i = layout.map_min_num, y = layout.map_y;
				i < layout.map_max_num; i++, y += layout.char_size)
		{
			if ((hover_map_idx != -1 && i == hover_map_idx)
					|| (hover_map_idx == -1 && i == map_num))
			{
				Draw_Character(layout.map_x, y, 13, true);
			}
			Draw_String(layout.map_x + layout.char_size, y,
						demo_seek_info.maps[i].name, true);
		}
	}
}
