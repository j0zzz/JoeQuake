#include "quakedef.h"

#define MAP_NAME_DRAW_CHARS	12


qboolean demoui_dragging_seek;


typedef struct
{
	int top;
	int char_size;
	int bar_x, bar_y, bar_width, bar_num_chars;
	int skip_prev_x, skip_next_x, skip_y;
} layout_t;


static void
GetLayout (layout_t *layout)
{
	layout->char_size = Sbar_GetScaledCharacterSize();
	layout->top = (int)(vid.height - 2.f * layout->char_size);
	layout->bar_num_chars = (int)(vid.width / layout->char_size);
	layout->bar_width = layout->bar_num_chars * layout->char_size;
	layout->bar_x = (vid.width - layout->bar_width) / 2;
	layout->bar_y = vid.height - layout->char_size;

	layout->skip_next_x = vid.width - 2 * layout->char_size;
	layout->skip_prev_x = vid.width - layout->char_size * (4 + MAP_NAME_DRAW_CHARS);
	layout->skip_y = vid.height - layout->char_size * 2;
}


qboolean Demo_MouseEvent(const mouse_state_t* ms)
{
	char command[64];
	float progress;
	qboolean handled = false;
	dseek_map_info_t *dsmi;
	layout_t layout;

	if (!ms->buttons[1]) {
		demoui_dragging_seek = false;
		return handled;
	}

	dsmi = CL_DemoGetCurrentMapInfo ();
	if (dsmi == NULL)
		return handled;

	if (dsmi->min_time == dsmi->max_time)
		return handled;  // only one frame in demo

	GetLayout(&layout);

	if (ms->button_down == 1)
	{
		if(ms->y >= layout.bar_y
			&& ms->y < layout.bar_y + layout.char_size
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
	}

	if (demoui_dragging_seek)
	{
		progress = (ms->x - layout.char_size / 2 - layout.bar_x - layout.char_size)
					/ (float)(layout.bar_width - layout.char_size * 2);

		progress = bound(0, progress, 1);

		Q_snprintfz(command, sizeof(command),
					"demoseek %f",
					(1 - progress) * dsmi->min_time + progress * dsmi->max_time);
		Cmd_ExecuteString(command, src_command);
		handled = true;
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
	char current_time_buf[32];
	char max_time_buf[32];
	char buf[64];
	char c;
	float progress;
	int i, x, playhead_x;
	dseek_map_info_t *dsmi;
	layout_t layout;

	dsmi = CL_DemoGetCurrentMapInfo ();
	if (dsmi == NULL)
		return;

	if (dsmi->min_time == dsmi->max_time)
		return;  // only one frame in demo

	GetLayout(&layout);

	// Backdrop
	Draw_AlphaFill(0, layout.top, vid.width, vid.height - layout.top, 0, 0.7);

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
	Draw_String(0, vid.height - layout.char_size * 2, buf, true);

	// Level
	Draw_Character(layout.skip_prev_x, layout.skip_y, 0xbc, true);
	Draw_Character(layout.skip_prev_x + layout.char_size, layout.skip_y, 0xbc, true);
	Q_snprintfz(buf, min(sizeof(buf), MAP_NAME_DRAW_CHARS + 1),
				"%s", dsmi->name);
	Draw_String(layout.skip_prev_x + layout.char_size
					+ layout.char_size * (MAP_NAME_DRAW_CHARS - strlen(buf)) / 2,
				layout.skip_y, buf, true);
	Draw_Character(layout.skip_next_x, layout.skip_y, 0xbe, true);
	Draw_Character(layout.skip_next_x + layout.char_size, layout.skip_y, 0xbe, true);
}
