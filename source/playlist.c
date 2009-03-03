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


int expand(struct playlist * list) {
	char url[512], ** response, * xml = NULL;
	const char * fmt =
		"http://ws.audioscrobbler.com/radio/xspf.php"
		"?sk=%s&discovery=%d&desktop=0";

	assert(list != NULL);

	memset(url, 0, sizeof(url));
	snprintf(
		url, sizeof(url), fmt,
		value(& data, "session"), !!enabled(DISCOVERY)
	);

	response = fetch(url, NULL, NULL, NULL);

	if(response != NULL) {
		int retval;

		xml = join(response, 0);
		response = NULL;

		retval = parsexspf(list, xml);
		return retval;
	}

	return 0;
}

void trim(char* string){
	int offset = 0;
	while (string[offset] == ' ')
		offset++;
	if (offset)
		memmove(string, string+offset, strlen(string+offset)+1);
}


int parsexspf(struct playlist * list, char * xml) {
	char * ptr = xml;
	unsigned i, tracks = 0;
	char * track;

	assert(list != NULL);
	assert(xml != NULL);

	while((track = strcasestr(ptr, "<track>")) != NULL) {
		struct tracknode * node = NULL;
		char * next = strcasestr(track + 7, "<track>"), * duration;

		const char * tags [] = {
			"location", "title", "album", "creator", "duration", "image",
			"lastfm:trackauth",
		};

		const char * links [] = { "artistpage", "albumpage", "trackpage", "freeTrackURL", };


		if(next)
			* (next - 1) = 0;

		node = malloc(sizeof(struct tracknode));
		assert(node != NULL);

		memset(node, 0, sizeof(struct tracknode));

		for(i = 0; i < (sizeof(tags) / sizeof(char *)); ++i) {
			char begin[32] = { 0 }, end[32] = { 0 };

			sprintf(begin, "<%s>", tags[i]);
			sprintf(end, "</%s>", tags[i]);

			if((ptr = strcasestr(track, begin)) != NULL) {
				char * text = strndup(
						ptr + strlen(begin),
						(strcasestr(ptr, end)) - (ptr + strlen(begin))
						);

				assert(text != NULL);

				unhtml(text);
				set(& node->track, tags[i], text);
				free(text);
			}
		}

		for(i = 0; i < (sizeof(links) / sizeof(char *)); ++i) {
			char begin[64] = { 0 };

			sprintf(begin, "<link rel=\"http://www.last.fm/%s\">", links[i]);

			if((ptr = strcasestr(track, begin)) != NULL) {
				char * text = strndup(
						ptr + strlen(begin),
						(strcasestr(ptr, "</link>")) - (ptr + strlen(begin))
						);

				assert(text != NULL);

				set(& node->track, links[i], text);
				free(text);
			}
		}

		trim(list->title);
		set(& node->track, "station", list->title);

		duration = strdup(value(& node->track, "duration"));
		if(duration != NULL) {
			duration[strlen(duration) - 3] = 0;
			set(& node->track, "duration", duration);
		}

		push(list, node);

		++tracks;

		if(!next)
			break;

		ptr = next;
	}

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
