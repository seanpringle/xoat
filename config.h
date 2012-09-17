// xoat config.

#define BORDER 2
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"
#define BORDER_URGENT "Red"
#define BORDER_ABOVE "Dark Green"

// There are three static tiles called SPOT1, SPOT2, and SPOT3.
// Want more tiles? Different layouts? Floating? Go away ;)
// -------------------------------
// |                   |         |
// |                   |         |
// |                   |    2    |
// |         1         |         |
// |                   |---------|
// |                   |         |
// |                   |    3    |
// -------------------------------

// Actually, there are a few layout choices, but only at build time.

// The layout can be flipped so SPOT1 is on the right.
// If you do this, review the directional move/focus key bindings too.
//#define SPOT1_ALIGN SPOT1_RIGHT
#define SPOT1_ALIGN SPOT1_LEFT

// Width of SPOT1 as percentage of screen width.
#define SPOT1_WIDTH_PCT 67

// Height of SPOT2 as percentage of screen height.
#define SPOT2_HEIGHT_PCT 67

// Make new windows go to the same spot as the current window.
// This implies auto-raise and focus stealing.
//#define SPOT_START SPOT_CURRENT

// Make new windows go to the spot of best fit.
// Works best when apps remember or specify their size.
// If spot is not current, window won't steal focus.
#define SPOT_START SPOT_SMART

// Make all new windows go to a specific spot.
// If spot is not current, window won't steal focus.
//#define SPOT_START SPOT1

// If on multi-head, place windows on monitor N.
// (0-based index, same order as xrandr list)
//#define MONITOR_START 0

// If on multi-head, place windows on monitor holding current window.
#define MONITOR_START MONITOR_CURRENT

// Should new windows be automatically focused, or ignored until focused manually?
// IGNORE means new windows only steal focus if they obscure the current window.
// STEAL means new windows always steal focus.
//#define FOCUS_START FOCUS_IGNORE
#define FOCUS_START FOCUS_STEAL

// If you use "AnyModifier" place those keys at the end of the array.
binding keys[] = {

	// Focus the top-most window in a spot.
	{ .mod = Mod4Mask, .key = XK_Left,  .act = action_focus, .num = SPOT1 },
	{ .mod = Mod4Mask, .key = XK_Up,    .act = action_focus, .num = SPOT2 },
	{ .mod = Mod4Mask, .key = XK_Right, .act = action_focus, .num = SPOT2 },
	{ .mod = Mod4Mask, .key = XK_Down,  .act = action_focus, .num = SPOT3 },

	// Move the current window to another spot.
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Left,  .act = action_move, .num = SPOT1 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Up,    .act = action_move, .num = SPOT2 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Right, .act = action_move, .num = SPOT2 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Down,  .act = action_move, .num = SPOT3 },

	// Flip between the top two windows in the current spot.
	{ .mod = Mod4Mask, .key = XK_Tab,    .act = action_other },

	// Cycle through all windows in the current spot.
	{ .mod = Mod4Mask, .key = XK_grave,  .act = action_cycle },

	// Gracefully close the current window.
	{ .mod = Mod4Mask, .key = XK_Escape, .act = action_close },

	// Toggle current window full screen.
	{ .mod = Mod4Mask, .key = XK_f, .act = action_fullscreen },

	// Toggle current window above.
	{ .mod = Mod4Mask, .key = XK_a, .act = action_above },

	// Switch focus between monitors.
	{ .mod = Mod4Mask, .key = XK_Next,  .act = action_focus_monitor, .num = +1 },
	{ .mod = Mod4Mask, .key = XK_Prior, .act = action_focus_monitor, .num = -1 },

	// Move windows between monitors.
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Next,  .act = action_move_monitor, .num = +1 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Prior, .act = action_move_monitor, .num = -1 },

	// Launcher
	{ .mod = Mod4Mask, .key = XK_x, .act = action_command, .data = "dmenu_run" },

	// Snapshot state
	{ .mod = Mod4Mask, .key = XK_s, .act = action_snapshot },
	{ .mod = Mod4Mask, .key = XK_r, .act = action_rollback },

	// Find or start apps by WM_CLASS (lower case match).
	// Only works for apps that use some form of their binary name as their class...
	{ .mod = AnyModifier, .key = XK_F1, .act = action_find_or_start, .data = "konsole"  },
	{ .mod = AnyModifier, .key = XK_F2, .act = action_find_or_start, .data = "chromium" },
	{ .mod = AnyModifier, .key = XK_F3, .act = action_find_or_start, .data = "pcmanfm"  },
	{ .mod = AnyModifier, .key = XK_F4, .act = action_find_or_start, .data = "kate"     },

	{ .mod = AnyModifier, .key = XK_F5, .act = action_find_or_start, .data = "firefox"  },
	{ .mod = AnyModifier, .key = XK_F6, .act = action_find_or_start, .data = "xchat"    },
	{ .mod = AnyModifier, .key = XK_F7, .act = action_find_or_start, .data = "pidgin"   },

	{ .mod = AnyModifier, .key = XK_Menu, .act = action_command, .data = "xowl" },
};