// Cerberus config.

#define BORDER 2
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"

// There are three static tiles called SPOT1, SPOT2, and SPOT3.
// Want more? Go away ;)
// 	------------------------------
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

// Height of SPOT2 as percentage of screen width.
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

	// Find or start apps by WM_CLASS (case insensitive).
	{ .mod = AnyModifier, .key = XK_F1, .act = ACTION_FIND_OR_START, .data = "urxvt"    },
	{ .mod = AnyModifier, .key = XK_F2, .act = ACTION_FIND_OR_START, .data = "kate"     },
	{ .mod = AnyModifier, .key = XK_F3, .act = ACTION_FIND_OR_START, .data = "chromium" },
	{ .mod = AnyModifier, .key = XK_F4, .act = ACTION_FIND_OR_START, .data = "pcmanfm"  },

	{ .mod = AnyModifier, .key = XK_F5, .act = ACTION_FIND_OR_START, .data = "firefox"  },
	{ .mod = AnyModifier, .key = XK_F6, .act = ACTION_FIND_OR_START, .data = "xchat"    },
};
