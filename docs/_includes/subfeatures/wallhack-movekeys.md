### Wallhacked monsters

It is now possible to see where the nearby monsters are moving, through walls! This new feature can be very beneficial to learn and analyze monster behavior when planning speedrun routes, especially if monster cooperation is involved.  
**Important**: this feature is not available while demo recording!
[![Wallhacked monsters]({{ site.github.url }}/assets/img/jq-wallhack.gif)]({{ site.github.url }}/assets/img/jq-wallhack.gif)

### Storing movement keys in demos

Quake demo files do not store the player inputs, but with some smart tweaking we made it available! Demos recorded from now on will contain the player's inputs for the following movement keys:
- move forward
- move backward
- strafe left
- strafe right 
- jump

[![Displayed movement keys]({{ site.github.url }}/assets/img/jq-movekeys.gif)]({{ site.github.url }}/assets/img/jq-movekeys.gif)

### Override ambient light levels

This is a big help to lighten dark areas which Quake is rich of. By using the console variable `r_ambient` it is now possible to add additional ambient lighting so very dark spots become much lighter.
[![Overridden ambient lights]({{ site.github.url }}/assets/img/jq-ambient-colors.gif)]({{ site.github.url }}/assets/img/jq-ambient-colors.gif)

### Quake palette colors available from the menu

Users can now pick colors right from the Quake palette when choosing a color for e.g. crosshair or model outlines.
[![Color chooser menu]({{ site.github.url }}/assets/img/jq-colorchooser-menu.jpg)]({{ site.github.url }}/assets/img/jq-colorchooser-menu.jpg)

### Animated zooming (from Ironwail)

Even though weapon zooming was already available via some smart bindings, JoeQuake now has built-in commands for zooming, with animation. Users may control the animation speed and zoom FOV values.
[![Zoom in and out]({{ site.github.url }}/assets/img/jq-zoom.gif)]({{ site.github.url }}/assets/img/jq-zoom.gif)
