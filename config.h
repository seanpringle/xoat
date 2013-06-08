// xoat config.

#define BORDER 2
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"
#define BORDER_URGENT "Red"
#define GAP 2

// Title bar xft font.
// Setting this to NULL will disable title bars
//#define TITLE NULL
#define TITLE "sans:size=8"

// Title bar style
#define TITLE_BLUR "Black"
#define TITLE_FOCUS "White"
#define TITLE_ELLIPSIS 32

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

// .spot_start = SMART|CURRENT|SPOT1|SPOT2|SPOT3
//   SMART: Make new windows go to the spot of best fit.
//     Works best when apps remember or specify their size.
//   CURRENT: New windows open in the focused spot.

// .spot1_align = LEFT|RIGHT
//   The layout can be flipped so SPOT1 is on the right.
//   If you do this, review the directional move/focus key bindings too.

// .spot1_width_pct = N
//   Width of SPOT1 as percentage of screen width.

// .spot1_height_pct = N
//   Height of SPOT2 as percentage of screen height.

layout layouts[] = {
	// Look at xrandr output to determine your monitor order.
	{ .spot_start = SMART, .spot1_align = LEFT,  .spot1_width_pct = 66, .spot2_height_pct = 66 }, // primary monitor
	{ .spot_start = SMART, .spot1_align = RIGHT, .spot1_width_pct = 60, .spot2_height_pct = 66 }, // secondary monitor, etc...
};

// Available actions...
// action_move             .num = SPOT1/2/3
// action_focus            .num = SPOT1/2/3
// action_move_direction   .num = UP/DOWN/LEFT/RIGHT
// action_focus_direction  .num = UP/DOWN/LEFT/RIGHT
// action_close
// action_cycle
// action_command
// action_find_or_start
// action_move_monitor
// action_focus_monitor
// action_fullscreen
// action_maximize_vert
// action_maximize_horz

// If you use "AnyModifier" place those keys at the end of the array.
binding keys[] = {

	// Change focus to a spot by direction.
	{ .mod = Mod4Mask, .key = XK_Left,  .act = action_focus_direction, .num = LEFT  },
	{ .mod = Mod4Mask, .key = XK_Up,    .act = action_focus_direction, .num = UP    },
	{ .mod = Mod4Mask, .key = XK_Right, .act = action_focus_direction, .num = RIGHT },
	{ .mod = Mod4Mask, .key = XK_Down,  .act = action_focus_direction, .num = DOWN  },

	// Move the current window to another spot by direction.
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Left,  .act = action_move_direction, .num = LEFT  },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Up,    .act = action_move_direction, .num = UP    },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Right, .act = action_move_direction, .num = RIGHT },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Down,  .act = action_move_direction, .num = DOWN  },

	// Flip between the top two windows in the current spot.
	{ .mod = Mod4Mask, .key = XK_Tab, .act = action_raise_nth, .num = 1 },

	// Cycle through all windows in the current spot.
	{ .mod = Mod4Mask, .key = XK_grave,  .act = action_cycle },

	// Raise nth window in the current spot.
	{ .mod = Mod4Mask, .key = XK_1, .act = action_raise_nth, .num = 1 },
	{ .mod = Mod4Mask, .key = XK_2, .act = action_raise_nth, .num = 2 },
	{ .mod = Mod4Mask, .key = XK_3, .act = action_raise_nth, .num = 3 },
	{ .mod = Mod4Mask, .key = XK_4, .act = action_raise_nth, .num = 4 },
	{ .mod = Mod4Mask, .key = XK_5, .act = action_raise_nth, .num = 5 },
	{ .mod = Mod4Mask, .key = XK_6, .act = action_raise_nth, .num = 6 },
	{ .mod = Mod4Mask, .key = XK_7, .act = action_raise_nth, .num = 7 },
	{ .mod = Mod4Mask, .key = XK_8, .act = action_raise_nth, .num = 8 },
	{ .mod = Mod4Mask, .key = XK_9, .act = action_raise_nth, .num = 9 },

	// Gracefully close the current window.
	{ .mod = Mod4Mask, .key = XK_Escape, .act = action_close },

	// Toggle current window full screen.
	{ .mod = Mod4Mask, .key = XK_f, .act = action_fullscreen },
	{ .mod = Mod4Mask, .key = XK_v, .act = action_maximize_vert },
	{ .mod = Mod4Mask, .key = XK_h, .act = action_maximize_horz },
	{ .mod = Mod4Mask, .key = XK_m, .act = action_maximize },

	// Switch focus between monitors.
	{ .mod = Mod4Mask, .key = XK_Next,  .act = action_focus_monitor, .num = -1 },
	{ .mod = Mod4Mask, .key = XK_Prior, .act = action_focus_monitor, .num = +1 },

	// Move windows between monitors.
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Next,  .act = action_move_monitor, .num = -1 },
	{ .mod = ShiftMask|Mod4Mask, .key = XK_Prior, .act = action_move_monitor, .num = +1 },

	// Launcher
	{ .mod = Mod4Mask, .key = XK_x,  .act = action_command, .data = "dmenu_run" },
	{ .mod = Mod4Mask, .key = XK_F1, .act = action_command, .data = "konsole"   },
	{ .mod = Mod4Mask, .key = XK_F2, .act = action_command, .data = "chromium"  },
	{ .mod = Mod4Mask, .key = XK_F3, .act = action_command, .data = "pcmanfm"   },

	// Find or start apps by WM_CLASS (lower case match).
	// Only works for apps that use some form of their binary name as their class...
	{ .mod = AnyModifier, .key = XK_F1, .act = action_find_or_start, .data = "konsole"  },
	{ .mod = AnyModifier, .key = XK_F2, .act = action_find_or_start, .data = "chromium" },
	{ .mod = AnyModifier, .key = XK_F3, .act = action_find_or_start, .data = "pcmanfm"  },
	{ .mod = AnyModifier, .key = XK_F4, .act = action_find_or_start, .data = "sublime-text" },

	{ .mod = AnyModifier, .key = XK_Print, .act = action_command, .data = "xowl"  },
};
