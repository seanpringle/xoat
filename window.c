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

int window_get_prop(Window w, Atom prop, Atom *type, int *items, void *buffer, int bytes)
{
	memset(buffer, 0, bytes);
	int format; unsigned long nitems, nbytes; unsigned char *ret = NULL;
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

// retrieve a text property from a window
// technically we could use window_get_prop(), but this is better for character set support
char* window_get_text_prop(Window w, Atom atom)
{
	XTextProperty prop; char *res = NULL;
	char **list = NULL; int count;
	if (XGetTextProperty(display, w, &prop, atom) && prop.value && prop.nitems)
	{
		if (prop.encoding == XA_STRING)
		{
			res = malloc(strlen((char*)prop.value)+1);
			strcpy(res, (char*)prop.value);
		}
		else
		if (XmbTextPropertyToTextList(display, &prop, &list, &count) >= Success && count > 0 && *list)
		{
			res = malloc(strlen(*list)+1);
			strcpy(res, *list);
			XFreeStringList(list);
		}
	}
	if (prop.value) XFree(prop.value);
	return res;
}

Atom wgp_type; int wgp_items;

#define GETPROP_ATOM(w, a, l, c) (window_get_prop((w), (a), &wgp_type, &wgp_items, (l), (c)*sizeof(Atom))          && wgp_type == XA_ATOM     ? wgp_items:0)
#define GETPROP_LONG(w, a, l, c) (window_get_prop((w), (a), &wgp_type, &wgp_items, (l), (c)*sizeof(unsigned long)) && wgp_type == XA_CARDINAL ? wgp_items:0)
#define GETPROP_WIND(w, a, l, c) (window_get_prop((w), (a), &wgp_type, &wgp_items, (l), (c)*sizeof(Window))        && wgp_type == XA_WINDOW   ? wgp_items:0)

#define SETPROP_ATOM(w, p, a, c) XChangeProperty(display, (w), (p), XA_ATOM,     32, PropModeReplace, (unsigned char*)(a), (c))
#define SETPROP_LONG(w, p, a, c) XChangeProperty(display, (w), (p), XA_CARDINAL, 32, PropModeReplace, (unsigned char*)(a), (c))
#define SETPROP_WIND(w, p, a, c) XChangeProperty(display, (w), (p), XA_WINDOW,   32, PropModeReplace, (unsigned char*)(a), (c))

int window_send_clientmessage(Window target, Window subject, Atom atom, unsigned long protocol, unsigned long mask)
{
	XEvent e;
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

void window_listen(Window win)
{
	XSelectInput(display, win, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
}

// build windows cache
void query_windows()
{
	unsigned int nwins; int i; Window w1, w2, *wins; client *c;
	if (windows.depth || !(XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins))
		return;
	for (i = nwins-1; i > -1 && windows.depth < STACK; i--)
	{
		if ((c = window_build_client(wins[i])) && c->visible)
		{
			windows.clients[windows.depth] = c;
			windows.windows[windows.depth++] = wins[i];
		}
		else client_free(c);
	}
	XFree(wins);
}

