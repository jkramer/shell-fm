# vim:ts=8 noexpandtab

BINPATH = "/usr/local/bin"

CC	:= /usr/bin/gcc
CODE	:=\
	hash.c\
	http.c\
	interface.c\
	main.c\
	play.c\
	radio.c\
	ropen.c\
	service.c\
	settings.c\
	autoban.c\
	getln.c\
	sckif.c\
	split.c\
	bookmark.c\
	tag.c
OBJS    = $(CODE:%.c=%.o)
OUTPUT	:= shell-fm

CFLAGS	= -Wall -W -std=c99 -Iinclude
LIBS    = -lcrypto -lmad -lreadline -lcurses

### detect libao

AO_CFLAGS = $(shell pkg-config --silence-errors ao --cflags)
AO_LIBS   = $(shell pkg-config --silence-errors ao --libs)

ifneq ($(AO_LIBS),)
CFLAGS    += $(AO_CFLAGS) -D__HAVE_LIBAO__
LIBS      += $(AO_LIBS)
endif

### debug version

ifeq ($(DEBUG),)
CFLAGS    += -DNDEBUG -Os
else
CFLAGS    += -g
endif

### building

.PHONY: all clean distclean depend 
all: depend $(OUTPUT)
	
$(OUTPUT): $(OBJS) Makefile
	$(CC) -o $@ $(LIBS) $(OBJS)
ifeq ($(DEBUG),)
	/usr/bin/strip $@
endif

$(OBJS): %.o: %.c .%.c.dep
	$(CC) -o $@ $(CFLAGS) -c $<

install: $(all)
	mkdir -p $(BINPATH)
	install -m 755 $(OUTPUT) $(BINPATH)

### ctags and cscope

.PHONY: tags cscope
tags: cscope.files
	ctags `cat cscope.files`

cscope: cscope.files
	cscope -b

cscope.files: $(wildcard *.h *.c)
	find . -name '*.h' -o -name '*.c' > cscope.files

### dependencies

DEPS=$(CODE:%.c=.%.c.dep)
EXISTING_DEPS=$(wildcard $(DEPS))

depend: $(DEPS) Makefile

$(DEPS): .%.c.dep: %.c
	$(CC) -MM $(CFLAGS) $< > $@

include $(EXISTING_DEPS)

### cleanup

clean:
	-rm -f *.o $(OUTPUT)

distclean: clean
	-rm -f tags cscope.out cscope.files
	-find . \( -name '*~' -o -name '.*.c.dep' \) -exec rm -f {} \;

