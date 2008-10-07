/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "settings.h"
#include "autoban.h"
#include "getln.h"


/*
	Check if a given artist is banned completely (it appears in
	"~/.shell-fm/autoban").
*/
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
			if((ptr = strrchr(line, 10)) != NULL)
				* ptr = (char) 0;

			match = !strncasecmp(line, artist, strlen(line));
		}
	}

	if(line)
		free(line);

	fclose(fd);

	return match;
}


/* Ban an artist by adding it to the autoban file. */
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
