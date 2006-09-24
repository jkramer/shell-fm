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
HEAD	:=\
	include/hash.h\
	include/http.h\
	include/interface.h\
	include/play.h\
	include/ropen.h\
	include/service.h\
	include/settings.h\
	include/autoban.h\
	include/version.h\
	include/sckif.h\
	include/split.h
CFLAGS	:= -Wall -W -pedantic -ansi -Os -lcrypto -lmad -lreadline -lncurses -ggdb
OUTPUT	:= shell-fm

.PHONY: all
all	: $(CODE) $(HEAD)
	$(CC) -o $(OUTPUT) $(CFLAGS) $(CODE) # && /usr/bin/strip $(OUTPUT)

ao	: $(CODE) $(HEAD)
	$(CC) -o $(OUTPUT) -D__HAVE_LIBAO__ $(CFLAGS) $(CODE) `pkg-config ao --cflags --libs` && /usr/bin/strip $(OUTPUT)

install	: $(all)
	mkdir -p $(BINPATH)
	install -m 755 shell-fm $(BINPATH)
