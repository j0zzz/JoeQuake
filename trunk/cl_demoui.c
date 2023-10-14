#include "quakedef.h"

#define MAP_NAME_DRAW_CHARS	12
#define HIDE_TIME	1.5f


static double last_event_time = -1.0f;
static qboolean over_ui = false;
static qboolean map_menu_open = false;
static int hover_map_idx = -1;

qboolean demoui_dragging_seek;


typedef struct
{
	int top;
	int char_size;
	int bar_x, bar_y, bar_width, bar_num_chars;
	int skip_prev_x, skip_next_x, skip_y;
	int time_y;

	int map_min_num, map_max_num, map_x, map_y, map_width, map_height;
} layout_t;


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

	if (map_menu_open
		&& ms->x >= layout.map_x
		&& ms->y >= layout.map_y
		&& ms->y < layout.map_y + layout.map_height)
	{
		hover_map_idx = layout.map_min_num
					+ (int)((ms->y - layout.map_y) / layout.char_size);
	}
	else
	{
		hover_map_idx = -1;
	}

	if (ms->button_down == 1)
	{
		if(ms->y >= layout.bar_y
			&& ms->y <= layout.bar_y + layout.char_size
			&& ms->x >= layout.bar_x
			&& ms->x < layout.bar_x + layout.bar_width)
		{
			demoui_dragging_seek = true;
		}
		else if (ms->y >= layout.skip_y
			&& ms->y < layout.skip_y + layout.char_size
			&& ms->x >= layout.skip_next_x
			&& ms->x < layout.skip_next_x + 2 * layout.char_size)
		{
			Cmd_ExecuteString("demoskip +1", src_command);
			handled = true;
		}
		else if (ms->y >= layout.skip_y
			&& ms->y < layout.skip_y + layout.char_size
			&& ms->x >= layout.skip_prev_x
			&& ms->x < layout.skip_prev_x + 2 * layout.char_size)
		{
			Cmd_ExecuteString("demoskip -1", src_command);
			handled = true;
		}
		else if (ms->y >= layout.skip_y
			&& ms->y < layout.skip_y + layout.char_size
			&& ms->x >= layout.skip_prev_x + 2 * layout.char_size
			&& ms->x < layout.skip_next_x)
		{
			map_menu_open = !map_menu_open;
			handled = true;
		}

		if (hover_map_idx != -1)
		{
			Q_snprintfz(command, sizeof(command), "demoskip %d", hover_map_idx);
			Cmd_ExecuteString(command, src_command);
			handled = true;
		}
	}

	if (!ms->buttons[1]) {
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

	if (ms->button_down == 1 && !handled) {
		map_menu_open = false;
	}

	return handled;
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

	// Current level
	if (map_num > 0)
	{
		Draw_Character(layout.skip_prev_x, layout.skip_y, 0xbc, true);
		Draw_Character(layout.skip_prev_x + layout.char_size, layout.skip_y, 0xbc, true);
	}
	Q_snprintfz(buf, min(sizeof(buf), MAP_NAME_DRAW_CHARS + 1),
				"%s", dsmi->name);
	Draw_String(layout.skip_prev_x + 2 * layout.char_size
					+ layout.char_size * (MAP_NAME_DRAW_CHARS - strlen(buf)) / 2,
				layout.skip_y, buf, true);
	if (map_num < demo_seek_info.num_maps - 1)
	{
		Draw_Character(layout.skip_next_x, layout.skip_y, 0xbe, true);
		Draw_Character(layout.skip_next_x + layout.char_size, layout.skip_y, 0xbe, true);
	}

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
