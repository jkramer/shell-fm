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
#include "xmlrpc.h"
#include "util.h"
#include "globals.h"

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

	popular = merge(toptags(key, data), usertags(value(& rc, "username")), 0);

	setup.line = oldtags(key, data);

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


char * oldtags(char key, struct hash track) {
	unsigned length, x;
	char * tags = NULL, * url = calloc(512, sizeof(char)),
		 * user = NULL, * artist = NULL, * arg = NULL,
		 * file = NULL, ** resp;
	
	assert(url != NULL);
	
	switch(key) {
		case 'a':
			file = "artisttags.xml";
			break;
		case 'l':
			file = "albumtags.xml";
			break;
		case 't':
		default:
			file = "tracktags.xml";
	}

	encode(value(& track, "creator"), & artist);
	stripslashes(artist);

	encode(value(& rc, "username"), & user);

	length = snprintf(
			url, 512, "http://ws.audioscrobbler.com/1.0/user/%s/%s?artist=%s",
			user, file, artist);

	free(user);
	free(artist);

	if(key == 'l') {
		encode(value(& track, "album"), & arg);
		stripslashes(arg);
		length += snprintf(url + length, 512 - length, "&album=%s", arg);
	} else if(key == 't') {
		encode(value(& track, "title"), & arg);
		stripslashes(arg);
		length += snprintf(url + length, 512 - length, "&track=%s", arg);
	}

	if(arg)
		free(arg);

	resp = fetch(url, NULL, NULL, NULL);
	free(url);

	if(!resp)
		return NULL;

	for(x = 0, length = 0; resp[x]; ++x) {
		char * pbeg = strstr(resp[x], "<name>"), * pend;
		if(pbeg) {
			pbeg += 6;
			pend = strstr(pbeg, "</name>");
			if(pend) {
				char * thistag = strndup(pbeg, pend - pbeg);
				unsigned nlength = strlen(thistag) + length;

				assert(thistag != NULL);

				if(length)
					++nlength;

				tags = realloc(tags, nlength + 1);

				assert(tags != NULL);

				sprintf(tags + length, "%s%s", length ? "," : "", thistag);

				free(thistag);
				length = nlength;
			}
		}
	}

	purge(resp);

	return tags;
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
	unsigned nsplt = 0;
	int result = 0;
	char ** splt = NULL;

	if(tagstring) {
		unsigned length = strlen(tagstring);
		/* remove trailing commas */
		while(tagstring[length-1] == ',')
			tagstring[--length] = 0;

		splt = split(tagstring, ",\n", & nsplt);
	}

	switch(key) {
		case 'a':
			result =
				xmlrpc("tagArtist", "sas", value(& data, "creator"), splt, "set");
			break;

		case 'l':
			result = xmlrpc(
					"tagAlbum", "ssas",
					value(& data, "creator"),
					value(& data, "album"),
					splt, "set"
					);
			break;

		case 't':
			result = xmlrpc(
					"tagTrack", "ssas",
					value(& data, "creator"),
					value(& data, "title"),
					splt, "set"
					);
			break;
	}

	if(!enabled(QUIET))
		puts(result ? "Tagged." : "Sorry, failed.");

	purge(splt);
	splt = NULL;
}
