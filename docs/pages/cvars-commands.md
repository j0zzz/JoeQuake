---
layout: page
title: Settings
category: cvars
permalink: /cvars-commands
---

1. TOC
{:toc}

### Cvars

#### Renderer

##### `r_explosiontype`

Sets various types of explosions:  
`0` - normal  
`1` - just sprite  
`2` - just particles  
`3` - slight blood  
Its value is `0` by default.

##### `r_explosionlight`

Turns explosion's light on/off, `1` by default.

##### `r_rocketlight`

Turns flying rocket's light on/off, `1` by default.

##### `r_rockettrail`

Sets flying rocket's trail:  
`0` - no trail  
`1` - default trail  
`2` - grenade trail

* when using `gl_part_trails 1`:
same as above, plus there's a new state: `3` - darkplaces trail.
* when using `gl_part_trails 2`:
there're no different trails, just a Quake 3 style one.

Value is `1` by default.

##### `r_grenadetrail`

Sets flying grenade's trail:  
`0` - no trail  
`1` - default trail  
`2` - rocket trail

* when using `gl_part_trails 1`:
same as above, plus there's a new state: `3` - darkplaces trail
* when using `gl_part_trails 2`:
there're no different trails, just a Quake 3 style one

Value is `1` by default.

##### `r_powerupglow`

Turns powerup glow around player's body on/off, `1` by default.

##### `r_coloredpowerupglow`

Turns forced coloration of powerup glow for quad and pentagram on/off, `1` by default.

##### `r_fullbrightskins`

Makes player(s)'s skins fullbright, `0` by default.

##### `r_explosionlightcolor`

Sets the color of explosions glows:  
`0` - default  
`1` - blue  
`2` - red  
`3` - purple  
`4` - random  
Only works if `r_explosionlight` is on.

##### `r_rocketlightcolor`

Sets the color of flying rockets glows.
Only works if `r_rocketlight` is on.
Values: same as for `r_explosionlightcolor`.

##### `r_skybox`

Sets a custom skybox, `""` by default (no skybox).
See more info about this at the `loadsky` command.

##### `r_ringalpha`

Sets the transparency of your weapon model when you're invisible.
Default value is `0.4`.

##### `r_skyfog`

Applies fog on the sky, valid values are from `0.0` to `1.0`. Used implicitly by newer maps.
It is only used if fog is also applied by using the `fog` command.
Default value is `0.5`.

##### `r_noshadow_list`

Ignores drawing shadow for mdl files added to this list.
Default value is `""` (empty list).

##### `r_oldwater`

Since version 0.17.4 water surfaces are drawn via the frame buffer.
Switching this setting on reverts to the original water surface rendering method.
`0` by default.

##### `r_litwater`

When switched on, water surfaces are lit on compatible maps.
`1` by default.

##### `r_waterquality`

Specifies the quality (tesselation level) of the water surfaces' warping animation.
Can be between `3` and `64`.
`8` by default.

##### `r_waterwarp`

When switched on, puts a warp effect on the screen while underwater.
`0` by default.

##### `r_particles`

Toggles drawing of classic particles:  
`0` - No particles drawn  
`1` - Circle shaped particles  
`2` - Square shaped particles  
Default value is `1`.

#### Client

##### `cl_r2g`

Replaces the rocket model with grenade, `0` by default.
(popular in deathmatch)

##### `cl_truelightning`

Makes the lightning gun's bolt more precise, `0` by default.
Can be `0.0` (off) - `1.0` (most precise).

##### `cl_bobbing`

Turns Quake3-style bobbing on, `0` by default.

##### `cl_deadbodyfilter`

Removes dead bodies (both for players and monsters).  
When using with value `1`, the dead body is to be removed in the last animation frame.  
When using with value `2`, it is removed immediately when the monster/player dies.  
Its value is `0` (off) by default.

##### `cl_gibfilter`

Removes gibs if turned on, `0` (off) by default.

##### `cl_sbar`

Toggles between transparent (`0`), old/original (`1`) and alternative (`2`) huds, `0` by default.

