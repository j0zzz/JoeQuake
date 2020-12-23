
# JoeQuake

                      JoeQuake - Release Notes
                        --------------------
                  <http://joequake.runecentral.com>


## New cvars

### `r_explosiontype`

Sets various types of explosions.

Values: 0 - normal, 1 - just sprite, 3 - just particles, 4 - slight blood.
Its value is 0 by default.

### `r_explosionlight`

Turns explosion's light on/off, 1 by default.

### `r_rocketlight`

Turns flying rocket's light on/off, 1 by default.

### `r_rockettrail`

Sets flying rocket's trail:
0 - no trail, 1 - default trail, 2 - grenade trail.

* when using `gl_part_trails 1`:
same as above, plus there's a new state: 3 - darkplaces trail.
* when using `gl_part_trails 2`:
there're no different trails, just a Q3 style one.

Value is 1 by default.

### `r_grenadetrail`

Sets flying grenade's trail:
0 - no trail, 1 - default trail, 2 - rocket trail.

* when using `gl_part_trails 1`:
same as above, plus there's a new state: 3 - darkplaces trail
* when using `gl_part_trails 2`:
there're no different trails, just a Q3 style one

Value is 1 by default.

### `cl_r2g`

Replaces the rocket model with grenade, 0 by default.
(popular in deathmatch)

### `cl_truelightning`

Makes the lightning gun's bolt more precise, 0 by default.
Can be 0.0 (off) - 1.0 (most precise).

### `cl_bobbing`

Turns Quake3-style bobbing on, 0 by default.

### `cl_deadbodyfilter`

Removes dead bodies (both for players and monsters).
When using with value 1, the dead body is to be removed in the last animation frame.
When using with value 2, it is removed immediately when the monster/player dies.
Its value is 0 (off) by default.

### `cl_gibfilter`

Removes gibs if turned on, 0 (off) by default.

### `vid_hwgammacontrol`

Turns gamma changing possibility on/off, 1 by default.
With using a value of 2, it applies gamma for windowed modes as well.

### `gamma` and `contrast` (or `gl_gammà` and `gl_contrast` in GL)

One of the most important features. These are for your viewing pleasure :)

### `scr_centermenu`

Makes the menu centered, 1 by default.

### `scr_centersbar`

Makes the status bar (hud) centered, 0 by default.

### `scr_widescreen_fov`

When turned on, fov (field of view) is properly calculated in widescreen
(16:9 or 16:10) resolutions also, "1" by default.

### `scr_sbarscale_amount`

Scales (multiplies) the size of console font and status bar icons
with the specified number, "2" by default.

### `cl_sbar`

Toggles between transparent (0), old/original (1) and alternative (2) huds, 0 by default.

### `v_gunkick`

Turns weapon's jarring every time when firing on/off, 0 by default.
Using with value 1 the weapon kicks the original (old) way, while using with
value 2 the kick is much smoother just like in QW.

### `r_powerupglow`

Turns powerup glow around player's body on/off, 1 by default.

### `r_coloredpowerupglow`

Turns forced coloration of powerup glow for quad and pentagram on/off, 1 by default.

### `scr_consize`

Adjust the console's size, 0.5 (half size) by default.
Can be 0.0 (nothing visible) - 1.0 (fully visble).

NOTE: when the console is pulled down, you can also adjust the size
with using the ctrl+UP and ctrl+DOWN keys.

### `show_stats`

Print actual statistics (time, secrets, kills) to the upper right corner
of the screen, 1 by default.

Values: 0 - off, 1 - only time, 2 - time + kills + secrets, 3 - only time, but
only on if some reasonable event happens, 4 - same as 3, plus kills + secrets too.

### `show_stats_length`

If show_stats is set to 2, the printing takes as long as it's set in this
cvar, 0.5 by default. The value represents seconds.

### `show_stats_small`

Replaces HUD numbers with console characters when showing `show_stats`'s
statistics. 0 by deafult.

### `cl_demorewind`

Toggles between normal and backward demoplaying, 0 by default.
Only works when playing a demo.

### `show_speed`

Prints the player's speed in units.

### `show_fps`, `show_fps_x`, `show_fps_y`

Prints the framerate in frames per second. Position can be changed with the
`_x` and `_y` coordinates.

### `r_fullbrightskins`

Makes player(s)'s skins fullbright, 0 by default.

### `mapname`

Contains actual map's name, for built-in use.

### `cl_bonusflash`

Turns pickup flashes on/off, 1 by default.

