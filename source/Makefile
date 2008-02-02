
CFLAGS	:= -Os -Wall -W -I./include/
SOURCE	:= $(wildcard *.c)
OBJECT	:= $(shell ls *.c | sed -e 's/\.c/.o/')
BINARY	:= shell-fm
LIBAO	:= $(shell [ -f /usr/lib/pkgconfig/ao.pc ] && pkg-config ao --libs)

.PHONY:		clean

shell-fm	: $(OBJECT)
	gcc -o $(BINARY) $(CFLAGS) $(OBJECT) -l mad $(LIBAO)

clean	:
	rm -f $(OBJECT) $(BINARY)