##### `cl_sbar_offset`

Lifts the position of hud icons from the bottom with the specified offset. Valid range is from `0` (no offset) to `8` (max offset). Only applied to hud styles `0` and `2`.  
Default value is `0`.

##### `cl_demorewind`

Toggles between normal and backward demoplaying, `0` by default.
Only works when playing a demo.

##### `mapname`

Contains actual map's name, for built-in use.

##### `cl_bonusflash`

Turns pickup flashes on/off, `1` by default.

##### `cl_muzzleflash`

Turns muzzle flashes (flashes when firing) on/off, `1` by default.

##### `cl_demospeed`

Changes the playback speed of a demo, `1` by default.
Values < `1` mean slow motion, while > `1` result fast forward.

##### `cl_autodemo`  
##### `cl_autodemo_name`

These commands handle automatic demo recording.  
- If `cl_autodemo` is `1`
    - and `cl_autodemo_name` is `empty`, a new .dem file is recorded at every level start. The file naming format is `<map_name>_<date_of_recording>_<time_of_recording>.dem`
    - and `cl_autodemo_name` is `not empty`, the `<cl_autodemo_name>.dem` file is recorded at every level start. To save the current recording session, use the `keepdemo` command. This command is only valid on the intermission/finale screen and renames the temporary recording using the following naming format: `<cl_autodemo_name>_<finished_time>_<skill>_<player_name>.dem`
- If `cl_autodemo` is `2`, the naming format changes to the following:
`<mapname>_<finished_time>_<skill>_<player_name>.dem`
- If `cl_autodemo` is `0`, there is no automatic recording.

##### `cl_maxfps`

Customizes the maximal fps, `72` by default.  
If `cl_independentphysics` is switched **OFF** (default is **ON**), the value of this cvar is reverted back to `72` if you start recording a demo.

##### `cl_advancedcompletion`

Toggles between advanced and normal (old) command/cvar completion.
Its value is `1` by default.

##### `cvar_savevars`

Switch for the `writeconfig` command.  
`0` - it will save only archieved vars.  
`1` - it will save the ones which have other than default values.  
`2` - it will save ALL variables.  
Its value is `0` by default.

##### `cl_confirmquit`

Switch for confirmation when exiting, `0` means no confirmation required.
Its value is `1` by default.

##### `cl_lag`

Sets the amount of synthetic lag, in milliseconds.  
The maximal allowed value is `1000`. Default value is `0`.  
You may also use `ping +<value>` to get the same effect.

##### `cl_keypad`

Toggles between the using of keypad keys or not, `1` by default.

##### `cl_chatmode`

If turned on (`1`), everything that Quake can't interpret
(not command or cvar or alias) is sent as chat. Similar to QW clients, you don't have to type `say ` every time you wanna say something.  
Default value is `1`.

##### `cl_newbob`

Toggles between old and new style weapon bobbing, `0` by default.

##### `cl_gun_fovscale`

If turned on (`1`), the gun model is scaled according to the current FOV. Decimal values are also accepted for partial scaling.  
Default value is `0`.

##### `cl_minpitch`

Minimum value of pitch angle. Cannot be smaller than `-90`.
Default value is `-70`.

##### `cl_maxpitch`

Maximum value of pitch angle. Cannot be greater than `90`.
Default value is `80`.

##### `cl_demoui`

Switch for the demo player UI, `0` means the player is hidden.
Default value is `1`.

##### `cl_demouitimeout`

Time that you have to keep the mouse still and not hovering over the demo UI
before the UI hiding animation starts.

##### `cl_demouihidespeed`

Speed of the demo UI hiding animation which begins once the above timeout has
elapsed.

##### `cl_independentphysics`

Server and Client framerates are independent. This setting is turned on by default. To turn it off you need to start joequake with the following command line:

```
+set cl_independentphysics 0
```

#### Server

##### `sv_altnoclip`

Switches on alternative noclip movement where the player always moves towards
the direction he's looking at.
`1` by default.

##### `sv_noclipspeed`

