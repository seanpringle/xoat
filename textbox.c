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

#define TB_AUTOHEIGHT 1<<0
#define TB_AUTOWIDTH 1<<1
#define TB_LEFT 1<<16
#define TB_RIGHT 1<<17
#define TB_CENTER 1<<18
#define TB_EDITABLE 1<<19

typedef struct {
	unsigned long flags;
	Window window, parent;
	short x, y, w, h, cursor;
	XftFont *font;
	XftColor color_fg, color_bg;
	char *text, *prompt;
	XIM xim;
	XIC xic;
	XGlyphInfo extents;
} Textbox;

void textbox_font(Textbox *tb, char *font, char *fg, char *bg);
void textbox_text(Textbox *tb, char *text);
void textbox_moveresize(Textbox *tb, int x, int y, int w, int h);

// Xft text box, optionally editable
Textbox* textbox_create(Window parent, unsigned long flags, short x, short y, short w, short h, char *font, char *fg, char *bg, char *text, char *prompt)
{
	Textbox *tb = calloc(1, sizeof(Textbox));

	tb->flags = flags;
	tb->parent = parent;

	tb->x = x; tb->y = y; tb->w = MAX(1, w); tb->h = MAX(1, h);

	XColor color; Colormap map = DefaultColormap(display, DefaultScreen(display));
	unsigned int cp = XAllocNamedColor(display, map, bg, &color, &color) ? color.pixel: None;

	tb->window = XCreateSimpleWindow(display, tb->parent, tb->x, tb->y, tb->w, tb->h, 0, None, cp);

	// need to preload the font to calc line height
	textbox_font(tb, font, fg, bg);

	tb->prompt = strdup(prompt ? prompt: "");
	textbox_text(tb, text ? text: "");

	// auto height/width modes get handled here
	textbox_moveresize(tb, tb->x, tb->y, tb->w, tb->h);

	// edit mode controls
	if (tb->flags & TB_EDITABLE)
	{
		tb->xim = XOpenIM(display, NULL, NULL, NULL);
		tb->xic = XCreateIC(tb->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, tb->window, XNFocusWindow, tb->window, NULL);
	}

	return tb;
}

// set an Xft font by name
void textbox_font(Textbox *tb, char *font, char *fg, char *bg)
{
	if (tb->font) XftFontClose(display, tb->font);
	tb->font = XftFontOpenName(display, DefaultScreen(display), font);

	XftColorAllocName(display, DefaultVisual(display, DefaultScreen(display)), DefaultColormap(display, DefaultScreen(display)), fg, &tb->color_fg);
	XftColorAllocName(display, DefaultVisual(display, DefaultScreen(display)), DefaultColormap(display, DefaultScreen(display)), bg, &tb->color_bg);
}

// outer code may need line height, width, etc
void textbox_extents(Textbox *tb)
{
	int length = strlen(tb->text) + strlen(tb->prompt);
	char *line = calloc(length + 1, sizeof(char));
	sprintf(line, "%s%s", tb->prompt, tb->text);
	XftTextExtentsUtf8(display, tb->font, (unsigned char*)line, length, &tb->extents);
	free(line);
}

// set the default text to display
void textbox_text(Textbox *tb, char *text)
{
	if (tb->text) free(tb->text);
	tb->text = strdup(text ? text: "");
	tb->cursor = MAX(0, MIN(strlen(tb->text), tb->cursor));
	textbox_extents(tb);
}

// set an input prompt for edit mode
void textbox_prompt(Textbox *tb, char *text)
{
	if (tb->prompt) free(tb->prompt);
	tb->prompt = strdup(text);
	textbox_extents(tb);
}

// within the parent. handled auto width/height modes
void textbox_moveresize(Textbox *tb, int x, int y, int w, int h)
{
	if (tb->flags & TB_AUTOHEIGHT)
		h = tb->font->ascent + tb->font->descent;

	if (tb->flags & TB_AUTOWIDTH)
		w = tb->extents.width;

	if (x != tb->x || y != tb->y || w != tb->w || h != tb->h)
	{
		tb->x = x; tb->y = y; tb->w = MAX(1, w); tb->h = MAX(1, h);
		XMoveResizeWindow(display, tb->window, tb->x, tb->y, tb->w, tb->h);
	}
}

void textbox_show(Textbox *tb)
{
	XMapWindow(display, tb->window);
}

void textbox_hide(Textbox *tb)
{
	XUnmapWindow(display, tb->window);
}

// will also unmap the window if still displayed
void textbox_free(Textbox *tb)
{
	if (tb->flags & TB_EDITABLE)
	{
		XDestroyIC(tb->xic);
		XCloseIM(tb->xim);
	}

	if (tb->text) free(tb->text);
	if (tb->prompt) free(tb->prompt);
	if (tb->font) XftFontClose(display, tb->font);

	XDestroyWindow(display, tb->window);
	free(tb);
}

