#include "quakedef.h"


static qboolean dragging_seek;


typedef struct
{
	int bar_x, bar_y, bar_width, bar_num_chars, char_size;
} layout_t;


static void
SeekBarLayout (layout_t *layout)
{
	layout->char_size = Sbar_GetScaledCharacterSize();
	layout->bar_num_chars = (int)(vid.width / layout->char_size);
	layout->bar_width = layout->bar_num_chars * layout->char_size;
	layout->bar_x = (vid.width - layout->bar_width) / 2;
	layout->bar_y = vid.height - layout->char_size;
}


qboolean Demo_MouseEvent(const mouse_state_t* ms)
{
	char command[64];
	float progress;
	qboolean handled = false;
	dseek_map_info_t *dsmi;
	layout_t layout;

	if (!ms->buttons[1]) {
		dragging_seek = false;
		return handled;
	}

	dsmi = CL_DemoGetCurrentMapInfo ();
	if (dsmi == NULL)
		return handled;

	if (dsmi->min_time == dsmi->max_time)
		return handled;  // only one frame in demo

	SeekBarLayout(&layout);

	if (ms->y >= layout.bar_y
			&& ms->y < layout.bar_y + layout.char_size
			&& ms->x >= layout.bar_x
			&& ms->x < layout.bar_x + layout.bar_width)
	{
		dragging_seek = true;
	}

	if (dragging_seek)
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


void Demo_DrawUI(void)
{
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

	SeekBarLayout(&layout);

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
}