### `cl_muzzleflash`

Turns muzzle flashes (flashes when firing) on/off, 1 by default.

### `v_contentblend`

Turns liquid blends on/off, 1 by default.

### `v_damagecshift`

Turns damage flashes on/off, 1 by default.

### `v_quadcshift`

Turns quad damage's blend on/off, 1 by default.

### `v_pentcsift`

Turns pentagram's blend on/off, 1 by default.

### `v_ringcshift`

Turns invisibilty's blend on/off, 1 by default.

### `v_suitcshift`

Turns environment suit's blend on/off, 1 by default.

### `r_viewmodelsize`

Adjust the amount of your weapon to be visible, 1 by default (fully visible).
Can be 0.0 (not visble, equals `r_drawviewmodel 0`) - 1.0 (see above).
This is useful for people using bigger fovs than 90.

### `m_rate`

Sets mouse rate. Only work when start with `-dinput` and `-m_smooth`.

### `m_showrate`

Shows current mouse rate. Useful to set m_rate. Only work with `-dinput -m_smooth`.

### `cl_clock`, `cl_clock_x`, `cl_clock_y`

Shows clock in the lower left corner. Position can be changed with the `_x` and `_y`
coordinates. There are 4 predefined clock formats. "0" by default.

### `crosshairsize`

Sets the size of the crosshair, "1.0" by default.

### `crosshaircolor`

Sets the color of the crosshair, "79" (red) by default.

### `cl_demospeed`

Changes the playback speed of a demo, 1 by default.
Values < 1 mean slow motion, while > 1 result fast forward.

### `cl_autodemo`, `cl_autodemo_name`

Turns on automatic demo recording.
If cl_autodemo is 1 and cl_autodemo_name is empty, a new .dem file is recorded at
every level start. the file naming format is `<map_name>_<date_of_recording>_<time_of_recording>.dem`
If cl_autodemo is 1 and cl_autodemo_name is not empty, the `<cl_autodemo_name>.dem` file is recorded
at every level start. To save the current recording session, use the 'keepdemo' command.
This command is only valid on the intermission/finale screen and renames the temporary recording using the
following naming format: `<cl_autodemo_name>_<finished_time>_<skill>_<player_name>.dem`
If cl_autodemo is 2, the naming format changes to the following:
`<mapname>_<finished_time>_<skill>_<player_name>.dem`
If cl_autodemo is 0, there is no automatic recording.

### `cl_maxfps`

Customizes the maximal fps, 72 by default.
You are not allowed to raise this above 72 if you're playing single player or coop.

### `cl_advancedcompletion`

Toggles between advanced and normal (old) command/cvar completion.
Its value is 1 by default.

### `cvar_savevars`

Switch for the `writeconfig` command.
If set to 0 then `writeconfig` will save only archieved vars.
If 1, it'll save the ones which have other than default values.
And with value 2 it'll save ALL variables.
Its value is 0 by default.

### `cl_confirmquit`

Switch for confirmation when exiting, 0 means no confirmation required.
Its value is 1 by default.

### `cl_lag`

Sets the amount of synthetic lag.
The maximal allowed value is 1000. Default value is 0.
You may also use `ping +<value>` to get the same effect.

### `freelook`

Mouse look is enabled when it's turned on. Default value is 1.
It does the same effect like switching `-mlook`/`+mlook`.

### `cl_keypad`

Toggles between the using of keypad keys or not, 1 by default.

### `cl_chatmode`

If turned on (1), everything that Quake can't interpret
(not command or cvar or alias) is sent as chat. Similar to QW clients, you don't
have to type "say " every time you wanna say something.
Default value is "1".

### `cl_oldbob`

Toggles between old and new style weapon bobbing, "0" by default.

### `con_notify_intermission`

Console notify messages are shown on the intermission screen, "0" by default.

### `gl_fb_bmodels`

Turn fullbright polys on brush models on/off, 1 by default.

### `gl_fb_models`

Turn fullbright polys on alias models on/off, 1 by default.

### `r_explosionlightcolor`

Sets the color of explosions glows.
Only works if `r_explosionlight` is on.

Values: 0 - default, 1 - blue, 2 - red, 3 - purple, 4 - random.

### `r_rocketlightcolor`

Sets the color of flying rockets glows.
Only works if `r_rocketlight` is on.
Values: same as for `r_explosionlightcolor`.

### `gl_conalpha`

Sets the transparency of the console, 0.8 by default.
Can be 0.0 (totally transparent) - 1.0 (not transparent).

