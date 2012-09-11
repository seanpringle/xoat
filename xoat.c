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
	return execlp("/bin/sh", "sh", "-c", cmd, NULL);
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
	query_visible_windows(&all);

	for (i = all.depth-1; i > -1; i--)
	{
		if (!((c = all.clients[i]) && c->manage)) continue;
		wins.clients[wins.depth] = c;
		wins.windows[wins.depth++] = c->window;
	}
	window_set_window_prop(root, atoms[_NET_CLIENT_LIST_STACKING], wins.windows, wins.depth);
	// hack for now, since we dont track window mapping history
	window_set_window_prop(root, atoms[_NET_CLIENT_LIST], wins.windows, wins.depth);
}

client* window_build_client(Window win)
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

		for (i = 0; i < nmonitors; i++)
			if (INTERSECT(monitors[i].x, monitors[i].y, monitors[i].w, monitors[i].h,
				c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1))
					{ c->monitor = i; break; }

		monitor *m = &monitors[c->monitor];

		for (c->spot = SPOT3; c->spot > SPOT1; c->spot--)
			if (INTERSECT(m->spots[c->spot].x, m->spots[c->spot].y, m->spots[c->spot].w, m->spots[c->spot].h,
				c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1)) break;

		if (c->visible)
		{
			window_get_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, MAX_NET_WM_STATES);
			c->urgent = client_has_state(c, atoms[_NET_WM_STATE_DEMANDS_ATTENTION]);

			XWMHints *hints = XGetWMHints(display, c->window);
			if (hints)
			{
				c->input  = hints->flags & InputHint && hints->input ? 1:0;
				c->urgent = c->urgent || hints->flags & XUrgencyHint ? 1:0;
				XFree(hints);
			}
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

int client_has_state(client *c, Atom state)
{
	int i;
	for (i = 0; i < MAX_NET_WM_STATES && c->states[i]; i++)
		if (c->states[i] == state) return 1;
	return 0;
}

int client_toggle_state(client *c, Atom state)
{
	int i, j, rc = 0;
	for (i = 0, j = 0; i < MAX_NET_WM_STATES && c->states[i]; i++, j++)
	{
		if (c->states[i] == state) i++;
		c->states[j] = c->states[i];
	}
	if (i == j && j < MAX_NET_WM_STATES)
	{
		c->states[j++] = state;
		rc = 1;
	}
	window_set_atom_prop(c->window, atoms[_NET_WM_STATE], c->states, j);
	return rc;
}

void query_visible_windows(stack *s)
{
	if (inplay.depth) // cached?
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
			if ((c = window_build_client(wins[i])) && c->visible && s->depth < STACK)
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
	Atom *protocols = NULL;
	int i, found = 0, num_pro = 0;
	if (XGetWMProtocols(display, c->window, &protocols, &num_pro))
		for (i = 0; i < num_pro && !found; i++)
			if (protocols[i] == protocol) found = 1;
	if (found)
		window_send_clientmessage(c->window, c->window, atoms[WM_PROTOCOLS], protocol, NoEventMask);
	if (protocols) XFree(protocols);
	return found;
}

