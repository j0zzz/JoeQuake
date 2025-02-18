### Bunnyhop practice tools

JoeQuake has tools to help you practice your bunnyhopping. These are turned on using the cvars described below. It displays data for the last specified amount of frames, in order to provide you with feedback you need to improve aspects of your bunnyhopping skill.

The available displays are as follows:
* text displays of average speed, +fwd accuracy on ground and in air, as well as strafe synchronisation with turning direction.
* graph display of precision of keypresses for each frame: the first bar show forward tapping. Green means perfect synchronisation, red means a mistake. The ground frame is drawn more brightly. The other bar, split in two halves, shows the left and right direction. Pressing `+left` or `+right` fills the corresponding bar with a red color. Turning left or right fills the corresponding bar with a green color. When they mix - aka, when strafes are accurate - they combine to a golden color.
* display of speed for each frame; the entire bar corresponds to 320 units, and fills up with other colors when this value is exceeded.
* display of acceleration for each frame, in a logarithmic form. So, even small gains in air will show as 1-2 pixels, while a sharper gain or loss of speed will be a larger spike, but not one going out of bounds of the bar.
* display under crosshair of the accuracy of viewangle: the green portion of the mark shows the area where speed would be gained during the last frame, while the red mark shows the optimal viewangle. Frames preceding the latest show only the optimal point; the more closely it aligns with the center line, the better.

Graphs are also shown and scrolled during intermission.

This feature is automatically turned off when recording a demo. This is a practice tool, not an assistance tool.

#### Cvar descriptions
##### `show_bhop_stats`
##### `show_bhop_stats_x`
##### `show_bhop_stats_y`
##### `show_bhop_frames`
##### `show_bhop_window`
##### `show_bhop_histlen`

Controls the displayed bunnyhopping practice tools. The desired value of `show_bhop_stats` is composed by adding up various flags:
* `1` - display average speed over window.
* `2` - display forward and strafe accuracy, as percentage.
* `4` - display speed during recent frames.
* `8` - display acceleration during recent frames; the display is logarithmic (so small gains in speed are visible).
* `16` - display precision of forward keypresses and strafe accuracy, as a bar. Top bar displays forward accuracy, while the bottom bar displays the left direction on top and right on bottom. When strafes are synced, colors mix to yellow.
* `32` - display under the crosshair the bar signifying where you needed to look to gain speed in air, as well as a history over recent frames. The closer aligned the center bar and the red line are, the more accurate your turning was.
* `64` - display above crosshair some number stats on each jump (gained speed in air, loss due to friction, time spent on ground and velocity of prestrafe), as well as multiple boxes more clearly marking forward tap accuracy. It shows the ground frame above the crosshair, and frames preceding and after the last ground touch; the amount of frames shown in both direction is controlled with the variable `show_bhop_frames`.

`_x` and `_y` control the position of the graphs.

The amount of frames shown for the graph displays is controlled by `show_bhop_window`.

Stats are also shown in intermission; the display shows everything that is in recorded history. The amount of frames stored is controlled by `show_bhop_histlen`.

The defaults are as follows:
* `show_bhop_stats` is `0`, signifying no display. To set everything on, use `127`. The displays are all turned off during demo recording, regardless of setting.
* `show_bhop_stats_x,y` are `1` and `4` respectively.
* `show_bhop_window` is `144`, corresponding to two seconds. This is the max effective value of this variable.
* `show_bhop_histlen` is `0`, corresponding to storing everything until the intermission.
* `show_bhop_frames` is `7`.

To display some of the graphs, a lot of rectangles are drawn. So you might lose some FPS due to that.