### `r_skybox`

Sets a custom skybox, "" by default (no skybox).
See more info about this at the `loadsky` command.

### `r_ringalpha`

Sets the transparency of your weapon model when you're invisible.
Default value is "0.4".

### `r_skyfog`

Applies fog on the sky, valid values are from 0.0 to 1.0. Used implicitly by newer maps.
It is only used if fog is also applied by using the `fog` command.
Default value is "0.5".

### `r_noshadow_list`

Ignores drawing shadow for mdl files added to this list.
Default value is "" (empty list).

### `capture_codec`

Contains the fourcc code of video codec's, 0 by default (no compression).
For example divx or xvid, etc.

### `capture_fps`

Sets on how many frames/sec you wish the video to be captured.
30.0 by default.

### `capture_console`

If set to 1, the console is also captured, otherwise not.
1 by default.

### `capture_dir`

Sets the directory where avis to be saved during capturing.
"capture" by default.

### `capture_mp3`

Turns mp3 audio compression on/off, 0 by default.

### `capture_mp3_kbps`

Sets mp3 compression's bitrate, 128 by default.
Only works if `capture_mp3` is 1 (trivial).

### `capture_avi_split`

Set this to the number of megabytes at which to split captured video into more
than one AVI file. The used video capture module has a problem with files
getting corrupted when reaching a size of over 2 gigabytes, so splitting them
into smaller files is a good idea to avoid this corruption. Default is
1900 megabytes. Setting to 0 disables splitting.

### `gl_externaltextures_world` and `gl_externaltextures_bmodels`

Load external textures when set to "1".
textures for _world should be placed inside textures/ or
textures/\<mapname\> dir while textures for _bmodels should be placed inside
textures/bmodels dir. Both are 1 by default.

### `gl_consolefont`

Specifies an external console charset.
Console charsets should be placed inside textures/charsets dir.

### `gl_smoothfont`

Gets fonts look smoother, "1" by default.

### `gl_loadlitfiles`

Loads static colored lights containing files (.lit) if "1".

### `gl_vertexlights`

Turns vertex lighting on alias models on/off, "0" by default.

### `scr_sshot_format`

Sets the type of screenshot image, "jpg" by default.
Can be "tga", "png" or "jpeg | jpg".

### `gl_waterfog`

Adds fog inside (not just water, but) all kinds of liquids, 1 by default.
Values: 0 - off, 2 - realistic, other - normal.

### `gl_waterfog_density`

For further customizing `gl_waterfog`, 1 by default.
Can be 0.0 (low amount of fog) - 1.0 (high amount of fog).

### `gl_caustics`

Turns caustic underwater polygons on/off, 0 by default.

### `gl_detail`

Turns detailed textures on/off, 0 by default.
Requires hi-end systems.

### `gl_solidparticles`

If set to "1" classic particles are to be drawn with Z buffer bits.
On some video cards this helps to keep the fps high.
0 by default.

### `gl_part_explosions`

Sets QMB explosions:
1 - normal, 2 - Q3 style.
0 by default.

### `gl_part_trails`

Sets QMB trails:
1 - normal, 2 - Q3 style.
0 by default.

### `gl_part_spikes`

Sets QMB spikes:
1 - normal, 2 - Q3 style (Q3 model required!).
0 by default.

### `gl_part_gunshots`

Sets QMB gunshots:
1 - normal, 2 - Q3 style (Q3 model required!).
0 by default.

### `gl_part_blood`

Sets QMB blood:
1 - normal, 2 - Q3 style.
0 by default.

### `gl_part_telesplash`

Sets QMB teleport splashes:
1 - normal, 2 - Q3 style (Q3 model required!).
0 by default.

### `gl_part_blobs`

Turns QMB spawn explosions on/off, 0 by default.

### `gl_part_lavasplash`

Turns QMB lavasplashes on/off, 0 by default.

### `gl_part_flames`

Sets QMB torch flames:
1 - normal, 2 - Q3 style.
0 by default.

### `gl_part_damagesplash`

Turns Q3 style on-screen damage splash on/off, 0 by default.

### `gl_bounceparticles`

Turns bouncing chunks on/off, 1 by default.
Only works for QMB particles.

### `crosshairalpha`

Sets the transparency of your crosshair. Default value is "1.0".

### `crosshairimage`

Sets an image as crosshair, "" (none) by default.
Crosshair images go to "crosshairs" subdirectory.

