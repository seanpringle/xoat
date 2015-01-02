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

int client_has_state(Client *c, Atom state)
{
	for (int i = 0; i < ATOMLIST && c->states[i]; i++)
		if (c->states[i] == state) return 1;
	return 0;
}

int client_toggle_state(Client *c, Atom state)
{
	int i, j, rc = 0;
	for (i = 0, j = 0; i < ATOMLIST && c->states[i]; i++, j++)
	{
		if (c->states[i] == state) i++;
		c->states[j] = c->states[i];
	}
	if (i == j && j < ATOMLIST)
	{
		c->states[j++] = state;
		rc = 1;
	}
	SETPROP_ATOM(c->window, atoms[_NET_WM_STATE], c->states, j);
	if (state == atoms[_NET_WM_STATE_FULLSCREEN])        c->full   = rc;
	if (state == atoms[_NET_WM_STATE_DEMANDS_ATTENTION]) c->urgent = rc;
	return rc;
}

Client* window_build_client(Window win)
{
	int i, j; XClassHint chint; XWMHints *hints; Monitor *m;
	if (win == None) return NULL;

	Client *c = calloc(1, sizeof(Client));
	c->window = win;

	if (XGetWindowAttributes(display, c->window, &c->attr))
	{
		c->visible = c->attr.map_state == IsViewable ? 1:0;
		if (!GETPROP_ATOM(win, atoms[_NET_WM_WINDOW_TYPE], &c->type, 1)) c->type = 0;

		c->manage = !c->attr.override_redirect
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_DESKTOP]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_DOCK]
			&& c->type != atoms[_NET_WM_WINDOW_TYPE_SPLASH]
			? 1:0;

		// detect our own title bars
		if (settings.title)
			for_monitors(i, m) for_spots(j)
				if (m->bars[j] && m->bars[j]->window == c->window)
					{ c->ours = 1; c->manage = 0; break; }

		if (c->manage)
		{
			XGetTransientForHint(display, c->window, &c->transient);
			if (!GETPROP_WIND(win, atoms[WM_CLIENT_LEADER], &c->leader, 1)) c->leader = None;
			c->spot = SPOT1; m = &monitors[0];

			for_monitors(i, m)
				if (INTERSECT(m->x, m->y, m->w, m->h, c->attr.x + c->attr.width/2, c->attr.y+c->attr.height/2, 1, 1))
					{ c->monitor = i; break; }

			int intersect_1 = INTERSECT(c->attr.x, c->attr.y, c->attr.width, c->attr.height,
				m->spots[SPOT1].x + m->spots[SPOT1].w/2, m->spots[SPOT1].y + m->spots[SPOT1].h/2, 1, 1);

			int intersect_2 = INTERSECT(c->attr.x, c->attr.y, c->attr.width, c->attr.height,
				m->spots[SPOT2].x + m->spots[SPOT2].w/2, m->spots[SPOT2].y + m->spots[SPOT2].h/2, 1, 1);

			int intersect_3 = INTERSECT(c->attr.x, c->attr.y, c->attr.width, c->attr.height,
				m->spots[SPOT3].x + m->spots[SPOT3].w/2, m->spots[SPOT3].y + m->spots[SPOT3].h/2, 1, 1);

			     if (intersect_3) c->spot = SPOT3;
			else if (intersect_2) c->spot = SPOT2;
			else if (intersect_1) c->spot = SPOT1;

			if (c->visible)
			{
				if (!GETPROP_ATOM(c->window, atoms[_NET_WM_STATE], c->states, ATOMLIST))
					memset(c->states, 0, sizeof(Atom) * ATOMLIST);

				c->urgent = client_has_state(c, atoms[_NET_WM_STATE_DEMANDS_ATTENTION]);
				c->full   = client_has_state(c, atoms[_NET_WM_STATE_FULLSCREEN]);
				c->maxv   = client_has_state(c, atoms[_NET_WM_STATE_MAXIMIZE_VERT]);
				c->maxh   = client_has_state(c, atoms[_NET_WM_STATE_MAXIMIZE_HORZ]);

				GETPROP_LONG(win, atoms[XOAT_MAXIMIZE], &c->max, 1);

				// proper spot detection based on toggled atoms.
				// XOAT_MAXIMIZE may apply to spot1 windows. Detect...
				if (c->max && intersect_1)
				{
					c->spot = SPOT1;
				}
				else
				// _NET_WM_STATE_MAXIMIZE_VERT may apply to spot2 windows. Detect...
				if (c->maxv && c->type != atoms[_NET_WM_WINDOW_TYPE_DIALOG]
					&& !intersect_1 && intersect_2)
				{
					c->spot = SPOT2;
				}
				else
				// _NET_WM_STATE_MAXIMIZE_HORZ may apply to spot3 windows with c->max. Detect...
				if (c->maxh && c->type != atoms[_NET_WM_WINDOW_TYPE_DIALOG]
					&& !intersect_1 && !intersect_2 && intersect_3)
				{
					c->spot = SPOT3;
				}
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
		}
		return c;
	}
	free(c);
	return NULL;
}

