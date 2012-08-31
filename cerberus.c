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

#include "cerberus.h"
#include "proto.h"
#include "config.h"

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

int execsh(char *cmd)
{
	// use sh for args parsing
	return execlp("/bin/sh", "sh", "-c", cmd, NULL);
}

// execute sub-process
pid_t exec_cmd(char *cmd)
{
	if (!cmd || !cmd[0]) return -1;
	signal(SIGCHLD, catch_exit);
	pid_t pid = fork();
	if (!pid)
	{
		setsid();
		execsh(cmd);
		exit(EXIT_FAILURE);
	}
	return pid;
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

// get pixel value for X named color
unsigned int color_get(const char *name)
{
	XColor color;
	Colormap map = DefaultColormap(display, DefaultScreen(display));
	return XAllocNamedColor(display, map, name, &color, &color) ? color.pixel: None;
}

// retrieve a property of any type from a window
int window_get_prop(Window w, Atom prop, Atom *type, int *items, void *buffer, int bytes)
{
	Atom _type; if (!type) type = &_type;
	int _items; if (!items) items = &_items;
	int format; unsigned long nitems, nbytes; unsigned char *ret = NULL;
	memset(buffer, 0, bytes);

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

// update _NET_CLIENT_LIST_STACKING
void ewmh_client_list()
{
	int i; stack all, wins;
	memset(&all,  0, sizeof(stack));
	memset(&wins, 0, sizeof(stack));
	windows_visible(&all);

	for (i = all.depth-1; i > -1; i--)
	{
		client *c = all.clients[i];
		if (c && c->manage)
		{
			wins.clients[wins.depth] = c;
			wins.windows[wins.depth++] = c->window;
		}
	}
	XChangeProperty(display, root, atoms[_NET_CLIENT_LIST_STACKING], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wins.windows, wins.depth);
}

// update _NET_ACTIVE_WINDOW
void ewmh_active_window(Window w)
{
	XChangeProperty(display, root, atoms[_NET_ACTIVE_WINDOW], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&w, 1);
}

// build a client struct
client* window_client(Window win)
{
	int i;
	if (win == None) return NULL;

	client *c = calloc(1, sizeof(client));
	c->window = win;

	if (XGetWindowAttributes(display, c->window, &c->attr))
	{
		c->visible = c->attr.map_state == IsViewable ? 1:0;
		XGetTransientForHint(display, c->window, &c->transient_for);
		window_get_atom_prop(win, atoms[_NET_WM_WINDOW_TYPE], &c->type, 1);

		c->manage = !c->attr.override_redirect
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_DESKTOP]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_DOCK]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_SPLASH]
			? 1:0;

		c->trans = c->transient_for ? 1:0;

		for (i = 0; i < nmonitors; i++)
			if (INTERSECT(monitors[i].x, monitors[i].y, monitors[i].w, monitors[i].h,
				c->attr.x, c->attr.y, c->attr.width, c->attr.height))
					{ c->monitor = i; break; }

		monitor *m = &monitors[c->monitor];

		c->spot = c->attr.x > m->x + m->w/2
			? (c->attr.y > m->y + m->h/2 ? SPOT3: SPOT2)
				: SPOT1;

		if (c->visible)
		{
			XWMHints *hints = XGetWMHints(display, c->window);
			if (hints)
			{
				memmove(&c->hints, hints, sizeof(XWMHints));
				XFree(hints);
			}
			long sr;
			XGetWMNormalHints(display, c->window, &c->size, &sr);
			c->input   = c->hints.flags & InputHint && c->hints.input ? 1:0;
			c->urgent  = c->hints.flags & XUrgencyHint ? 1: 0;
		}
		return c;
	}

	free(c);
	return NULL;
}

