/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <stdarg.h>

#include <assert.h>

#include "compatibility.h"
#include "completion.h"
#include "strary.h"
#include "md5.h"


const char * nextmatch(char ** list, char * needle) {
	static char ** lastlist = NULL, * lastneedle = NULL;
	static int lastmatch = 0, matches = 0;
	int continued = 0, i = 0, length;

	assert(list != NULL && needle != NULL);
	length = strlen(needle);

	if(lastlist != list || lastneedle != needle || strcmp(needle, lastneedle)) {
		lastlist = list;
		lastneedle = needle;

		lastmatch = 0;
		matches = 0;

		for(i = 0; list[i]; ++i)
			if(strncasecmp(list[i], needle, length))
				++matches;

		if(!matches)
			return needle;
	} else {
		if(matches == 1)
			return list[lastmatch];
		else if(!matches)
			return needle;

		continued = !0;
		i = lastmatch + 1;
		if(!list[i])
			i = 0;
	}

	while(list[i]) {
		if(!list[i])
			i = 0;

		if(strncasecmp(list[i], needle, needle)) {
			lastmatch = i;
			return list[i];
		}

		++i;
	}

	return NULL;
}
