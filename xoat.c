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

#define ATOM_ENUM(x) x
#define ATOM_CHAR(x) #x

#define GENERAL_ATOMS(X) \
	X(XOAT_SPOT),\
	X(XOAT_EXIT),\
	X(XOAT_RESTART),\
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
	X(WM_CLIENT_LEADER),\
	X(WM_TAKE_FOCUS),\
	X(WM_PROTOCOLS)

enum { GENERAL_ATOMS(ATOM_ENUM), ATOMS };
const char *atom_names[] = { GENERAL_ATOMS(ATOM_CHAR) };
Atom atoms[ATOMS];

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))
#define execsh(cmd) execlp("/bin/sh", "sh", "-c", (cmd), NULL)

#define MAX_STRUT 150
#define MAX_MONITORS 3
#define MAX_ATOMLIST 10
#define STACK 64

enum { MONITOR_CURRENT=-1 };
enum { SPOT1=1, SPOT2, SPOT3, SPOT_CURRENT, SPOT_SMART, SPOT1_LEFT, SPOT1_RIGHT };
enum { FOCUS_IGNORE=1, FOCUS_STEAL, };
enum { LEFT=1, RIGHT, UP, DOWN };

typedef struct {
	short x, y, w, h;
} box;

typedef struct {
	short x, y, w, h;
	box spots[SPOT3+1];
} monitor;

typedef struct {
	Window window;
	XWindowAttributes attr;
	Window transient, leader;
	Atom type, states[MAX_ATOMLIST+1];
	short monitor, spot, visible, manage, input, urgent;
	char *class;
} client;

typedef struct {
	long left, right, top, bottom,
		left_start_y, left_end_y, right_start_y, right_end_y,
		top_start_x, top_end_x, bottom_start_x, bottom_end_x;
} wm_strut;

typedef struct {
	short depth;
	client *clients[STACK];
	Window windows[STACK];
} stack;

typedef struct {
	unsigned int mod;
	KeySym key;
	void (*act)(void*, int, client*);
	void *data;
	int num;
} binding;

void action_move(void*, int, client*);
void action_focus(void*, int, client*);
void action_move_direction(void*, int, client*);
void action_focus_direction(void*, int, client*);
void action_close(void*, int, client*);
void action_cycle(void*, int, client*);
void action_other(void*, int, client*);
void action_command(void*, int, client*);
void action_find_or_start(void*, int, client*);
void action_move_monitor(void*, int, client*);
void action_focus_monitor(void*, int, client*);
void action_fullscreen(void*, int, client*);
void action_above(void*, int, client*);
void action_snapshot(void*, int, client*);
void action_rollback(void*, int, client*);

#include "config.h"

Display *display;
Time latest;
char *self;
unsigned int NumlockMask;
monitor monitors[MAX_MONITORS];
int nmonitors = 1;
short current_spot, current_mon;
Window root, ewmh, current = None;
stack windows, snapshot;
static int (*xerror)(Display *, XErrorEvent *);

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

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void exec_cmd(char *cmd)
{
	if (!cmd || !cmd[0]) return;
	signal(SIGCHLD, catch_exit);
	if (fork()) return;

	setsid();
	execsh(cmd);
	exit(EXIT_FAILURE);
}

// X error handler
int oops(Display *d, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
		|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
		|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
		|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
		|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
		) return 0;
	fprintf(stderr, "error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerror(display, ee);
}

unsigned int color_name_to_pixel(const char *name)
{
	XColor color; Colormap map = DefaultColormap(display, DefaultScreen(display));
	return XAllocNamedColor(display, map, name, &color, &color) ? color.pixel: None;
}

int window_get_prop(Window w, Atom prop, Atom *type, int *items, void *buffer, int bytes)
{
	memset(buffer, 0, bytes);
	int format; unsigned long nitems, nbytes; unsigned char *ret = NULL;
	if (XGetWindowProperty(display, w, prop, 0, bytes/4, False, AnyPropertyType, type,
		&format, &nitems, &nbytes, &ret) == Success && ret && *type != None && format)
	{
		if (format ==  8) memmove(buffer, ret, MIN(bytes, nitems));
		if (format == 16) memmove(buffer, ret, MIN(bytes, nitems * sizeof(short)));
		if (format == 32) memmove(buffer, ret, MIN(bytes, nitems * sizeof(long)));
		*items = (int)nitems; XFree(ret);
		return 1;
	}
	return 0;
}

