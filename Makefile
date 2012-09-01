CFLAGS?=-Wall -O2 -g
LDADD?=$(shell pkg-config --cflags --libs x11 xinerama)

normal:
	$(CC) -o xoat xoat.c $(CFLAGS) $(LDADD) $(LDFLAGS)

proto:
	cat *.c | egrep '^(void|int|char|unsigned|Window|client)' | sed -r 's/\)/);/' > proto.h

docs:
	pandoc -s -w man xoat.md -o xoat.1

clean:
	rm -f xoat

all: docs proto normal