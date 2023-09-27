### 32bit textures support

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

### Coloured lights

Both dynamic and static colored lights are available in JoeQuake.
* For dynamic colored lights, you can choose from various colors to set your
desired explosion or rocket lighting. See more details at 
`r_explosionlightcolor` and `r_rocketlightcolor`
* For static colored lights, you can download .lit files (files containing the color
information for maps) from the internet (just google for them). You need to place 
lit files either to "maps/lits" or to "lits" folder.