Dedicated max speed value used only while noclip.
Its value is `320` by default.

#### Network

##### `net_connectsearch`

Allows to enable or disable the search for hosts on use of the `connect`
command. It allows connecting by the server's hostname instead of by IP address.
However, the search takes a significant amount of time. So disabling it can
greatly speedup the time it takes to connect to a server by IP address.
`1` by default. Set to `0` to try to speedup connecting by IP address.

##### `net_getdomainname`

Allows to enable or disable trying to find domain names for IP addresses using
DNS lookups. These can find domain names like `nl2.badplace.eu` from its IP
address. However, on Windows this lookup seems to take a long time. Disabling it
can therefore speed up connecting to a server significantly. As there will
barely be any usecase for the domain names, it is `0` by default. Set to `1` to
restore the original Quake behaviour with the lookups enabled.

#### Video

##### `vid_hwgammacontrol`

Turns gamma changing possibility on/off, `1` by default.
With using a value of `2`, it applies gamma for windowed modes as well.

##### `vid_vsync`

Turns vertical synchronization on/off, `0` by default.

##### `vid_displayfrequency`

If not equals to `0`, overrides the monitor's refresh rate to the given value.
`0` by default.

#### OpenGL

##### `gamma` (or `gl_gamma`)  
##### `contrast` (or `gl_contrast`)  

One of the most important features. These are for your viewing pleasure :)

##### `gl_fb_bmodels`

Turn fullbright polys on brush models on/off, `1` by default.

##### `gl_fb_models`

Turn fullbright polys on alias models on/off, `1` by default.

##### `gl_overbright`

Sets lighting overbright on map surfaces, `1` by default.

##### `gl_overbright_models`

Sets lighting overbright on models, `1` by default.

##### `gl_interpolate_animation`

Sets lerped model animation:  
`0` - Animations are not lerped (original "software" Quake behavior)  
`1` - Animations are lerped except weapon muzzleflash animations  
`2` - Animations are lerped including weapon muzzleflash animations  
Default value is `1`.

##### `gl_interpolate_movement`

Switches on lerped model movement, default value is `1` (on).

##### `gl_interpolate_distance`

Sets the maximum distance between vertexes where animation lerping takes place.
This variable was implemented exclusively for gl_interpolate_animation, when used with the value `2`.  
It is rather a technical thing, it serves fine-tuning of muzzleflash animations.
If you're not entirely familiar of this feature I recommend not adjusting it.
Its default value is `135`. It cannot be set higher than `300`.

##### `gl_externaltextures_world`  
##### `gl_externaltextures_bmodels`
##### `gl_externaltextures_models`

Load external textures when set to `1`.  
Textures for _world should be placed inside `textures/` or `textures/<mapname>` folder.  
Textures for _bmodels should be placed inside `textures/bmodels` folder.  
Textures for _models should be placed inside `textures/models` folder.  
Both are `1` by default.

##### `gl_consolefont`

Specifies an external console charset.
Console charsets should be placed inside textures/charsets dir.

##### `gl_smoothfont`

Gets fonts look smoother, `1` by default.

##### `gl_loadlitfiles`

Loads static colored lights containing files (.lit) if `1`.

##### `gl_vertexlights`

Turns vertex lighting on alias models on/off, `0` by default.

##### `gl_waterfog`

Adds fog inside (not just water, but) all kinds of liquids, `1` by default.  
Possible values:  
`0` - off  
`2` - realistic  
`<other>` - normal

##### `gl_waterfog_density`

For further customizing `gl_waterfog`, `1` by default.
Can be `0.0` (low amount of fog) - `1.0` (high amount of fog).

##### `gl_caustics`

Turns caustic underwater polygons on/off, `0` by default.

##### `gl_detail`

Turns detailed textures on/off, `0` by default.
Requires hi-end systems.

##### `gl_solidparticles`

If set to "1" classic particles are to be drawn with Z-buffer bits.
On some video cards this helps to keep the fps high.
`0` by default.

##### `gl_part_explosions`

Sets type of explosions:  
`0` - Classic  
`1` - QMB  
`2` - Quake 3  
`0` by default.