// build a list of visible windows
void windows_visible(stack *s)
{
	// check if the window list is cached
	if (inplay.depth)
	{
		memmove(s, &inplay, sizeof(stack));
		return;
	}
	s->depth = 0;
	unsigned int nwins; int i; Window w1, w2, *wins;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = nwins-1; i > -1; i--)
		{
			client *c = window_client(wins[i]);
			if (c && c->visible && s->depth < STACK)
			{
				s->clients[s->depth] = c;
				s->windows[s->depth++] = wins[i];
			}
			else
				client_free(c);
		}
	}
	if (wins) XFree(wins);
	memmove(&inplay, s, sizeof(stack));
}

// send a ClientMessage (for WM_PROTOCOLS)
int window_message(Window target, Window subject, Atom atom, unsigned long protocol, unsigned long mask)
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

// release a client struct
void client_free(client *c)
{
	free(c);
}

// WM_PROTOCOLS
int client_protocol(client *c, Atom protocol)
{
	Atom *protocols = NULL;
	int i, found = 0, num_pro = 0;
	if (XGetWMProtocols(display, c->window, &protocols, &num_pro))
		for (i = 0; i < num_pro && !found; i++)
			if (protocols[i] == protocol) found = 1;
	if (found)
		window_message(c->window, c->window, atoms[WM_PROTOCOLS], protocol, NoEventMask);
	if (protocols) XFree(protocols);
	return found;
}

