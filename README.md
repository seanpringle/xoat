xoat
====

*X Obstinate Asymmetric Tiler*

* Designed for wide screens, including multi-head support.
* Static tiling; you get just three fixed tiles and windows never move automatically.
* Bare minimum EWMH to support panels and [simpleswitcher](https://github.com/seanpringle/simpleswitcher)
* A few keyboard controls for moving, focusing, cycling, closing, and finding windows.
* Transient windows and dialogs are centered on parent, not tiled.
* Splash screens and notification popups are displayed as requested, not tiled.
* config.h for customization of borders and keys.

### The Layout

	--------------------------------
	|                    |         |
	|                    |         |
	|                    |    2    |
	|          1         |         |
	|                    |---------|
	|                    |         |
	|                    |    3    |
	--------------------------------

### Philosophy

* Asymmetric tiling layouts are more useful than symmetric ones.
	* A master/stack layout is fine but only if the stack is itself asymmetric.
* Static tiling is more practical than dynamic tiling or floating.
	* Windows moving and resizing without user input is bad.
	* Having a choice of tiling layouts at run time is distracting.
	* Apps that remember their size can handily be placed back in the correct tile.
* Three tiles suffice because:
	* App windows are always one of four types:
		1. Large work-being-done apps
		2. Medium monitoring-something apps
		3. Small background-chat-music apps
		4. Apps people should not use ;-)
* Keyboard controls generally preferred over the mouse, when practical.
	* Faster. Muscle memory.
* Click-to-focus model preferred over focus-follows-mouse.
	* FfM means mentally keeping track of the mouse or warping the pointer around.
	* CtF is harder to get wrong and makes it easy to forget about the mouse for some tasks.
* 2/3 is a nice fraction.
