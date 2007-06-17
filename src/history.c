/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - history.c
	Copyright (C) 2007 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "settings.h"

extern unsigned getln(char **, unsigned *, FILE *);

void histapp(const char * radio) {
	FILE * fd;
	int match = 0;
	
	if(!radio)
		return;

	if(!strncasecmp("lastfm://", radio, 9))
		radio += 9;

	if((fd = fopen(rcpath("radio-history"), "r")) != NULL) {
		char * line = NULL;
		unsigned size = 0;

		while(!feof(fd)) {
			if(!getln(& line, & size, fd))
				continue;
		
			if(strlen(line) > 1) {
				char * ptr;
				(ptr = strrchr(line, 10)) && (* ptr = (char) 0);
				ptr = strncasecmp("lastfm://", line, 9) ? line : line + 9;
				match = !strncasecmp(line, radio, strlen(line));
			}
		}

		if(line)
			free(line);

		fclose(fd);
	}

	if(!match) {
		if((fd = fopen(rcpath("radio-history"), "a+")) != NULL) {
			fprintf(fd, "%s\n", radio);
			fclose(fd);
		}
	}

	return;
}
