% XOAT(1)

# NAME

xoat \- X11 Obstinate Asymmetric Tiler

# SYNOPSIS

**xoat** [restart] [exit]

# DESCRIPTION

A static tiling window manager.

License: MIT/X11

# USAGE

See config.h for customization.

These are the default key bindings:

Mod4-Left
:	Focus tile 1.

Mod4-Right  (and Up)
:	Focus tile 2.

Mod4-Down
:	Focus tile 3.

Shift-Mod4-Left
:	Move window to tile 1.

Shift-Mod4-Right  (and Up)
:	Move window to tile 2.

Shift-Mod4-Down
:	Move window to tile 3.

Mod4-Tab
:	Flip top two windows in a tile.

Mod4-` (grave)
:	Cycle all windows in a tile.

Mod4-[1-9]
:	Raise nth window in a tile.

Mod4-Escape
:	Close a window.

Mod4-f
:	Toggle state fullscreen. While in fullscreen mode, an window is considered to be in tile 1.

Mod4-Next  (Page Down)
:	Focus next monitor.

Mod4-Prior  (Page Up)
:	Focus previous monitor.

Shift-Mod4-Next
:	Move window to next monitor.

Shift-Mod4-Prior
:	Move window to previous monitor.

Shift-Mod4-x
:	Launch dmenu_run

F1
:	Launch urxvt

# OPTIONS

All configuration is done via config.h.

The xoat binary may be used to send messages to a running instance of xoat. They include:

xoat exit
:	Exit the window manager.

xoat restart
:	Restart the window manager in place without affecting the X session.

# SEE ALSO

**dmenu** (1)

# AUTHOR

Sean Pringle <sean.pringle@gmail.com>
