CFLAGS?=-Wall -Os -std=c99
LDADD?=`pkg-config --cflags --libs x11 xinerama`

normal:
	$(CC) -o xoat xoat.c $(CFLAGS) $(LDADD) $(LDFLAGS)
	$(CC) -o xoat-debug xoat.c $(CFLAGS) -g $(LDADD) $(LDFLAGS)
	strip xoat

docs:
	pandoc -s -w man xoat.md -o xoat.1

clean:
	rm -f xoat xoat-debug

all: docs normal
