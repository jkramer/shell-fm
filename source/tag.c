/*
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "settings.h"
#include "http.h"
#include "split.h"
#include "interface.h"
#include "completion.h"
#include "md5.h"
#include "feeds.h"

#include "readline.h"
#include "tag.h"
#include "util.h"
#include "globals.h"
#include "rest.h"
#include "json.h"

char ** popular = NULL;

void tag(struct hash data) {
	char key, * tagstring;
	struct prompt setup = {
		.prompt = "Tags (comma separated): ",
		.line = NULL,
		.history = NULL,
		.callback = tagcomplete,
	};

	if(!data.content)
		return;

	fputs("Tag (a)rtist, a(l)bum, (t)rack or (c)ancel?\n", stderr);

	while(!strchr("altc", (key = fetchkey(2))));

	if(key == 'c')
		return;

	popular = merge(toptags(key, & data), usertags(value(& rc, "username")), 0);

	setup.line = load_tags(key, & data);

	assert((tagstring = strdup(readline(& setup))) != NULL);

	if(setup.line) {
		free(setup.line);
		setup.line = NULL;
	}

	purge(popular);
	popular = NULL;

	sendtag(key, tagstring, data);
	free(tagstring);
}


char * load_tags(char key, struct hash * track) {
	char * response, * tag_string = NULL;
	const char * method;
	struct hash h = { 0, NULL };
	unsigned n;
	size_t total_length = 0;
	json_value * json;

	switch(key) {
		case 'a':
			method = "artist.getTags";
			break;

		case 'l':
			method = "album.getTags";
			set(& h, "album", value(track, "album"));
			break;

		case 't':
		default:
			set(& h, "track", value(track, "title"));
			method = "track.getTags";
	}

	set(& h, "artist", value(track, "creator"));

	response = rest(method, & h);
	if(!response)
		return NULL;

	json = json_parse(response);

	empty(& h);

	json_hash(json, & h, NULL);

	dump_hash(& h);

	for(n = 0; n < h.size; ++n) {
		int x = 0;
		char name[32];

		if(
			strcmp(h.content[n].key, "tags.tag.name") == 0
			||
			(
				sscanf(h.content[n].key, "tags.tag.%d.%31s", & x, name) > 0
				&&
				strcmp(name, "name") == 0
			)
		) {
			size_t length = strlen(h.content[n].value);

			tag_string = realloc(tag_string, total_length + length + 1);

			sprintf(tag_string + total_length, n > 0 ? ",%s" : "%s", h.content[n].value);

			total_length += length + n;
		}
	}

	free(response);
	json_value_free(json);
	empty(& h);

	return tag_string;
}


void stripslashes(char * string) {
	unsigned x = 0;
	while(x < strlen(string)) {
		if(string[x] == 0x2F)
			strncpy(string + x, string + x + 1, strlen(string + x + 1));
		else
			++x;
	}
}


int tagcomplete(char * line, const unsigned max, int changed) {
	unsigned length, nres = 0;
	int retval = 0;
	char * ptr = NULL;
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

	/* No need for tab completion if there are no popular tags. */
	if(!popular || !popular[0])
		return retval;

	/* Get pointer to the beginning of the last tag in tag string. */
	if((ptr = strrchr(line, ',')) == NULL)
		ptr = line;
	else
		++ptr;

	length = strlen(ptr);
	if(!length)
		changed = !0;

	/* Get next match in list of popular tags. */
	if((match = nextmatch(popular, changed ? ptr : NULL, & nres)) != NULL) {
		snprintf(ptr, max - (ptr - line) - 1, "%s%s", match, nres < 2 ? "," : "");
		retval = !0;
	}

	return retval;
}


void sendtag(char key, char * tagstring, struct hash data) {
	struct hash h = { 0, NULL };
	const char * method, * error;
	char * response;

	switch(key) {
		case 'a':
			method = "artist.addTags";
			break;

		case 'l':
			method = "album.addTags";
			set(& h, "album", value(& data, "album"));
			break;

		case 't':
			method = "track.addTags";
			set(& h, "track", value(& data, "title"));
			break;

		default:
			method = ""; // May not happen.
			break;
	}

	set(& h, "artist", value(& data, "creator"));
	set(& h, "tags", tagstring);

	response = rest(method, & h);

	if(!enabled(QUIET) && response) {
		error = error_message(response);
		puts(error == NULL ? "Tagged." : error);
	}

	free(response);
	empty(& h);
}
