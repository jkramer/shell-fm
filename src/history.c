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
#include <assert.h>

#include "settings.h"
#include "history.h"
#include "getln.h"
#include "strary.h"

int grep(char **, char *);

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
				if((ptr = strrchr(line, 10)) != NULL)
					* ptr = (char) 0;
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


char ** slurp(const char * path) {
	char ** history = NULL;
	unsigned items = 0;
	FILE * fd;

	if((fd = fopen(path, "r")) != NULL) {
		char * line = NULL;
		unsigned size = 0;

		while(!feof(fd)) {
			if(!getln(& line, & size, fd))
				continue;

			if(strlen(line) > 1) {
				char * ptr;
				if((ptr = strrchr(line, 10)) != NULL)
					* ptr = (char) 0;

				if(strlen(line) > 0) {
					history = realloc(history, sizeof(char *) * (items + 2));
					history[items] = strdup(line);
					history[++items] = NULL;
				}
			}
		}

		if(line)
			free(line);

		fclose(fd);
	}

	return history;
}


char ** uniq(char ** list) {
	char ** uniqlist = NULL;

	if(list != NULL) {
		int size = 0, n = 0;

		while(list[n] != NULL) {
			if(grep(uniqlist, list[n])) {
				free(list[n]);
				list[n] = NULL;
			} else {
				uniqlist = realloc(uniqlist, (sizeof(char *)) * (size + 2));
				assert(uniqlist != NULL);
				uniqlist[size++] = list[n];
				uniqlist[size] = NULL;
			}

			++n;
		}

		free(list);
	}

	return uniqlist;
}


int grep(char ** list, char * needle) {
	register unsigned x = 0;
	
	if(!needle)
		return 0;

	if(list != NULL) {
		while(list[x] != NULL) {
			if(!strcmp(list[x], needle))
				return !0;
			++x;
		}
	}

	return 0;
}
