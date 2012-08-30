CFLAGS?=-Wall -O2
LDADD?=$(shell pkg-config --cflags --libs x11)

normal:
	$(CC) -o cerberus cerberus.c $(CFLAGS) $(LDADD) $(LDFLAGS)

proto:
	cat *.c | egrep '^(void|int|char|unsigned|Window|client)' | sed -r 's/\)/);/' > proto.h

clean:
	rm -f cerberus

all: proto normal