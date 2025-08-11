### SDA demo browser

The existing Demos menu have been improved with additional tabs to give access to the [Speed Demos Archive website](https://quake.speeddemosarchive.com/quake) and downloadable content. You can read the latest news and watch any demo listed on the SDA site: JoeQuake will download the selected demo and start playback. This entire feature was added by [Karol Urbanski](https://github.com/dexsda).  
Please note that all the demo information (map, player, skill, etc) displayed on the Local Demos tab are only visible in the SDL version.  
[![Local demos]({{ site.github.url }}/assets/img/jq-local-demos.jpg)]({{ site.github.url }}/assets/img/jq-local-demos.jpg)
[![SDA news]({{ site.github.url }}/assets/img/jq-sda-news.jpg)]({{ site.github.url }}/assets/img/jq-sda-news.jpg)
[![SDA demos]({{ site.github.url }}/assets/img/jq-sda-demos.jpg)]({{ site.github.url }}/assets/img/jq-sda-demos.jpg)

### Orbit camera mode

Orbit camera mode is a special camera mode during demo playback, implemented by [Matthew Earl](https://github.com/matthewearl). It is a freefly camera around the player (in 3rd person view) looking from a specified distance. The distance and angle can be freely changed by the viewer. You can access orbit mode on the Demo player UI.  
[![Wallhacked monsters]({{ site.github.url }}/assets/img/jq-orbit-mode.gif)]({{ site.github.url }}/assets/img/jq-orbit-mode.gif)

### Path tracer

Path tracer draws sequence of points with the player's actual position. These positions form a virtual path the player took. This method can be helpful primarily for speedrunning, to learn another player's movement technique. This feature was added by [Philipp Kölmel](https://github.com/philippkoelmel). You can read more details about this feature [here]({{ site.github.url }}/cvars-commands#path-tracer).
[![Path tracer]({{ site.github.url }}/assets/img/jq-path-tracer.jpg)]({{ site.github.url }}/assets/img/jq-path-tracer.jpg)

### Bunnyhop practice display tools

On top of the path tracer, you can now also switch on a lot of movement/speed related information to be shown on the UI. This entire idea was introduced and fully implemented by [Karol Urbanski](https://github.com/dexsda). Read the complete list of all available options [here]({{ site.github.url }}/cvars-commands#show_bhop_stats).
[![Bhop practice tools]({{ site.github.url }}/assets/img/jq-bhop-practice-tools.gif)]({{ site.github.url }}/assets/img/jq-bhop-practice-tools.gif)

### Cross platform support of mp3/ogg music

[Karol](https://github.com/dexsda) also replaced the old and outdated fmod module used for mp3 and ogg music playback with the more modern libogg and libvorbis modules. This should make life of especially the Linux users a lot more comfortable.
