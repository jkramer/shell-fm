/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "feeds.h"
#include "tag.h"
#include "util.h"
#include "strary.h"
#include "rest.h"
#include "json.h"
#include "hash.h"

char ** load_feed(const char *, const char *, const char *, const char *, struct hash *);

char ** neighbors(const char * user) {
	return load_feed(user, "user.getNeighbours", "neighbours.user", "name", NULL);
}


char ** topartists(const char * user) {
	return load_feed(user, "user.getTopArtists", "topartists.artist", "name", NULL);
}

char ** friends(const char * user) {
	return load_feed(user, "user.getFriends", "friends.user", "name", NULL);
}


char ** toptags(char key, struct hash * track) {
	struct hash h = { 0, NULL };
	char ** names;

	set(& h, "artist", value(track, "creator"));

	switch(key) {
		case 'a':
			names = load_feed(NULL, "artist.getTopTags", "toptags.tag", "name", & h);
			break;
		case 'l':
			set(& h, "album", value(track, "album"));
			names = load_feed(NULL, "album.getTopTags", "toptags.tag", "name", & h);
			break;
		case 't':
			set(& h, "track", value(track, "title"));
			names = load_feed(NULL, "track.getTopTags", "toptags.tag", "name", & h);
			break;
		default:
			names = NULL;
			break;
	}

	empty(& h);

	return names;
}


char ** overalltags(void) {
	return load_feed(NULL, "tag.getTopTags", "toptags.tag", "name", NULL);
}


char ** usertags(const char * user) {
	return load_feed(user, "user.getTopTags", "toptags.tag", "name", NULL);
}


char ** load_feed(
	const char * user,
	const char * method,
	const char * path,
	const char * last_node,
	struct hash * track
) {
	struct hash h = { 0, NULL };
	char * response, ** names = NULL, format[64], single_path[64];
	unsigned n;
	json_value * json;

	if(user != NULL)
		set(& h, "user", user);

	if(track != NULL) {
		for(n = 0; n < track->size; ++n) {
			set(& h, track->content[n].key, track->content[n].value);
		}
	}

	response = rest(method, & h);

	json = json_parse(response);

	empty(& h);

	json_hash(json, & h, NULL);

	snprintf(format, sizeof(format), "%s.%%d.%%31s", path);
	snprintf(single_path, sizeof(single_path), "%s.%s", path, last_node);

	for(n = 0; n < h.size; ++n) {
		int x = 0;
		char name[32];

		if(
			strcmp(h.content[n].key, path) == 0
			||
			(
				sscanf(h.content[n].key, format, & x, name) > 0
				&&
				strcmp(name, last_node) == 0
			)
		) {
			names = append(names, h.content[n].value);
		}
	}

	empty(& h);
	free(response);
	json_value_free(json);

	return names;
}