// friendly close followed by stab in the back
void client_close(client *c)
{
	if (!client_protocol(c, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, c->window);
}

// move/resize a window
void client_position(client *c, int x, int y, int w, int h)
{
	if (!c) return;

	w -= BORDER*2; h -= BORDER*2;

	int basew = 0, baseh = 0;
	int sw = w, sh = h;

	if (c->size.flags & PBaseSize)
	{
		basew = c->size.base_width;
		baseh = c->size.base_height;
	}
	if (c->size.flags & PMinSize)
	{
		w = MAX(w, c->size.min_width);
		h = MAX(h, c->size.min_height);
	}
	if (c->size.flags & PMaxSize)
	{
		w = MIN(w, c->size.max_width);
		h = MIN(h, c->size.max_height);
	}
	if (c->size.flags & PResizeInc)
	{
		w -= basew; h -= baseh;
		w -= w % c->size.width_inc;
		h -= h % c->size.height_inc;
		w += basew; h += baseh;
	}
	if (c->size.flags & PAspect)
	{
		double ratio = (double) w / h;
		double minr  = (double) c->size.min_aspect.x / c->size.min_aspect.y;
		double maxr  = (double) c->size.max_aspect.x / c->size.max_aspect.y;
			if (ratio < minr) h = (int)(w / minr);
		else if (ratio > maxr) w = (int)(h * maxr);
	}

	// center if smaller than supplied size
	if (w < sw) x += (sw-w)/2;
	if (h < sh) y += (sh-h)/2;

	monitor *m = &monitors[c->monitor];

	// bump onto screen
	x = MAX(0, MIN(x, m->x + m->w - w - BORDER*2));
	y = MAX(0, MIN(y, m->y + m->h - h - BORDER*2));

	XMoveResizeWindow(display, c->window, x, y, w, h);
}

// return co-ords for a screen "spot"
void spot_xywh(int spot, int mon, int *x, int *y, int *w, int *h)
{
	spot = MAX(SPOT1, MIN(SPOT3, spot));
	monitor *m = &monitors[MIN(nmonitors-1, MAX(0, mon))];

	int width_spot1  = (double)m->w / 100 * MIN(90, MAX(10, SPOT1_WIDTH_PCT));
	int height_spot2 = (double)m->h / 100 * MIN(90, MAX(10, SPOT2_HEIGHT_PCT));

	// default, left 2/3 of screen
	*x = m->x, *y = m->y, *w = width_spot1, *h = m->h;

	switch (spot)
	{
		// right top 2/9 of screen
		case SPOT2:
			*x = m->x + width_spot1;
			*y = m->y;
			*w = m->w - width_spot1;
			*h = height_spot2;
			break;

		// right bottom 1/9 of screen
		case SPOT3:
			*x = m->x + width_spot1;
			*y = m->y + height_spot2;
			*w = m->w - width_spot1;
			*h = m->h - height_spot2;
			break;
	}
}

// move a window to a screen "spot"
void client_spot(client *c, int spot, int force)
{
	if (!c) return;
	int x, y, w, h;
	spot = MAX(SPOT1, MIN(SPOT3, spot));

	if (c->trans && !force)
	{
		client *t = window_client(c->transient_for);
		spot = t->spot;
		client_free(t);
	}
	spot_xywh(spot, c->monitor, &x, &y, &w, &h);

	if (c->type == atoms[_NET_WM_WINDOW_TYPE_DIALOG])
	{
		x += (w - c->attr.width)/2;
		y += (h - c->attr.height)/2;
		w = c->attr.width + BORDER*2;
		h = c->attr.height + BORDER*2;
	}

	c->spot = spot;
	client_position(c, x, y, w, h);
}

// cycle through windows in a screen "spot"
void client_cycle(client *c)
{
	spot_active(c->spot, c->monitor, c->window);
	client_lower(c);
}

// find a window within a screen "spot" and raise/activate
Window spot_active(int spot, int mon, Window except)
{
	int x, y, w, h;
	spot_xywh(spot, mon, &x, &y, &w, &h);

	int i; stack wins;
	memset(&wins, 0, sizeof(stack));
	windows_visible(&wins);

	for (i = 0; i < wins.depth; i++)
	{
		client *o = wins.clients[i];
		if (o->window != except && o->manage
			&& INTERSECT(x + w/2, y + h/2, 1, 1,
				o->attr.x, o->attr.y, o->attr.width, o->attr.height))
		{
			client_raise(o);
			client_active(o);
			return o->window;
		}
	}
	return None;
}

// build a stack of a window's transients and itself
void client_stack(client *c, stack *all, stack *raise)
{
	int i; client *self = NULL;
	for (i = 0; i < all->depth; i++)
	{
		client *o = all->clients[i];
		if (o->manage && o->visible && o->transient_for == c->window)
			client_stack(o, all, raise);
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

// raise a window and all its transients
void client_raise(client *c)
{
	int i; stack raise, all, family;
	memset(&all,    0, sizeof(stack));
	memset(&raise,  0, sizeof(stack));
	memset(&family, 0, sizeof(stack));

	windows_visible(&all);

	for (i = 0; i < all.depth; i++)
	{
		client *o = all.clients[i];
		// docks stay on top
		if (o && o->type == atoms[_NET_WM_WINDOW_TYPE_DOCK])
		{
			raise.clients[raise.depth] = o;
			raise.windows[raise.depth++] = o->window;
		}
	}
	while (c->trans)
	{
		client *t = window_client(c->transient_for);
		if (t)
		{
			family.clients[family.depth++] = t;
			c = t;
		}
	}
	client_stack(c, &all, &raise);

	XRaiseWindow(display, raise.windows[0]);
	XRestackWindows(display, raise.windows, raise.depth);

	while (family.depth)
		client_free(family.clients[--family.depth]);
}

// lower a window and all its transients
void client_lower(client *c)
{
	stack lower, all;
	memset(&all,   0, sizeof(stack));
	memset(&lower, 0, sizeof(stack));

	windows_visible(&all);

	client_stack(c, &all, &lower);

	XLowerWindow(display, lower.windows[0]);
	XRestackWindows(display, lower.windows, lower.depth);
}

// focus a window
void client_active(client *c)
{
	client *o;
	if (!c || !c->visible) return;

	Window old = current;
	current = c->window;
	current_spot = c->spot;
	current_mon  = c->monitor;

	if (old && (o = window_client(old)))
	{
		client_review(o);
		client_free(o);
	}

	client_protocol(c, atoms[WM_TAKE_FOCUS]);
	XSetInputFocus(display, c->input ? c->window: PointerRoot, RevertToPointerRoot, CurrentTime);
	client_review(c);

	ewmh_active_window(c->window);
}

// real simple switcher/launcher
void find_or_start(char *class)
{
	int i; stack all;
	memset(&all, 0, sizeof(stack));
	windows_visible(&all);
	client *found = NULL;

	for (i = 0; !found && i < all.depth; i++)
	{
		XClassHint chint;
		client *c = all.clients[i];
		if (c && c->manage && XGetClassHint(display, c->window, &chint))
		{
			if (!strcasecmp(chint.res_class, class) || !strcasecmp(chint.res_name, class))
				found = c;

			XFree(chint.res_class);
			XFree(chint.res_name);
		}
	}
	if (found)
	{
		client_raise(found);
		client_active(found);
		return;
	}
	exec_cmd(class);
}

// client events we care about
void window_listen(Window win)
{
	XSelectInput(display, win, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
}

// set border color based on focus
void client_review(client *c)
{
	XSetWindowBorder(display, c->window, color_get(c->window == current ? BORDER_FOCUS: BORDER_BLUR));
	XSetWindowBorderWidth(display, c->window, BORDER);
}

// ------- event handlers --------

void create_notify(XCreateWindowEvent *e)
{
	client *c = window_client(e->window);

	if (c && c->manage)
		window_listen(c->window);

	client_free(c);
}

void configure_request(XConfigureRequestEvent *e)
{
	client *c = window_client(e->window);

	if (c)
	{
		if (c->manage && c->visible && !c->trans)
		{
			client_review(c);
			client_spot(c, c->spot, 0);
		}
		else
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
	}
	client_free(c);
}

void map_request(XMapEvent *e)
{
	client *c = window_client(e->window);

	if (c && c->manage)
	{
		client_review(c);

		int spot = SPOT_START;

		if (SPOT_START == SPOT_CURRENT)
			spot = current_spot;

		if (SPOT_START == SPOT_SMART)
		{
			int x, y, w, h;
			spot = SPOT1;

			spot_xywh(SPOT2, c->monitor, &x, &y, &w, &h);
			if (c->attr.width <= w && c->attr.height <= h)
				spot = SPOT2;

			spot_xywh(SPOT3, c->monitor, &x, &y, &w, &h);
			if (c->attr.width <= w && c->attr.height <= h)
				spot = SPOT3;
		}

		int monitor = MAX(nmonitors-1, MIN(0, MONITOR_START));

		if (MONITOR_START == MONITOR_CURRENT)
			monitor = current_mon;

		c->monitor = monitor;
		client_spot(c, spot, 0);
	}
	client_free(c);
	XMapWindow(display, e->window);
}

void map_notify(XMapEvent *e)
{
	client *a = NULL, *c = window_client(e->window);

	if (c && c->manage)
	{
		client_raise(c);

		if (current)
			a = window_client(current);

		// if no current window, or new window has opened in the
		// current spot, activate it
		if (!a || (a && a->spot == c->spot))
			client_active(c);
		else
			client_review(c);

		client_free(a);
	}
	client_free(c);
	ewmh_client_list();
}

void unmap_notify(XUnmapEvent *e)
{
	if (e->window == current && !spot_active(current_spot, current_mon, current))
	{
		// find something to activate
		if (!spot_active(SPOT1, current_mon, current)
			&& !spot_active(SPOT2, current_mon, current)
			&& !spot_active(SPOT3, current_mon, current))
				current = None;
	}

	ewmh_client_list();
}

void key_press(XKeyEvent *e)
{
	int i; XEvent ev;
	while (XCheckTypedEvent(display, KeyPress, &ev));

	latest = e->time;
	client *c = window_client(current);

	short act = ACTION_NONE; void *data = NULL;
	KeySym key = XkbKeycodeToKeysym(display, e->keycode, 0, 0);
	unsigned int state = e->state & ~(LockMask|NumlockMask);

	for (i = 0; i < sizeof(keys)/sizeof(binding); i++)
	{
		if (keys[i].key == key && (keys[i].mod == AnyModifier || keys[i].mod == state))
		{
			act  = keys[i].act;
			data = keys[i].data;
			break;
		}
	}
	if (c && c->visible)
	{
		switch (act)
		{
			case ACTION_MOVE_SPOT1:
				client_raise(c);
				client_spot(c, SPOT1, 1);
				break;
			case ACTION_MOVE_SPOT2:
				client_raise(c);
				client_spot(c, SPOT2, 1);
				break;
			case ACTION_MOVE_SPOT3:
				client_raise(c);
				client_spot(c, SPOT3, 1);
				break;
			case ACTION_CYCLE:
				client_cycle(c);
				break;
			case ACTION_OTHER:
				spot_active(c->spot, c->monitor, c->window);
				break;
			case ACTION_CLOSE:
				client_close(c);
				break;
			case ACTION_MOVE_MONITOR_INC:
				if (c->monitor < nmonitors-1)
				{
					c->monitor++;
					client_spot(c, c->spot, 1);
				}
				break;
			case ACTION_MOVE_MONITOR_DEC:
				if (c->monitor > 0)
				{
					c->monitor--;
					client_spot(c, c->spot, 1);
				}
				break;
		}
	}
	switch (act)
	{
		case ACTION_FOCUS_SPOT1:
			spot_active(SPOT1, current_mon, None);
			break;
		case ACTION_FOCUS_SPOT2:
			spot_active(SPOT2, current_mon, None);
			break;
		case ACTION_FOCUS_SPOT3:
			spot_active(SPOT3, current_mon, None);
			break;
		case ACTION_COMMAND:
			exec_cmd(data);
			break;
		case ACTION_FIND_OR_START:
			find_or_start(data);
			break;
		case ACTION_FOCUS_MONITOR_INC:
			if (current_mon < nmonitors-1)
			{
				if (spot_active(current_spot, current_mon+1, None)) break;
				if (spot_active(SPOT1, current_mon+1, None)) break;
				if (spot_active(SPOT2, current_mon+1, None)) break;
				if (spot_active(SPOT3, current_mon+1, None)) break;
			}
			break;
		case ACTION_FOCUS_MONITOR_DEC:
			if (current_mon > 0)
			{
				if (spot_active(current_spot, current_mon-1, None)) break;
				if (spot_active(SPOT1, current_mon-1, None)) break;
				if (spot_active(SPOT2, current_mon-1, None)) break;
				if (spot_active(SPOT3, current_mon-1, None)) break;
			}
			break;
	}
	client_free(c);
	ewmh_client_list();
}

void button_press(XButtonEvent *e)
{
	latest = e->time;
	client *c = window_client(e->subwindow);

	if (c)
	{
		if (c->manage)
		{
			client_raise(c);
			client_active(c);
		}
	}
	client_free(c);

	XAllowEvents(display, ReplayPointer, CurrentTime);
	ewmh_client_list();
}

void client_message(XClientMessageEvent *e)
{
	client *c = window_client(e->window);

	if (c && c->manage)
	{
		if (e->message_type == atoms[_NET_ACTIVE_WINDOW])
		{
			client_raise(c);
			client_active(c);
		}
		else
		if (e->message_type == atoms[_NET_CLOSE_WINDOW])
		{
			client_close(c);
		}
	}
	client_free(c);
}

void focus_change(XFocusChangeEvent *e)
{
	client *c = window_client(e->window);

	if (c && c->manage)
		client_review(c);

	client_free(c);
}

int main(int argc, char *argv[])
{
	int i, j;

	signal(SIGCHLD, catch_exit);

	if(!(display = XOpenDisplay(0))) return 1;

	screen   = DefaultScreenOfDisplay(display);
	scr_id   = DefaultScreen(display);
	root     = DefaultRootWindow(display);
	xerror   = XSetErrorHandler(oops);

	// default non-multi-head setup
	monitors[0].x = 0;
	monitors[0].y = 0;
	monitors[0].w = WidthOfScreen(screen);
	monitors[0].h = HeightOfScreen(screen);
	nmonitors = 1;

	XSelectInput(display, root, StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask);

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
		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod, root,
			True, GrabModeAsync, GrabModeAsync);

		if (keys[i].mod == AnyModifier) continue;

		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod|LockMask, root,
			True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod|NumlockMask, root,
			True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, XKeysymToKeycode(display, keys[i].key), keys[i].mod|LockMask|NumlockMask, root,
			True, GrabModeAsync, GrabModeAsync);
	}

	// we grab buttons to do click-to-focus. all clicks get passed through to apps.
	XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	XGrabButton(display, Button3, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);

	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);

	stack wins;
	memset(&wins, 0, sizeof(stack));

	// detect panel struts
	windows_visible(&wins);
	inplay.depth = 0;
	for (i = 0; i < wins.depth; i++)
	{
		client *c = wins.clients[i];
		if (c)
		{
			unsigned long strut[12]; memset(strut, 0, sizeof(strut));
			if (window_get_cardinal_prop(c->window, atoms[_NET_WM_STRUT_PARTIAL], strut, 12)
				|| window_get_cardinal_prop(c->window, atoms[_NET_WM_STRUT], strut, 4))
			{
				struts[LEFT]   = MAX(struts[LEFT],   strut[LEFT]);
				struts[RIGHT]  = MAX(struts[RIGHT],  strut[RIGHT]);
				struts[TOP]    = MAX(struts[TOP],    strut[TOP]);
				struts[BOTTOM] = MAX(struts[BOTTOM], strut[BOTTOM]);
			}
		}
		client_free(c);
	}
	// support multi-head.
	if (XineramaIsActive(display))
	{
		XineramaScreenInfo *info = XineramaQueryScreens(display, &nmonitors);
		if (info)
		{
			nmonitors = MIN(nmonitors, MAX_MONITORS);
			for (i = 0; i < nmonitors; i++)
			{
				monitors[i].x = info[i].x_org;
				monitors[i].y = info[i].y_org + struts[TOP];
				monitors[i].w = info[i].width;
				monitors[i].h = info[i].height - struts[TOP] - struts[BOTTOM];

				// left struts affect first monitor
				if (!i)
				{
					monitors[i].x += struts[LEFT];
					monitors[i].w -= struts[LEFT];
				}
				// right struts affect last monitor
				if (i == nmonitors-1)
				{
					monitors[i].w -= struts[RIGHT];
				}
			}
			XFree(info);
		}
	}
	// setup existing managable windows
	windows_visible(&wins);
	inplay.depth = 0;
	for (i = 0; i < wins.depth; i++)
	{
		client *c = wins.clients[i];
		if (c && c->manage)
		{
			window_listen(c->window);
			// activate first one
			if (!current && c->visible)
			{
				client_active(c);
				client_raise(c);
			}
			if (c->visible)
				client_review(c);
		}
		client_free(c);
	}
	// main event loop
	for (;;)
	{
		while (inplay.depth)
			client_free(inplay.clients[--inplay.depth]);

		XEvent ev;
		XNextEvent(display, &ev);

		switch (ev.type)
		{
			case CreateNotify:
				create_notify(&ev.xcreatewindow);
				break;
			case ConfigureRequest:
				configure_request(&ev.xconfigurerequest);
				break;
			case MapRequest:
				map_request(&ev.xmap);
				break;
			case MapNotify:
				map_notify(&ev.xmap);
				break;
			case UnmapNotify:
				unmap_notify(&ev.xunmap);
				break;
			case KeyPress:
				key_press(&ev.xkey);
				break;
			case ButtonPress:
				button_press(&ev.xbutton);
				break;
			case ClientMessage:
				client_message(&ev.xclient);
				break;
			case FocusIn:
			case FocusOut:
				focus_change(&ev.xfocus);
				break;
		}
	}
	return 0;
}
