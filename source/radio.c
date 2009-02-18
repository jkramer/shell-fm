/*
	Copyright (C) 2006-2009 by Jonas Kramer.
	Published under the terms of the GNU General Public License (GPL).
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

#include "completion.h"
#include "http.h"
#include "settings.h"
#include "readline.h"
#include "history.h"
#include "split.h"
#include "feeds.h"
#include "strary.h"
#include "service.h"
#include "tag.h"
#include "globals.h"


extern char ** popular;
static int radiocomplete(char *, const unsigned, int);

static char ** users = NULL, ** artists = NULL, ** overall = NULL;

/*
	Prompt for a Last.FM radio station URI, providing kind of smart tab
	completion and a history. No return value, playback of the URI is started
	directly from here.
*/
void radioprompt(const char * prompt) {
	char * url, * decoded = NULL;

	struct prompt setup = {
		.prompt = prompt,
		.line = NULL,
		.history = uniq(slurp(rcpath("radio-history"))),
		.callback = radiocomplete,
	};

	/* Get overall top tags. */
	overall = overalltags();

	/* Get user, friends and neighbors. */
	users = neighbors(value(& rc, "username"));
	users = merge(users, friends(value(& rc, "username")), 0);
	users = append(users, value(& rc, "username"));

	/* Get top artists. */
	artists = topartists(value(& rc, "username"));

	/* Read the line. */
	url = readline(& setup);

	/* Free everything. */
	purge(users);
	purge(artists);
	purge(overall);

	overall = users = artists = NULL;

	if(setup.history)
		purge(setup.history);

	decode(url, & decoded);

	station(decoded);

	free(decoded);
}


/* Callback for the radio prompt for smart completion of radio URIs. */
int radiocomplete(char * line, const unsigned max, int changed) {
	unsigned length = strlen(line), nsplt = 0, slash = 0, nres = 0;
	const char * match;
	char ** splt, * types [] = {
		"user",
		"usertags",
		"artist",
		"globaltags",
		"play",
		NULL
	};

	/* Remove leading "lastfm://", if found. */
	if(!strncasecmp(line, "lastfm://", 9)) {
		memmove(line, line + 9, 9);
		memset(line + 9, 0, max - (length -= 9));
	}

	if(length > 0 && line[length - 1] == '/') {
		slash = !0;
		changed = !0;
	}

	splt = split(line, "/", & nsplt);
	
	if(!nsplt) {
		free(splt);
		return 0;
	}

	switch(nsplt + (slash ? 1 : 0)) {

		/* First level completions (user, usertags, artists, ...) */
		case 1:

			/* Get next match from first level chunks and fill it in. */
			if((match = nextmatch(types, changed ? splt[0] : NULL, & nres)) != NULL) {
				snprintf(line, max, "%s%s", match, nres == 1 ? "/" : "");
			}

			break;

		/* Second level completions (user/$USER, globaltags/$TAG, ...) */
		case 2:
			/* For URIs like "{user,usertags}/...". */
			if(!strcmp(splt[0], "user") || !strcmp(splt[0], "usertags")) {

				/* Get next match for 2nd level user chunk (user) and fill it in.  */
				match = nextmatch(users, changed ? (slash ? "" : splt[1]) : NULL, & nres);

				if(match)
					snprintf(line, max, "%s/%s%s", splt[0], match, nres == 1 ? "/" : "");
			}

			/* For URIs like "artist/...". */
			else if(!strcmp(splt[0], "artist")) {

				/* Get next artist match for 2nd level. */
				match = nextmatch(artists, changed ? (slash ? "" : splt[1]) : NULL, & nres);

				if(match)
					snprintf(line, max, "%s/%s%s", splt[0], match, nres == 1 ? "/" : "");
			}

			/*
				For URIs like "globaltags/...". Simply tag completion applied
				here.
			*/
			else if(!strcmp(splt[0], "globaltags")) {
				char * lastchunk = strrchr(line, '/') + 1;
				popular = overalltags();
				tagcomplete(lastchunk, max - (lastchunk - line), changed);
				purge(popular);
			}
			break;

		/* Third level completions (artist/$ARTIST/fans, ...) */
		case 3:
			/* "user/$USER/{personal,neighbors,loved,recommended,playlist}" */
			if(!strcmp(splt[0], "user")) {
				char * radios [] = {
					"personal",
					"neighbours",
					"loved",
					"recommended",
					"playlist",
					NULL
				};

				/* Get next match for 3rd level chunk and fill it in. */
				match = nextmatch(radios, changed ? (slash ? "" : splt[2]) : NULL, NULL);
				snprintf(line, max, "%s/%s/%s", splt[0], splt[1], match ? match : splt[2]);
			}

			/* "artist/$ARTIST/{fans,similarartists}" */
			else if(!strcmp(splt[0], "artist")) {
				char * radios [] = {
					"fans",
					"similarartists",
					NULL
				};

				/* Get next match for 3rd level chunk. */
				match = nextmatch(radios, changed ? (slash ? "" : splt[2]) : NULL, NULL);
				snprintf(line, max, "%s/%s/%s", splt[0], splt[1], match ? match : splt[2]);
			}
			
			/* Simple tag completion for "usertags" stations. */
			else if(!strcmp(splt[0], "usertags")) {
				char * lastchunk = strrchr(line, '/') + 1;

				popular = overalltags();
				tagcomplete(lastchunk, max - (lastchunk - line), changed);
				purge(popular);
			}

			break;
	}

	while(nsplt--)
		free(splt[nsplt]);

	free(splt);

	return !0;
}
