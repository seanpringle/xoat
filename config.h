
#define BORDER 2
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"
#define SPOT_START SPOT1

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
