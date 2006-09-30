# vim:ts=8

BINPATH = "/usr/local/bin"

CC	:= /usr/bin/gcc
CODE	:=\
	hash.c\
	http.c\
	interface.c\
	main.c\
	play.c\
	ropen.c\
	service.c\
	settings.c\
	autoban.c\
	getln.c\
	sckif.c\
	split.c
OBJS    = $(CODE:%.c=%.o)
OUTPUT	:= shell-fm

CFLAGS	= -Wall -W -pedantic -ansi -Os 
LIBS    = -lcrypto -lmad -lreadline -lncurses

### detect libao

AO_CFLAGS = $(shell pkg-config --silence-errors ao --cflags)
AO_LIBS   = $(shell pkg-config --silence-errors ao --libs)

ifneq ($(AO_LIBS),)
CFLAGS    += $(AO_CFLAGS) -D__HAVE_LIBAO__
LIBS      += $(AO_LIBS)
endif

### building

.PHONY: all clean distclean depend
all: depend $(OUTPUT)
	
$(OUTPUT): $(OBJS) Makefile
	$(CC) -o $@ $(LIBS) $(OBJS)
	/usr/bin/strip $@

$(OBJS): %.o: %.c .%.c.dep
	$(CC) -o $@ $(CFLAGS) -c $<

install: $(all)
	mkdir -p $(BINPATH)
	install -m 755 $(OUTPUT) $(BINPATH)

### dependencies

DEPS=$(CODE:%.c=.%.c.dep)
EXISTING_DEPS=$(wildcard $(DEPS))

depend: $(DEPS) Makefile

$(DEPS): .%.c.dep: %.c
	$(CC) -MM $< > $@

include $(EXISTING_DEPS)

### cleanup

clean:
	-rm -f *.o $(OUTPUT)

distclean: clean
	-rm -f *~ .*.c.dep
