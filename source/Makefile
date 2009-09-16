MAIN	:= main.c
SOURCE	:= $(filter-out $(MAIN),$(wildcard *.c))
OBJECT	:= $(subst .c,.o,$(SOURCE))
DEP	:= $(subst .c,.d,$(SOURCE))
BINARY	:= shell-fm
LIB	:= libshellfm.so
STATIC	:= libshellfm.a
CFLAGS  += -Os -Wall -MD -W -I./include/

ifdef EXTERN_ONLY
	CFLAGS += -DEXTERN_ONLY
else
ifeq ($(shell uname -s), OpenBSD)
	LDFLAGS += -lossaudio
endif
ifeq ($(shell uname -s), NetBSD)
	LDFLAGS += -lossaudio
endif
ifeq ($(shell uname -s), Darwin)
	CFLAGS += -D__darwin__
endif

CFLAGS  += -DLIBAO \
	   $(shell pkg-config --cflags mad) \
	   $(shell pkg-config --cflags ao)
LDFLAGS += $(shell pkg-config --libs mad) \
	   $(shell pkg-config --libs ao)

ifeq ($(shell pkg-config --exists taglib_c && echo 1), 1)
	CFLAGS  += $(shell pkg-config --cflags taglib_c) -DTAGLIB
	LDFLAGS += $(shell pkg-config --libs taglib_c)
endif
endif

.PHONY: clean tags cscope

all	: $(BINARY)

$(LIB)	: $(OBJECT)
	$(CC) -shared -Wl,-soname,$(LIB) -o $(LIB) $(OBJECT)

$(STATIC) : $(OBJECT)
	$(AR) -cvq $(STATIC) $(OBJECT)

$(BINARY)	: $(STATIC) $(MAIN)
	$(CC) -o $(BINARY) $(CFLAGS) $(MAIN) $(LDFLAGS) $(STATIC)

clean		:
	rm -f $(OBJECT) $(BINARY) $(LIB) $(STATIC) $(DEP)

tags cscope	:
	$(MAKE) -C .. $@

sinclude $(DEP)
