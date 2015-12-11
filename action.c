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

void action_move(void *data, int num, Client *cli)
{
	if (!cli) return;
	client_place_spot(cli, num, cli->monitor, 1);
	client_raise_family(cli);
	current_spot = cli->spot; current_mon = cli->monitor;
	spot_warp_pointer(cli->spot, cli->monitor);
}

void action_move_direction(void *data, int num, Client *cli)
{
	if (!cli) return;
	client_place_spot(cli, spot_choose_by_direction(cli->spot, cli->monitor, num), cli->monitor, 1);
	client_raise_family(cli);
	current_spot = cli->spot; current_mon = cli->monitor;
	spot_warp_pointer(cli->spot, cli->monitor);
}

void action_focus(void *data, int num, Client *cli)
{
	spot_try_focus_top_window(num, current_mon, None);
	spot_warp_pointer(current_spot, current_mon);
}

void action_focus_direction(void *data, int num, Client *cli)
{
	spot_try_focus_top_window(spot_choose_by_direction(current_spot, current_mon, num), current_mon, None);
	spot_warp_pointer(current_spot, current_mon);
}

void action_close(void *data, int num, Client *cli)
{
	if (cli && !client_send_wm_protocol(cli, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, cli->window);
}

void action_cycle(void *data, int num, Client *cli)
{
	if (!cli) return;
	STACK_INIT(order);
	if (spot_stack_clients(cli->spot, cli->monitor, &order) > 1)
	{
		int i; Client *c = NULL;
		for_stack_rev(&order, i, c) if (c->manage && c->transient == None && c->window != cli->window)
		{
			client_activate(c);
			break;
		}
	}
}

void action_raise_nth(void *data, int num, Client *cli)
{
	if (!cli) return;
	int i, n = 0; Client *c;
	for_windows(i, c) if (c->manage && c->spot == cli->spot && c->monitor == cli->monitor && num == n++)
		{ client_activate(c); break; }
}

void action_command(void *data, int num, Client *cli)
{
	exec_cmd(data);
}

void action_find_or_start(void *data, int num, Client *cli)
{
	int i; Client *c; char *class = data;
	for_windows(i, c)
		if (c->visible && c->manage && c->class && strcasestr(c->class, class))
			{ client_activate(c); spot_warp_pointer(c->spot, c->monitor); return; }
	exec_cmd(class);
}

void action_move_monitor(void *data, int num, Client *cli)
{
	if (!cli) return;
	cli->monitor = MAX(0, MIN(current_mon+num, nmonitors-1));
	client_place_spot(cli, cli->spot, cli->monitor, 1);
	client_raise_family(cli);
	current_mon = cli->monitor;
	spot_warp_pointer(cli->spot, cli->monitor);
}

void action_focus_monitor(void *data, int num, Client *cli)
{
	int i, mon = MAX(0, MIN(current_mon+num, nmonitors-1));
	if (!spot_focus_top_window(current_spot, mon, None))
		for_spots(i) if (spot_focus_top_window(i, mon, None)) break;
	spot_warp_pointer(current_spot, current_mon);
}

void action_fullscreen(void *data, int num, Client *cli)
{
	if (!cli) return;

	unsigned long spot;
	if (cli->full && GETPROP_LONG(cli->window, atoms[XOAT_SPOT], &spot, 1))
		cli->spot = spot;

	SETPROP_LONG(cli->window, atoms[XOAT_SPOT], &cli->spot, 1);
	client_toggle_state(cli, atoms[_NET_WM_STATE_FULLSCREEN]);
	client_place_spot(cli, cli->full ? SPOT1: cli->spot, cli->monitor, 1);
	client_update_border(cli);
	client_raise_family(cli);
}

void action_maximize(void *data, int num, Client *cli)
{
	if (!cli) return;
	cli->max = !cli->max;
	SETPROP_LONG(cli->window, atoms[XOAT_MAXIMIZE], &cli->max, 1);
	client_place_spot(cli, cli->spot, cli->monitor, 1);
}

void action_maximize_vert(void *data, int num, Client *cli)
{
	if (!cli) return;
	cli->maxv = client_toggle_state(cli, atoms[_NET_WM_STATE_MAXIMIZE_VERT]);
	client_place_spot(cli, cli->spot, cli->monitor, 1);
}

void action_maximize_horz(void *data, int num, Client *cli)
{
	if (!cli) return;
	cli->maxh = client_toggle_state(cli, atoms[_NET_WM_STATE_MAXIMIZE_HORZ]);
	client_place_spot(cli, cli->spot, cli->monitor, 1);
}

void action_above(void *data, int num, Client *cli)
{
	if (!cli) return;
	cli->above = client_toggle_state(cli, atoms[_NET_WM_STATE_ABOVE]);
	client_place_spot(cli, cli->spot, cli->monitor, 1);
}

void action_menu(void *data, int num, Client *cli)
{
	menu_create(current_spot, current_mon);
}
