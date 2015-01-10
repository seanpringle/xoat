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

#define MB_AUTOHEIGHT 1<<0
#define MB_AUTOWIDTH 1<<1
#define MB_MINWIDTH 1<<3

typedef struct {
	unsigned long flags;
	Window window, parent;
	short x, y, w, h, line_height, padding;
	char *font, *fg_focus, *bg_focus, *fg_blur, *bg_blur;
	unsigned int box_count, box_active;
	Textbox **boxes;
	int *ids;
	int *show;
	Textbox *input;
} Menubox;

void menubox_moveresize(Menubox*, short, short, short, short);

// Xft text box, optionally editable
Menubox* menubox_create(Window parent, unsigned long flags, short x, short y, short w, short h, short padding, char *font, char *fg_focus, char *bg_focus, char *fg_blur, char *bg_blur)
{
	Menubox *mb = calloc(1, sizeof(Menubox));
	mb->boxes = (Textbox**) calloc(1, sizeof(Textbox));
	mb->ids = (int*) calloc(1, sizeof(int));
	mb->show = (int*) calloc(1, sizeof(int));

	mb->flags = flags;
	mb->parent = parent;
	mb->font = font;
	mb->fg_focus = fg_focus;
	mb->fg_blur = fg_blur;
	mb->bg_focus = bg_focus;
	mb->bg_blur = bg_blur;
	mb->padding = padding;
	mb->box_active = 0;

	mb->x = x; mb->y = y; mb->w = MAX(1, w); mb->h = MAX(1, h);

	XColor color; Colormap map = DefaultColormap(display, DefaultScreen(display));
	unsigned int cp = XAllocNamedColor(display, map, bg_blur, &color, &color) ? color.pixel: None;

	mb->window = XCreateSimpleWindow(display, mb->parent, mb->x, mb->y, mb->w, mb->h, 0, None, cp);

	// calc line height
	mb->input = textbox_create(mb->window, TB_AUTOHEIGHT|TB_EDITABLE, mb->padding, mb->padding, mb->w, 0, mb->font, mb->fg_blur, mb->bg_blur, "", "> ");
	mb->line_height = mb->input->h;

	// auto height/width modes get handled here
	menubox_moveresize(mb, mb->x, mb->y, mb->w, mb->h);

	return mb;
}

void menubox_moveresize(Menubox *mb, short x, short y, short w, short h)
{
	int ow = w;

	if (mb->flags & MB_AUTOHEIGHT)
		h = (mb->box_count+1) * mb->line_height + (mb->padding*2);

	if (mb->flags & MB_AUTOWIDTH)
	{
		for (uint i = 0; i < mb->box_count; i++)
			w = MAX(mb->boxes[i]->extents.width, w);
		w += (mb->padding*2);
	}

	if (mb->flags & MB_MINWIDTH)
	{
		w = MAX(ow, w);
	}

	if (x != mb->x || y != mb->y || w != mb->w || h != mb->h)
	{
		mb->x = x; mb->y = y; mb->w = MAX(1, w); mb->h = MAX(1, h);
		XMoveResizeWindow(display, mb->window, mb->x, mb->y, mb->w, mb->h);
	}
}

void menubox_add(Menubox *mb, int id, char *text)
{
	uint slot = mb->box_count++;

	mb->boxes = (Textbox**) realloc(mb->boxes, sizeof(Textbox) * mb->box_count);
	mb->ids   = (int*)      realloc(mb->ids,   sizeof(int)     * mb->box_count);
	mb->show  = (int*)      realloc(mb->show,  sizeof(int)     * mb->box_count);
	
	mb->boxes[slot] = textbox_create(mb->window, TB_AUTOHEIGHT|TB_AUTOWIDTH,
		mb->padding, mb->line_height*(slot+1)+mb->padding, mb->w-(mb->padding*2), 0,
		mb->font, mb->fg_blur, mb->bg_blur, text, NULL);

	mb->ids[slot]   = id;
	mb->show[slot]  = 1;
	
	if (mb->flags & (MB_AUTOWIDTH|MB_AUTOHEIGHT))
		menubox_moveresize(mb, mb->x, mb->y, mb->w, mb->h);
}