int window_get_atom_prop(Window w, Atom atom, Atom *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(Atom)) && type == XA_ATOM ? items:0;
}

void window_set_atom_prop(Window w, Atom prop, Atom *atoms, int count)
{
	XChangeProperty(display, w, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, count);
}

int window_get_cardinal_prop(Window w, Atom atom, unsigned long *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(unsigned long)) && type == XA_CARDINAL ? items:0;
}

void window_set_cardinal_prop(Window w, Atom prop, unsigned long *values, int count)
{
	XChangeProperty(display, w, prop, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)values, count);
}

int window_get_window_prop(Window w, Atom atom, Window *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(Window)) && type == XA_WINDOW ? items:0;
}

void window_set_window_prop(Window w, Atom prop, Window *values, int count)
{
	XChangeProperty(display, w, prop, XA_WINDOW, 32, PropModeReplace, (unsigned char*)values, count);
}

int client_has_state(client *c, Atom state)
{
	int i;
	for (i = 0; i < MAX_ATOMLIST && c->states[i]; i++)
		if (c->states[i] == state) return 1;
	return 0;
}

void client_update_border(client *c)
{
	XSetWindowBorder(display, c->window,
		color_name_to_pixel(c->window == current ? BORDER_FOCUS: (c->urgent ? BORDER_URGENT:
			(client_has_state(c, atoms[_NET_WM_STATE_ABOVE]) ? BORDER_ABOVE: BORDER_BLUR))));
	XSetWindowBorderWidth(display, c->window, client_has_state(c, atoms[_NET_WM_STATE_FULLSCREEN]) ? 0: BORDER);
}

int client_toggle_state(client *c, Atom state)
{
	int i, j, rc = 0;
	for (i = 0, j = 0; i < MAX_ATOMLIST && c->states[i]; i++, j++)
	{
		if (c->states[i] == state) i++;
		c->states[j] = c->states[i];
	}
	if (i == j && j < MAX_ATOMLIST)
	{
		c->states[j++] = state;
		rc = 1;
	}
	window_set_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, j);
	return rc;
}

client* window_build_client(Window win)
{
	int i; XClassHint chint; XWMHints *hints;
	if (win == None) return NULL;

	client *c = calloc(1, sizeof(client));
	c->window = win;

	if (XGetWindowAttributes(display, c->window, &c->attr))
	{
		c->visible = c->attr.map_state == IsViewable ? 1:0;
		XGetTransientForHint(display, c->window, &c->transient);
		window_get_atom_prop(win, atoms[_NET_WM_WINDOW_TYPE], &c->type, 1);
		window_get_window_prop(win, atoms[WM_CLIENT_LEADER], &c->leader, 1);

		c->manage = !c->attr.override_redirect
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_DESKTOP]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_DOCK]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_SPLASH]
			? 1:0;

		for (i = 0; i < nmonitors; i++)
			if (INTERSECT(monitors[i].x, monitors[i].y, monitors[i].w, monitors[i].h,
				c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1))
					{ c->monitor = i; break; }

		monitor *m = &monitors[c->monitor];

		c->spot = SPOT1; for_spots_rev(i)
			if (INTERSECT(m->spots[i].x, m->spots[i].y, m->spots[i].w, m->spots[i].h,
				c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1))
					{ c->spot = i; break; }

		if (c->visible)
		{
			window_get_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, MAX_ATOMLIST);
			c->urgent = client_has_state(c, atoms[_NET_WM_STATE_DEMANDS_ATTENTION]);

			if ((hints = XGetWMHints(display, c->window)))
			{
				c->input  = hints->flags & InputHint && hints->input ? 1:0;
				c->urgent = c->urgent || hints->flags & XUrgencyHint ? 1:0;
				XFree(hints);
			}
			if (XGetClassHint(display, c->window, &chint))
			{
				c->class = strdup(chint.res_class);
				XFree(chint.res_class); XFree(chint.res_name);
			}
		}
		return c;
	}
	free(c);
	return NULL;
}

