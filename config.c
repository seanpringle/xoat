/*

MIT/X11 License
Copyright (c) 2014 Sean Pringle <sean.pringle@gmail.com>

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

#define CONFIG_BORDER "border"
#define CONFIG_BORDER_BLUR "border_blur"
#define CONFIG_BORDER_FOCUS "border_focus"
#define CONFIG_BORDER_URGENT "border_urgent"
#define CONFIG_GAP "gap"
#define CONFIG_TITLE "title"
#define CONFIG_TITLE_BLUR "title_blur"
#define CONFIG_TITLE_FOCUS "title_focus"
#define CONFIG_TITLE_ELLIPSIS "title_ellipsis"
#define CONFIG_LAYOUTS "layouts"
#define CONFIG_MENU "menu"

#define CONFIG_UINT_NAMES CONFIG_BORDER "|" CONFIG_GAP "|" CONFIG_TITLE_ELLIPSIS
#define CONFIG_STR_NAMES CONFIG_BORDER_BLUR "|" CONFIG_BORDER_FOCUS "|" CONFIG_BORDER_URGENT "|" CONFIG_TITLE "|" \
	CONFIG_TITLE_BLUR "|" CONFIG_TITLE_FOCUS "|" CONFIG_MENU

#define CONFIG_ACTIONS "action_move_direction|action_move_monitor|action_focus_monitor|action_focus_direction" \
	"|action_move|action_focus|action_close|action_cycle|action_raise_nth|action_command|action_find_or_start" \
	"|action_fullscreen|action_maximize_vert|action_maximize_horz|action_maximize|action_above|action_menu"

void
rtrim(char *str)
{
	char *end = str + strlen(str);
	while (end && --end >= str && isspace(*end)) *end = 0;
}

void
ltrim(char *str)
{
	while (isspace(*str)) memmove(str, str+1, strlen(str+1)+1);
}

void
trim(char *str)
{
	ltrim(str);
	rtrim(str);
}

void
configure()
{
	unsigned int i;
	FILE *file = NULL;
	char tmp[1024];
	const char *home = getenv("HOME");

	regex_cache_init();

	settings.border         = BORDER;
	settings.border_blur    = strdup(BORDER_BLUR);
	settings.border_focus   = strdup(BORDER_FOCUS);
	settings.border_urgent  = strdup(BORDER_URGENT);
	settings.gap            = GAP;
	settings.title          = TITLE ? strdup(TITLE): NULL;
	settings.title_blur     = strdup(TITLE_BLUR);
	settings.title_focus    = strdup(TITLE_FOCUS);
	settings.title_ellipsis = TITLE_ELLIPSIS;
	settings.menu           = strdup(MENU);

	settings.layout_count = sizeof(layouts) / sizeof(Layout);
	settings.layouts = calloc(settings.layout_count, sizeof(Layout));

	for (i = 0; i < settings.layout_count; i++)
		memmove(&settings.layouts[i], &layouts[i], sizeof(Layout));

	settings.binding_count = sizeof(keys) / sizeof(Binding);
	settings.bindings = calloc(settings.binding_count, sizeof(Binding));

	settings.launchcmd_count = 0;
	settings.launchcmds = calloc(1, sizeof(char*));

	for (i = 0; i < settings.binding_count; i++)
		memmove(&settings.bindings[i], &keys[i], sizeof(Binding));

	snprintf(tmp, sizeof(tmp), "%s/.xoatrc", home);

	if (home && (file = fopen(tmp, "r")) && file)
	{
		while (fgets(tmp, sizeof(tmp), file))
		{
			if (regex_match("^[[:space:]]*(" CONFIG_UINT_NAMES ")[[:space:]]+([0-9]+)", tmp))
			{
				unsigned n = strtoul(regex_matches[2], NULL, 10);

				char *names[] = {
					CONFIG_BORDER,
					CONFIG_GAP,
					CONFIG_TITLE_ELLIPSIS,
				};
				unsigned int *values[] = {
					&settings.border,
					&settings.gap,
					&settings.title_ellipsis,
				};
				for (i = 0; i < sizeof(names) / sizeof(char*); i++)
				{
					if (strcmp(regex_matches[1], names[i]) == 0)
					{
						fprintf(stderr, "set [%s] to [%u]\n", names[i], n);
						*(values[i]) = n;
						break;
					}
				}
			}
			else
			if (regex_match("^[[:space:]]*(" CONFIG_STR_NAMES ")[[:space:]]+(.+)", tmp))
			{
				rtrim(regex_matches[2]);

				char *names[] = {
					CONFIG_BORDER_BLUR,
					CONFIG_BORDER_FOCUS,
					CONFIG_BORDER_URGENT,
					CONFIG_TITLE,
					CONFIG_TITLE_BLUR,
					CONFIG_TITLE_FOCUS,
					CONFIG_MENU,
				};
				char **values[] = {
					&settings.border_blur,
					&settings.border_focus,
					&settings.border_urgent,
					&settings.title,
					&settings.title_blur,
					&settings.title_focus,
					&settings.menu,
				};
				for (i = 0; i < sizeof(names) / sizeof(char*); i++)
				{
					if (strcmp(regex_matches[1], names[i]) == 0)
					{
						fprintf(stderr, "set [%s] to [%s]\n", names[i], regex_matches[2]);
						free(*(values[i]));
						*(values[i]) = strdup(regex_matches[2]);
						break;
					}
				}
			}
			else
			if (regex_match("^[[:space:]]*(layouts)[[:space:]]+([1-9])", tmp))
			{
				unsigned int n = strtoul(regex_matches[2], NULL, 10);
				fprintf(stderr, "using [%u] monitor layout%s\n", n, n > 1 ? "s":"");

				free(settings.layouts);
				settings.layout_count = n;
				settings.layouts = calloc(settings.layout_count, sizeof(Layout));

				for (i = 0; i < settings.layout_count && i < sizeof(layouts) / sizeof(Layout); i++)
					memmove(&settings.layouts[i], &layouts[i], sizeof(Layout));
			}
			else
			if (regex_match("^[[:space:]]*layout[[:space:]]([0-9])(.+)", tmp))
			{
				unsigned int n = strtoul(regex_matches[1], NULL, 10);
				if (!have_layout(n))
				{
					for (i = settings.layout_count; i < n+1; i++)
					{
						settings.layout_count++;
						settings.layouts = realloc(settings.layouts, sizeof(Layout) * settings.layout_count);
						memmove(&settings.layouts[i], &settings.layouts[0], sizeof(Layout));
						fprintf(stderr, "creating layout [%u]\n", i);
					}
				}

				snprintf(tmp, sizeof(tmp), "%s", regex_matches[2]);

				if (regex_match("^[[:space:]]spot_start[[:space:]]+(smart|current|spot1|spot2|spot3)", tmp))
				{
					char *names[] = {
						"smart",
						"current",
						"spot1",
						"spot2",
						"spot3",
					};
					short values[] = {
						SMART,
						CURRENT,
						SPOT1,
						SPOT2,
						SPOT3,
					};
					for (i = 0; i < sizeof(names) / sizeof(char*); i++)
					{
						if (strcmp(regex_matches[1], names[i]) == 0)
						{
							fprintf(stderr, "layout [%u] spot_start [%s]\n", n, regex_matches[1]);
							settings.layouts[n].spot_start = values[i];
							break;
						}
					}
				}
				else
				if (regex_match("^[[:space:]]spot1_align[[:space:]]+(left|right)", tmp))
				{
					char *names[] = {
						"left",
						"right",
					};
					short values[] = {
						LEFT,
						RIGHT,
					};
					for (i = 0; i < sizeof(names) / sizeof(char*); i++)
					{
						if (strcmp(regex_matches[1], names[i]) == 0)
						{
							fprintf(stderr, "layout [%u] spot1_align [%s]\n", n, regex_matches[1]);
							settings.layouts[n].spot1_align = values[i];
							break;
						}
					}
				}
				else
				if (regex_match("^[[:space:]]spot1_width_pct[[:space:]]+([0-9]+)", tmp))
				{
					unsigned int w = strtoul(regex_matches[1], NULL, 10);
					if (w < 50 || w > 90)
					{
						fprintf(stderr, "layout [%u] spot1_width_pct out of bounds [%u]!\n", n, w);
						continue;
					}
					fprintf(stderr, "layout [%u] spot1_width_pct [%u%%]\n", n, w);
					settings.layouts[n].spot1_width_pct = w;
				}
				else
				if (regex_match("^[[:space:]]spot2_height_pct[[:space:]]+([0-9]+)", tmp))
				{
					unsigned int h = strtoul(regex_matches[1], NULL, 10);
					if (h < 50 || h > 90)
					{
						fprintf(stderr, "layout [%u] spot2_height_pct out of bounds [%u]!\n", n, h);
						continue;
					}
					fprintf(stderr, "layout [%u] spot2_height_pct [%u%%]\n", n, h);
					settings.layouts[n].spot2_height_pct = h;
				}
			}
			else
			if (regex_match("^[[:space:]]*bind[[:space:]]+([^[:space:]]+)[[:space:]]+(" CONFIG_ACTIONS ")(.*)", tmp))
			{
				unsigned int modifier = 0;
				char *str = regex_matches[1];
				fprintf(stderr, "binding");

				while (*str)
				{
					if ((strncmp(str, "Mod1" , 4)) == 0)
					{
						modifier |= Mod1Mask;
						fprintf(stderr, " [Mod1]");
						str += 4;
					}
					else
					if ((strncmp(str, "Mod2" , 4)) == 0)
					{
						modifier |= Mod2Mask;
						fprintf(stderr, " [Mod2]");
						str += 4;
					}
					else
					if ((strncmp(str, "Mod3" , 4)) == 0)
					{
						modifier |= Mod3Mask;
						fprintf(stderr, " [Mod3]");
						str += 4;
					}
					else
					if ((strncmp(str, "Mod4" , 4)) == 0)
					{
						modifier |= Mod4Mask;
						fprintf(stderr, " [Mod4]");
						str += 4;
					}
					else
					if ((strncmp(str, "Mod5" , 4)) == 0)
					{
						modifier |= Mod5Mask;
						fprintf(stderr, " [Mod5]");
						str += 4;
					}
					else
					if ((strncmp(str, "Shift", 5)) == 0)
					{
						modifier |= ShiftMask;
						fprintf(stderr, " [Shift]");
						str += 5;
					}
					else
					if ((strncmp(str, "Control", 7)) == 0)
					{
						modifier |= ControlMask;
						fprintf(stderr, " [Control]");
						str += 7;
					}
					else
					{
						break;
					}
					while (*str == '+') str++;
				}

				if (modifier == 0)
				{
					fprintf(stderr, " AnyModifier");
					modifier = AnyModifier;
				}

				if (!*str)
				{
					fprintf(stderr, " malformed key binding [%s]\n", regex_matches[1]);
					continue;
				}

				KeySym keysym = strncmp("0x", str, 2) == 0 ? strtoul(str, NULL, 16): XStringToKeysym(str);

				if (keysym == NoSymbol)
				{
					fprintf(stderr, " unknown key symbol [%s]\n", str);
					continue;
				}

				fprintf(stderr, " [%s]", str);

				int num = 0;
				char *data = NULL;
				void *action = NULL;
				trim(regex_matches[3]);

				char *names[] = {
					"action_move",
					"action_focus",
					"action_move_direction",
					"action_focus_direction",
					"action_close",
					"action_cycle",
					"action_raise_nth",
					"action_command",
					"action_find_or_start",
					"action_move_monitor",
					"action_focus_monitor",
					"action_fullscreen",
					"action_maximize_vert",
					"action_maximize_horz",
					"action_maximize",
					"action_above",
					"action_menu",
				};
				void *actions[] = {
					action_move,
					action_focus,
					action_move_direction,
					action_focus_direction,
					action_close,
					action_cycle,
					action_raise_nth,
					action_command,
					action_find_or_start,
					action_move_monitor,
					action_focus_monitor,
					action_fullscreen,
					action_maximize_vert,
					action_maximize_horz,
					action_maximize,
					action_above,
					action_menu,
				};
				for (i = 0; i < sizeof(names) / sizeof(char*); i++)
				{
					if (strcmp(names[i], regex_matches[2]) == 0)
					{
						action = actions[i];
						fprintf(stderr, " to [%s %s]\n", regex_matches[2], regex_matches[3]);
						break;
					}
				}
				if ((0 == strcmp(regex_matches[2], "action_move")) || 0 == strcmp(regex_matches[2], "action_focus"))
				{
					if (strcmp(regex_matches[3], "spot1") == 0) num = SPOT1;
					if (strcmp(regex_matches[3], "spot2") == 0) num = SPOT2;
					if (strcmp(regex_matches[3], "spot3") == 0) num = SPOT3;
				}
				else
				if (0 == strcmp(regex_matches[2], "action_move_direction") || 0 == strcmp(regex_matches[2], "action_focus_direction"))
				{
					if (strcmp(regex_matches[3], "left" ) == 0) num = LEFT;
					if (strcmp(regex_matches[3], "right") == 0) num = RIGHT;
					if (strcmp(regex_matches[3], "up"   ) == 0) num = UP;
					if (strcmp(regex_matches[3], "down" ) == 0) num = DOWN;
				}
				else
				if (0 == strcmp(regex_matches[2], "action_command") || 0 == strcmp(regex_matches[2], "action_find_or_start"))
				{
					data = regex_matches[3];
				}
				else
				if (0 == strcmp(regex_matches[2], "action_raise_nth") || 0 == strcmp(regex_matches[2], "action_move_monitor") || 0 == strcmp(regex_matches[2], "action_focus_monitor"))
				{
					num = strtol(regex_matches[3], NULL, 10);
				}
				for (i = 0; i < settings.binding_count; i++)
				{
					if (settings.bindings[i].mod == modifier && settings.bindings[i].key == keysym)
					{
						settings.bindings[i].act  = action;
						settings.bindings[i].data = data ? strdup(data): NULL;
						settings.bindings[i].num  = num;
						break;
					}
				}
				if (i == settings.binding_count)
				{
					settings.binding_count++;
					settings.bindings = realloc(settings.bindings, sizeof(Binding) * settings.binding_count);
					settings.bindings[i].mod  = modifier;
					settings.bindings[i].key  = keysym;
					settings.bindings[i].act  = action;
					settings.bindings[i].data = data ? strdup(data): NULL;
					settings.bindings[i].num  = num;
				}
			}
			else
			if (regex_match("^launch[[:space:]]+(.+)$", tmp))
			{
				rtrim(regex_matches[1]);

				settings.launchcmd_count++;
				settings.launchcmds = realloc(settings.launchcmds, sizeof(char*) * settings.launchcmd_count);
				settings.launchcmds[settings.launchcmd_count-1] = strdup(regex_matches[1]);
				fprintf(stderr, "launching [%s]\n", regex_matches[1]);
			}
		}
		fclose(file);

		regex_cache_empty();

		if (settings.title && strcmp(settings.title, "null") == 0)
		{
			free(settings.title);
			settings.title = NULL;
		}
	}
}