### `gl_loadq3models`

Loads quake3 models (.md3) if value is 1. By default, it's 0.

* mdl replacement files go to /progs folder
* bsp replacement files go to /maps folder
* ALL q3 texture files go to /textures/q3models folder

### `vid_vsync`

Turns vertical synchronization on/off, "0" by default.

### `gl_lerptextures`

Smooth out textures on alias models if turned on.
1 by default.

### `cl_independentphysics`

Server and Client framerates are independent. This setting is turned on by default.
To turn it off you need to start joequake with the following command line:

```
+set cl_independentphysics 0
```

### `vid_displayfrequency`

If not equals to 0, overrides the monitor's refresh rate to the given value.
0 by default.

### `gl_externaltextures_gfx`

Load external textures for menu and hud/status bar.
1 by default.

### `gl_texturemode_hud`

Set texture filtering mode for HUD elements, for example health display, menu
images and so on. Valid modes: `GL_LINEAR` and `GL_NEAREST`, `GL_LINEAR` by
default.

### `gl_texturemode_sky`

Set texture filtering mode for sky textures. Valid modes: `GL_LINEAR` and
`GL_NEAREST`, `GL_LINEAR` by default.

### `net_connectsearch`

Allows to enable or disable the search for hosts on use of the `connect`
command. It allows connecting by the server's hostname instead of by IP address.
However, the search takes a significant amount of time. So disabling it can
greatly speedup the time it takes to connect to a server by IP address.
"1" by default. Set to "0" to try to speedup connecting by IP address.

### `net_getdomainname`

Allows to enable or disable trying to find domain names for IP addresses using
DNS lookups. These can find domain names like nl2.badplace.eu from its IP
address. However, on Windows this lookup seems to take a long time. Disabling it
can therefore speed up connecting to a server significantly. As there will
barely be any usecase for the domain names, it is "0" by default. Set to "1" to
restore the original Quake behaviour with the lookups enabled.

## New commands

### `cmdlist`

Lists all console commands.

### `cvarlist`

Lists all console variables.

### `gamedir <path>`

Changes the mod's directory.
If you started Quake with e.g. `-game ctf` and during the game
you wish to play with an other mod, you can change it dinamically,
without quitting the game.
Relative pathnames like "newdir/ctf" are not allowed.

### `dir <wildcard>`

Lists all files matching wildcard.
Works the same way like other shells' dir command.

### `demdir <wildcard>`

The same as dir, except that this cmd is only for .dem files.
So please omit the .dem extension from the wildcard.

### `menu_demos`

Pops up the Demos menu, the same as you enter it from the Main Menu

### `writeconfig <filename>`

Writes a .cfg file with your actual game settings.

### `printtxt <filename>`

Prints a text file into the console.
You can now read SDA demos' txt files without quitting the game :)

### `getcoords <filename>`

Writes the player's origin into the given file or to "camfile.cam" if there
was no filename given.

### `volumeup`, `volumedown`

volumeup increases the volume by 0.1 and volumedown decreases by the same value.
The maximum volume is 1.0, it won't raise higher than that. You'll see a graphical
volume control bar at the top right corner of the screen for 2 seconds any time
you change the volume using volumeup or volumedown.

### `iplog`, `ipmerge`, `identify`

These commands are exported from ProQuake.
Please read the ProQuake documentation on using them.

### `loadsky <filename>`

Loads a custom skybox.
As you know (or not) a skybox consists of 6 images (6 sides of a box).
To set a skybox, you only have to add the common name of these six files,
since they contain 2 characters of skybox extension at the end.
For example, you have a skybox called "day", you have six images called
dayrt, daybk, daylf, dayft, dayup, daydn.
Than you only have to use `loadsky day`.
To get skyboxes, go to the FuhQuake page.
Skyboxes goes to "/env" directory.
This command also sets `r_skybox`'s value. You can change skyboxes through
this cvar as well.

### `loadcharset <filename>`

Loads an external console charset.
Console charsets should be placed inside textures/charsets dir.

### `capture_start <filename>`

Starts capturing an .avi file.

### `capture_stop`

Stops capturing.

### `capturedemo <filename>`

Starts playing a demo and starts capturing it also with the same name.

### `keepdemo`

See description about `cl_autodemo` in the Cvars section.

### `toggleparticles`

Changes all `gl_part_*` vars' values between 0 (off), 1 (QMB) or 2 (Quake3).
A pleasant way if you wouldn't like to set all variables one by one.

### `toggledecals`

