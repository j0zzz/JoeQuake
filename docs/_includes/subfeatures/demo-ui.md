### Demo UI

JoeQuake features a media player-style interface for navigating demos.  Simply
move the mouse when a demo is playing to activate the UI.

The interface has the following elements, from bottom-to-top, left-to-right:

- **Pause/play**: Click to pause and resume demo playback.
- **Seek bar**: Click to seek through time on the current map.  Using the
  mouse wheel in this area jumps back and forward in one second increments.
  There is a small marker above the seek bar indicating the time when the player
  finishes the map.
- **Time**: Elapsed time and total time (including intermission) for the current
  map.
- **Speed**: Increase and decrease demo playback speed by clicking the arrow
  controls or by using the mouse wheel in this area.
- **Bounding boxes (BB)**: Toggle client-side bounding boxes.
- **Camera mode (CAM)**: Cycles between first-person, freefly, and orbit camera
  modes.
- **Map selector**: Shows the current map.  On a multi-map (marathon) demo
  clicking the arrow controls or using the mouse wheel in this area will skip
  from map to map. Click between the arrows to toggle the map menu.
- **Map menu**: Shows all maps in the current demo.  Click on a map to skip
  straight to it.  Clicking outside of the demo UI will close the map menu.

Tips and tricks:

- Clicking outside of the demo UI (when the map menu is closed) will pause or
  resume demo playback.
- Leave the mouse still and the UI will disappear after a short wait.  The
  timeout can be changed with `cl_demouitimeout` and the hiding animation sped
  up with `cl_demouihidespeed`.
- Bind keys to the commands `demoskip` and `demoseek` to enable keyboard
  shortcut navigation.  For example, bind `demoskip +1` to skip to the next map,
  or `demoseek -5` to seek backwards 5 seconds.
- Navigate to the end of a marathon demo, pause, and load a ghost to get instant
  split times between the two demos.
- Disable the UI with the `cl_demoui` cvar.  `demoseek` and `demoskip` commands
  will still work when the UI is disabled.
- When in freefly mode, hold the right mouse button to control the camera's view angle.

Skipping between maps of marathon demos is only currently supported for single
file marathons.  Demos which consist of multiple individual demo files are not
yet supported by the UI.
