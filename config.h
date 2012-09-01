// xoat config.

#define BORDER 2
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"
#define BORDER_URGENT "Red"

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

// If you use "AnyModifier" place those keys at the end of the array.
binding keys[] = {

	// Focus the top-most window in a spot.
	{ .mod = Mod4Mask, .key = XK_1, .act = ACTION_FOCUS_SPOT1 },
	{ .mod = Mod4Mask, .key = XK_2, .act = ACTION_FOCUS_SPOT2 },
	{ .mod = Mod4Mask, .key = XK_3, .act = ACTION_FOCUS_SPOT3 },

	// Move the current window to another spot.
	{ .mod = ShiftMask|Mod4Mask, .key = XK_1, .act = ACTION_MOVE_SPOT1 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_2, .act = ACTION_MOVE_SPOT2 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_3, .act = ACTION_MOVE_SPOT3 },

	// Flip between the top two windows in the current spot.
	{ .mod = Mod4Mask, .key = XK_Tab,    .act = ACTION_OTHER },

	// Cycle through all windows in the current spot.
	{ .mod = Mod4Mask, .key = XK_grave,  .act = ACTION_CYCLE },

	// Gracefully close the current window.
	{ .mod = Mod4Mask, .key = XK_Escape, .act = ACTION_CLOSE },

	// Toggle current window full screen.
	{ .mod = Mod4Mask, .key = XK_f, .act = ACTION_FULLSCREEN_TOGGLE },

	// Switch focus between monitors.
	{ .mod = Mod4Mask, .key = XK_Right, .act = ACTION_FOCUS_MONITOR_INC },
	{ .mod = Mod4Mask, .key = XK_Left,  .act = ACTION_FOCUS_MONITOR_DEC },

	// Move windows between monitors.
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Right, .act = ACTION_MOVE_MONITOR_INC },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Left,  .act = ACTION_MOVE_MONITOR_DEC },

	// Launcher
	{ .mod = Mod4Mask, .key = XK_x, .act = ACTION_COMMAND, .data = "dmenu_run" },

	// Find or start apps by WM_CLASS (lower case match).
	{ .mod = AnyModifier, .key = XK_F1, .act = ACTION_FIND_OR_START, .data = "urxvt"    },
	{ .mod = AnyModifier, .key = XK_F2, .act = ACTION_FIND_OR_START, .data = "kate"     },
	{ .mod = AnyModifier, .key = XK_F3, .act = ACTION_FIND_OR_START, .data = "chromium" },
	{ .mod = AnyModifier, .key = XK_F4, .act = ACTION_FIND_OR_START, .data = "pcmanfm"  },

	{ .mod = AnyModifier, .key = XK_F5, .act = ACTION_FIND_OR_START, .data = "firefox"  },
	{ .mod = AnyModifier, .key = XK_F6, .act = ACTION_FIND_OR_START, .data = "xchat"    },
};