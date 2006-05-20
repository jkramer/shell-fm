/*
	vim:syntax=c tabstop=2 shiftwidth=2
	
	Shell.FM - sckif.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

extern unsigned getln(char **, unsigned *, FILE *);

static FILE * unxsck = NULL;
static char * sckpath = NULL;

int mksckif(const char * path) {
	if(!path)
		return 0;

	unlink(path);
	
	if(mkfifo(path, 0600)) {
		fprintf(stderr, "Couldn't create FIFO %s. %s.\n", path, strerror(errno));
		return 0;
	}

	if(!(unxsck = fopen(path, "r"))) {
		fprintf(stderr, "Failed to open FIFO %s. %s.\n", path, strerror(errno));
		unlink(path);
		return 0;
	}

	sckpath = strdup(path);
	return !0;
}

void rmsckif(void) {
	if(unxsck)
		fclose(unxsck);

	if(sckpath) {
		unlink(sckpath);
		free(sckpath);
	}
}

void sckif(FILE * fd) {
	char * line = NULL, * ptr;
	unsigned size = 0, length;
	
	if(!(fd || (fd = unxsck))) {
		fputs("Nothing to read from.\n", stderr);
		return;
	}
	
	length = getln(& line, & size, fd);
	if(!size)
		return;

	(ptr = strchr(line, 10)) && (* ptr = 0);
	if(strnlen(line, length))
		/* execcmd(line) */ ;

	free(line);
}
