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
	int i, j; client *c; monitor *m;
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
	for_windows(i, c)
	{
		wm_strut strut; memset(&strut, 0, sizeof(wm_strut));
		int v2 = GETPROP_LONG(c->window, atoms[_NET_WM_STRUT_PARTIAL], (unsigned long*)&strut, 12);
		int v1 = v2 ? 0: GETPROP_LONG(c->window, atoms[_NET_WM_STRUT], (unsigned long*)&strut, 4);
		if (!c->visible || (!v1 && !v2)) continue;

		for_monitors(j, m)
		{
			// convert _NET_WM_STRUT to _PARTIAL
			if (v1)
			{
				strut.ly1 = m->y; strut.ly2 = m->y + m->h;
				strut.ry1 = m->y; strut.ry3 = m->y + m->h;
				strut.tx1 = m->x; strut.tx2 = m->x + m->w;
				strut.bx1 = m->x; strut.bx2 = m->x + m->w;
			}
			if (strut.left > 0 && !m->x
				&& INTERSECT(0, strut.ly1, strut.left, strut.ly2 - strut.ly1, m->x, m->y, m->w, m->h))
			{
				m->x += strut.left;
				m->w -= strut.left;
			}
			if (strut.right > 0 && m->x + m->w == screen_w
				&& INTERSECT(screen_w - strut.right, strut.ry1, strut.right, strut.ry3 - strut.ry1, m->x, m->y, m->w, m->h))
			{
				m->w -= strut.right;
			}
			if (strut.top > 0 && !m->y
				&& INTERSECT(strut.tx1, 0, strut.tx2 - strut.tx1, strut.top, m->x, m->y, m->w, m->h))
			{
				m->y += strut.top;
				m->h -= strut.top;
			}
			if (strut.bottom > 0 && m->y + m->h == screen_h
				&& INTERSECT(strut.bx1, screen_h - strut.bottom, strut.bx2 - strut.bx1, strut.bottom, m->x, m->y, m->w, m->h))
			{
				m->h -= strut.bottom;
			}
		}
	}

	// calculate spot boxes
	for_monitors(i, m)
	{
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

				m->spots[j].y = SPOT1_ALIGN == SPOT1_LEFT ? y + height_spot1 + GAP: y;
				m->spots[j].h = h - height_spot1 - GAP;
				m->spots[j].w = w - width_spot2 - GAP;
				if (j == SPOT3) continue;

				m->spots[j].x = x + w - width_spot2;
				m->spots[j].w = width_spot2;
			}
			continue;
		}
		// normal wide screen
		int width_spot1  = (double)w / 100 * MIN(90, MAX(10, SPOT1_WIDTH_PCT));
		int height_spot2 = (double)h / 100 * MIN(90, MAX(10, SPOT2_HEIGHT_PCT));
		for_spots(j)
		{
			m->spots[j].x = SPOT1_ALIGN == SPOT1_LEFT ? x: x + w - width_spot1;
			m->spots[j].y = y;
			m->spots[j].w = width_spot1;
			m->spots[j].h = h;
			if (j == SPOT1) continue;

			m->spots[j].x = SPOT1_ALIGN == SPOT1_LEFT ? x + width_spot1 + GAP: x;
			m->spots[j].w = w - width_spot1 - GAP;
			m->spots[j].h = height_spot2;
			if (j == SPOT2) continue;

			m->spots[j].y = y + height_spot2 + GAP;
			m->spots[j].h = h - height_spot2 - GAP;
		}
	}

	// become the window manager
	XSelectInput(display, root, StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask);

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

	// create title bars
	STACK_FREE(&windows);
	for_monitors(i, m)
	{
		for_spots(j)
		{
			m->bars[j] = textbox_create(root, TB_AUTOHEIGHT|TB_LEFT, m->spots[j].x, m->spots[j].y, m->spots[j].w, 0,
				TITLE, TITLE_BLUR, BORDER_BLUR, NULL, NULL);
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
}
