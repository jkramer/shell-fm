/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "xmlrpc.h"
#include "feeds.h"
#include "hash.h"
#include "completion.h"
#include "readline.h"
#include "interface.h"
#include "settings.h"
#include "getln.h"

static char ** users = NULL;
static int usercomplete(char *, const unsigned, int);

void recommend(struct hash track) {
	char key, * message = NULL, * recipient = NULL;
	unsigned result = 0;

	struct prompt setup = {
		.prompt = "Recipient: ",
		.line = NULL,
		.history = NULL,
		.callback = usercomplete,
	};

	struct prompt comment = {
		.prompt = "Comment: ",
		.line = NULL,
		.history = NULL,
		.callback = NULL,
	};

	if(!track.content)
		return;

	fputs("Recommend (a)rtist, a(l)bum, (t)rack or (c)ancel?\n", stderr);

	while(!strchr("altc", (key = fetchkey(2))));

	if(key == 'c')
		return;

	users = neighbors(value(& rc, "username"));
	users = merge(users, friends(value(& rc, "username")), 0);

	recipient = readline(& setup);

	purge(users);
	users = NULL;

	message = readline(& comment);

	switch(key) {
		case 'a':
			result = xmlrpc(
				"recommendArtist", "ssss",
				value(& track, "creator"),
				recipient, message, "en"
			);
			break;

		case 'l':
			result = xmlrpc(
				"recommendAlbum", "sssss",
				value(& track, "creator"),
				value(& track, "album"),
				recipient, message, "en"
			);
			break;

		case 't':
			result = xmlrpc(
				"recommendTrack", "sssss",
				value(& track, "creator"),
				value(& track, "title"),
				recipient, message, "en"
			);
			break;
	}

	puts(result ? "Recommended." : "Sorry, failed.");
}


static int usercomplete(char * line, const unsigned max, int changed) {
	unsigned length, nres = 0;
	int retval = 0;
	const char * match = NULL;

	assert(line != NULL);
	length = strlen(line);

	/* Remove spaces at the beginning of the string. */
	while(isspace(line[0])) {
		retval = !0;
		memmove(line, line + 1, strlen(line + 1));
		line[--length] = 0;
	}

	/* Remove spaces at the end of the string. */
	while((length = strlen(line)) > 0 && isspace(line[length - 1])) {
		retval = !0;
		line[--length] = 0;
	}

	if(!users || !users[0])
		return retval;

	if((match = nextmatch(users, changed ? line : NULL, & nres)) != NULL) {
		snprintf(line, max, "%s", match);
		retval = !0;
	}

	return retval;
}
