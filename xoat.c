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

#define _GNU_SOURCE

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xft/Xft.h>
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
#include <regex.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

Display *display;

#include "regex.c"
#include "atom.c"
#include "textbox.c"
#include "menubox.c"

#define STACK 64
#define MONITORS 3
#define ATOMLIST 10
enum { SPOT1=1, SPOT2, SPOT3, CURRENT, SMART };
enum { LEFT=1, RIGHT, UP, DOWN };

typedef struct _Box {
	short x, y, w, h;
} Box;

typedef struct _Monitor {
	short x, y, w, h;
	Box spots[SPOT3+1];
	Textbox *bars[SPOT3+1];
} Monitor;

typedef struct _MenuMap {
	Window window;
	char *name;
} MenuMap;

typedef struct _Menu {
	short spot, monitor;
	MenuMap items[100];
	Menubox *mb;
} Menu;

typedef struct _Client {
	Window window;
	XWindowAttributes attr;
	Window transient, leader;
	Atom type, states[ATOMLIST+1];
	short monitor, visible, manage, input, urgent, full, ours, maxv, maxh, bar, menu, above;
	unsigned long spot, max;
	char *class;
} Client;

typedef struct _Stack {
	short depth;
	Client *clients[STACK];
	Window windows[STACK];
} Stack;

typedef struct _Binding {
	unsigned int mod;
	KeySym key;
	void (*act)(void*, int, Client*);
	void *data;
	int num;
} Binding;

typedef struct _Layout {
	short spot_start, spot1_align, spot1_width_pct, spot2_height_pct;
} Layout;

typedef struct _Settings {
	unsigned int border;
	char *border_blur;
	char *border_focus;
	char *border_urgent;
	unsigned int gap;
	char *title;
	char *title_blur;
	char *title_focus;
	unsigned int title_ellipsis;
	char *menu;

	unsigned int layout_count;
	Layout *layouts;

	unsigned int binding_count;
	Binding *bindings;

	unsigned int launchcmd_count;
	char **launchcmds;
} Settings;

#define have_layout(i) (settings.layout_count > (i))

Client* window_build_client(Window);
void client_free(Client*);
void action_move(void*, int, Client*);
void action_focus(void*, int, Client*);
void action_move_direction(void*, int, Client*);
void action_focus_direction(void*, int, Client*);
void action_close(void*, int, Client*);
void action_cycle(void*, int, Client*);
void action_raise_nth(void*, int, Client*);
void action_command(void*, int, Client*);
void action_find_or_start(void*, int, Client*);
void action_move_monitor(void*, int, Client*);
void action_focus_monitor(void*, int, Client*);
void action_fullscreen(void*, int, Client*);
void action_maximize_vert(void*, int, Client*);
void action_maximize_horz(void*, int, Client*);
void action_maximize(void*, int, Client*);
void action_above(void*, int, Client*);
void action_menu(void*, int, Client*);

#include "config.h"

#define EXECSH(cmd) execlp("/bin/sh", "sh", "-c", (cmd), NULL)
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))

#define STACK_INIT(n) Stack (n); memset(&(n), 0, sizeof(Stack))
#define STACK_FREE(s) while ((s)->depth) client_free((s)->clients[--(s)->depth])

#define for_stack(s,i,c)\
	for ((i) = 0; (i) < (s)->depth; (i)++)\
		if (((c) = (s)->clients[(i)]))

#define for_stack_rev(s,i,c)\
	for ((i) = (s)->depth-1; (i) > -1; (i)--)\
		if (((c) = (s)->clients[(i)]))

#define for_windows(i,c)\
	for (query_windows(), (i) = 0; (i) < windows.depth; (i)++)\
		if (((c) = windows.clients[(i)]))

#define for_windows_rev(i,c)\
	for (query_windows(), (i) = windows.depth-1; (i) > -1; (i)--)\
		if (((c) = windows.clients[(i)]))

#define for_spots(i)\
	for ((i) = SPOT1; (i) <= SPOT3; (i)++)

#define for_spots_rev(i)\
	for ((i) = SPOT3; (i) >= SPOT1; (i)--)

#define for_monitors(i, m)\
	for ((i) = 0; (i) < nmonitors && (m = &monitors[(i)]); (i)++)

Time latest_action;
char *self;
unsigned int NumlockMask;
Monitor monitors[MONITORS];
int nmonitors = 1;
short current_spot, current_mon;
Window root, ewmh, current = None;
Stack windows;
static int (*xerror)(Display *, XErrorEvent *);
Settings settings;
Menu *menu, spot_menu;

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void exec_cmd(char *cmd)
{
	signal(SIGCHLD, catch_exit);
	if (!cmd || !cmd[0] || fork()) return;
	setsid();
	EXECSH(cmd);
	exit(EXIT_FAILURE);
}

#include "window.c"
#include "ewmh.c"
#include "client.c"
#include "menu.c"
#include "spot.c"
#include "event.c"
#include "action.c"
#include "config.c"
#include "setup.c"

void (*handlers[LASTEvent])(XEvent*) = {
	[CreateNotify]     = create_notify,
	[ConfigureRequest] = configure_request,
	[ConfigureNotify]  = configure_notify,
	[MapRequest]       = map_request,
	[MapNotify]        = map_notify,
	[UnmapNotify]      = unmap_notify,
	[KeyPress]         = key_press,
	[ButtonPress]      = button_press,
	[ClientMessage]    = client_message,
	[PropertyNotify]   = property_notify,
	[Expose]           = expose,
	[FocusIn]          = any_event,
	[FocusOut]         = any_event,
};

int oops(Display *d, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
		|| (ee->request_code == X_SetInputFocus   && ee->error_code == BadMatch)
		|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
		|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadValue)
		|| (ee->request_code == X_GrabButton      && ee->error_code == BadAccess)
		|| (ee->request_code == X_GrabKey         && ee->error_code == BadAccess)
		) return 0;
	fprintf(stderr, "error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerror(display, ee);
}

int main(int argc, char *argv[])
{
	int i; XEvent ev; Atom msg = None;

	if (!(display = XOpenDisplay(0))) return 1;

	self   = argv[0];
	root   = DefaultRootWindow(display);
	xerror = XSetErrorHandler(oops);
	menu   = &spot_menu;

	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);

	// check for restart/exit
	if (argc > 1)
	{
		Window cli = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, None, None);
		     if (!strcmp(argv[1], "restart")) msg = atoms[XOAT_RESTART];
		else if (!strcmp(argv[1], "exit"))    msg = atoms[XOAT_EXIT];
		else errx(EXIT_FAILURE, "huh? %s", argv[1]);
		window_send_clientmessage(root, cli, msg, 0, SubstructureNotifyMask | SubstructureRedirectMask);
		exit(EXIT_SUCCESS);
	}

	configure();
	setup();

	// main event loop
	for (;;)
	{
		STACK_FREE(&windows);
		XNextEvent(display, &ev);
		if (handlers[ev.type])
			handlers[ev.type](&ev);
	}
	return EXIT_SUCCESS;
}
