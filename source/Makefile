
SOURCE	:= $(wildcard *.c)
OBJECT	:= $(subst .c,.o,$(SOURCE))
BINARY	:= shell-fm
LIBAO	:= $(shell [ -f /usr/lib/pkgconfig/ao.pc ] && pkg-config ao --cflags && echo "-DLIBAO" )
LDFLAGS	:= $(shell [ -f /usr/lib/pkgconfig/ao.pc ] && pkg-config ao --libs)
CFLAGS	:= -Os -Wall -W -I./include/ $(LIBAO)

.PHONY: clean

shell-fm	: $(OBJECT)
	gcc -o $(BINARY) $(CFLAGS) $(OBJECT) $(shell pkg-config mad --libs) $(LDFLAGS)

clean		:
	rm -f $(OBJECT) $(BINARY)