void client_free(client *c)
{
	if (!c) return;
	free(c->class);
	free(c);
}

void stack_free(stack *s)
{
	while (s->depth)
		client_free(s->clients[--s->depth]);
}

// build windows cache
void query_windows()
{
	unsigned int nwins; int i; Window w1, w2, *wins; client *c;
	if (windows.depth || !(XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins))
		return;
	for (i = nwins-1; i > -1 && windows.depth < STACK; i--)
	{
		if ((c = window_build_client(wins[i])) && c->visible)
		{
			windows.clients[windows.depth] = c;
			windows.windows[windows.depth++] = wins[i];
		}
		else client_free(c);
	}
	XFree(wins);
}

void ewmh_client_list()
{
	int i; client *c; stack wins;
	memset(&wins, 0, sizeof(stack));

	for_windows_rev(i, c) if (c->manage)
		wins.windows[wins.depth++] = c->window;

	window_set_window_prop(root, atoms[_NET_CLIENT_LIST_STACKING], wins.windows, wins.depth);
	// hack for now, since we dont track window mapping history
	window_set_window_prop(root, atoms[_NET_CLIENT_LIST], wins.windows, wins.depth);
}

int window_send_clientmessage(Window target, Window subject, Atom atom, unsigned long protocol, unsigned long mask)
{
	XEvent e; memset(&e, 0, sizeof(XEvent));
	e.xclient.type         = ClientMessage;
	e.xclient.message_type = atom;
	e.xclient.window       = subject;
	e.xclient.data.l[0]    = protocol;
	e.xclient.data.l[1]    = latest;
	e.xclient.send_event   = True;
	e.xclient.format       = 32;
	int r = XSendEvent(display, target, False, mask, &e) ?1:0;
	XFlush(display);
	return r;
}

int client_send_wm_protocol(client *c, Atom protocol)
{
	Atom protocols[MAX_ATOMLIST]; int i, n;
	if ((n = window_get_atom_prop(c->window, atoms[WM_PROTOCOLS], protocols, MAX_ATOMLIST)))
		for (i = 0; i < n; i++) if (protocols[i] == protocol)
			return window_send_clientmessage(c->window, c->window, atoms[WM_PROTOCOLS], protocol, NoEventMask);
	return 0;
}

