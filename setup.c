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

typedef struct {
	// _NET_WM_STRUT_PARTIAL
	long left, right, top, bottom, ly1, ly2, ry1, ry3, tx1, tx2, bx1, bx2;
} wm_strut;

void setup()
{
	int i, j; Client *c; Monitor *m;

	int screen_w = WidthOfScreen(DefaultScreenOfDisplay(display));
	int screen_h = HeightOfScreen(DefaultScreenOfDisplay(display));

	// default non-multi-head setup
	monitors[0].w = screen_w;
	monitors[0].h = screen_h;

	// support multi-head.
	XineramaScreenInfo *info;
	if (XineramaIsActive(display) && (info = XineramaQueryScreens(display, &nmonitors)))
	{
		nmonitors = MIN(nmonitors, MONITORS);
		for_monitors(i, m)
		{
			m->x = info[i].x_org;
			m->y = info[i].y_org;
			m->w = info[i].width;
			m->h = info[i].height;
		}
		XFree(info);
	}

	// detect and adjust for panel struts
	Monitor padded[MONITORS];
	memmove(padded, monitors, sizeof(Monitor) * MONITORS);
	wm_strut all_struts; memset(&all_struts, 0, sizeof(wm_strut));

	for_windows(i, c)
	{
		wm_strut strut; memset(&strut, 0, sizeof(wm_strut));
		int v2 = GETPROP_LONG(c->window, atoms[_NET_WM_STRUT_PARTIAL], (unsigned long*)&strut, 12);
		int v1 = v2 ? 0: GETPROP_LONG(c->window, atoms[_NET_WM_STRUT], (unsigned long*)&strut, 4);
		if (!c->visible || (!v1 && !v2)) continue;

		all_struts.left   = MAX(all_struts.left,   strut.left);
		all_struts.right  = MAX(all_struts.right,  strut.right);
		all_struts.top    = MAX(all_struts.top,    strut.top);
		all_struts.bottom = MAX(all_struts.bottom, strut.bottom);
	}

	for_monitors(j, m)
	{
		Monitor *p = &padded[j];

		// monitor left side of root window?
		if (all_struts.left > 0 && !m->x)
		{
			p->x += all_struts.left;
			p->w -= all_struts.left;
		}
		// monitor right side of root window?
		if (all_struts.right > 0 && m->x + m->w == screen_w)
		{
			p->w -= all_struts.right;
		}
		// monitor top side of root window?
		if (all_struts.top > 0 && !m->y)
		{
			p->y += all_struts.top;
			p->h -= all_struts.top;
		}
		// monitor bottom side of root window?
		if (all_struts.bottom > 0 && m->y + m->h == screen_h)
		{
			p->h -= all_struts.bottom;
		}
	}
	memmove(monitors, padded, sizeof(Monitor) * MONITORS);

	// calculate spot boxes
	for_monitors(i, m)
	{
		int x = m->x, y = m->y, w = m->w, h = m->h;

		// determine spot layout for this monitor
		int spot1_align      = have_layout(i) ? settings.layouts[i].spot1_align      : LEFT ;
		int spot1_width_pct  = have_layout(i) ? settings.layouts[i].spot1_width_pct  : 66;
		int spot2_height_pct = have_layout(i) ? settings.layouts[i].spot2_height_pct : 66;

		// monitor rotated?
		if (m->w < m->h)
		{
			int height_spot1 = (double)h / 100 * MIN(90, MAX(10, spot1_width_pct));
			int width_spot2  = (double)w / 100 * MIN(90, MAX(10, spot2_height_pct));
			for_spots(j)
			{
				m->spots[j].x = x;
				m->spots[j].y = spot1_align == LEFT ? y: y + h - height_spot1;
				m->spots[j].w = w;
				m->spots[j].h = height_spot1;
				if (j == SPOT1) continue;

				m->spots[j].y = spot1_align == LEFT ? y + height_spot1 + GAP: y;
				m->spots[j].h = h - height_spot1 - GAP;
				m->spots[j].w = w - width_spot2 - GAP;
				if (j == SPOT3) continue;

				m->spots[j].x = x + w - width_spot2;
				m->spots[j].w = width_spot2;
			}
			continue;
		}
		// normal wide screen
		int width_spot1  = (double)w / 100 * MIN(90, MAX(10, spot1_width_pct));
		int height_spot2 = (double)h / 100 * MIN(90, MAX(10, spot2_height_pct));
		for_spots(j)
		{
			m->spots[j].x = spot1_align == LEFT ? x: x + w - width_spot1;
			m->spots[j].y = y;
			m->spots[j].w = width_spot1;
			m->spots[j].h = h;
			if (j == SPOT1) continue;

			m->spots[j].x = spot1_align == LEFT ? x + width_spot1 + GAP: x;
			m->spots[j].w = w - width_spot1 - GAP;
			m->spots[j].h = height_spot2;
			if (j == SPOT2) continue;

			m->spots[j].y = y + height_spot2 + GAP;
			m->spots[j].h = h - height_spot2 - GAP;
		}
	}

	// become the window manager
	XSelectInput(display, root, PropertyChangeMask | StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask);

	// ewmh support
	unsigned long pid = getpid();
	ewmh = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);

	SETPROP_ATOM(root, atoms[_NET_SUPPORTED],           atoms, ATOMS);
	SETPROP_WIND(root, atoms[_NET_SUPPORTING_WM_CHECK], &ewmh,     1);
	SETPROP_LONG(ewmh, atoms[_NET_WM_PID],              &pid,      1);

	XChangeProperty(display, ewmh, atoms[_NET_WM_NAME], XA_STRING, 8, PropModeReplace, (const unsigned char*)"xoat", 4);

	// figure out NumlockMask
	XModifierKeymap *modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++) for (j = 0; j < (int)modmap->max_keypermod; j++)
		if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(display, XK_Num_Lock))
			{ NumlockMask = (1<<i); break; }
	XFreeModifiermap(modmap);

	// process config.h key bindings
	for (i = 0; i < settings.binding_count; i++)
	{
		XGrabKey(display, XKeysymToKeycode(display, settings.bindings[i].key), settings.bindings[i].mod, root, True, GrabModeAsync, GrabModeAsync);
		if (settings.bindings[i].mod == AnyModifier) continue;

		XGrabKey(display, XKeysymToKeycode(display, settings.bindings[i].key), settings.bindings[i].mod|LockMask, root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, XKeysymToKeycode(display, settings.bindings[i].key), settings.bindings[i].mod|NumlockMask, root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, XKeysymToKeycode(display, settings.bindings[i].key), settings.bindings[i].mod|LockMask|NumlockMask, root, True, GrabModeAsync, GrabModeAsync);
	}

	// we grab buttons to do click-to-focus. all clicks get passed through to apps.
	XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	XGrabButton(display, Button3, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);

	// create title bars
	if (settings.title)
	{
		STACK_FREE(&windows);
		for_monitors(i, m) for_spots(j)
		{
			m->bars[j] = textbox_create(root, TB_AUTOHEIGHT|TB_LEFT, m->spots[j].x, m->spots[j].y, m->spots[j].w, 0,
				settings.title, settings.title_blur, settings.border_blur, NULL, NULL);
			XSelectInput(display, m->bars[j]->window, ExposureMask);

			m->spots[j].y += m->bars[j]->h;
			m->spots[j].h -= m->bars[j]->h;
			spot_update_bar(j, i);
		}
	}

	// setup existing managable windows
	STACK_FREE(&windows);
	for_windows(i, c) if (c->manage)
	{
		window_listen(c->window);
		client_update_border(c);
		client_place_spot(c, c->spot, c->monitor, 0);
		if (!current) client_activate(c);
	}

	// auto launch
	if (settings.launchcmd_count)
	{
		for (i = 0; i < settings.launchcmd_count; i++)
			exec_cmd(settings.launchcmds[i]);
	}
}