void client_close(client *c)
{
	if (!client_send_wm_protocol(c, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, c->window);
}

void client_place_spot(client *c, int spot, int force)
{
	if (!c) return;

	if (c->transient_for && !force)
	{
		client *t = window_build_client(c->transient_for);
		spot = t->spot;
		client_free(t);
	}
	c->spot = spot;
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
		int basew = size.flags & PBaseSize ? size.base_width : 0;
		int baseh = size.flags & PBaseSize ? size.base_height: 0;

		if (size.flags & PMinSize)
		{
			w = MAX(w, size.min_width);
			h = MAX(h, size.min_height);
		}
		if (size.flags & PMaxSize)
		{
			w = MIN(w, size.max_width);
			h = MIN(h, size.max_height);
		}
		if (size.flags & PResizeInc)
		{
			w -= basew; h -= baseh;
			w -= w % size.width_inc;
			h -= h % size.height_inc;
			w += basew; h += baseh;
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

void client_spot_cycle(client *c)
{
	if (!c) return;

	spot_focus_top_window(c->spot, c->monitor, c->window);

	stack lower; memset(&lower, 0, sizeof(stack));
	stack all; query_visible_windows(&all);

	client_stack_family(c, &all, &lower);
	XLowerWindow(display, lower.windows[0]);
	XRestackWindows(display, lower.windows, lower.depth);
}

Window spot_focus_top_window(int spot, int mon, Window except)
{
	int i; client *c;
	stack wins; query_visible_windows(&wins);
	for (i = 0; i < wins.depth; i++)
	{
		if ((c = wins.clients[i]) && c->window != except && c->manage && c->spot == spot && c->monitor == mon)
		{
			client_raise_family(c);
			client_set_focus(c);
			return c->window;
		}
	}
	return None;
}

void client_stack_family(client *c, stack *all, stack *raise)
{
	int i; client *o, *self = NULL;
	for (i = 0; i < all->depth; i++)
	{
		if ((o = all->clients[i]) && o->manage && o->visible && o->transient_for == c->window)
			client_stack_family(o, all, raise);
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

void client_raise_family(client *c)
{
	if (!c) return;

	int i; client *o; stack raise, all, family;
	memset(&raise,  0, sizeof(stack));
	memset(&family, 0, sizeof(stack));
	query_visible_windows(&all);

	for (i = 0; i < all.depth; i++)
		if ((o = all.clients[i]) && o->type == atoms[_NET_WM_WINDOW_TYPE_DOCK])
			client_stack_family(o, &all, &raise);

	// above only counts for fullscreen windows
	if (client_has_state(c, atoms[_NET_WM_STATE_FULLSCREEN]))
		for (i = 0; i < all.depth; i++)
			if ((o = all.clients[i]) && client_has_state(o, atoms[_NET_WM_STATE_ABOVE]))
				client_stack_family(o, &all, &raise);

	while (c->transient_for)
	{
		client *t = window_build_client(c->transient_for);
		if (t) c = family.clients[family.depth++] = t;
	}

	client_stack_family(c, &all, &raise);
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

void window_listen(Window win)
{
	XSelectInput(display, win, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
}

void client_update_border(client *c)
{
	XSetWindowBorder(display, c->window,
		color_name_to_pixel(c->window == current ? BORDER_FOCUS: (c->urgent ? BORDER_URGENT:
			(client_has_state(c, atoms[_NET_WM_STATE_ABOVE]) ? BORDER_ABOVE: BORDER_BLUR))));
	XSetWindowBorderWidth(display, c->window, client_has_state(c, atoms[_NET_WM_STATE_FULLSCREEN]) ? 0: BORDER);
}

// ------- key actions -------

void action_move(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	client_place_spot(cli, num, 1);
}

void action_focus(void *data, int num, client *cli)
{
	spot_focus_top_window(num, current_mon, None);
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
	stack all; query_visible_windows(&all);

	for (i = 0; i < all.depth; i++)
		if ((c = all.clients[i]) && c->manage && !strcasecmp(c->class, class))
			{ client_activate(c); return; }

	exec_cmd(class);
}

void action_move_monitor(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	cli->monitor = MAX(0, MIN(current_mon+num, nmonitors-1));
	client_place_spot(cli, cli->spot, 1);
}

void action_focus_monitor(void *data, int num, client *cli)
{
	int i, mon = MAX(0, MIN(current_mon+num, nmonitors-1));
	if (spot_focus_top_window(current_spot, mon, None)) return;
	for (i = SPOT1; i <= SPOT3 && !spot_focus_top_window(i, mon, None); i++);
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
	client_place_spot(cli, cli->spot, 1);
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
	int i; client *c; stack wins;
	stack_free(&snapshot);
	query_visible_windows(&wins);
	for (i = 0; i < wins.depth; i++)
	{
		if ((c = wins.clients[i]) && c->manage)
		{
			snapshot.clients[snapshot.depth] = window_build_client(c->window);
			snapshot.windows[snapshot.depth++] = c->window;
		}
	}
}

void action_rollback(void *data, int num, client *cli)
{
	int i; client *c = NULL, *s, *a = NULL;
	for (i = snapshot.depth-1; i > -1; i--)
	{
		if ((s = snapshot.clients[i]) && (c = window_build_client(s->window))
			&& !strcmp(s->class, c->class) && c->visible && c->manage)
		{
			c->monitor = s->monitor;
			client_place_spot(c, s->spot, 1);
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
	[ACTION_SNAPSHOT]          = action_snapshot,
	[ACTION_ROLLBACK]          = action_rollback,
};

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
	if (c && c->manage && c->visible && !c->transient_for)
	{
		client_update_border(c);
		client_place_spot(c, c->spot, 0);
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

		int spot = SPOT_START == SPOT_CURRENT ? current_spot: SPOT_START;

		if (SPOT_START == SPOT_SMART) // find spot of best fit
			for (spot = SPOT3; spot > SPOT1; spot--)
				if (c->attr.width <= m->spots[spot].w && c->attr.height <= m->spots[spot].h)
					break;

		client_place_spot(c, spot, 0);
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
		for (i = SPOT1; i <= SPOT3 && !spot_focus_top_window(i, current_mon, current); i++);
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
		actions[bind->act](bind->data, bind->num, cli);
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

message messages[ATOMS] = {
	[_NET_ACTIVE_WINDOW] = client_activate,
	[_NET_CLOSE_WINDOW]  = client_close,
};

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
	if (c && c->manage && messages[e->message_type])
		messages[e->message_type](c);
	client_free(c);
}

void any_event(XEvent *e)
{
	client *c = window_build_client(e->xany.window);
	if (c && c->visible && c->manage)
		client_update_border(c);
	client_free(c);
}

handler handlers[LASTEvent] = {
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
	int i, j; client *c; XEvent ev; stack wins;

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
	memset(monitors, 0, sizeof(monitors));
	monitors[0].w = WidthOfScreen(DefaultScreenOfDisplay(display));
	monitors[0].h = HeightOfScreen(DefaultScreenOfDisplay(display));

	// detect panel struts
	query_visible_windows(&wins);
	for (i = 0; i < wins.depth; i++)
	{
		if (!(c = wins.clients[i])) continue;

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
	stack_free(&inplay);

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
		int width_spot1  = (double)m->w / 100 * MIN(90, MAX(10, SPOT1_WIDTH_PCT));
		int height_spot2 = (double)m->h / 100 * MIN(90, MAX(10, SPOT2_HEIGHT_PCT));
		for (j = SPOT1; j <= SPOT3; j++)
		{
			m->spots[j].x = SPOT1_ALIGN == SPOT1_LEFT ? m->x: m->x + m->w - width_spot1;
			m->spots[j].y = m->y;
			m->spots[j].w = width_spot1;
			m->spots[j].h = m->h;
			if (j == SPOT1) continue;

			m->spots[j].x = SPOT1_ALIGN == SPOT1_LEFT ? m->x + width_spot1: m->x;
			m->spots[j].w = m->w - width_spot1;
			m->spots[j].h = height_spot2;
			if (j == SPOT2) continue;

			m->spots[j].y = m->y + height_spot2;
			m->spots[j].h = m->h - height_spot2;
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
	memset(&snapshot, 0, sizeof(stack));
	query_visible_windows(&wins);
	for (i = 0; i < wins.depth; i++)
	{
		if (!(c = wins.clients[i]) || !c->manage) continue;

		window_listen(c->window);
		client_update_border(c);
		client_place_spot(c, c->spot, 0);

		// only activate first one
		if (!current) client_activate(c);
	}

	// main event loop
	for (;;)
	{
		stack_free(&inplay);
		XNextEvent(display, &ev);
		if (handlers[ev.type])
			handlers[ev.type](&ev);
	}
	return EXIT_SUCCESS;
}
