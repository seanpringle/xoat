% XOAT(1)

# NAME

xoat \- X11 Obstinate Asymmetric Tiler

# SYNOPSIS

**xoat**

# DESCRIPTION

A static tiling window manager.

License: MIT/X11

# USAGE

See config.h for customization.

These are the default key bindings:

Mod4-1
:	Focus tile 1.

Mod4-2
:	Focus tile 2.

Mod4-3
:	Focus tile 3.

Shift-Mod4-1
:	Move window to tile 1.

Shift-Mod4-2
:	Move window to tile 2.

Shift-Mod4-3
:	Move window to tile 3.

Mod4-Tab
:	Flip top two windows in a tile.

Mod4-` (grave)
:	Cycle all windows in a tile.

Mod4-Escape
:	Close a window.

Mod4-f
:	Toggle state fullscreen. While in fullscreen mode, an window is considered to be in tile 1.

Mod4-a
:	Toggle state above (only placed above fullscreen windows).

Mod4-Right
:	Focus next monitor.

Mod4-Left
:	Focus previous monitor.

Shift-Mod4-Right
:	Move window to next monitor.

Shift-Mod4-Left
:	Move window to previous monitor.

Shift-Mod4-x
:	Launch dmenu_run

F1
:	Launch urxvt

Mod4-F1
:	Raise windows in tag 1.

Shift-Mod4-F1
:	Place current window in tag 1.

Shift-Control-Mod4-F1
:	Remove current window from tag 1.

Mod4-F2
:	Raise windows in tag 2.

Shift-Mod4-F2
:	Place current window in tag 2.

Shift-Control-Mod4-F2
:	Remove current window from tag 2.

Mod4-F3
:	Raise windows in tag 3.

Shift-Mod4-F3
:	Place current window in tag 3.

Shift-Control-Mod4-F3
:	Remove current window from tag 3.

# SEE ALSO

**dmenu** (1)

# AUTHOR

Sean Pringle <sean.pringle@gmail.com>