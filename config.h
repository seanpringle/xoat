
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
	{ .mod = Mod4Mask, .key = XK_1, .act = ACTION_FOCUS_SPOT1 },
	{ .mod = Mod4Mask, .key = XK_2, .act = ACTION_FOCUS_SPOT2 },
	{ .mod = Mod4Mask, .key = XK_3, .act = ACTION_FOCUS_SPOT3 },

	{ .mod = ShiftMask|Mod4Mask, .key = XK_1, .act = ACTION_MOVE_SPOT1 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_2, .act = ACTION_MOVE_SPOT2 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_3, .act = ACTION_MOVE_SPOT3 },

	{ .mod = Mod4Mask, .key = XK_Tab,    .act = ACTION_OTHER },
	{ .mod = Mod4Mask, .key = XK_grave,  .act = ACTION_CYCLE },
	{ .mod = Mod4Mask, .key = XK_Escape, .act = ACTION_CLOSE },
};
