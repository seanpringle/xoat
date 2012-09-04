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
#include "xoat.h"
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
	warnx("exec_cmd %s", cmd);
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
	XColor color; Colormap map = DefaultColormap(display, DefaultScreen(display));
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

void window_set_window_prop(Window w, Atom prop, Window *values, int count)
{
	XChangeProperty(display, w, prop, XA_WINDOW, 32, PropModeReplace, (unsigned char*)values, count);
}

void ewmh_client_list()
{
	int i; client *c; stack all, wins;
	memset(&wins, 0, sizeof(stack));
	windows_visible(&all);

	for (i = all.depth-1; i > -1; i--)
	{
		if ((c = all.clients[i]) && c->manage)
		{
			wins.clients[wins.depth] = c;
			wins.windows[wins.depth++] = c->window;
		}
	}
	window_set_window_prop(root, atoms[_NET_CLIENT_LIST_STACKING], wins.windows, wins.depth);
	// hack for now, since we dont track window mapping history
	window_set_window_prop(root, atoms[_NET_CLIENT_LIST], wins.windows, wins.depth);
}

// build a client struct
client* window_client(Window win)
{
	int i, x, y, w, h;
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

		c->spot = SPOT1;

		spot_xywh(SPOT2, c->monitor, &x, &y, &w, &h);
		if (INTERSECT(x, y, w, h, c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1))
			c->spot = SPOT2;

		spot_xywh(SPOT3, c->monitor, &x, &y, &w, &h);
		if (INTERSECT(x, y, w, h, c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1))
			c->spot = SPOT3;

		if (c->visible)
		{
			window_get_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, MAX_NET_WM_STATES);
			c->urgent = client_state(c, atoms[_NET_WM_STATE_DEMANDS_ATTENTION]);

			unsigned long tags = 0;
			if (window_get_cardinal_prop(c->window, atoms[XOAT_TAGS], &tags, 1))
				c->tags = tags;

			XWMHints *hints = XGetWMHints(display, c->window);
			if (hints)
			{
				memmove(&c->hints, hints, sizeof(XWMHints));
				XFree(hints);
			}
			long sr; XGetWMNormalHints(display, c->window, &c->size, &sr);
			c->input  = c->hints.flags & InputHint && c->hints.input ? 1:0;
			c->urgent = c->urgent || c->hints.flags & XUrgencyHint ? 1: 0;

			XClassHint chint;
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

int client_state(client *c, Atom state)
{
	int i;
	for (i = 0; i < MAX_NET_WM_STATES && c->states[i]; i++)
		if (c->states[i] == state)
			return 1;
	return 0;
}

int client_add_state(client *c, Atom state)
{
	int i;
	for (i = 0; i < MAX_NET_WM_STATES; i++)
	{
		if (c->states[i]) continue;
		c->states[i] = state;
		window_set_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, i+1);
		return 1;
	}
	return 0;
}