void client_free(Client *c)
{
	if (!c) return;
	free(c->class);
	free(c);
}

void client_update_border(Client *c)
{
	XColor color; Colormap map = DefaultColormap(display, DefaultScreen(display));
	char *colorname = c->window == current ? settings.border_focus: (c->urgent ? settings.border_urgent: settings.border_blur);
	XSetWindowBorder(display, c->window, XAllocNamedColor(display, map, colorname, &color, &color) ? color.pixel: None);
	XSetWindowBorderWidth(display, c->window, c->full ? 0: settings.border);
}

int client_send_wm_protocol(Client *c, Atom protocol)
{
	Atom protocols[ATOMLIST]; int i, n;
	if ((n = GETPROP_ATOM(c->window, atoms[WM_PROTOCOLS], protocols, ATOMLIST)))
		for (i = 0; i < n; i++) if (protocols[i] == protocol)
			return window_send_clientmessage(c->window, c->window, atoms[WM_PROTOCOLS], protocol, NoEventMask);
	return 0;
}

void client_place_spot(Client *c, int spot, int mon, int force)
{
	if (!c) return;
	int i; Client *t;

	// try to center over our transient parent
	if (!force && c->transient && (t = window_build_client(c->transient)))
	{
		if (t->manage)
		{
			spot = t->spot;
			mon = t->monitor;
		}
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

	Monitor *m = &monitors[c->monitor];
	int x = m->spots[spot].x, y = m->spots[spot].y, w = m->spots[spot].w, h = m->spots[spot].h;

	if (c->type == atoms[_NET_WM_WINDOW_TYPE_DIALOG])
	{
		x += (w - c->attr.width)/2;
		y += (h - c->attr.height)/2;
		w = c->attr.width + settings.border * 2;
		h = c->attr.height + settings.border * 2;
	}
	else
	if (c->full)
	{
		XMoveResizeWindow(display, c->window, m->x, m->y, m->w, m->h);
		return;
	}
	else
	// _NET_WM_STATE_MAXIMIZE_VERT may apply to a window in spot2
	if (c->maxv && spot == SPOT2)
	{
		h = m->h - (y - m->y);
	}
	else
	// _NET_WM_STATE_MAXIMIZE_HORZ may apply to a window in spot3
	if (c->maxh && spot == SPOT3)
	{
		w = m->w;
	}
	else
	if (c->max && spot == SPOT1)
	{
		h = m->h - (y - m->y);
		w = m->w;
	}

	w -= settings.border * 2; h -= settings.border * 2;
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
	x = MAX(m->x, MIN(x, m->x + m->w - w - settings.border * 2));
	y = MAX(m->y, MIN(y, m->y + m->h - h - settings.border * 2));

	XMoveResizeWindow(display, c->window, x, y, w, h);
}

void client_stack_family(Client *c, Stack *raise)
{
	int i; Client *o, *self = NULL;
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

void client_raise_family(Client *c)
{
	if (!c) return;
	int i; Client *o; STACK_INIT(raise); STACK_INIT(family);

	for_windows(i, o) if (o->type == atoms[_NET_WM_WINDOW_TYPE_DOCK])
		client_stack_family(o, &raise);

	while (c->transient && (o = window_build_client(c->transient)) && o->manage)
		c = family.clients[family.depth++] = o;

	client_stack_family(c, &raise);

	if (!c->full && settings.title)
	{
		// raise spot's title bar in case some other fullscreen or max v/h window has obscured
		Monitor *m = &monitors[c->monitor];
		raise.windows[raise.depth] = m->bars[c->spot]->window;
		raise.clients[raise.depth] = NULL;
		raise.depth++;
	}

	XRaiseWindow(display, raise.windows[0]);
	XRestackWindows(display, raise.windows, raise.depth);

	STACK_FREE(&family);
}

void client_set_focus(Client *c)
{
	if (!c || !c->visible || c->window == current) return;
	Client *o; Window old = current;

	current      = c->window;
	current_spot = c->spot;
	current_mon  = c->monitor;

	if (old && (o = window_build_client(old)))
	{
		client_update_border(o);
		client_free(o);
	}
	client_send_wm_protocol(c, atoms[WM_TAKE_FOCUS]);
	XSetInputFocus(display, c->input ? c->window: PointerRoot, RevertToPointerRoot, CurrentTime);
	SETPROP_WIND(root, atoms[_NET_ACTIVE_WINDOW], &c->window, 1);
	client_update_border(c);
}

void client_activate(Client *c)
{
	client_raise_family(c);
	client_set_focus(c);
}