##### `gl_part_trails`

Sets type of particle trails:  
`0` - Classic  
`1` - QMB  
`2` - Quake 3  
`0` by default.

##### `gl_part_spikes`

Sets type of particle spikes:  
`0` - Classic  
`1` - QMB  
`2` - Quake 3  
`0` by default.

##### `gl_part_gunshots`

Sets type of particle gunshots:  
`0` - Classic  
`1` - QMB  
`2` - Quake 3  
`0` by default.

##### `gl_part_blood`

Sets type of blood:  
`0` - Classic  
`1` - QMB  
`2` - Quake 3  
`0` by default.

##### `gl_part_telesplash`

Sets type of teleport splashes:  
`0` - Classic  
`1` - QMB  
`2` - Quake 3  
`3` - Super secret Quake Live style :)  
`0` by default.

##### `gl_part_blobs`

Sets type of spawn explosions:  
`0` - Classic  
`1` - QMB  
`0` by default.

##### `gl_part_lavasplash`

Sets type of lava splash:  
`0` - Classic  
`1` - QMB  
`0` by default.

##### `gl_part_flames`

Sets type of torch flames:  
`0` - Classic  
`1` - QMB  
`0` by default.

##### `gl_part_damagesplash`

Turns Quake 3 style on-screen damage splash on/off, `0` by default.

##### `gl_part_muzzleflash`

Hides muzzleflash model objects from weapons when fired. Instead it replaces the muzzle flashes with particle effects.
`0` by default.

##### `gl_bounceparticles`

Turns bouncing chunks on/off, `1` by default.
Only works for QMB particles.

##### `gl_decal_blood`

Turns blood stain decal patches on/off, `0` by default.

##### `gl_decal_bullets`

Turns bullet mark decal patches on/off, `0` by default.

##### `gl_decal_sparks`

Turns nail spark decal patches on/off, `0` by default.

##### `gl_decal_explosions`

Turns explosion stain decal patches on/off, `0` by default.

##### `gl_decal_viewdistance`

Sets distance how far decal patches are visible from.
Default value is `2048`.

##### `gl_decaltime`

Sets the time (in seconds) how long decal patches remain visible.
Default value is `30`.

##### `gl_loadq3models`

Loads quake3 models (.md3) if value is `1`. By default, it's `0`.

* mdl replacement files must go to `/progs` folder
* bsp replacement files must go to `/maps` folder
* ALL Quake 3 texture files must go to `/textures/q3models` folder

Read a complete description about Quake 3 models support [here](/md3-models).

##### `gl_lerptextures`

Smooth out textures on alias models if turned on.
`1` by default.

##### `gl_externaltextures_gfx`

Load external textures for menu and hud/status bar.
`1` by default.

##### `gl_texturemode_hud`

Set texture filtering mode for HUD elements, for example health display, menu
images and so on. Valid modes: `GL_LINEAR` and `GL_NEAREST`, `GL_LINEAR` by
default.

##### `gl_texturemode_sky`

Set texture filtering mode for sky textures. Valid modes: `GL_LINEAR` and
`GL_NEAREST`, `GL_LINEAR` by default.

##### `gl_texture_anisotropy`

