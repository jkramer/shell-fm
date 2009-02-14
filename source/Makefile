SOURCE	:= $(wildcard *.c)
OBJECT	:= $(subst .c,.o,$(SOURCE))
BINARY	:= shell-fm
LDFLAGS := $(shell pkg-config --exists ao && pkg-config ao --libs) \
	$(shell pkg-config mad --libs)
ifeq ($(shell uname -s), OpenBSD)
	LDFLAGS += -lossaudio
endif
ifeq ($(shell uname -s), NetBSD)
	LDFLAGS += -lossaudio
endif
LIBAO	:= $(shell pkg-config --exists ao && echo -DLIBAO && pkg-config ao --cflags)
CFLAGS  := $(CFLAGS) -Os -Wall -W -I./include/ $(LIBAO) $(shell pkg-config mad --cflags)

.PHONY: clean tags cscope

shell-fm	: $(OBJECT)
	$(CC) -o $(BINARY) $(CFLAGS) $(OBJECT) $(LDFLAGS)

clean		:
	rm -f $(OBJECT) $(BINARY)

tags cscope	:
	$(MAKE) -C .. $@
