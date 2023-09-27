---
layout: page
title: Features
category: features
permalink: /features
---

### Parameter completion

Parameter completion means if you're using a command requiring a filename
(`playdemo` for example), then when you're pressing TAB you're gonna get a list
of files matching to the written wildcard, just as `dir` or `demdir`.
For example you typed the following to the console:

```
]playdemo e1m1
```

After pressing TAB you're getting a list of all e1m1*.dem files.
The wildcard will be updated with the longest common string of the matching
filenames. If only one file was found, the wildcard's gonna be replaced with
that. It's that simple :) but it's handy imho.

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

{% include_absolute 'pages/subfeatures/demos-menu.md' %}

{% include_absolute 'pages/subfeatures/better-look.md' %}

### Avi capturing

JoeQuake uses avi capturing written by Anthony Bailey.
An important thing you should know about: this is mainly for capturing demos,
not gameplay.

Sound issues: capturing supports up to 44 KHz sounds too, but I personally don't
recommend recording the audio on that sample. The reason is that all the Quake
sounds are originally recorded on 11 KHz, and simply playing them on higher sample
doesn't actually make them sound better. If you want to use 22 or 44 KHz sounds
at any price, I advise you to get a good editor (like SoundForge), and resample
the audio using that.

### Commands history

Added from [sons]Quake.
The last 64 console lines are stored in a file cmdhist.dat in the main Quake
folder, so than you can use cmd history after quitting/restarting the game.

### Demoplaying from .dz files

You may now use .dz files as playdemo command's parameter, it'll be
automatically extracted and played. Works from the Demos menu too.
NOTE: make sure the dzip binary (dzip.exe or dzip-linux, comes with the zip)
is present in your main Quake folder.

### Framerate independent physics

Server and Client calculates fps differently.
The server always operates at a maximum of 72 fps. This guarantees that vanilla Quake
physics are kept and also demo files remain backward compatible.
The client can operate as many fps as possible, so users can for example match their
desired max fps to their monitor's refresh rate to achieve a much smoother gameplay
experience.

{% include_absolute 'pages/subfeatures/ghost-recording.md' %}
