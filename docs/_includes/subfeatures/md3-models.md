### MD3 models

JoeQuake supports model files in MD3 format. To get MD3 models loaded, set `gl_loadq3models 1` in the console, or in the menu turn the *Options / Renderer options / Load MD3 models* setting to ON.

All MD3 models must be placed in the `progs` subfolder and have the same name as the regular MDL model files. For example to override the `grenade.mdl` file, you have to put the `grenade.md3` file into the `progs` subfolder.

You can also use Quake 3 player multimodels in JoeQuake the following way:
- Create a `player` subfolder in the `progs` folder.
- Copy the `lower.md3`, `upper.md3` and `head.md3` model files of your desired Quake 3 player.
- Copy `skin` files of each multimodel (e.g. `lower_*.skin`, `head_*.skin`, etc). JoeQuake can only handle 1 skin at a time, so make sure you only place 1 skin per model, otherwise the texture for the multimodel will not be displayed properly.
- Copy the `animation.cfg` file.
- You can also have view weapons displayed in the player's hand. You have to place the weapon models to the `progs` folder with the following naming convention: `w_<q1-weapon-name>.md3` (e.g. `w_shot.md3` for shotgun, `w_nail.md3` for nailgun, etc). I encourage you to familiarize yourself with Quake weapon names by looking into the Quake *.pak files.

![Quake 3 players](/assets/img/jq-md3-players.jpg)