## JoeQuake SDL Linux fork

This is a fork of
[Joe's JoeQuake repository](https://github.com/j0zzz/JoeQuake), the speedrunning
focused Quake engine.  It adds files to allow building the SDL version of
JoeQuake on Linux.  To build first install dependencies:

```bash
# For Ubuntu users
sudo apt-get update
sudo apt-get install cmake build-essential libz-dev libsdl2-dev libjpeg-dev libgl1-mesa-dev libpng-dev
```
```bash
# For Arch users
sudo pacman -Syu base-devel cmake zlib sdl2 libjpeg-turbo mesa libpng
```

...and then run the following from the repository root:
```bash
mkdir build
cd build
cmake ..
make
# binary is written to `<repo root>/build/trunk/joequake-gl`
```

Further documentation:
- [Guide for SDL specific JoeQuake features](SDL.md)


This branch is heavily based on
[Sphere's Linux fork of JoeQuake](https://github.com/kugelrund/JoeQuake/tree/linux).

The original JoeQuake README is below.

---

## Description

JoeQuake is a custom Quake engine designed exclusively for speedrunning.  
It was originated from the official id software [GLQuake source code](https://github.com/id-Software/Quake).

JoeQuake features several improvements over the vanilla GLQuake client, just to name a few:
* Independent server-client physics
* Ghost mode recording
* Browsable Demos/Maps/Mods menus
* Restructured, more detailed Options menu
* Mouse cursor in menu
* Supporting FitzQuake and RMQ protocols
* Supporting a wide range of custom mods (including Quake remastered)
* Colored static and dynamic lighting
* External texture formats (TGA/PNG/JPG)
* Capturing demos to AVI file
* SDA demo database, for directly downloading/watching speedrun records from speeddemosarchive.com/quake/

## Binaries

### Windows

You can download JoeQuake Windows (x86) releases here:

http://joequake.runecentral.com/downloads.html

### Linux

Unfortunately this main fork does not provide any Linux binaries.
You may compile your own Linux binaries from the following JoeQuake 
contributors (look for linux related branches):

https://github.com/kugelrund/JoeQuake

https://github.com/matthewearl/JoeQuake-1

## License and Warranty

JoeQuake is released under the GNU GPL license.
You may freely redistribute or modify JoeQuake as you wish.

## Credits

### Contributors

* Jozsef Szalontai - lead programmer
* Sphere - server/client bugfixes, several QoL improvements
* Matthew Earl - entire ghost recording feature, SDL port
* Karol Urbański - vorbis/mp3 cross-platform support, bhop practice display, SDA demo browser

### Authors whose code was re-used in JoeQuake

* A. "Fuh" Nourai (FuhQuake additions)
* Anton "Tonik" Gavrilov (ZQuake additions)
* QuakeSpasm developers for every QS addition and their useful hints, tips
	* John Fitzgibbons
	* Ozkan Sezer
	* Eric Wasylishen
	* Axel Gneiting
	* Andrei Drexler
	* Spike
* Creators of the Minizip library: http://www.winimage.com/zLibDll/minizip.html
* Creators of the cJSON library: https://github.com/DaveGamble/cJSON
* Creators of the set library: https://github.com/barrust/set
* fenix@io.com, for alias model interpolation
* LordHavoc, for lerping alias model textures
* Alexander Kovalchuk, for plenty of [sons]Quake additions
* Anthony Bailey, for the avi capturing code
* Slawomir Mazurek, for his help on linux
* CrashFort, for NeaQuake additions
* R00k, for Tremor/QRack additions

### Inspirational persons

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
* mouse cursor (unknown artist, found on gfx.quakeworld.nu)

```
Jozsef Szalontai
joequake@gmail.com
```