Sets the level of anisotropic texture filtering. A valid value should be a power of two: `2 4 8 16` (depending on your hardware's limits).
If you don't wanna use anisotropic filtering, set the value to `1`.

##### `gl_zfix`

Tries to prevent overlapping textures (z-fighting fix).
`0` by default.

##### `fog_custom`

Can store custom fog parameters which are only applied if the `fog_override` variable is set to 1.  
The values must be specified the following way: `<density> <red> <green> <blue>`  
- `<density>` defines the thickness of the fog.
- `<red> <green> <blue>` values define the color of the fog.

Default value is `0 0 0 0`, which means:  
`0` density (invisible)  
RGB: `0 0 0` (black color)

##### `fog_override`

Overrides the default fog parameters with the ones defined by the `fog_custom` variable.
`0` by default.

#### Screen

##### `scr_centermenu`

Makes the menu centered, `1` by default.

##### `scr_centersbar`

Makes the status bar (hud) centered, `0` by default.

##### `scr_widescreen_fov`

When turned on, fov (field of view) is properly calculated in widescreen
(16:9 or 16:10) resolutions also, `1` by default.

##### `scr_sbarscale_amount`

Scales (multiplies) the size of console font and status bar icons
with the specified number, `2` by default.

##### `scr_scorebarmode`

Toggles between vanilla (`0`) and QuakeSpasm (`1`) style scorebar layouts, `0` by default.

##### `scr_consize`

Adjust the console's size, `0.5` (half size) by default.
Can be `0.0` (nothing visible) - `1.0` (fully visble).

NOTE: when the console is pulled down, you can also adjust the size
with using the Ctrl+UP and Ctrl+DOWN keys.

##### `scr_cursor_sensitivity`

Sets the speed of the mouse cursor, `1` by default.

##### `scr_cursor_scale`

Sets the size of the mouse cursor, `1` by default.

##### `scr_cursor_alpha`

Sets the transparency of the mouse cursor, `1` by default.
Possible values are between `0.0` (fully transparent) and `1.0` (fully opaque).

##### `scr_sshot_format`

Sets the type of screenshot image, `jpg` by default.
Can be `tga`, `png` or `[jpeg|jpg]`.

##### `png_compression_level`

Sets the compression level of `png` screenshots.  
The value can be between `0` - `9`, where `0` means no compression and `9` means maximum compression.  
Default value is `1`.

##### `jpeg_compression_level`

Sets the quality level of `jpg` screenshots.  
The value can be between `0` - `100`, where `0` means worst quality (maximum compression) and `100` means best quality (least compression).  
Default value is `90`.

##### `scr_precisetime`

Show two decimal places of precision on the intermission screen time, `0` by
default.

#### Console

##### `gl_conalpha`

Sets the transparency of the console, `0.8` by default.
Can be `0.0` (totally transparent) - `1.0` (not transparent).

##### `con_notify_intermission`

Console notify messages are shown on the intermission screen, `0` by default.

##### `con_logcenterprint`

Centerprinted messages are shown in the console, `0` by default.

#### View

##### `v_gunkick`

Turns weapon's jarring every time when firing on/off, `0` by default.
Using with value `1` the weapon kicks the original (old) way, while using with
value `2` the kick is much smoother.

##### `v_contentblend`

Adjusts liquid blends between `0.0` - `1.0`.  
`1` by default.

##### `v_damagecshift`

Adjusts damage flashes between `0.0` - `1.0`.  
`1` by default.

##### `v_quadcshift`

Adjusts quad damage's blend between `0.0` - `1.0`.  
`1` by default.

##### `v_pentcsift`

Adjusts pentagram's blend between `0.0` - `1.0`.  
`1` by default.

##### `v_ringcshift`

Adjusts invisibilty's blend between `0.0` - `1.0`.  
`1` by default.

##### `v_suitcshift`

Adjusts environment suit's blend between `0.0` - `1.0`.  
`1` by default.

#### Hud

##### `crosshairsize`

Sets the size of the crosshair, `1.0` by default.

##### `crosshaircolor`

Sets the color of the crosshair, `79` (red) by default.

##### `crosshairalpha`

Sets the transparency of your crosshair. Default value is `1.0`.

##### `crosshairimage`

Sets an image as crosshair, `""` (none) by default.
Crosshair images go to `crosshairs` subdirectory.

##### `show_stats`

Print actual statistics (time, secrets, kills) to the upper right corner
of the screen, `1` by default.

Possible values:  
`0` - off  
`1` - only time  
`2` - time + kills + secrets  
`3` - only time, but only on if some reasonable event happens  
`4` - time + kills + secrets, but only on if some reasonable event happens  

##### `show_stats_length`

If show_stats is set to `2`, the printing takes as long as it's set in this
cvar, `0.5` by default. The value represents seconds.

##### `show_stats_small`

Replaces HUD numbers with console characters when showing `show_stats`'s
statistics. `0` by deafult.

##### `show_speed`  
##### `show_speed_x`  
##### `show_speed_y`  
##### `show_speed_alpha`

Prints the player's speed in units.
Position can be changed with the `_x` and `_y` coordinates. 
Transparency of the speed bar can be changed with the `_alpha` value.

##### `show_fps`  
##### `show_fps_x`  
##### `show_fps_y`

Prints the framerate in frames per second.
Position can be changed with the `_x` and `_y` coordinates.

##### `cl_clock`  
##### `cl_clock_x`  
##### `cl_clock_y`

Shows clock in the lower left corner. Position can be changed with the `_x` and `_y` coordinates. There are 4 predefined clock formats. `0` by default.

#### Mouse

##### `m_rate`

Sets mouse rate. Only works when the game is started with `-dinput` and `-m_smooth`.

##### `m_showrate`

Shows current mouse rate. Useful to set m_rate. Only works when the game is started with `-dinput -m_smooth`.

##### `freelook`

Mouse look is enabled when it's turned on. Default value is `1`.
It does the same effect like switching `-mlook`/`+mlook`.

#### AVI Capture

##### `capture_codec`

Contains the fourcc code of video codec's, `0` by default (no compression).
For example `divx` or `xvid`, etc.

##### `capture_fps`

Sets on how many frames/sec you wish the video to be captured.
`30.0` by default.

##### `capture_console`

If set to `1`, the console is also captured, otherwise not.
`1` by default.

##### `capture_dir`

Sets the directory where avis to be saved during capturing.
`capture` by default.

##### `capture_mp3`

Turns mp3 audio compression on/off, `0` by default.

##### `capture_mp3_kbps`

Sets mp3 compression's bitrate, `128` by default.
Only works if `capture_mp3` is `1` (trivial).

##### `capture_avi_split`

Set this to the number of megabytes at which to split captured video into more
than one AVI file. The used video capture module has a problem with files
getting corrupted when reaching a size of over 2 gigabytes, so splitting them
into smaller files is a good idea to avoid this corruption.  
Default is `1900` megabytes.  
Setting to `0` disables splitting.

### Commands

#### All

##### `cmdlist`

Lists all console commands.

##### `cvarlist`

Lists all console variables.

##### `find`  
##### `apropos`

Finds all relevant console commands and variables.

##### `gamedir <path>`

Changes the mod's directory.  
If you started Quake with e.g. `-game ctf` and during the game
you wish to play with an other mod, you can change it dinamically,
without quitting the game.  
Relative pathnames like `newdir/ctf` are not allowed.

##### `dir <wildcard>`

Lists all files matching wildcard. Works the same way like other shells' dir command.

##### `demdir <wildcard>`

The same as dir, except that this cmd is only for .dem files.
So please omit the .dem extension from the wildcard.

##### `menu_maps`  
##### `menu_demos`  
##### `menu_mods`

These commands display the appropriate new main menus, 
as if you were opened them from the *Main* menu.

##### `menu_mouse`  
##### `menu_hud`  
##### `menu_sound`  
##### `menu_view`  
##### `menu_renderer`  
##### `menu_textures`  
##### `menu_particles`  
##### `menu_decals`  
##### `menu_weapons`  
##### `menu_screenflahes`  
##### `menu_miscellaneous`

These commands display the appropriate options submenus,
as if you were opened them from the *Options* menu.

##### `menu_crosshair_colorchooser`  
##### `menu_sky_colorchooser`

These commands display the color chooser submenus,
as if you were opened them from:
- the *Options / Hud options / Crosshair color* menu
- the *Options / View options / Solid sky color* menu

##### `menu_namemaker`

This command displays the name maker submenu,
as if you were opened it from the *Multiplayer / Setup / Name maker* menu.

##### `writeconfig <filename>`

Writes a .cfg file with your actual game settings.

##### `printtxt <filename>`

Prints a text file into the console. You can now read SDA demos' txt files without quitting the game :)

##### `getcoords <filename>`

Writes the player's origin into the given file or to `camfile.cam` if there
was no filename given.

##### `volumeup`  
##### `volumedown`

`volumeup` increases the volume by `0.1`.  
`volumedown` decreases by the same value.  
The maximum volume is `1.0`, it won't raise higher than that. You'll see a graphical volume control bar at the top right corner of the screen for 2 seconds any time you change the volume using volumeup or volumedown.

##### `iplog`  
##### `ipmerge`  
##### `identify`

These commands are exported from ProQuake. Please read the ProQuake documentation on using them.

##### `loadsky <filename>`

Loads a custom skybox.  
As you know (or not) a skybox consists of 6 images (6 sides of a box).
To set a skybox, you only have to add the common name of these six files,
since they contain 2 characters of skybox extension at the end.  
For example, you have a skybox called `day`, you have six images called
`dayrt`, `daybk`, `daylf`, `dayft`, `dayup`, `daydn`. Then you only have to use `loadsky day`.  
Skyboxes goes to `/env` directory.
This command also sets `r_skybox`'s value. You can change skyboxes using
this cvar as well.

##### `loadcharset <filename>`

Loads an external console charset. Console charsets should be placed inside textures/charsets dir.

##### `capture_start <filename>`

Starts capturing an .avi file.

##### `capture_stop`

Stops capturing.

##### `capturedemo <filename>`

Starts playing a demo and starts capturing it also with the same name.

##### `keepdemo`

See description about `cl_autodemo` in the Cvars section.

##### `demoskip`

When playing back a marathon demo, skips the amount of levels by the set value. Negative numbers are also valid.  
For example: when playing back an episode 1 marathon (e1m1 - e1m7):
- using `skipdemo +2` on e1m2 will jump to e1m4
- using `skipdemo -3` on e1m6 will jump to e1m3

##### `demoseek`

Seeks the amount of seconds by the set value during demo playback.
Available usage modes (example values):
- `demoseek 12` will seek to the time 0:12
- `demoseek +3` will seek forward 3 seconds from the actual position
- `demoseek -5` will seek back 5 seconds from the actual position

##### `toggleparticles`

Changes all `gl_part_*` cvar values between `0` (off), `1` (QMB) or `2`(Quake3).  
A pleasant way if you want to toggle between particle themes and wouldn't like to set each variable one by one.

##### `toggledecals`

Changes all `gl_decal_*` cvar values between `[0|1]`.

##### `fog`

Applies fog to the map. Used implicitly by newer maps.  
Usage:  
`fog <density>`  
`fog <red> <green> <blue>`  
`fog <density> <red> <green> <blue>`  
`<density>` defines the thickness of the fog.  
`<red> <green> <blue>` values define the color of the fog.

##### `fog_set`

Saves the actual fog values (set at map start) to the `fog_custom` variable.

##### `printspawnparams [clientnum]`

Prints the spawn parameters of clients on the server. If `clientnum` is given,
prints the spawn parameters for the client with that number. If it is not given, prints the spawn parameters for all active clients on the server.

##### `setspawnparam <paramnum> <value> [clientnum]`

Sets the spawn parameter with index `paramnum` to `value`. If `clientnum` is
given, the spawn parameter of the client with that number is changed. If it is
not given, the spawn parameter of the host is changed.

##### `writenextspawnparams [clientnum] [filename]`

Writes a config to store the spawn parameters as they would be if the current
map is finished as is. Can be executed to start playing the next map with those
stored spawn parameters.

If this client is not the server host, the spawn parameters of this client are
written and assigned to the number `clientnum` (`0` if not given). If this
client is the server host, writes spawn parameters of the client with number
`clientnum`. If `clientnum` is not given, writes the spawn parameters for all
active clients on the server.

The name of the written config file can be given as `filename`. If not present,
an automatic name based on current map and time is used.

##### `sv_protocol`

Overrides the default `15` (NetQuake) protocol version with given value. Recognized values are `666` (FitzQuake) and `999` (RMQ).
This command primarly aims to keep JoeQuake compatible with mods.
