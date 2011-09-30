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


int cmpchar(const void *, const void *);


/* Save a stream URL with the given number as bookmark. */
void setmark(const char * streamURL, int n) {
	FILE * fd;
	char * bookmarks[256] = { NULL, };

	if(!streamURL || !isalnum(n))
		return;

	if((fd = fopen(rcpath("bookmarks"), "r"))) {
		while(!feof(fd)) {
			char * line = NULL, x = 0;
			unsigned size = 0, length;

			length = getln(& line, & size, fd);
			if(line && length > 4) {
				if(sscanf(line, "%c = ", & x) == 1 && !bookmarks[(int) x]) {
					bookmarks[(int) x] = strdup(line + 4);
					debug("%c = %s\n", x, bookmarks[(int) x]);
					assert(bookmarks[(int) x] != NULL);
				}
				else {
					debug("read fail: %s\n", line);
				}

				free(line);
			}
		}

		fclose(fd);
	}

	bookmarks[n] = strdup(streamURL);
	assert(bookmarks[n] != NULL);

	if((fd = fopen(rcpath("bookmarks"), "w"))) {
		unsigned i;
		for(i = 0; i < sizeof(bookmarks) / sizeof(bookmarks[0]); ++i)
			if(bookmarks[i] != NULL) {
				char * ptr = strchr(bookmarks[i], 10);
				if(ptr != NULL)
					* ptr = 0;
				fprintf(fd, "%c = %s\n", i, bookmarks[i]);
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
		char * line = NULL, x;
		unsigned size = 0, length;

		length = getln(& line, & size, fd);
		if(line && length > 4)
			if(sscanf(line, "%c = ", & x) == 1 && x == n) {
				char * ptr = strchr(line, 10);

				if(ptr != NULL)
					* ptr = 0;

				streamURL = strdup(line + 4);
				assert(streamURL != NULL);
			}

		if(size && line)
			free(line);
	}

	fclose(fd);
	return streamURL;
}

void printmarks(void) {
	char ** list = slurp(rcpath("bookmarks"));
	unsigned n;

	if(list == NULL) {
		puts("No bookmarks.");
		return;
	}

	qsort(list, count(list), sizeof(char *), cmpchar);

	for(n = 0; list[n] != NULL; ++n)
		puts(list[n]);

	purge(list);
}


int cmpchar(const void * a, const void * b) {
	return (** (char **) a) - (** (char **) b);
}
