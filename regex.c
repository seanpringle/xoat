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

#define REGEX_CACHE 8
#define REGEX_GROUPS 8

typedef struct {
	unsigned int hash;
	char *pattern;
	regex_t regex;
	unsigned int used;
} RegexCache;

RegexCache regex_cache[REGEX_CACHE];

regex_t*
regex(const char *pattern)
{
	int i = 0;
	unsigned int hash = 0;
	regex_t *re = NULL;
	RegexCache *rc = NULL;

	while (pattern[i])
	{
		hash = 33 * hash + pattern[i++];
	}

	// have we compiled this regex before?
	for (i = 0; i < REGEX_CACHE; i++)
	{
		rc = &regex_cache[i];
		if (rc->hash == hash && rc->pattern && !strcmp(pattern, rc->pattern))
		{
			re = &rc->regex;
			rc->used++;
			break;
		}
	}
	if (!re)
	{
		int slot = 0;
		// look for a free cache slot
		for (i = 0; i < REGEX_CACHE; i++)
		{
			if (!regex_cache[i].pattern)
			{
				slot = i;
				break;
			}
		}
		if (regex_cache[slot].pattern)
		{
			// choose the least used
			for (i = 0; i < REGEX_CACHE; i++)
			{
				if (regex_cache[i].used < regex_cache[slot].used)
				{
					slot = i;
					break;
				}
			}
		}

		rc = &regex_cache[slot];

		if (rc->pattern)
		{
			regfree(&rc->regex);
			free(rc->pattern);
		}

		rc->used = 0;
		rc->hash = 0;
		rc->pattern = NULL;

		re = &rc->regex;

		if (regcomp(re, pattern, REG_EXTENDED) != 0)
		{
			fprintf(stderr, "regex compile failure: %s", pattern);
			re = NULL;
		}
		if (re)
		{
			rc->pattern = strdup(pattern);
			rc->hash = hash;
			rc->used = 1;
		}
	}
	return re;
}

void
flush_regex_cache()
{
	for (int i = 0; i < REGEX_CACHE; i++)
	{
		RegexCache *rc = &regex_cache[i];
		if (rc->pattern)
		{
			regfree(&rc->regex);
			free(rc->pattern);
			rc->used = 0;
			rc->hash = 0;
			rc->pattern = NULL;
		}
	}
}

char *regex_matches[REGEX_GROUPS];

int
regex_match(const char *pattern, const char *subject)
{
	int r = 0; regmatch_t pmatch[REGEX_GROUPS];
	regex_t *re = regex(pattern);
	if (re && subject && (r = regexec(re, subject, REGEX_GROUPS, pmatch, 0) == 0))
	{
		for (int i = 0; i < REGEX_GROUPS && pmatch[i].rm_so != (size_t)-1; i++)
		{
			free(regex_matches[i]);
			int len = pmatch[i].rm_eo - pmatch[i].rm_so;
			regex_matches[i] = malloc(len + 1);
			strncpy(regex_matches[i], subject + pmatch[i].rm_so, len);
			regex_matches[i][len] = 0;
		}
	}
	return r;
}

int
regex_split(char *pattern, char **subject)
{
	int r = 0; regmatch_t pmatch;
	regex_t *re = regex(pattern);
	if (re && *subject)
	{
		r = regexec(re, *subject, 1, &pmatch, 0) == 0 ?-1:0;
		if (r)
		{
			memset(*subject + pmatch.rm_so, 0, pmatch.rm_eo - pmatch.rm_so);
			*subject += pmatch.rm_eo;
		}
		else
		{
			*subject += strlen(*subject);
		}
	}
	return r;
}
