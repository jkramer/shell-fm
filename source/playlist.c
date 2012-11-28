/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>


#include "hash.h"
#include "http.h"
#include "settings.h"
#include "service.h"
#include "strary.h"
#include "util.h"

#include "playlist.h"
#include "globals.h"

#include "interface.h"
#include "json.h"
#include "rest.h"


int expand(struct playlist * list) {
	struct hash p = { 0, NULL };
	char * response;

	assert(list != NULL);

	set(& p, "discovery", (!!enabled(DISCOVERY)) ? "true" : "false");
	response = rest("radio.getPlaylist", & p);

	if(response != NULL) {
		int result = parse_playlist(list, response);
		free(response);
		return result;
	}
	else {
		return 0;
	}
}

void trim(char * string){
	int offset = 0;
	while(string[offset] == ' ')
		offset++;

	if(offset)
		memmove(string, string + offset, strlen(string + offset) + 1);
}


int parse_playlist(struct playlist * list, char * plain_json) {
	json_value * playlist, * track_array, * track, * extension;
	unsigned n;

	assert(list != NULL);
	assert(plain_json != NULL);

	playlist = json_parse(plain_json);

	assert(playlist != NULL);

	track_array = json_query(playlist, "playlist", "trackList", "track", NULL);

	assert(track_array != NULL);

	for(n = 0; n < track_array->u.array.length; ++n) {
		struct tracknode * node = NULL;
		char * duration;

		node = malloc(sizeof(struct tracknode));
		assert(node != NULL);

		memset(node, 0, sizeof(struct tracknode));


		track = track_array->u.array.values[n];
		extension = json_query(track, "extension", NULL);

		json_hash(track, & node->track, NULL);
		json_hash(extension, & node->track, NULL);

		if((duration = strdup(value(& node->track, "duration"))) != NULL) {
			duration[strlen(duration) - 3] = 0;
			set(& node->track, "duration", duration);
			free(duration);
		}

		if(list->title) {
			trim(list->title);

			set(& node->track, "station", list->title);
		}
		else {
			set(& node->track, "station", "Unknown Station");
		}

		push(list, node);

		debug("track location: %s\n", value(& node->track, "location"));
	}

	json_value_free(playlist);

	return 1;
}


void freelist(struct playlist * list) {
	if(list->title != NULL)
		free(list->title);

	while(list->track)
		shift(list);

	memset(list, 0, sizeof(struct playlist));
}


void push(struct playlist * list, struct tracknode * node) {
	if(!list->track)
		list->track = node;
	else {
		struct tracknode * last = list->track;
		while(last->next != NULL)
			last = last->next;
		last->next = node;
	}

	++list->left;
}


void shift(struct playlist * list) {
	if(list->track) {
		struct tracknode * node = list->track;
		list->track = node->next;
		empty(& node->track);
		free(node);
		--(list->left);
	}
}

void preview(struct playlist list) {
	struct tracknode * node;
	unsigned n = 0;

	if (list.track != NULL)
		node = list.track->next;
	else
	{
		puts("No tracks in queue.");
		return;
	}

	if(node == NULL) {
		puts("No tracks in queue.");
	}
	else {
		puts("Upcoming tracks:");
		while(node != NULL) {
			const char * format;

			format = haskey(& rc, "preview-format")
				? value(& rc, "preview-format")
				: "%a - %t";

			printf("%2d %s\n", n++, meta(format, M_COLORED, & node->track));

			node = node->next;
		}
	}
}
