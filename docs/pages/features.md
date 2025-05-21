---
layout: page
title: Features
category: features
permalink: /features
---

1. TOC
{:toc}

### Framerate independent physics

Server and Client calculates fps differently.
The server always operates at a maximum of 72 fps. This guarantees that vanilla Quake physics are kept and also demo files remain backward compatible.
The client can operate as many fps as possible, so users can - for example - match their desired max fps to their monitor's refresh rate to achieve a much smoother gameplay experience.

Use `cl_maxfps <value>` to set your desired framerate.  
Use `vid_displayfrequency <value>` to set your desired monitor refresh rate.

{% include subfeatures/ghost-recording.md %}

{% include subfeatures/demo-ui.md %}

### Parameter completion

Parameter completion means if you're using a command requiring a filename
(`playdemo` for example), then when you're pressing TAB you're gonna get a list of files matching to the written wildcard, just like with the `dir` or `demdir` commands.
For example you typed the following to the console:

```
]playdemo e1m1
```

After pressing TAB you're getting a list of all e1m1*.dem files.
The wildcard will be updated with the longest common string of the matching
filenames. If only one file was found, the wildcard's gonna be replaced with
that.

Parameter completion works for the following commands:
* `playdemo`
* `capture_start`
* `capturedemo`
* `printtxt`
* `map`
* `exec`
* `load`
* `loadsky\r_skybox`
* `loadcharset\gl_consolefont`
* `crosshairimage`

None of them require any extension to be given, they'll automatically find
their filetypes.

{% include subfeatures/demos-menu.md %}

### Avi capturing

JoeQuake uses avi capturing written by Anthony Bailey.
An important thing you should know about: this is mainly for capturing demos,
not gameplay.

#### Commands

- `capture_start <avi-file>`: Start capturing to the given AVI file.
- `capture_end`: Stop an ongoing capturing.
- `capturedemo <demo-file>`: Start playing a demo and at the same time also start capturing to an AVI file with the same name.

#### Cvars

- `capture_avi [0|1]`: When set to `1`, the captured video stream will be saved to an AVI file. Otherwise every captured frame will be saved as an individual TGA image.
- `capture_avi_split <value>`: This to the number of megabytes at which to split captured video into more than one AVI file. The used video capture module has a problem with files getting corrupted when reaching a size of over 2 gigabytes, so splitting them into smaller files is a good idea to avoid this corruption. Default is 1900 megabytes. Setting to 0 disables splitting.
- `capture_codec <codec_fourcc>`: Look for the codec having the specified fourcc code to compress the video with. If the fourcc code is set to `0`, the created AVI file will be uncompressed.
- `capture_fps <value>`: Set on how many frames/sec you wish the video to be captured.
- `capture_dir <path>`: Set the directory path where avis to be saved while capturing. You can define relative and absolute paths: e.g. `capture` would mean the subfolder in your Quake root directory and e.g. `c:\My captures` is an absolute path.
- `capture_mp3 [0|1]`: When set to `1`, the captured audio will be mp3 compressed, otherwise uncompressed.
- `capture_mp3_kbps <value>`: Set mp3 compression bitrate, only works if `capture_mp3` is set to `1`.
- `capture_console [0|1]`: When set to `0`, capturing will be suspended while the console is pulled down.

Sound issues: capturing supports up to 44 KHz sounds too, but I personally don't recommend recording the audio on that sample. The reason is that all the Quake sounds are originally recorded on 11 KHz, and simply playing them on higher sample doesn't actually make them sound better. If you want to use 22 or 44 KHz sounds at any price, I advise you to get a good editor (like SoundForge), and resample the audio using that.

### Commands history

Added from [sons]Quake.
The last 64 console lines are stored in a file cmdhist.dat in the main Quake
folder, so than you can use cmd history after quitting/restarting the game.

### Demoplaying from .dz files

Speed demos archive uses its own demo compression tool called *Dzip*, which was founded exclusively for Quake demo files. The speedruns you download from SDA are all dzip compressed.

You can use .dz files as the playdemo command's parameter, it'll be
automatically extracted and played. This also works from the Demos menu.  
IMPORTANT: make sure the dzip binary (dzip.exe, comes with the JoeQuake zip) is present in your main Quake folder.

{% include subfeatures/better-look.md %}

{% include subfeatures/md3-models.md %}

### Path Tracer

Sometimes it might be helpful to see the path the player took. That can help to analyse the situation. There are several options:

- Record your movement: If `pathtracer_record_player` is set to `1` then the path and movement keys are recorded. You can switch this on and off as you like. You can bind this to some keys or mouse buttons
- Show your movement: `pathtracer_show_player` to `1` draws the previously recorded information into the current scene.
- Show the path of a ghost or demo: `pathtracer_show_demo`, `pathtracer_show_ghost` on `1` will draw the path of the demo/ghost. It can draw the movement keys if the demo/ghost contains that information
- Fadeout: `pathtracer_fadeout_ghost` and `pathtracer_fadeout_demo` if set to `1` will fadeout the path around the current position. This helps to keep an overview if the path gets a bit convoluted. Per default the fadeout is disabled for ghosts, enabled for demos. The time the path will be drawn is defined by `pathtracer_fadeout_seconds` in seconds (defaults to `3`).
- Bolder lines: `pathtracer_line_smooth` to `1` produces bolder lines, making them better visible on videos and screenshots.

{% include subfeatures/bhop-practice.md %}
