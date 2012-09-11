/*

MIT/X11 License
Copyright (c) 2012 Sean Pringle <sean.pringle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xinerama.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define NEAR(a,o,b) ((b) > (a)-(o) && (b) < (a)+(o))
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))

Display *display;
Window root;
Time latest;
char *self;
Window ewmh;

enum {
	MONITOR_CURRENT=-1
};

enum {
	SPOT1=1, // large left pane
	SPOT2, // medium top right pane
	SPOT3,  // small bottom right pane
	SPOT_CURRENT,
	SPOT_SMART,
	SPOT1_LEFT,
	SPOT1_RIGHT
};

enum {
	FOCUS_IGNORE=1,
	FOCUS_STEAL,
};

typedef struct {
	short x, y, w, h;
} box;

typedef struct {
	short x, y, w, h;
	box spots[SPOT3+1];
} monitor;

#define MAX_STRUT 150
#define MAX_MONITORS 3
monitor monitors[MAX_MONITORS];
int nmonitors = 1;

#define MAX_NET_WM_STATES 5

typedef struct {
	Window window;
	XWindowAttributes attr;
	Window transient_for;
	Atom type, states[MAX_NET_WM_STATES+1];
	short monitor, spot, visible, manage, input, urgent;
	char *class;
} client;

#define STACK 64

typedef struct {
	short depth;
	client *clients[STACK];
	Window windows[STACK];
} stack;

short current_spot = 0, current_mon = 0;
Window current = None;
stack inplay, snapshot;

static int (*xerror)(Display *, XErrorEvent *);

typedef struct {
	long left, right, top, bottom,
		left_start_y, left_end_y, right_start_y, right_end_y,
		top_start_x, top_end_x, bottom_start_x, bottom_end_x;
} wm_strut;

wm_strut struts;

#define ATOM_ENUM(x) x
#define ATOM_CHAR(x) #x

#define GENERAL_ATOMS(X) \
	X(XOAT_SPOT),\
	X(XOAT_EXIT),\
	X(XOAT_RESTART),\
	X(_MOTIF_WM_HINTS),\
	X(_NET_SUPPORTED),\
	X(_NET_ACTIVE_WINDOW),\
	X(_NET_CLOSE_WINDOW),\
	X(_NET_CLIENT_LIST_STACKING),\
	X(_NET_CLIENT_LIST),\
	X(_NET_SUPPORTING_WM_CHECK),\
	X(_NET_WM_NAME),\
	X(_NET_WM_PID),\
	X(_NET_WM_STRUT),\
	X(_NET_WM_STRUT_PARTIAL),\
	X(_NET_WM_WINDOW_TYPE),\
	X(_NET_WM_WINDOW_TYPE_DESKTOP),\
	X(_NET_WM_WINDOW_TYPE_DOCK),\
	X(_NET_WM_WINDOW_TYPE_SPLASH),\
	X(_NET_WM_WINDOW_TYPE_NOTIFICATION),\
	X(_NET_WM_WINDOW_TYPE_DIALOG),\
	X(_NET_WM_STATE),\
	X(_NET_WM_STATE_FULLSCREEN),\
	X(_NET_WM_STATE_ABOVE),\
	X(_NET_WM_STATE_DEMANDS_ATTENTION),\
	X(WM_DELETE_WINDOW),\
	X(WM_TAKE_FOCUS),\
	X(WM_PROTOCOLS)

enum { GENERAL_ATOMS(ATOM_ENUM), ATOMS };
const char *atom_names[] = { GENERAL_ATOMS(ATOM_CHAR) };
Atom atoms[ATOMS];

unsigned int NumlockMask = 0;

enum {
	ACTION_NONE,
	ACTION_MOVE,
	ACTION_FOCUS,
	ACTION_CYCLE,
	ACTION_CLOSE,
	ACTION_OTHER,
	ACTION_COMMAND,
	ACTION_FIND_OR_START,
	ACTION_MOVE_MONITOR,
	ACTION_FOCUS_MONITOR,
	ACTION_FULLSCREEN_TOGGLE,
	ACTION_ABOVE_TOGGLE,
	ACTION_SNAPSHOT,
	ACTION_ROLLBACK,
	ACTIONS
};

typedef void (*handler)(XEvent*);
typedef void (*action)(void*, int, client*);
typedef void (*message)(client*);

typedef struct {
	unsigned int mod;
	KeySym key;
	short act;
	void *data;
	int num;
} binding;
