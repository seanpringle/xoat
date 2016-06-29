/*

MIT/X11 License
Copyright (c) 2016 Sean Pringle <sean.pringle@gmail.com>

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

int monitor_choose_by_direction(int cmon, int dir)
{
  int mon = cmon;
  int i; Monitor *m;
  if (dir == LEFT)
  {
    for_monitors(i, m)
      if (OVERLAP(m->y, m->h, monitors[cmon].y, monitors[cmon].h) && m->x < monitors[cmon].x && (mon == cmon || m->x >= monitors[mon].x))
        mon = i;
    if (mon == cmon) for_monitors(i, m)
      if (m->x < monitors[cmon].x && (mon == cmon || m->x >= monitors[mon].x))
        mon = i;
  }
  else
  if (dir == RIGHT)
  {
    for_monitors(i, m)
      if (OVERLAP(m->y, m->h, monitors[cmon].y, monitors[cmon].h) && m->x >= monitors[cmon].x + monitors[cmon].w && (mon == cmon || m->x <= monitors[mon].x))
        mon = i;
    if (mon == cmon) for_monitors(i, m)
      if (m->x >= monitors[cmon].x + monitors[cmon].w && (mon == cmon || m->x <= monitors[mon].x))
        mon = i;
  }
  else
  if (dir == UP)
  {
    for_monitors(i, m)
      if (OVERLAP(m->x, m->w, monitors[cmon].x, monitors[cmon].w) && m->y < monitors[cmon].y && (mon == cmon || m->y >= monitors[mon].y))
        mon = i;
    if (mon == cmon) for_monitors(i, m)
      if (m->y < monitors[cmon].y && (mon == cmon || m->y >= monitors[mon].y))
        mon = i;
  }
  else
  if (dir == DOWN)
  {
    for_monitors(i, m)
      if (OVERLAP(m->x, m->w, monitors[cmon].x, monitors[cmon].w) && m->y >= monitors[cmon].y + monitors[cmon].h && (mon == cmon || m->y <= monitors[mon].y))
        mon = i;
    if (mon == cmon) for_monitors(i, m)
      if (m->y >= monitors[cmon].y + monitors[cmon].h && (mon == cmon || m->y <= monitors[mon].y))
        mon = i;
  }
  return mon;
}
