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

#define SPOT_BUFF 1024

void spot_update_bar(int spot, int mon)
{
	int i, n = 0, len = 0; Client *o, *c = NULL;
	char title[SPOT_BUFF]; *title = 0;
	Monitor *m = &monitors[mon];

	if (spot == SPOT1)
	{
		// Show X root window name in spot1 bar
		// Use xsetroot -name for a custom status
		char *tmp = NULL;
		if (XFetchName(display, root, &tmp))
			len += snprintf(title+len, MAX(0, SPOT_BUFF-len), " %s  ", tmp);
		if (tmp) XFree(tmp);
	}
	for_windows(i, o) if (o->manage && o->spot == spot && o->monitor == mon)
	{
		if (!c) c = o;
		char *name = NULL, *tmp = NULL;
		if (!(name = window_get_text_prop(o->window, atoms[_NET_WM_NAME])))
			if (XFetchName(display, o->window, &tmp))
				name = strdup(tmp);
		if (name)
		{
			if (settings.title_ellipsis > 0 && strlen(name) > settings.title_ellipsis)
			{
				name = realloc(name, strlen(name)+4);
				strcpy(name+settings.title_ellipsis, "...");
			}
			len += snprintf(title+len, MAX(0, SPOT_BUFF-len), " [%d] %s  ", n++, name);
			free(name);
		}
		if (tmp) XFree(tmp);
	}
	if (settings.title)
	{
		if (c && !c->full && *title && m->bars[spot])
		{
			int focus = c->window == current || (spot == current_spot && mon == current_mon);
			char *color  = focus && c->window == current ? settings.title_focus : settings.title_blur;
			char *border = focus && c->window == current ? settings.border_focus: settings.border_blur;
			textbox_font(m->bars[spot], settings.title, color, border);
			textbox_text(m->bars[spot], title);
			textbox_draw(m->bars[spot]);
			textbox_show(m->bars[spot]);
		}
		else
		if (m->bars[spot])
			textbox_hide(m->bars[spot]);
	}
}

void update_bars()
{
	int i, j; Monitor *m;
	if (settings.title) for_monitors(i, m) for_spots(j)
		spot_update_bar(j, i);
}

Window spot_focus_top_window(int spot, int mon, Window except)
{
	int i; Client *c;
	for_windows(i, c) if (c->window != except && c->manage && c->spot == spot && c->monitor == mon)
	{
		client_raise_family(c);
		client_set_focus(c);
		return c->window;
	}
	return None;
}

Window spot_try_focus_top_window(int spot, int mon, Window except)
{
	Window w = spot_focus_top_window(spot, mon, except);
	if (w == None)
	{
		current      = None;
		current_mon  = mon;
		current_spot = spot;
		update_bars();

		XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
	}
	return w;
}

int spot_choose_by_direction(int spot, int mon, int dir)
{
	Monitor *m = &monitors[mon];
	int spot1_align = have_layout(mon) ? settings.layouts[mon].spot1_align : LEFT ;

	if (m->w < m->h) // rotated?
	{
		if (dir == LEFT)  return SPOT3;
		if (dir == RIGHT) return SPOT2;
		if (dir == UP)    return spot1_align == LEFT ? SPOT1: SPOT2;
		if (dir == DOWN)  return spot1_align == LEFT ? SPOT2: SPOT1;
		return spot;
	}
	if (dir == UP)    return SPOT2;
	if (dir == DOWN)  return SPOT3;
	if (dir == LEFT)  return spot1_align == LEFT ? SPOT1: SPOT2;
	if (dir == RIGHT) return spot1_align == LEFT ? SPOT2: SPOT1;
	return spot;
}

int spot_count_windows(int spot, int mon)
{
	int i, n = 0; Client *c;
	for_windows(i, c) if (c->manage && c->spot == spot && c->monitor == mon) n++;
	return n;
}

int spot_stack_clients(int spot, int mon, Stack *stk)
{
	int i; Client *c;
	for_windows(i, c) if (c->manage && c->spot == spot && c->monitor == mon)
		client_stack_family(c, stk);
	return stk->depth;
}