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

#include "completion.h"
#include "http.h"
#include "settings.h"
#include "readline.h"
#include "history.h"
#include "split.h"
#include "feeds.h"
#include "strary.h"
#include "service.h"


static int radiocomplete(char *, const unsigned, int);

static char ** users = NULL, ** artists = NULL;

/*
 * This function is called to change the station
 * the user is prompted for the new radio station.
 *
 * Something like:
 *
 * lastfm://user/BartTrojanowski/loved
 * lastfm://user/BartTrojanowski/personal
 * lastfm://user/BartTrojanowski/neighbour
 * lastfm://user/BartTrojanowski/recommended/100
 * lastfm://usertags/BartTrojanowski/trance
 * lastfm://artist/QED/similarartists
 * lastfm://artist/QED/fans
 * lastfm://globaltags/goa
 * lastfm://globaltags/classical,miles davis,whatever
 * lastfm://multipleartists/QED,Chicane
 * lastfm://play/tracks/########[,#####, ...]
 */
void radioprompt(const char * prompt) {
	char * url, * decoded = NULL;

	struct prompt setup = {
		.prompt = prompt,
		.line = NULL,
		.history = slurp(rcpath("radio-history")),
		.callback = radiocomplete,
	};

	users = neighbors(value(& rc, "username"));
	users = merge(users, friends(value(& rc, "username")), 0);
	users = append(users, value(& rc, "username"));

	artists = topartists(value(& rc, "username"));

	url = readline(& setup);

	purge(users);
	purge(artists);

	users = artists = NULL;

	if(setup.history)
		purge(setup.history);

	if(strlen(url)) {
		decode(url, & decoded);
		station(decoded);
		free(decoded);
	}
}


int radiocomplete(char * line, const unsigned max, int changed) {
	unsigned length = strlen(line), nsplt = 0, slash = 0;
	const char * match;
	char
		** splt,
		* types [] = {
			"user",
			"usertags",
			"artist",
			"globaltags",
			"play",
			NULL
		};

	if(!strncasecmp(line, "lastfm://", 9)) {
		memmove(line, line + 9, 9);
		memset(line + 9, 0, max - (length -= 9));
	}

	if(line[length - 1] == '/')
		slash = 1;

	splt = split(line, "/", & nsplt);
	
	if(!nsplt) {
		free(splt);
		return 0;
	}

	/*
	fprintf(stderr, "\nCHANGED=<%d>\nLEVEL=<%d>\nLINE=<%s>\n", changed, nsplt, line);
	for(i = 0; splt[i] != NULL; ++i)
		fprintf(stderr, "%i=<%s>\n", i, splt[i]);
	fputs("\n", stderr);
	*/

	switch(nsplt + slash) {
		case 1:
			if((match = nextmatch(types, changed ? splt[0] : NULL)) != NULL)
				snprintf(line, max, "%s", match);
			break;

		case 2:
			if(!strcmp(splt[0], "user") || !strcmp(splt[0], "usertags")) {
				match = nextmatch(users, changed ? (slash ? "" : splt[1]) : NULL);
				snprintf(line, max, "%s/%s", splt[0], match ? match : splt[1]);
			} else if(!strcmp(splt[0], "artist")) {
				match = nextmatch(artists, changed ? (slash ? "" : splt[1]) : NULL);
				snprintf(line, max, "%s/%s", splt[0], match ? match : splt[1]);
			}
			break;

		case 3:
			if(!strcmp(splt[0], "user")) {
				char * radios [] = {
					"personal",
					"neighbours",
					"loved",
					"recommended",
					NULL
				};

				match = nextmatch(radios, changed ? (slash ? "" : splt[2]) : NULL);
				snprintf(line, max, "%s/%s/%s", splt[0], splt[1], match ? match : splt[2]);
			} else if(!strcmp(splt[0], "artist")) {
				char * radios [] = {
					"fans",
					"similarartists",
					NULL
				};
				match = nextmatch(radios, changed ? (slash ? "" : splt[2]) : NULL);
				snprintf(line, max, "%s/%s/%s", splt[0], splt[1], match ? match : splt[2]);
			}
			break;
	}

	while(nsplt--)
		free(splt[nsplt]);

	free(splt);

	return !0;
}
