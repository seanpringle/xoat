CFLAGS?=-Wall -Os
LDADD?=$(shell pkg-config --cflags --libs x11 xinerama)

normal:
	$(CC) -o xoat xoat.c $(CFLAGS) $(LDADD) $(LDFLAGS)
	$(CC) -o xoat-debug xoat.c $(CFLAGS) -g $(LDADD) $(LDFLAGS)
	strip xoat

proto:
	cat *.c | egrep '^(void|int|char|unsigned|Window|client)' | sed -r 's/\)/);/' > proto.h

docs:
	pandoc -s -w man xoat.md -o xoat.1

clean:
	rm -f xoat xoat-debug

all: docs proto normal