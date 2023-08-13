# JoeQuake SDL support

This release includes an experiment SDL build.  The SDL version of JoeQuake uses
the [Simple DirectMedia Layer](https://www.libsdl.org/) API for sound, video,
and input support, much like other modern ports of Quake (eg. ironwail,
Quakespasm, vkQuake).  The hope is that this version will be easier to maintain,
be more portable, and ultimately support more features than the standard windows
APIs that are used by the classic build.  The code for the SDL version of
JoeQuake is heavily based on ironwail, so new features and bug fixes should be
relatively easy to port over.

## Changing video mode

Generally, video modes should be selected from the menu.  But if you want to
everything can be done via the console.  The following cvars are supported:

- `vid_width` / `vid_height`: Resolution in pixels.
- `vid_refreshrate`: Game refresh rate in hertz.
- `vid_fullscreen`: 0 for windowed mode, 1 for fullscreen mode.
- `vid_desktopfullscreen`: When fullscreen is enabled, use a "fake" fullscreen mode which creates a window at the desktop resolution.  In this mode, `vid_width` and `vid_height` do nothing.

Any changes to the above variables are not applied until `vid_forcemode` is run,
or the game is restarted.

By default, the game will only run on the primary monitor.  To run on a
different monitor pass `-display <monitor number>`. For example, passing
`-display 1` will run on the second monitor.

If something goes seriously wrong with your graphics you can clear out the above
variables from your `config.cfg`, and the game should start in a safe windowed
mode (currently 800x600).

## Errors

Since this is the first release, we anticipate some issues.  To help us discover
these, the game will show an error message when a fatal error is encountered.
Non-fatal errors will be printed to the console with a message like one of the
following:

```
OpenGL error, please report: ...
SDL error, please report: ...
```

Be sure to report any bugs you find!  If you want to suppress printing of
non-fatal video errors, set `vid_ignoreerrors 1`.

## Features to be added

- CD audio.  (Background music via separate audio files should continue to
  work as usual.)
- Allow selecting display after startup via a cvar / menu.
- Add mode for testing video modes like with the classic build.
- Add desktop full screen support to the video menu.
- Vsync support.
- Slider for setting `r_scale` on the video menu.

## Credits

- ironwail,  Quakespasm, and developers of ancestor ports from which the input
  and video code was in large part copied.
- Sphere for porting the sound code.
- Joe for testing with Windows.