void textbox_draw(Textbox *tb)
{
	XGlyphInfo extents;

	GC context    = XCreateGC(display, tb->window, 0, 0);
	Pixmap canvas = XCreatePixmap(display, tb->window, tb->w, tb->h, DefaultDepth(display, DefaultScreen(display)));
	XftDraw *draw = XftDrawCreate(display, canvas, DefaultVisual(display, DefaultScreen(display)), DefaultColormap(display, DefaultScreen(display)));

	// clear canvas
	XftDrawRect(draw, &tb->color_bg, 0, 0, tb->w, tb->h);

	char *line  = tb->text,
		*text   = tb->text ? tb->text: "",
		*prompt = tb->prompt ? tb->prompt: "",
		*eline  = NULL;

	int text_len    = strlen(text);
	int length      = text_len;
	int line_height = tb->font->ascent + tb->font->descent;

	int cursor_x      = 0;
	int cursor_offset = 0;
	int cursor_width  = MAX(2, line_height/10);

	if (tb->flags & TB_EDITABLE)
	{
		int prompt_len = strlen(prompt);
		length = text_len + prompt_len;
		cursor_offset = MIN(tb->cursor + prompt_len, length);

		eline = calloc(length+10, sizeof(char)); line = eline;
		sprintf(line, "%s%s", prompt, text);

		// replace trailing space so XftTextExtents8 includes its width
		if (length > 0 && isspace(line[length-1])) line[length-1] = '.';

		// calc cursor position
		XftTextExtentsUtf8(display, tb->font, (unsigned char*)line, cursor_offset, &extents);
		cursor_x = extents.width;

		// restore correct text string
		sprintf(line, "%s%s", prompt, text);
	}

	// calc full input text width
	XftTextExtentsUtf8(display, tb->font, (unsigned char*)line, length, &extents);
	int line_width = extents.width;

	int x = 0, y = tb->font->ascent;
	if (tb->flags & TB_RIGHT)  x = tb->w - line_width;
	if (tb->flags & TB_CENTER) x = MAX(0, (tb->w - line_width) / 2);

	// draw the text, including any prompt in edit mode
	XftDrawStringUtf8(draw, &tb->color_fg, tb->font, x, y, (unsigned char*)line, length);

	// draw the cursor
	if (tb->flags & TB_EDITABLE)
		XftDrawRect(draw, &tb->color_fg, cursor_x, 2, cursor_width, line_height-4);

	// flip canvas to window
	XCopyArea(display, canvas, tb->window, context, 0, 0, tb->w, tb->h, 0, 0);

	XFreeGC(display, context);
	XftDrawDestroy(draw);
	XFreePixmap(display, canvas);

	free(eline);
}

// cursor handling for edit mode
void textbox_cursor(Textbox *tb, int pos)
{
	tb->cursor = MAX(0, MIN(strlen(tb->text), pos));
}

// move right
void textbox_cursor_inc(Textbox *tb)
{
	textbox_cursor(tb, tb->cursor+1);
}

// move left
void textbox_cursor_dec(Textbox *tb)
{
	textbox_cursor(tb, tb->cursor-1);
}

// beginning of line
void textbox_cursor_home(Textbox *tb)
{
	tb->cursor = 0;
}

// end of line
void textbox_cursor_end(Textbox *tb)
{
	tb->cursor = strlen(tb->text);
}

// insert text
void textbox_insert(Textbox *tb, int pos, char *str)
{
	int len = strlen(tb->text), slen = strlen(str);
	pos = MAX(0, MIN(len, pos));
	// expand buffer
	tb->text = realloc(tb->text, len + slen + 1);
	// move everything after cursor upward
	char *at = tb->text + pos;
	memmove(at + slen, at, len - pos + 1);
	// insert new str
	memmove(at, str, slen);
	textbox_extents(tb);
}

// remove text
void textbox_delete(Textbox *tb, int pos, int dlen)
{
	int len = strlen(tb->text);
	pos = MAX(0, MIN(len, pos));
	// move everything after pos+dlen down
	char *at = tb->text + pos;
	memmove(at, at + dlen, len - pos);
	textbox_extents(tb);
}

// insert one character
void textbox_cursor_ins(Textbox *tb, char c)
{
	char tmp[2] = { c, 0 };
	textbox_insert(tb, tb->cursor, tmp);
	textbox_cursor_inc(tb);
}

// delete on character
void textbox_cursor_del(Textbox *tb)
{
	textbox_delete(tb, tb->cursor, 1);
}

// back up and delete one character
void textbox_cursor_bkspc(Textbox *tb)
{
	if (tb->cursor > 0)
	{
		textbox_cursor_dec(tb);
		textbox_cursor_del(tb);
	}
}

// handle a keypress in edit mode
// 0 = unhandled
// 1 = handled
// -1 = handled and return pressed (finished)
int textbox_keypress(Textbox *tb, XEvent *ev)
{
	if (!(tb->flags & TB_EDITABLE)) return 0;

	KeySym key; Status stat; char pad[32];

	int len = XmbLookupString(tb->xic, &ev->xkey, pad, sizeof(pad), &key, &stat);
	pad[len] = 0;

	switch (key)
	{
		case XK_Left      : textbox_cursor_dec(tb);   return 1;
		case XK_Right     : textbox_cursor_inc(tb);   return 1;
		case XK_Home      : textbox_cursor_home(tb);  return 1;
		case XK_End       : textbox_cursor_end(tb);   return 1;
		case XK_Delete    : textbox_cursor_del(tb);   return 1;
		case XK_BackSpace : textbox_cursor_bkspc(tb); return 1;
		case XK_Return    : return -1;
		default:
			if (!iscntrl(*pad))
			{
				textbox_insert(tb, tb->cursor, pad);
				textbox_cursor_inc(tb);
				return 1;
			}
	}
	return 0;
}
