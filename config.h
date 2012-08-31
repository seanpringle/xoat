
#define BORDER 2
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"

// new windows go to the same tile as the active window.
// this implies auto-raise and focus stealing.
//#define SPOT_START SPOT_CURRENT

// new windows go to the tile of best fit.
// works best when apps remember their size.
// if tile is not current, window won't steal focus.
#define SPOT_START SPOT_SMART

// all new windows go to a specific tile.
// if tile is not current, window won't steal focus.
//#define SPOT_START SPOT1

#define SPOT1_WIDTH_PCT 67
#define SPOT2_HEIGHT_PCT 67

binding keys[] = {

	// focus switching
	{ .mod = Mod4Mask, .key = XK_1, .act = ACTION_FOCUS_SPOT1 },
	{ .mod = Mod4Mask, .key = XK_2, .act = ACTION_FOCUS_SPOT2 },
	{ .mod = Mod4Mask, .key = XK_3, .act = ACTION_FOCUS_SPOT3 },

	// moving the current window
	{ .mod = ShiftMask|Mod4Mask, .key = XK_1, .act = ACTION_MOVE_SPOT1 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_2, .act = ACTION_MOVE_SPOT2 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_3, .act = ACTION_MOVE_SPOT3 },

	// flip between two windows in the current tile
	{ .mod = Mod4Mask, .key = XK_Tab,    .act = ACTION_OTHER },

	// cycle through all windows in the current tile
	{ .mod = Mod4Mask, .key = XK_grave,  .act = ACTION_CYCLE },

	// gracefully close the current window
	{ .mod = Mod4Mask, .key = XK_Escape, .act = ACTION_CLOSE },

	// find or start apps by WM_CLASS (case insensitive)
	{ .mod = AnyModifier, .key = XK_F1, .act = ACTION_FIND_OR_START, .data = "urxvt"    },
	{ .mod = AnyModifier, .key = XK_F2, .act = ACTION_FIND_OR_START, .data = "kate"     },
	{ .mod = AnyModifier, .key = XK_F3, .act = ACTION_FIND_OR_START, .data = "chromium" },
};
