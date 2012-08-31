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
Screen *screen;
int scr_id;
Window root;
Time latest;

typedef struct {
	short x, y, w, h;
} monitor;

#define MAX_MONITORS 3
monitor monitors[MAX_MONITORS];
int nmonitors;

#define MAX_NET_WM_STATES 5

typedef struct {
	Window window;
	XWindowAttributes attr;
	XWMHints hints;
	XSizeHints size;
	Window transient_for;
	Atom type, states[MAX_NET_WM_STATES];
	short monitor, spot, visible, trans, manage, input, urgent;
} client;

#define STACK 64

typedef struct {
	short depth;
	client *clients[STACK];
	Window windows[STACK];
} stack;

short current_spot = 0, current_mon = 0;
Window current = None;
stack inplay;

static int (*xerror)(Display *, XErrorEvent *);

enum { LEFT, RIGHT, TOP, BOTTOM };
int struts[4] = { 0, 0, 0, 0 };

#define ATOM_ENUM(x) x
#define ATOM_CHAR(x) #x

#define GENERAL_ATOMS(X) \
	X(_MOTIF_WM_HINTS),\
	X(WM_DELETE_WINDOW),\
	X(WM_TAKE_FOCUS),\
	X(_NET_ACTIVE_WINDOW),\
	X(_NET_CLOSE_WINDOW),\
	X(_NET_WM_STRUT_PARTIAL),\
	X(_NET_WM_STRUT),\
	X(_NET_WM_WINDOW_TYPE),\
	X(_NET_WM_WINDOW_TYPE_DESKTOP),\
	X(_NET_WM_WINDOW_TYPE_DOCK),\
	X(_NET_WM_WINDOW_TYPE_SPLASH),\
	X(_NET_WM_WINDOW_TYPE_NOTIFICATION),\
	X(_NET_WM_WINDOW_TYPE_DIALOG),\
	X(_NET_CLIENT_LIST_STACKING),\
	X(_NET_WM_STATE),\
	X(_NET_WM_STATE_FULLSCREEN),\
	X(_NET_WM_STATE_DEMANDS_ATTENTION),\
	X(WM_PROTOCOLS)

enum { GENERAL_ATOMS(ATOM_ENUM), ATOMS };
const char *atom_names[] = { GENERAL_ATOMS(ATOM_CHAR) };
Atom atoms[ATOMS];

unsigned int NumlockMask = 0;

enum {
	ACTION_NONE,
	ACTION_MOVE_SPOT1,
	ACTION_MOVE_SPOT2,
	ACTION_MOVE_SPOT3,
	ACTION_FOCUS_SPOT1,
	ACTION_FOCUS_SPOT2,
	ACTION_FOCUS_SPOT3,
	ACTION_CYCLE,
	ACTION_CLOSE,
	ACTION_OTHER,
	ACTION_COMMAND,
	ACTION_FIND_OR_START,
	ACTION_MOVE_MONITOR_INC,
	ACTION_MOVE_MONITOR_DEC,
	ACTION_FOCUS_MONITOR_INC,
	ACTION_FOCUS_MONITOR_DEC,
	ACTION_FULLSCREEN_TOGGLE,
	ACTIONS
};

typedef struct {
	unsigned int mod;
	KeySym key;
	short act;
	void *data;
} binding;

enum {
	MONITOR_CURRENT=-1
};

enum {
	SPOT_CURRENT,
	SPOT_SMART,
	SPOT1, // large left pane
	SPOT2, // medium top right pane
	SPOT3  // small bottom right pane
};
