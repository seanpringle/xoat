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
	{
		while (XCheckTypedEvent(display, ConfigureNotify, e));
		ewmh_client_list();
		update_bars();
	}
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
		client_set_focus(c);
		client_free(a);
		ewmh_client_list();
		update_bars();
	}
	client_free(c);
}

void unmap_notify(XEvent *e)
{
	// if this window was focused, find something else
	if (e->xunmap.window == current && !spot_focus_top_window(current_spot, current_mon, current))
		{ int i; for_spots(i) if (spot_focus_top_window(i, current_mon, current)) break; }
	ewmh_client_list();
	update_bars();
}

void key_press(XEvent *ev)
{
	XKeyEvent *e = &ev->xkey; latest = e->time;
	KeySym key = XkbKeycodeToKeysym(display, e->keycode, 0, 0);
	unsigned int state = e->state & ~(LockMask|NumlockMask);
	while (XCheckTypedEvent(display, KeyPress, ev));

	binding *bind = NULL;
	for (int i = 0; i < sizeof(keys)/sizeof(binding) && !bind; i++)
		if (keys[i].key == key && (keys[i].mod == AnyModifier || keys[i].mod == state))
			bind = &keys[i];

	if (bind && bind->act)
	{
		client *cli = window_build_client(current);
		bind->act(bind->data, bind->num, cli);
		client_free(cli);
		update_bars();
	}
}

void button_press(XEvent *ev)
{
	int i, j; monitor *m;
	XButtonEvent *e = &ev->xbutton; latest = e->time;
	client *c = window_build_client(e->subwindow);
	if (c && c->manage)
		client_activate(c);
	else
		for_monitors(i, m) for_spots(j)
			if (m->bars[j]->window == e->subwindow)
				spot_focus_top_window(j, i, None);
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
		EXECSH(self);
	}
	client *c = window_build_client(e->window);
	if (c && c->manage)
	{
		if (e->message_type == atoms[_NET_ACTIVE_WINDOW]) client_activate(c);
		if (e->message_type == atoms[_NET_CLOSE_WINDOW])  action_close(NULL, 0, c);
	}
	client_free(c);
}

void property_notify(XEvent *ev)
{
	XPropertyEvent *e = &ev->xproperty;

	client *c = window_build_client(e->window);
	if (c && c->visible && c->manage)
	{
		client_update_border(c);
		if (e->atom == atoms[WM_NAME] || e->atom == atoms[_NET_WM_NAME])
			spot_update_bar(c->spot, c->monitor);
	}
	client_free(c);

	// prevent spam
	while (XCheckTypedWindowEvent(display, ev->xproperty.window, PropertyNotify, ev));
}

void expose(XEvent *e)
{
	while (XCheckTypedEvent(display, Expose, e));
	update_bars();
}

void any_event(XEvent *e)
{
	client *c = window_build_client(e->xany.window);
	if (c && c->visible && c->manage)
		client_update_border(c);
	client_free(c);
}