int client_drop_state(client *c, Atom state)
{
	int i, j;
	for (i = 0, j = 0; i < MAX_NET_WM_STATES && c->states[i]; i++)
	{
		if (c->states[i] == state) continue;
		c->states[i] = c->states[j++];
	}
	window_set_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, j);
	int rc = i != j ? 1:0;

	for (; j < MAX_NET_WM_STATES; j++)
		c->states[j] = None;

	return rc;
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
	memset(s, 0, sizeof(stack));
	unsigned int nwins; int i; Window w1, w2, *wins; client *c;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = nwins-1; i > -1; i--)
		{
			if ((c = window_client(wins[i])) && c->visible && s->depth < STACK)
			{
				s->clients[s->depth] = c;
				s->windows[s->depth++] = wins[i];
			}
			else client_free(c);
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

void client_free(client *c)
{
	if (!c) return;
	free(c->class);
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

void client_close(client *c)
{
	if (!client_protocol(c, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, c->window);
}

void client_position(client *c, int x, int y, int w, int h)
{
	if (!c) return;
	monitor *m = &monitors[c->monitor];

	if (client_state(c, atoms[_NET_WM_STATE_FULLSCREEN]))
	{
		XMoveResizeWindow(display, c->window, m->x, m->y, m->w, m->h);
		return;
	}

	w -= BORDER*2; h -= BORDER*2;
	int basew = 0, baseh = 0, sw = w, sh = h;

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

	if (spot != SPOT1)
	{
		*x = m->x + width_spot1;
		*w = m->w - width_spot1;
		// right top 2/9 of screen
		if (spot == SPOT2)
		{
			*h = height_spot2;
		}
		// right bottom 1/9 of screen
		if (spot == SPOT3)
		{
			*y = m->y + height_spot2;
			*h = m->h - height_spot2;
		}
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
	if (!c) return;

	spot_active(c->spot, c->monitor, c->window);

	stack lower, all;
	memset(&lower, 0, sizeof(stack));
	windows_visible(&all);

	client_stack(c, &all, &lower);
	XLowerWindow(display, lower.windows[0]);
	XRestackWindows(display, lower.windows, lower.depth);
}

// find a window within a screen "spot" and raise/activate
Window spot_active(int spot, int mon, Window except)
{
	int i, x, y, w, h; client *o;
	spot_xywh(spot, mon, &x, &y, &w, &h);
	stack wins; windows_visible(&wins);

	for (i = 0; i < wins.depth; i++)
	{
		if ((o = wins.clients[i]) && o->window != except && o->manage && o->spot == spot
			&& INTERSECT(x + w/2, y + h/2, 1, 1, o->attr.x, o->attr.y, o->attr.width, o->attr.height))
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
	int i; client *o, *self = NULL;
	for (i = 0; i < all->depth; i++)
	{
		if ((o = all->clients[i]) && o->manage && o->visible && o->transient_for == c->window)
			client_stack(o, all, raise);
		else
		if (o && o->visible && o->window == c->window)
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
	if (!c) return;

	int i; client *o; stack raise, all, family;
	memset(&raise,  0, sizeof(stack));
	memset(&family, 0, sizeof(stack));
	windows_visible(&all);

	for (i = 0; i < all.depth; i++)
		if ((o = all.clients[i]) && o->type == atoms[_NET_WM_WINDOW_TYPE_DOCK])
			client_stack(o, &all, &raise);

	// above only counts for fullscreen windows
	if (client_state(c, atoms[_NET_WM_STATE_FULLSCREEN]))
		for (i = 0; i < all.depth; i++)
			if ((o = all.clients[i]) && client_state(o, atoms[_NET_WM_STATE_ABOVE]))
				client_stack(o, &all, &raise);

	while (c->trans)
	{
		client *t = window_client(c->transient_for);
		if (t) c = family.clients[family.depth++] = t;
	}

	client_stack(c, &all, &raise);
	XRaiseWindow(display, raise.windows[0]);
	XRestackWindows(display, raise.windows, raise.depth);

	while (family.depth)
		client_free(family.clients[--family.depth]);
}

// focus a window
void client_active(client *c)
{
	if (!c || !c->visible) return;

	client *o;
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
	window_set_window_prop(root, atoms[_NET_ACTIVE_WINDOW], &c->window, 1);
	client_review(c);
}

// client events we care about
void window_listen(Window win)
{
	XSelectInput(display, win, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
}

// set border color based on focus
void client_review(client *c)
{
	XSetWindowBorder(display, c->window,
		color_get(c->window == current ? BORDER_FOCUS: (c->urgent ? BORDER_URGENT:
			(client_state(c, atoms[_NET_WM_STATE_ABOVE]) ? BORDER_ABOVE: BORDER_BLUR))));
	XSetWindowBorderWidth(display, c->window, client_state(c, atoms[_NET_WM_STATE_FULLSCREEN]) ? 0: BORDER);
}

void client_set_tags(client *c)
{
	unsigned long tags = c->tags;
	window_set_cardinal_prop(c->window, atoms[XOAT_TAGS], &tags, 1);

	unsigned long desktop = tags & TAG3 ? 2: (tags & TAG2 ? 1: (tags & TAG1 ? 0: 0xffffffff));
	window_set_cardinal_prop(c->window, atoms[_NET_WM_DESKTOP], &desktop, 1);
}

// ------- key actions -------

void action_move(void *data, int num)
{
	client *c = window_client(current);
	if (c)
	{
		client_raise(c);
		client_spot(c, num, 1);
	}
}

void action_focus(void *data, int num)
{
	spot_active(num, current_mon, None);
}

void action_close(void *data, int num)
{
	client_close(window_client(current));
}

void action_cycle(void *data, int num)
{
	client_cycle(window_client(current));
}

void action_other(void *data, int num)
{
	client *c = window_client(current);
	if (c) spot_active(c->spot, c->monitor, c->window);
}

void action_command(void *data, int num)
{
	exec_cmd(data);
}

// real simple switcher/launcher
void action_find_or_start(void *data, int num)
{
	int i; client *c; char *class = data;
	stack all; windows_visible(&all);

	for (i = 0; i < all.depth; i++)
	{
		if ((c = all.clients[i]) && c->manage && !strcasecmp(c->class, class))
		{
			client_raise(c);
			client_active(c);
			return;
		}
	}
	exec_cmd(class);
}

void action_move_monitor(void *data, int num)
{
	int mon = MAX(0, MIN(current_mon+num, nmonitors-1));
	client *c = window_client(current);
	if (c)
	{
		client_raise(c);
		c->monitor = mon;
		client_spot(c, c->spot, 1);
	}
}

void action_focus_monitor(void *data, int num)
{
	int mon = MAX(0, MIN(current_mon+num, nmonitors-1));
	if (spot_active(current_spot, mon, None)) return;
	if (spot_active(SPOT1, mon, None)) return;
	if (spot_active(SPOT2, mon, None)) return;
	if (spot_active(SPOT3, mon, None)) return;
}

void action_raise_tag(void *data, int tag)
{
	int i; client *c = NULL, *t = NULL;
	stack all; windows_visible(&all);

	for (i = all.depth-1; i > -1; i--)
	{
		if ((c = all.clients[i]) && c->manage && c->visible && c->tags & tag)
		{
			if (c->monitor == current_mon && c->spot == current_spot) t = c;
			client_raise(c);
		}
	}
	if (t) client_active(t);

	unsigned long desktop = tag & TAG2 ? 1: (tag & TAG3 ? 2: 0);
	window_set_cardinal_prop(root, atoms[_NET_CURRENT_DESKTOP], &desktop, 1);
}

void action_fullscreen(void *data, int num)
{
	client *c = window_client(current);
	if (!c) return;

	unsigned long spot = c->spot;
	client_raise(c);
	if (client_state(c, atoms[_NET_WM_STATE_FULLSCREEN]))
	{
		client_drop_state(c, atoms[_NET_WM_STATE_FULLSCREEN]);
		if (window_get_cardinal_prop(c->window, atoms[XOAT_SPOT], &spot, 1))
			c->spot = spot;
	}
	else
	{
		client_add_state(c, atoms[_NET_WM_STATE_FULLSCREEN]);
		window_set_cardinal_prop(c->window, atoms[XOAT_SPOT], &spot, 1);
		c->spot = SPOT1;
	}
	client_review(c);
	client_spot(c, c->spot, 1);
}

void action_above(void *data, int num)
{
	client *c = window_client(current);
	if (!c) return;

	if (client_state(c, atoms[_NET_WM_STATE_ABOVE]))
		client_drop_state(c, atoms[_NET_WM_STATE_ABOVE]);
	else
		client_add_state(c, atoms[_NET_WM_STATE_ABOVE]);

	client_review(c);
	client_raise(c);
}

void action_tag(void *data, int num)
{
	client *c = window_client(current);
	if (!c) return;

	c->tags |= (unsigned int)num;
	client_set_tags(c);
	warnx("tags %d 0x%08lx %s", c->tags, (long)c->window, c->class);
}

void action_untag(void *data, int num)
{
	client *c = window_client(current);
	if (!c) return;

	c->tags &= ~((unsigned int)num);
	client_set_tags(c);
	warnx("tags %d 0x%08lx %s", c->tags, (long)c->window, c->class);
}

// key actions
action actions[ACTIONS] = {
	[ACTION_NONE]              = NULL,
	[ACTION_MOVE]              = action_move,
	[ACTION_FOCUS]             = action_focus,
	[ACTION_CYCLE]             = action_cycle,
	[ACTION_CLOSE]             = action_close,
	[ACTION_OTHER]             = action_other,
	[ACTION_COMMAND]           = action_command,
	[ACTION_FIND_OR_START]     = action_find_or_start,
	[ACTION_MOVE_MONITOR]      = action_move_monitor,
	[ACTION_FOCUS_MONITOR]     = action_focus_monitor,
	[ACTION_FULLSCREEN_TOGGLE] = action_fullscreen,
	[ACTION_ABOVE_TOGGLE]      = action_above,
	[ACTION_TAG]               = action_tag,
	[ACTION_UNTAG]             = action_untag,
	[ACTION_RAISE_TAG]         = action_raise_tag,
};

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
		int x, y, w, h, spot = SPOT_START,
			monitor = MAX(nmonitors-1, MIN(0, MONITOR_START));

		if (SPOT_START == SPOT_CURRENT)
			spot = current_spot;

		if (SPOT_START == SPOT_SMART)
		{
			spot = SPOT1;

			spot_xywh(SPOT2, c->monitor, &x, &y, &w, &h);
			if (c->attr.width <= w && c->attr.height <= h)
				spot = SPOT2;

			spot_xywh(SPOT3, c->monitor, &x, &y, &w, &h);
			if (c->attr.width <= w && c->attr.height <= h)
				spot = SPOT3;
		}

		if (MONITOR_START == MONITOR_CURRENT)
			monitor = current_mon;

		c->monitor = monitor;
		client_spot(c, spot, 0);
		client_review(c);
		client_set_tags(c);
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
		client_review(c);
		// if no current window, or new window has opened in the
		// current spot, activate it
		if (!(a = window_client(current)) || (a && a->spot == c->spot))
			client_active(c);
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
	int i; XEvent ev; latest = e->time;

	short act = ACTION_NONE; void *data = NULL; int num = 0;
	KeySym key = XkbKeycodeToKeysym(display, e->keycode, 0, 0);
	unsigned int state = e->state & ~(LockMask|NumlockMask);
	while (XCheckTypedEvent(display, KeyPress, &ev));

	for (i = 0; i < sizeof(keys)/sizeof(binding); i++)
	{
		if (keys[i].key == key && (keys[i].mod == AnyModifier || keys[i].mod == state))
		{
			act  = keys[i].act;
			data = keys[i].data;
			num  = keys[i].num;
			break;
		}
	}
	if (actions[act])
		actions[act](data, num);
}

void button_press(XButtonEvent *e)
{
	latest = e->time;
	client *c = window_client(e->subwindow);

	if (c && c->manage)
	{
		client_raise(c);
		client_active(c);
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
		warnx("client message 0x%lx 0x%08lx %s", e->message_type, (long)c->window, c->class);
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
	else
	if (e->message_type == atoms[XOAT_EXIT])
	{
		warnx("exit!");
		exit(EXIT_SUCCESS);
	}
	else
	if (e->message_type == atoms[XOAT_RESTART])
	{
		warnx("restart!");
		execsh(self);
	}
	client_free(c);
}

void any_event(XEvent *e)
{
	client *c = window_client(e->xany.window);

	if (c && c->manage)
		client_review(c);

	client_free(c);
}

int main(int argc, char *argv[])
{
	int i, j; self = argv[0]; stack wins;
	memset(&wins, 0, sizeof(stack));
	signal(SIGCHLD, catch_exit);

	if (!(display = XOpenDisplay(0))) return 1;

	screen = DefaultScreenOfDisplay(display);
	scr_id = DefaultScreen(display);
	root   = DefaultRootWindow(display);
	xerror = XSetErrorHandler(oops);

	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);

	// check for restart
	if (argc > 1 && !strcmp(argv[1], "restart"))
	{
		Window cli = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, None, None);
		window_message(root, cli, atoms[XOAT_RESTART], 0, SubstructureNotifyMask | SubstructureRedirectMask);
		exit(EXIT_SUCCESS);
	}
	// check for exit
	if (argc > 1 && !strcmp(argv[1], "exit"))
	{
		Window cli = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, None, None);
		window_message(root, cli, atoms[XOAT_EXIT], 0, SubstructureNotifyMask | SubstructureRedirectMask);
		exit(EXIT_SUCCESS);
	}

	// default non-multi-head setup
	memset(monitors, 0, sizeof(monitors));
	monitors[0].w = WidthOfScreen(screen);
	monitors[0].h = HeightOfScreen(screen);

	warnx("screen(%d): %dx%d+%d+%d", scr_id, monitors[0].w, monitors[0].h, monitors[0].x, monitors[0].y);

	// detect panel struts
	windows_visible(&wins);
	inplay.depth = 0;
	for (i = 0; i < wins.depth; i++)
	{
		client *c = wins.clients[i];
		if (!c) continue;

		wm_strut strut; memset(&strut, 0, sizeof(wm_strut));
		if (window_get_cardinal_prop(c->window, atoms[_NET_WM_STRUT_PARTIAL], (unsigned long*)&strut, 12)
			|| window_get_cardinal_prop(c->window, atoms[_NET_WM_STRUT], (unsigned long*)&strut, 4))
		{
			struts.left   = MAX(struts.left,   strut.left);
			struts.right  = MAX(struts.right,  strut.right);
			struts.top    = MAX(struts.top,    strut.top);
			struts.bottom = MAX(struts.bottom, strut.bottom);
			warnx("struts %ld %ld %ld %ld 0x%08lx %s", strut.left, strut.left, strut.left, strut.left, (long)c->window, c->class);
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
				monitors[i].y = info[i].y_org + struts.top;
				monitors[i].w = info[i].width;
				monitors[i].h = info[i].height - struts.top - struts.bottom;
				warnx("monitor %d %dx%d+%d+%d", i, monitors[i].w, monitors[i].h, monitors[i].x, monitors[i].y);
			}
			XFree(info);
		}
	}

	// left struts affect first monitor
	monitors[0].x += struts.left;
	monitors[0].w -= struts.left;

	// right struts affect last monitor
	monitors[nmonitors-1].w -= struts.right;

	// dump atoms for debug
	for (i = 0; i < ATOMS; i++) warnx("atom 0x%lx %s", (long)atoms[i], atom_names[i]);

	// become the window manager
	XSelectInput(display, root, StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask);

	// ewmh support
	unsigned long desktop = 0, desktops = 3, pid = getpid(), viewport[2] = { 0, 0 },
		geometry[2] = { WidthOfScreen(screen), HeightOfScreen(screen) };

	ewmh = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);

	window_set_atom_prop(root,     atoms[_NET_SUPPORTED],           atoms, ATOMS);
	window_set_window_prop(root,   atoms[_NET_SUPPORTING_WM_CHECK], &ewmh,     1);
	window_set_cardinal_prop(root, atoms[_NET_CURRENT_DESKTOP],     &desktop,  1);
	window_set_cardinal_prop(root, atoms[_NET_NUMBER_OF_DESKTOPS],  &desktops, 1);
	window_set_cardinal_prop(root, atoms[_NET_DESKTOP_GEOMETRY],    geometry,  2);
	window_set_cardinal_prop(root, atoms[_NET_DESKTOP_VIEWPORT],    viewport,  2);
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

	// setup existing managable windows
	windows_visible(&wins);
	inplay.depth = 0;
	for (i = 0; i < wins.depth; i++)
	{
		client *c = wins.clients[i];
		if (c && c->manage)
		{
			warnx("window 0x%08lx (%d,%d,%d) %s", (long)c->window, c->tags, c->monitor, c->spot, c->class);
			window_listen(c->window);
			// activate first one
			if (!current && c->visible)
			{
				client_raise(c);
				client_active(c);
			}
			if (c->visible)
			{
				client_review(c);
				client_set_tags(c);
			}
		}
		client_free(c);
	}
	// main event loop
	for (;;)
	{
		while (inplay.depth)
			client_free(inplay.clients[--inplay.depth]);

		XEvent ev; XNextEvent(display, &ev);

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
			case PropertyNotify:
				any_event(&ev);
				break;
		}
	}
	return 0;
}
