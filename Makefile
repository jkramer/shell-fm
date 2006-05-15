# vim:ts=8

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
	autoban.c
HEAD	:=\
	include/hash.h\
	include/http.h\
	include/interface.h\
	include/play.h\
	include/ropen.h\
	include/service.h\
	include/settings.h\
	include/autoban.h\
	include/version.h
CFLAGS	:= -Wall -W -pedantic -ansi -Os -lcrypto -lmad -lreadline -lncurses
OUTPUT	:= shell-fm
BACKUP	:= $(PWD)/../backup/shell-fm_`date +"%F_%T"`

all	: $(CODE) $(HEAD)
	$(CC) -o $(OUTPUT) $(CFLAGS) $(CODE) && /usr/bin/strip $(OUTPUT)

install	: $(all)
	mkdir -p /usr/local/bin
	install -m 755 shell-fm /usr/local/bin

backup	:;
	cp -r $(PWD) $(BACKUP)