Changes all `gl_decals_*` vars' values to their opposite.

### `fog`

Applies fog to the map. Used implicitly by newer maps.
Usage:
`fog <density>`
`fog <red> <green> <blue>`
`fog <density> <red> <green> <blue>`
`<density>` defines the thickness of the fog.
`<red> <green> <blue>` values define the color of the fog.

## Other features

### Parameter completion

I think people will prefer this feature alot more to using `dir` or `demdir`.
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
`playdemo`, `capture_start`, `capturedemo`, `printtxt`, `map`, `exec`, `load`,
`loadsky`, `r_skybox`, `loadcharset`, `gl_consolefont` and `crosshairimage`.
None of them require any extension to be given, they'll automatically find
their filetypes.

### Maps, Demos, Mods menu

JoeQuake added 3 new menus to Quake:
The Maps menu contains a list of all available maps in the actual path
including inside pak files as well.
The Demos menu gives you a shell about all the Quake subdirectories, and you
may browse between these dirs and watch demos in any folder.
The Mods menu also shows all the Quake subdirectories, and choosing any of them
reloads the specific mod immediately (as if you have started Quake `-game <moddir>`)

### 32bit textures support (GL only)

The whole function copied from FuhQuake which contained loading TGA and PNG
images and I extended this with JPEG/JPG support as well. The handling of
directories is purely the same as in FuhQuake:

All textures go to a "textures" folder, which can contain subfolders:

* "bmodels" for brush model textures (i.e. textures/bmodels)
* "models" for alias model textures (i.e. textures/models)
* "sprites" for sprite model textures (i.e. textures/sprites)
* "wad" for hud pictures (i.e. textures/wad)
* "charsets" for custom character sets (i.e. textures/charsets)
* "particles" for custom particle images (i.e. textures/particles)

You can also create map name folders, so then the textures for that map will
be loaded from there (e.g. textures/ztndm3).
The only difference to FuhQuake is that JoeQuake doesn't use map groups, so
don't try "exmy" folder for id1 maps as you did in FuhQuake.

I can only provide a few hud textures made by myself, but if you want more,
head over to the FuhQuake page and download the installer which contain lots
of cool textures to plenty of maps, status bar images, new menu pictures etc.
You can also get textures from the Quake Retexturing project:
<http://www.quake.cz/winclan/qe1>

(NOTE that there're no textures/ parent folder in the following paths)
Custom crosshairimages go to "crosshairs" folder.
Custom skyboxes go either to "env" or to "gfx/env" folder.

### Coloured lights (GL only)

Both dynamic and static colored lights are available in JoeQuake.
You can choose from various colors to set your explosion or rocket lightning.
For static colored lights, you can find a .lit files (files containing the color
information for maps) package on the FuhQuake page.

You may place lit files either to "maps/lits" or to "lits" folder.

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

## NOTE for linux GLX users

You need to have GLX installed and set up correctly. To install GLX, you need
to download the GLX drivers from your graphic card manufacturer's home page.

You also need to have `libfmod-3.73.so` (or later) to get music if you play with
Nehahra. You can get it from <http://www.fmod.org/>

## License and Warranty

JoeQuake is released under the GNU GPL license.
You may freely redistribute or modify JoeQuake as you wish.

## Credits

I would like to thank the following people for using their stuff:

* Sphere, for his help and participation in development of JoeQuake 🖤
* A. "Fuh" Nourai, for every FuhQuake addition
* Anton "Tonik" Gavrilov, for every ZQuake addition
* John Fitzgibbons, Eric Wasylishen, Spike and all the QuakeSpasm devs for every QS addition
* fenix@io.com, for alias model interpolation
* LordHavoc, for lerping alias model textures
* Alexander Kovalchuk, for plenty of [sons]Quake additions
* Anthony Bailey, for the avi capturing code
* Slawomir Mazurek, for his help on linux

I would also like to thank the following people for inspiring, giving ideas:

* Attila Csernyik
* Thomas Stubgaard
* Denis Nazarov
* Martin Selinus
* Nolan Pflug
* people @ RuneCentral forum, especially Baker and Sputnikutah
* people @ QuakeSpeedrunning discord channel

## External resources

The JoeQuake package contains hi-resolution images made by the following artists:

* menu graphics, HUD icons, character sets by Moon[Drunk]
* HUD player faces by PrimeviL
* console background by Joseph "BootGuyJoe" Hanley

```
Jozsef Szalontai
joequake@gmail.com
```
