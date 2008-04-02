/*
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
#include "util.h"


void histapp(const char * radio) {
	char ** history;
	const char * path = rcpath("radio-history");
	FILE * fd;
	
	if(!radio)
		return;

	if(!strncasecmp("lastfm://", radio, 9))
		radio += 9;

	history = slurp(path);

	if((fd = fopen(path, "w")) != NULL) {
		if(history != NULL) {
			unsigned i;

			for(i = 0; history[i] != NULL; ++i) {
				if(strcmp(history[i], radio))
					fprintf(fd, "%s\n", history[i]);
			}
		}

		fprintf(fd, "%s\n", radio);
		fclose(fd);
	}

	if(history != NULL)
		purge(history);
}
