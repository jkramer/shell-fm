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

#include "playlist.h"
#include "globals.h"


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


int parsexspf(struct playlist * list, const char * xml) {
	char * ptr;
	unsigned i, tracks = 0;

	assert(list != NULL);
	assert(xml != NULL);

	if((ptr = strcasestr(xml, "<title>")) != NULL) {
		char * track, * radio = NULL;

		radio = strndup(ptr + 7, strcasestr(xml, "</title>") - ptr - 7);

		if(list->title != NULL) {
			free(list->title);
			list->title = NULL;
		}

		decode(radio, & list->title);
		unhtml(list->title);
		free(radio);

		while((track = strcasestr(ptr, "<track>")) != NULL) {
			struct tracknode * node = NULL;
			char * next = strcasestr(track + 7, "<track>");

			const char * tags [] = {
				"location", "title", "album", "creator", "duration",
				"lastfm:trackauth",
			};

			const char * links [] = { "artistpage", "albumpage", "trackpage" };


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

			set(& node->track, "station", list->title);
			push(list, node);

			++tracks;

			if(!next)
				break;

			ptr = next;
		}

		return !0;
	}

	return 0;
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