int menubox_get_id(Menubox *mb)
{
	return mb->show[mb->box_active] ? mb->ids[mb->box_active]: -1;
}

void menubox_grab(Menubox *mb)
{
	for (uint i = 0; i < 1000; i++)
	{
		if (XGrabKeyboard(display, mb->window, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
			break;
		usleep(1000);
	}
}

void menubox_draw(Menubox *mb)
{
	textbox_draw(mb->input);
	int filter = strlen(mb->input->text) > 0;

	for (uint i = 0, n = 0; i < mb->box_count; i++)
	{
		if (!filter || mb->show[i])
		{
			textbox_font(mb->boxes[i], mb->font, mb->box_active == i ? mb->fg_focus: mb->fg_blur, mb->box_active == i ? mb->bg_focus: mb->bg_blur);
			textbox_moveresize(mb->boxes[i], mb->padding, mb->line_height*(n+1)+mb->padding, mb->w-(mb->padding*2), 0);
			textbox_show(mb->boxes[i]);	
			textbox_draw(mb->boxes[i]);
			n++;
		}
		else
		{
			textbox_hide(mb->boxes[i]);
		}
	}
}

void menubox_show(Menubox *mb)
{
	textbox_show(mb->input);
	XMapWindow(display, mb->window);
}

void menubox_hide(Menubox *mb)
{
	XUnmapWindow(display, mb->window);
}

void menubox_free(Menubox *mb)
{
	for (uint i = 0; i < mb->box_count; i++)
		textbox_free(mb->boxes[i]);
	free(mb->boxes);
	free(mb->ids);
	free(mb->show);
	textbox_free(mb->input);
	XDestroyWindow(display, mb->window);
	free(mb);
}

void menubox_filter(Menubox *mb)
{
	int filter = strlen(mb->input->text) > 0;
	char *pattern = strdup(mb->input->text);
	for (char *p = pattern; *p; *p = tolower(*p), p++);

	for (int i = 0; i < mb->box_count; i++)
	{
		char *subject = strdup(mb->boxes[i]->text);
		for (char *p = subject; *p; *p = tolower(*p), p++);
		mb->show[i] = (!filter || regex_match(pattern, subject));
		free(subject);
	}
	free(pattern);

	// active entry maybe filtered. step backward
	while (!mb->show[mb->box_active] && mb->box_active > 0)
		mb->box_active--;

	// active entry maybe filtered. step forward
	while (!mb->show[mb->box_active] && mb->box_active < mb->box_count-1)
		mb->box_active++;
}

void menubox_key_up(Menubox *mb)
{
	if (mb->box_active > 0) mb->box_active--;
}

void menubox_key_down(Menubox *mb)
{
	if (mb->box_active < mb->box_count-1) mb->box_active++;
}

// handle a keypress in edit mode
// 0 = unhandled
// 1 = handled
// -1 = handled and return pressed 
// -2 = handled and escape pressed
int menubox_keypress(Menubox *mb, XEvent *ev)
{
	KeySym key; Status stat; char pad[32];

	int len = XmbLookupString(mb->input->xic, &ev->xkey, pad, sizeof(pad), &key, &stat);
	pad[len] = 0;

	int rc = 0;

	switch (key)
	{
		case XK_Up     : { menubox_key_up(mb);   rc = 1; break; }
		case XK_Down   : { menubox_key_down(mb); rc = 1; break; }
		case XK_Escape : { rc = -2; break; }
		case XK_Return : { rc = -1; break; }
		default: rc = textbox_keypress(mb->input, ev);
	}

	menubox_filter(mb);
	return rc;
}