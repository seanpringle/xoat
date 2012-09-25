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

void action_move(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	client_place_spot(cli, num, cli->monitor, 1);
}

void action_move_direction(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	client_place_spot(cli, spot_choose_by_direction(cli->spot, cli->monitor, num), cli->monitor, 1);
}

void action_focus(void *data, int num, client *cli)
{
	spot_focus_top_window(num, current_mon, None);
}

void action_focus_direction(void *data, int num, client *cli)
{
	spot_focus_top_window(spot_choose_by_direction(current_spot, current_mon, num), current_mon, None);
}

void action_close(void *data, int num, client *cli)
{
	if (cli && !client_send_wm_protocol(cli, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, cli->window);
}

void action_cycle(void *data, int num, client *cli)
{
	if (!cli) return;
	STACK_INIT(lower);
	spot_focus_top_window(cli->spot, cli->monitor, cli->window);
	client_stack_family(cli, &lower);
	XLowerWindow(display, lower.windows[0]);
	XRestackWindows(display, lower.windows, lower.depth);
}

void action_raise_nth(void *data, int num, client *cli)
{
	if (!cli) return;
	int i, n = 0; client *c;
	for_windows(i, c) if (c->manage && c->spot == cli->spot && c->monitor == cli->monitor && num == n++)
		{ client_activate(c); break; }
}

void action_command(void *data, int num, client *cli)
{
	exec_cmd(data);
}

void action_find_or_start(void *data, int num, client *cli)
{
	int i; client *c; char *class = data;
	for_windows(i, c)
		if (c->visible && c->manage && c->class && !strcasecmp(c->class, class))
			{ client_activate(c); return; }
	exec_cmd(class);
}

void action_move_monitor(void *data, int num, client *cli)
{
	if (!cli) return;
	client_raise_family(cli);
	cli->monitor = MAX(0, MIN(current_mon+num, nmonitors-1));
	client_place_spot(cli, cli->spot, cli->monitor, 1);
	current_mon = cli->monitor;
}

void action_focus_monitor(void *data, int num, client *cli)
{
	int i, mon = MAX(0, MIN(current_mon+num, nmonitors-1));
	if (spot_focus_top_window(current_spot, mon, None)) return;
	for_spots(i) if (spot_focus_top_window(i, mon, None)) break;
}

void action_fullscreen(void *data, int num, client *cli)
{
	if (!cli) return;
	if (cli->full && GETPROP_LONG(cli->window, atoms[XOAT_SPOT], &cli->spot, 1));
	else SETPROP_LONG(cli->window, atoms[XOAT_SPOT], &cli->spot, 1);
	client_toggle_state(cli, atoms[_NET_WM_STATE_FULLSCREEN]);
	client_place_spot(cli, cli->full ? SPOT1: cli->spot, cli->monitor, 1);
	client_update_border(cli);
	client_raise_family(cli);
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
	int i; client *c;
	STACK_FREE(&snapshot);
	for_windows(i, c) if (c->manage && c->class)
		snapshot.clients[snapshot.depth++] = window_build_client(c->window);
}

void action_rollback(void *data, int num, client *cli)
{
	int i; client *c = NULL, *s, *a = NULL;
	for (i = snapshot.depth-1; i > -1; i--)
	{
		if ((s = snapshot.clients[i]) && (c = window_build_client(s->window))
			&& c->class && !strcmp(s->class, c->class) && c->visible && c->manage)
		{
			client_place_spot(c, s->spot, s->monitor, 1);
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

