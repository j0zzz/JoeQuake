### Ghost recording

The ghost feature shows the player from a demo file while you are playing the
game or watching another demo file.  This is useful for speedruns to know where
you are relative to a reference demo, and to indicate where you could be faster.

#### Setting a ghost from the demo menu

From the demo menu, press ctrl-enter to set a demo file as a ghost, and
ctrl-shift-enter to remove the ghost.

#### Commands

- `ghost <demo-file>`:  Add ghost from the given demo file.  The ghost will be
  loaded on next map load.  With no arguments it will show the current ghost's
  demo file, if any.  Only one ghost may be added at a time.
- `ghost_remove`: Remove the current ghost, if any is added.  The change will
  take effect on next map load.
- `ghost_shift <t>`: Shift the ghost to be the given number of seconds infront
  of the player.  Useful if you lose the ghost but you still want to see its
  route.
- `ghost_shift_reset`: Undo the effect of `ghost_shift`, and put the ghost back
  to its correct position.

#### Cvars

- `ghost_delta [0|1]`: Show how far ahead or behind the ghost you currently are.
  This is unaffected by `ghost_shift`.
- `ghost_range <distance>`:  Hide the ghost when it is within this distance.
- `ghost_alpha <float>`: Change how transparent the ghost is.  `0` is fully
  transparent, `1` is fully opaque.
- `ghost_marathon_split`: When set to `1`, show the live marathon split, rather
  than the level split. Default `0`.
- `ghost_bar_x`, `ghost_bar_y`: Set the position of the ghost bar.
- `ghost_bar_alpha`: Set the transparency of the ghost bar.

#### Marathon split times

When a ghost is loaded a split time is printed to the console at the end of each
level.  If a marathon demo is loaded as the ghost, and the player is also
running a marathon, then split times for each level are shown.  Splits are also
shown when playing a demo with a ghost loaded. 