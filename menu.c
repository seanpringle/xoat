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

void menu_update()
{
	if (menu->mb)
	{
		menubox_draw(menu->mb);
	}
}

void menu_close()
{
	if (menu->mb)
	{
		menubox_free(menu->mb);
		menu->mb = NULL;

		for (int i = 0; i < sizeof(menu->items)/sizeof(MenuMap); i++)
		{
			free(menu->items[i].name);
			menu->items[i].name = NULL;
		}
	}
}

void menu_select()
{
	int id; Client *c;

	if (menu->mb)
	{
		id = menubox_get_id(menu->mb);
		if (id >= 0 && (c = window_build_client(menu->items[id].window)) && c)
		{
			if (c->manage)
				client_activate(c);
			client_free(c);
		}
		else
		if (strlen(menu->mb->input->text))
		{
			exec_cmd(menu->mb->input->text);
		}
		menu_close();
	}
}

int menu_sort(const void *a, const void *b)
{
	const MenuMap *ma = a;
	const MenuMap *mb = b;
	char as[strlen(ma->name)+1];
	char bs[strlen(mb->name)+1];
	for (int i = 0; ma->name[i]; i++) as[i] = tolower(ma->name[i]);
	for (int i = 0; mb->name[i]; i++) bs[i] = tolower(mb->name[i]);
	return strcmp(as, bs);
}

void menu_create(int spot, int mon)
{
	short x, y, w, h;
	int i, n = 0; Client *c;
	char *name = NULL;

	Monitor *m = &monitors[menu->monitor];
	
	if (menu->mb)
		menu_close();

	x = m->spots[spot].x;
	y = m->spots[spot].y;
	w = m->spots[SPOT3].w/2;
	h = 10;

	Menubox *mb = menubox_create(root, MB_AUTOHEIGHT|MB_AUTOWIDTH|MB_MINWIDTH, x, y, w, h, 
		settings.border, settings.menu, settings.title_focus, settings.border_focus, settings.title_blur, settings.border_blur);
	
	menu->mb = mb;
	menu->spot = spot;
	menu->monitor = mon;

	for_windows(i,c) if (n < sizeof(menu->items)/sizeof(MenuMap) && c->manage && c->spot == spot && c->monitor == mon && (name = window_get_name(c->window)) && name)
	{
		menu->items[n].name = name;
		menu->items[n].window = c->window;
		n++;
	}

	qsort(menu->items, n, sizeof(MenuMap), menu_sort);

	for (i = 0; i < n; i++)
		menubox_add(mb, i, menu->items[i].name);

	x = (m->spots[spot].w - MIN(mb->w, m->spots[spot].w-100))/2;
	y = (m->spots[spot].h - MIN(mb->h, m->spots[spot].h-100))/2;

	x += m->spots[spot].x;
	y += m->spots[spot].y;

	menubox_moveresize(mb, x, y, w, h);

	menubox_show(mb);
	menubox_draw(mb);
	menubox_grab(mb);
}