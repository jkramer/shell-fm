
SOURCE	:= $(wildcard *.c)
OBJECT	:= $(subst .c,.o,$(SOURCE))
BINARY	:= shell-fm
LDFLAGS := $(shell pkg-config --exists ao && pkg-config ao --libs | sed -e 's/-lpthread//')
LIBAO	:= $(shell pkg-config --exists ao && echo -DLIBAO)
CFLAGS  := -Os -Wall -W -I./include/ $(LIBAO) $(shell pkg-config mad --cflags)

.PHONY: clean tags cscope

shell-fm	: $(OBJECT)
	gcc -o $(BINARY) $(CFLAGS) $(OBJECT) $(shell pkg-config mad --libs) $(LDFLAGS)

clean		:
	rm -f $(OBJECT) $(BINARY)

tags cscope	:
	${MAKE} -C .. $@
