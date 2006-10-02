/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - bookmark.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "include/settings.h"

extern unsigned getln(char **, unsigned *, FILE *);

char *strstrip(char *s)
{
	size_t size;
	char *end;

	size = strlen(s);

	if (!size)
		return s;

	end = s + size - 1;
	while (end != s && isspace(*end))
		end--;
	*(end + 1) = '\0';

	while (*s && isspace(*s))
		s++;

	return s;
}

void setmark(const char * streamURL, int n) {
	FILE * fd;
	char * bookmarks[10] = { NULL, };
	int index = 0;

	if(!streamURL || n < 0 || n > 9)
		return;
	
	if((fd = fopen(rcpath("bookmarks"), "r"))) {
		while(!feof(fd)) {
			char * line = NULL;
			unsigned size = 0, length;
			int x = 0;

			length = getln(& line, & size, fd);
			if(line && length > 4) {
				if(sscanf(line, "%d = ", & x) == 1 && !bookmarks[x])
					bookmarks[x] = strdup(line + 4);
				free(line);
			}
		}

		fclose(fd);
	}

	/* check for duplicates */
	for (index=0; index < 10; index++) {
		if (!bookmarks[index])
			continue;
		
		if (!strcmp(strstrip(streamURL), strstrip(bookmarks[index]))) {
			printf("Stream already saved as number %d\n", index);
			return ;
		}
	}

	bookmarks[n] = strdup(streamURL);

	if((fd = fopen(rcpath("bookmarks"), "w"))) {
		int i;
		for(i = 0; i < 10; ++i)
			if(bookmarks[i]) {
				char * ptr = strchr(bookmarks[i], 10);
				(ptr) && (* ptr = 0);
				fprintf(fd, "%d = %s\n", i, bookmarks[i]);
				free(bookmarks[i]);
			}

		fclose(fd);
	}
}

char * getmark(int n) {
	FILE * fd = fopen(rcpath("bookmarks"), "r");
	char * streamURL = NULL;

	if(!fd)
		return NULL;

	while(!streamURL && !feof(fd)) {
		char * line = NULL;
		unsigned size = 0, length;
		int x;

		length = getln(& line, & size, fd);
		if(line && length > 4)
			if(sscanf(line, "%d = ", & x) == 1 && x == n) {
				char * ptr = strchr(line, 10);
				(ptr) && (* ptr = 0);
				streamURL = strdup(line + 4);
			}

		if(size && line)
			free(line);
	}

	fclose(fd);
	return streamURL;
}

void printmarks(void) {
	FILE * fd = fopen(rcpath("bookmarks"), "r");
	char ch;

	if(!fd) {
		puts("No bookmarks.");
		return;
	}
	
	while(!feof(fd) && (ch = fgetc(fd)) > 0)
		fputc(ch, stdout);

	fclose(fd);
}
