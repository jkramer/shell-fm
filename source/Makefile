MAIN	:= main.c
SOURCE	:= $(filter-out $(MAIN),$(wildcard *.c))
OBJECT	:= $(subst .c,.o,$(SOURCE))
BINARY	:= shell-fm
LIB	:= libshellfm.so
STATIC	:= libshellfm.a

ifeq ($(shell uname -s), OpenBSD)
	LDFLAGS += -lossaudio
endif
ifeq ($(shell uname -s), NetBSD)
	LDFLAGS += -lossaudio
endif

CFLAGS  += -Os -Wall -W -I./include/ -DLIBAO \
	   $(shell pkg-config --cflags mad) \
	   $(shell pkg-config --cflags ao)
LDFLAGS += $(shell pkg-config --libs mad) \
	   $(shell pkg-config --libs ao)

.PHONY: clean tags cscope

all	: shell-fm

objects	: $(SOURCE)
	$(CC) $(CFLAGS) -fPIC -c $(SOURCE)	

$(LIB)	: $(OBJECT)
	$(CC) -shared -Wl,-soname,$(LIB) -o $(LIB) $(OBJECT) 

$(STATIC) : $(OBJECT)
	$(AR) -cvq $(STATIC) $(OBJECT)

$(BINARY)	: $(STATIC)
	$(CC) -o $(BINARY) $(CFLAGS) $(MAIN) $(LDFLAGS) $(STATIC)

clean		:
	rm -f $(OBJECT) $(BINARY) $(LIB) $(STATIC)

tags cscope	:
	$(MAKE) -C .. $@