void client_close(client *c)
{
	if (!client_send_wm_protocol(c, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, c->window);
}

void client_place_spot(client *c, int spot, int mon, int force)
{
	if (!c) return;
	int i; client *t;

	// try to center over our transient parent
	if (!force && c->transient && (t = window_build_client(c->transient)))
	{
		spot = t->spot;
		mon = t->monitor;
		client_free(t);
	}
	else
	// try to center over top-most window in our group
	if (!force && c->leader && c->type == atoms[_NET_WM_WINDOW_TYPE_DIALOG])
	{
		for_windows(i, t) if (t->manage && t->window != c->window && t->leader == c->leader)
		{
			spot = t->spot;
			mon = t->monitor;
			break;
		}
	}
	c->spot = spot; c->monitor = mon;

	monitor *m = &monitors[c->monitor];
	int x = m->spots[spot].x, y = m->spots[spot].y, w = m->spots[spot].w, h = m->spots[spot].h;

	if (c->type == atoms[_NET_WM_WINDOW_TYPE_DIALOG])
	{
		x += (w - c->attr.width)/2;
		y += (h - c->attr.height)/2;
		w = c->attr.width + BORDER*2;
		h = c->attr.height + BORDER*2;
	}
	else
	if (client_has_state(c, atoms[_NET_WM_STATE_FULLSCREEN]))
	{
		XMoveResizeWindow(display, c->window, m->x, m->y, m->w, m->h);
		return;
	}
	w -= BORDER*2; h -= BORDER*2;
	int sw = w, sh = h; long sr; XSizeHints size;

	if (XGetWMNormalHints(display, c->window, &size, &sr))
	{
		w = MIN(MAX(w, size.flags & PMinSize ? size.min_width : 16), size.flags & PMaxSize ? size.max_width : m->w);
		h = MIN(MAX(h, size.flags & PMinSize ? size.min_height: 16), size.flags & PMaxSize ? size.max_height: m->h);

		if (size.flags & PResizeInc)
		{
			w -= (w - (size.flags & PBaseSize ? size.base_width : 0)) % size.width_inc;
			h -= (h - (size.flags & PBaseSize ? size.base_height: 0)) % size.height_inc;
		}
		if (size.flags & PAspect)
		{
			double ratio = (double) w / h;
			double minr  = (double) size.min_aspect.x / size.min_aspect.y;
			double maxr  = (double) size.max_aspect.x / size.max_aspect.y;
				if (ratio < minr) h = (int)(w / minr);
			else if (ratio > maxr) w = (int)(h * maxr);
		}
	}
	// center if smaller than supplied size
	if (w < sw) x += (sw-w)/2;
	if (h < sh) y += (sh-h)/2;

	// bump onto screen
	x = MAX(m->x, MIN(x, m->x + m->w - w - BORDER*2));
	y = MAX(m->y, MIN(y, m->y + m->h - h - BORDER*2));

	XMoveResizeWindow(display, c->window, x, y, w, h);
}

void client_stack_family(client *c, stack *raise)
{
	int i; client *o, *self = NULL;
	for_windows(i, o)
	{
		if (o->manage && o->visible && o->transient == c->window)
			client_stack_family(o, raise);
		else
		if (o->visible && o->window == c->window)
			self = o;
	}
	if (self)
	{
		raise->clients[raise->depth] = self;
		raise->windows[raise->depth++] = self->window;
	}
}

void client_raise_family(client *c)
{
	if (!c) return;
	int i; client *o; stack raise, family;
	memset(&raise,  0, sizeof(stack));
	memset(&family, 0, sizeof(stack));

	for_windows(i, o) if (o->type == atoms[_NET_WM_WINDOW_TYPE_DOCK])
		client_stack_family(o, &raise);

	// above only counts for fullscreen windows
	if (client_has_state(c, atoms[_NET_WM_STATE_FULLSCREEN]))
		for_windows(i, o) if (client_has_state(o, atoms[_NET_WM_STATE_ABOVE]))
			client_stack_family(o, &raise);

	while (c->transient && (o = window_build_client(c->transient)))
		c = family.clients[family.depth++] = o;

	client_stack_family(c, &raise);
	XRaiseWindow(display, raise.windows[0]);
	XRestackWindows(display, raise.windows, raise.depth);
	stack_free(&family);
}

void client_set_focus(client *c)
{
	if (!c || !c->visible || c->window == current) return;
	Window old   = current;
	current      = c->window;
	current_spot = c->spot;
	current_mon  = c->monitor;

	client *o;
	if (old && (o = window_build_client(old)))
	{
		client_update_border(o);
		client_free(o);
	}
	client_send_wm_protocol(c, atoms[WM_TAKE_FOCUS]);
	XSetInputFocus(display, c->input ? c->window: PointerRoot, RevertToPointerRoot, CurrentTime);
	window_set_window_prop(root, atoms[_NET_ACTIVE_WINDOW], &c->window, 1);
	client_update_border(c);
}

void client_activate(client *c)
{
	client_raise_family(c);
	client_set_focus(c);
}

Window spot_focus_top_window(int spot, int mon, Window except)
{
	int i; client *c;
	for_windows(i, c)
	{
		if (c->window != except && c->manage && c->spot == spot && c->monitor == mon)
		{
			client_raise_family(c);
			client_set_focus(c);
			return c->window;
		}
	}
	return None;
}

int spot_choose_by_direction(int spot, int mon, int dir)
{
	monitor *m = &monitors[mon];
	int rotate = m->w < m->h ? 1:0;
	if (rotate)
	{
		if (dir == LEFT)  return SPOT3;
		if (dir == RIGHT) return SPOT2;
		if (SPOT1_ALIGN == SPOT1_LEFT)
		{
			if (dir == UP)   return SPOT1;
			if (dir == DOWN) return SPOT2;
		}
		if (dir == UP)   return SPOT2;
		if (dir == DOWN) return SPOT1;
		return spot;
	}
	if (dir == UP)    return SPOT2;
	if (dir == DOWN)  return SPOT3;
	if (SPOT1_ALIGN == SPOT1_LEFT)
	{
		if (dir == LEFT)  return SPOT1;
		if (dir == RIGHT) return SPOT2;
	}
	if (dir == LEFT)  return SPOT2;
	if (dir == RIGHT) return SPOT1;
	return spot;
}

void client_spot_cycle(client *c)
{
	if (!c) return;
	spot_focus_top_window(c->spot, c->monitor, c->window);
	stack lower; memset(&lower, 0, sizeof(stack));
	client_stack_family(c, &lower);
	XLowerWindow(display, lower.windows[0]);
	XRestackWindows(display, lower.windows, lower.depth);
}

void window_listen(Window win)
{
	XSelectInput(display, win, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
}

// ------- key actions -------

void action_move(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	client_place_spot(cli, num, cli->monitor, 1);
}

void action_move_direction(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	client_place_spot(cli, spot_choose_by_direction(cli->spot, cli->monitor, num), cli->monitor, 1);
}

void action_focus(void *data, int num, client *cli)
{
	spot_focus_top_window(num, current_mon, None);
}

void action_focus_direction(void *data, int num, client *cli)
{
	spot_focus_top_window(spot_choose_by_direction(current_spot, current_mon, num), current_mon, None);
}

void action_close(void *data, int num, client *cli)
{
	if (cli) client_close(cli);
}

void action_cycle(void *data, int num, client *cli)
{
	if (cli) client_spot_cycle(cli);
}

void action_other(void *data, int num, client *cli)
{
	if (cli) spot_focus_top_window(cli->spot, cli->monitor, cli->window);
}

void action_command(void *data, int num, client *cli)
{
	exec_cmd(data);
}

void action_find_or_start(void *data, int num, client *cli)
{
	int i; client *c; char *class = data;

	for_windows(i, c)
		if (c->visible && c->manage && c->class && !strcasecmp(c->class, class))
			{ client_activate(c); return; }

	exec_cmd(class);
}

void action_move_monitor(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	cli->monitor = MAX(0, MIN(current_mon+num, nmonitors-1));
	client_place_spot(cli, cli->spot, cli->monitor, 1);
	current_mon = cli->monitor;
}

void action_focus_monitor(void *data, int num, client *cli)
{
	int i, mon = MAX(0, MIN(current_mon+num, nmonitors-1));
	if (spot_focus_top_window(current_spot, mon, None)) return;
	for_spots(i) if (spot_focus_top_window(i, mon, None)) break;
}

void action_fullscreen(void *data, int num, client *cli)
{
	if (!cli) return;
	unsigned long spot = cli->spot;
	client_raise_family(cli);

	if (client_toggle_state(cli, atoms[_NET_WM_STATE_FULLSCREEN]))
	{
		window_set_cardinal_prop(cli->window, atoms[XOAT_SPOT], &spot, 1);
		cli->spot = SPOT1;
	}
	else
	if (window_get_cardinal_prop(cli->window, atoms[XOAT_SPOT], &spot, 1))
		cli->spot = spot;

	client_update_border(cli);
	client_place_spot(cli, cli->spot, cli->monitor, 1);
}

void action_above(void *data, int num, client *cli)
{
	if (!cli) return;
	client_toggle_state(cli, atoms[_NET_WM_STATE_ABOVE]);
	client_update_border(cli);
	client_raise_family(cli);
}

void action_snapshot(void *data, int num, client *cli)
{
	int i; client *c;
	stack_free(&snapshot);

	for_windows(i, c) if (c->manage)
	{
		snapshot.clients[snapshot.depth] = window_build_client(c->window);
		snapshot.windows[snapshot.depth++] = c->window;
	}
}

void action_rollback(void *data, int num, client *cli)
{
	int i; client *c = NULL, *s, *a = NULL;
	for (i = snapshot.depth-1; i > -1; i--)
	{
		if ((s = snapshot.clients[i]) && (c = window_build_client(s->window))
			&& c->class && !strcmp(s->class, c->class) && c->visible && c->manage)
		{
			client_place_spot(c, s->spot, s->monitor, 1);
			client_raise_family(c);
			if (s->spot == current_spot && s->monitor == current_mon)
			{
				client_free(a);
				a = c; c = NULL;
			}
		}
		client_free(c);
	}
	if (a)
	{
		client_set_focus(a);
		client_free(a);
	}
}

// ------- event handlers --------

void create_notify(XEvent *e)
{
	client *c = window_build_client(e->xcreatewindow.window);
	if (c && c->manage)
		window_listen(c->window);
	client_free(c);
}

void configure_request(XEvent *ev)
{
	XConfigureRequestEvent *e = &ev->xconfigurerequest;
	client *c = window_build_client(e->window);
	if (c && c->manage && c->visible && !c->transient)
	{
		client_update_border(c);
		client_place_spot(c, c->spot, c->monitor, 0);
	}
	else
	if (c)
	{
		XWindowChanges wc;
		if (e->value_mask & CWX) wc.x = e->x;
		if (e->value_mask & CWY) wc.y = e->y;
		if (e->value_mask & CWWidth)  wc.width  = e->width;
		if (e->value_mask & CWHeight) wc.height = e->height;
		if (e->value_mask & CWStackMode)   wc.stack_mode   = e->detail;
		if (e->value_mask & CWBorderWidth) wc.border_width = BORDER;
		XConfigureWindow(display, c->window, e->value_mask, &wc);
	}
	client_free(c);
}

void configure_notify(XEvent *e)
{
	client *c = window_build_client(e->xconfigure.window);
	if (c && c->manage)
		ewmh_client_list();
	client_free(c);
}

void map_request(XEvent *e)
{
	client *c = window_build_client(e->xmaprequest.window);
	if (c && c->manage)
	{
		c->monitor = MONITOR_START == MONITOR_CURRENT ? current_mon: MONITOR_START;
		c->monitor = MIN(nmonitors-1, MAX(0, c->monitor));
		monitor *m = &monitors[c->monitor];

		int i, spot = SPOT_START == SPOT_CURRENT ? current_spot: SPOT_START;

		if (SPOT_START == SPOT_SMART) // find spot of best fit
		{
			spot = SPOT1; for_spots_rev(i)
				if (c->attr.width <= m->spots[i].w && c->attr.height <= m->spots[i].h)
					{ spot = i; break; }
		}
		client_place_spot(c, spot, c->monitor, 0);
		client_update_border(c);
	}
	if (c) XMapWindow(display, c->window);
	client_free(c);
}

void map_notify(XEvent *e)
{
	client *a = NULL, *c = window_build_client(e->xmap.window);
	if (c && c->manage)
	{
		client_raise_family(c);
		client_update_border(c);
		// if no current window, or new window has opened in the current spot, focus it
		if (FOCUS_START == FOCUS_STEAL || !(a = window_build_client(current)) || (a && a->spot == c->spot))
			client_set_focus(c);
		client_free(a);
		ewmh_client_list();
	}
	client_free(c);
}

void unmap_notify(XEvent *e)
{
	int i;
	// if this window was focused, find something else
	if (e->xunmap.window == current && !spot_focus_top_window(current_spot, current_mon, current))
		for_spots(i) if (spot_focus_top_window(i, current_mon, current)) break;
	ewmh_client_list();
}

void key_press(XEvent *ev)
{
	XKeyEvent *e = &ev->xkey;
	latest = e->time;
	KeySym key = XkbKeycodeToKeysym(display, e->keycode, 0, 0);
	unsigned int state = e->state & ~(LockMask|NumlockMask);
	while (XCheckTypedEvent(display, KeyPress, ev));

	int i; binding *bind = NULL;
	for (i = 0; i < sizeof(keys)/sizeof(binding) && !bind; i++)
		if (keys[i].key == key && (keys[i].mod == AnyModifier || keys[i].mod == state))
			bind = &keys[i];

	if (bind && bind->act)
	{
		client *cli = window_build_client(current);
		bind->act(bind->data, bind->num, cli);
		client_free(cli);
	}
}

void button_press(XEvent *ev)
{
	XButtonEvent *e = &ev->xbutton;
	latest = e->time;
	client *c = window_build_client(e->subwindow);
	if (c && c->manage)
		client_activate(c);
	client_free(c);
	XAllowEvents(display, ReplayPointer, CurrentTime);
}

void client_message(XEvent *ev)
{
	XClientMessageEvent *e = &ev->xclient;
	if (e->message_type == atoms[XOAT_EXIT])
	{
		warnx("exit!");
		exit(EXIT_SUCCESS);
	}
	if (e->message_type == atoms[XOAT_RESTART])
	{
		warnx("restart!");
		execsh(self);
	}
	client *c = window_build_client(e->window);
	if (c && c->manage)
	{
		if (e->message_type == atoms[_NET_ACTIVE_WINDOW]) client_activate(c);
		if (e->message_type == atoms[_NET_CLOSE_WINDOW])  client_close(c);
	}
	client_free(c);
}

void any_event(XEvent *e)
{
	client *c = window_build_client(e->xany.window);
	if (c && c->visible && c->manage)
		client_update_border(c);
	client_free(c);
}

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
	[FocusIn]          = any_event,
	[FocusOut]         = any_event,
	[PropertyNotify]   = any_event,
};

int main(int argc, char *argv[])
{
	int i, j; client *c; XEvent ev;
	wm_strut struts; memset(&struts, 0, sizeof(wm_strut));

	if (!(display = XOpenDisplay(0))) return 1;

	self   = argv[0];
	root   = DefaultRootWindow(display);
	xerror = XSetErrorHandler(oops);

	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);

	// check for restart/exit
	if (argc > 1)
	{
		Atom msg = None;
		Window cli = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, None, None);
		     if (!strcmp(argv[1], "restart")) msg = atoms[XOAT_RESTART];
		else if (!strcmp(argv[1], "exit"))    msg = atoms[XOAT_EXIT];
		else errx(EXIT_FAILURE, "huh? %s", argv[1]);
		window_send_clientmessage(root, cli, msg, 0, SubstructureNotifyMask | SubstructureRedirectMask);
		exit(EXIT_SUCCESS);
	}

	// default non-multi-head setup
	monitors[0].w = WidthOfScreen(DefaultScreenOfDisplay(display));
	monitors[0].h = HeightOfScreen(DefaultScreenOfDisplay(display));

	// detect panel struts
	for_windows(i, c)
	{
		wm_strut strut; memset(&strut, 0, sizeof(wm_strut));
		if (window_get_cardinal_prop(c->window, atoms[_NET_WM_STRUT_PARTIAL], (unsigned long*)&strut, 12)
			|| window_get_cardinal_prop(c->window, atoms[_NET_WM_STRUT], (unsigned long*)&strut, 4))
		{
			struts.left   = MIN(MAX_STRUT, MAX(struts.left,   strut.left));
			struts.right  = MIN(MAX_STRUT, MAX(struts.right,  strut.right));
			struts.top    = MIN(MAX_STRUT, MAX(struts.top,    strut.top));
			struts.bottom = MIN(MAX_STRUT, MAX(struts.bottom, strut.bottom));
		}
	}

	// support multi-head.
	XineramaScreenInfo *info;
	if (XineramaIsActive(display) && (info = XineramaQueryScreens(display, &nmonitors)))
	{
		nmonitors = MIN(nmonitors, MAX_MONITORS);
		for (i = 0; i < nmonitors; i++)
		{
			monitors[i].x = info[i].x_org;
			monitors[i].y = info[i].y_org + struts.top;
			monitors[i].w = info[i].width;
			monitors[i].h = info[i].height - struts.top - struts.bottom;
		}
		XFree(info);
	}

	// left struts affect first monitor
	monitors[0].x += struts.left;
	monitors[0].w -= struts.left;

	// right struts affect last monitor
	monitors[nmonitors-1].w -= struts.right;

	// calculate spot boxes
	for (i = 0; i < nmonitors; i++)
	{
		monitor *m = &monitors[i];
		int x = m->x, y = m->y, w = m->w, h = m->h;
		// monitor rotated?
		if (m->w < m->h)
		{
			int height_spot1 = (double)h / 100 * MIN(90, MAX(10, SPOT1_WIDTH_PCT));
			int width_spot2  = (double)w / 100 * MIN(90, MAX(10, SPOT2_HEIGHT_PCT));
			for_spots(j)
			{
				m->spots[j].x = x;
				m->spots[j].y = SPOT1_ALIGN == SPOT1_LEFT ? y: y + h - height_spot1;
				m->spots[j].w = w;
				m->spots[j].h = height_spot1;
				if (j == SPOT1) continue;

				m->spots[j].y = SPOT1_ALIGN == SPOT1_LEFT ? y + height_spot1: y;
				m->spots[j].h = h - height_spot1;
				m->spots[j].w = w - width_spot2;
				if (j == SPOT3) continue;

				m->spots[j].x = x + w - width_spot2;
				m->spots[j].w = width_spot2;
			}
			continue;
		}
		int width_spot1  = (double)w / 100 * MIN(90, MAX(10, SPOT1_WIDTH_PCT));
		int height_spot2 = (double)h / 100 * MIN(90, MAX(10, SPOT2_HEIGHT_PCT));
		for_spots(j)
		{
			m->spots[j].x = SPOT1_ALIGN == SPOT1_LEFT ? x: x + w - width_spot1;
			m->spots[j].y = y;
			m->spots[j].w = width_spot1;
			m->spots[j].h = h;
			if (j == SPOT1) continue;

			m->spots[j].x = SPOT1_ALIGN == SPOT1_LEFT ? x + width_spot1: x;
			m->spots[j].w = w - width_spot1;
			m->spots[j].h = height_spot2;
			if (j == SPOT2) continue;

			m->spots[j].y = y + height_spot2;
			m->spots[j].h = h - height_spot2;
		}
	}

	// become the window manager
	XSelectInput(display, root, StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask);

	// ewmh support
	unsigned long pid = getpid();
	ewmh = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);

	window_set_atom_prop(root,     atoms[_NET_SUPPORTED],           atoms, ATOMS);
	window_set_window_prop(root,   atoms[_NET_SUPPORTING_WM_CHECK], &ewmh,     1);
	window_set_cardinal_prop(ewmh, atoms[_NET_WM_PID],              &pid,      1);

	XChangeProperty(display, ewmh, atoms[_NET_WM_NAME], XA_STRING, 8, PropModeReplace, (const unsigned char*)"xoat", 4);

	// figure out NumlockMask
	XModifierKeymap *modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < (int)modmap->max_keypermod; j++)
			if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(display, XK_Num_Lock))
				{ NumlockMask = (1<<i); break; }
	XFreeModifiermap(modmap);

	// process config.h key bindings
	for (i = 0; i < sizeof(keys)/sizeof(binding); i++)
	{
		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
		if (keys[i].mod == AnyModifier) continue;

		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod|LockMask, root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod|NumlockMask, root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod|LockMask|NumlockMask, root, True, GrabModeAsync, GrabModeAsync);
	}

	// we grab buttons to do click-to-focus. all clicks get passed through to apps.
	XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	XGrabButton(display, Button3, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);

	// setup existing managable windows
	stack_free(&windows);
	for_windows(i, c) if (c->manage)
	{
		window_listen(c->window);
		client_update_border(c);
		client_place_spot(c, c->spot, c->monitor, 0);

		// only activate first one
		if (!current) client_activate(c);
	}

	// main event loop
	for (;;)
	{
		stack_free(&windows);
		XNextEvent(display, &ev);
		if (handlers[ev.type])
			handlers[ev.type](&ev);
	}
	return EXIT_SUCCESS;
}