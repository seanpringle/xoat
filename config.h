// xoat config.

#define BORDER 1
#define BORDER_BLUR "Dark Gray"
#define BORDER_FOCUS "Royal Blue"
#define BORDER_URGENT "Red"
#define GAP 1

// Title bar xft font.
// Setting this to NULL will disable title bars
//#define TITLE NULL
#define TITLE "sans:size=8"

// Title bar style
#define TITLE_BLUR "Black"
#define TITLE_FOCUS "White"
#define TITLE_ELLIPSIS 32

// Window lists
# define MENU "sans:size=8"

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

Layout layouts[] = {
	// Look at xrandr output to determine your monitor order.
	{ .spot_start = SMART, .spot1_align = LEFT,  .spot1_width_pct = 60, .spot2_height_pct = 60 }, // primary monitor
	{ .spot_start = SMART, .spot1_align = RIGHT, .spot1_width_pct = 60, .spot2_height_pct = 60 }, // secondary monitor, etc...
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
// action_maximize
// action_menu

// If using "AnyModifier" place those keys at the end of the array.
Binding keys[] = {

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

	// Gracefully close the current window.
	{ .mod = Mod4Mask, .key = XK_Escape, .act = action_close },

	// Popup list of windows in current spot.
	{ .mod = Mod4Mask, .key = XK_Menu, .act = action_menu },

	// Toggle current window full screen.
	{ .mod = Mod4Mask, .key = XK_f, .act = action_fullscreen },
	{ .mod = Mod4Mask, .key = XK_v, .act = action_maximize_vert },
	{ .mod = Mod4Mask, .key = XK_h, .act = action_maximize_horz },
	{ .mod = Mod4Mask, .key = XK_m, .act = action_maximize },

	// Launcher
	{ .mod = Mod4Mask, .key = XK_x,  .act = action_command, .data = "dmenu_run" },

	// Example
	// { .mod = AnyModifier, .key = XK_F1, .act = action_find_or_start, .data = "xterm" },
};