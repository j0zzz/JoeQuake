#include "quakedef.h"


qboolean Demo_MouseEvent(const mouse_state_t* ms)
{
}


void Demo_DrawUI(void)
{
	char c;
	float progress;
	int i, x, bar_y, bar_x, bar_width, playhead_x;
	int bar_num_chars;
	int char_size = Sbar_GetScaledCharacterSize();
	dseek_map_info_t *dsmi;

	dsmi = CL_DemoGetCurrentMapInfo ();
	if (dsmi == NULL)
		return;

	if (dsmi->min_time == dsmi->max_time)
		return;  // only one frame in demo

	// Seek bar
	bar_num_chars = (int)(vid.width / char_size);
	bar_width = bar_num_chars * char_size;

	bar_x = (vid.width - bar_width) / 2;
	bar_y = vid.height - char_size;

	for (i = 0, x = bar_x; i < bar_num_chars; i++, x += char_size)
	{
		if (i == 0)
			c = 128;
		else if (i == bar_num_chars - 1)
			c = 130;
		else
			c = 129;

		Draw_Character (x, bar_y, c, true);
	}

	progress = (cl.mtime[0] - dsmi->min_time) / (dsmi->max_time - dsmi->min_time);
	playhead_x = (int)((bar_x + char_size) * (1 - progress)
						+ (bar_x + bar_width - char_size * 2) * progress);

	Draw_Character(playhead_x, bar_y, 131, true);
}
