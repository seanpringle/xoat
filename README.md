xoat
====

*X Obstinate Asymmetric Tiler*

* Designed for wide screens, including multi-head support.
* Static tiling! Just configure sizes of the three fixed tiles.
* Windows never move between tiles without user input.
* Basic EWMH to support panels and [simpleswitcher](https://github.com/seanpringle/simpleswitcher).
* A few keyboard controls for moving, focusing, cycling, closing, and finding windows.
* Transient windows and dialogs are centered on parent, not tiled.
* Splash screens and notification popups are displayed as requested, not tiled.
* Pointer warps appropriately when keyboard controls and EWMH affect focus.
* .xoatrc or config.h for customization.

### The Layout

* Tile proportions can be adjusted.
* Layout can be flipped horizontally.
* Xrandr rotated monitor also rotates the layout (always 90 degrees, right).
* Fullscreen (no borders, above panels) windows suported normally.
* Maximized windows (with borders and panels) supported in spot 1.
* Maximized-vertically windows supportted in spot 2.
* Maximized-horizontally windows supported in spot 3.

...

	---------------------------------     ---------------------------------
	|                     |         |     |         |                     |
	|                     |         |     |         |                     |
	|                     |    2    |     |    2    |                     |
	|          1          |         |     |         |           1         |
	|                     |---------|     |---------|                     |
	|                     |         |     |         |                     |
	|                     |    3    |     |    3    |                     |
	---------------------------------     ---------------------------------

	---------------------     ---------------------
	|                   |     |       |           |
	|                   |     |   3   |     2     |
	|                   |     |       |           |
	|         1         |     |-------------------|
	|                   |     |                   |
	|                   |     |                   |
	|                   |     |                   |
	|                   |     |                   |
	|-------------------|     |         1         |
	|       |           |     |                   |
	|   3   |     2     |     |                   |
	|       |           |     |                   |
	---------------------     ---------------------

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
	* Want more tiles? Buy more monitors!
* Keyboard controls generally preferred over the mouse, when practical.
	* Faster. Muscle memory.
* Click-to-focus model preferred over focus-follows-mouse.
	* CtF is harder to get wrong in all cases.
	* FfM means mentally keeping track of the mouse pointer.
	* Avoiding the mouse entirely should just work, and not unexpectedly affect focus.
* 60:40 is a nice ratio.

### config.h

All customization happens in config.h or .xoatrc; see in-line comments. The former is tracked, so use a local git branch or a merge tool to protect any customization.

















