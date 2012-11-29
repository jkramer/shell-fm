/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "feeds.h"
#include "hash.h"
#include "completion.h"
#include "readline.h"
#include "interface.h"
#include "settings.h"
#include "getln.h"
#include "rest.h"

static char ** users = NULL;
static int usercomplete(char *, const unsigned, int);

void recommend(struct hash track) {
	char key, * message = NULL, * recipient = NULL, * response = NULL;
	const char * method, * error;
	struct hash h = { 0, NULL };

	struct prompt recipient_prompt = {
		.prompt = "Recipient: ",
		.line = NULL,
		.history = NULL,
		.callback = usercomplete,
	};

	struct prompt comment_prompt = {
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

	recipient = strdup(readline(& recipient_prompt));

	purge(users);
	users = NULL;

	message = strdup(readline(& comment_prompt));

	switch(key) {
		case 'a':
			method = "artist.share";
			break;

		case 'l':
			method = "album.share";
			set(& h, "album", value(& track, "album"));
			break;

		case 't':
			method = "track.share";
			set(& h, "track", value(& track, "title"));
			break;

		default:
			method = ""; /* This can't happen. */
			break;
	}

	set(& h, "artist", value(& track, "creator"));
	set(& h, "message", message);
	set(& h, "recipient", recipient);

	free(recipient);
	free(message);

	response = rest(method, & h);

	error = error_message(response);

	if(error != NULL) {
		puts(error);
	}

	free(response);
	empty(& h);
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
