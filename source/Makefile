
SOURCE	:= $(wildcard *.c)
OBJECT	:= $(subst .c,.o,$(SOURCE))
BINARY	:= shell-fm
override LDFLAGS += $(shell pkg-config --exists ao && pkg-config ao --libs)
LIBAO	:= $(shell pkg-config --exists ao && echo -DLIBAO)
override CFLAGS += -Os -Wall -W -I./include/ $(LIBAO)

.PHONY: clean

shell-fm	: $(OBJECT)
	gcc -o $(BINARY) $(CFLAGS) $(OBJECT) $(shell pkg-config mad --libs) $(LDFLAGS)

clean		:
	rm -f $(OBJECT) $(BINARY)
