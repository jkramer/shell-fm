/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - autoban.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/settings.h"

extern unsigned getln(char **, unsigned *, FILE *);

int banned(const char * artist) {
	FILE * fd;
	signed match = 0;
	char * line = NULL;
	unsigned int size = 0;
	
	if(!artist)
		return 0;

	if(!(fd = fopen(rcpath("autoban"), "r")))
		return 0;

	while(!feof(fd) && !match) {
		char * ptr;
		if(!getln(& line, & size, fd))
			continue;
	
		if(strlen(line) > 1) {
			(ptr = strrchr(line, 10)) && (* ptr = (char) 0);
			match = !strncasecmp(line, artist, strlen(line));
		}
	}

	if(line)
		free(line);

	fclose(fd);
	return match;
}

int autoban(const char * artist) {
	FILE * fd;
	const char * file = rcpath("autoban");

	if(!artist)
		return 0;
	
	if(!(fd = fopen(file, "a"))) {
		printf("Sorry, %s could not be written.\n", file);
		return 0;
	}

	fprintf(fd, "%s\n", artist);
	fclose(fd);

	return !0;
}
