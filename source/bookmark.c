/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "settings.h"
#include "bookmark.h"
#include "getln.h"
#include "util.h"
#include "strary.h"
#include "globals.h"
#include "readline.h"


int cmpdigit(const void *, const void *);


/* Save a stream URL with the given number as bookmark. */
void setmark(const char * streamURL, int n) {
	FILE * fd;
	char * bookmarks[999] = { NULL, };

	if(!streamURL || n < 0 || n > 999)
		return;
	
	if((fd = fopen(rcpath("bookmarks"), "r"))) {
		while(!feof(fd)) {
			char * line = NULL;
			unsigned size = 0, length;
			int x = 0, numlength = 0;

			length = getln(& line, & size, fd);
			if(line && length > 4) {
				if(sscanf(line, "%d = ", & x) == 1 && !bookmarks[x]) {
				        numlength = snprintf(NULL, 0, "%d", x);
					bookmarks[x] = strdup(numlength + line + 3);
					assert(bookmarks[x] != NULL);
				}

				free(line);
			}
		}

		fclose(fd);
	}

	bookmarks[n] = strdup(streamURL);
	assert(bookmarks[n] != NULL);

	if((fd = fopen(rcpath("bookmarks"), "w"))) {
		int i;
		for(i = 0; i < 999; ++i)
			if(bookmarks[i]) {
				char * ptr = strchr(bookmarks[i], 10);
				if(ptr != NULL)
					* ptr = 0;
				fprintf(fd, "%d = %s\n", i, bookmarks[i]);
				free(bookmarks[i]);
			}

		fclose(fd);
		if(!enabled(QUIET))
			puts("Bookmark saved.");
	}
}

/* Get bookmarked stream. */
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
				int length = 0;

				if(ptr != NULL)
					* ptr = 0;
				
				length = snprintf(NULL, 0, "%d", n);
				streamURL = strdup(line + length + 3);
				assert(streamURL != NULL);
			}

		if(size && line)
			free(line);
	}

	fclose(fd);
	return streamURL;
}

/* Prompt for user specific bookmark number */
char * promptmark() {
	char * number = NULL;

	struct prompt prompt = {
		.prompt = "Bookmark number: ",
		.line = NULL,
		.history = NULL,
		.callback = NULL,
	};
	number = readline(& prompt);
	return getmark(atoi(number));
}

void printmarks(void) {
	char ** list = slurp(rcpath("bookmarks"));
	unsigned n;

	if(list == NULL) {
		puts("No bookmarks.");
		return;
	}

	qsort(list, count(list), sizeof(char *), cmpdigit);
	
	for(n = 0; list[n] != NULL; ++n)
		puts(list[n]);

	purge(list);
}


int cmpdigit(const void * a, const void * b) {
	return (** (char **) a) - (** (char **) b);
}
