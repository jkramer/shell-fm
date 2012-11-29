/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#include <assert.h>
#include <time.h>

#include "hash.h"
#include "http.h"
#include "md5.h"
#include "settings.h"
#include "split.h"
#include "getln.h"
#include "strary.h"
#include "rest.h"

#include "submit.h"
#include "globals.h"

#define ERROR (strerror(errno))

struct hash submission, queue = { 0, NULL };
unsigned queue_length = 0;

pid_t subfork = 0;

extern pid_t playfork;
extern struct hash data, rc, track;

static const char * key_no(const char *, int);

/* Add a track to the scrobble queue. */
int enqueue(struct hash * track) {
	char timestamp[16];

	assert(track != NULL);

	snprintf(timestamp, sizeof(timestamp), "%lu", (unsigned long) time(NULL));

	set(& queue, key_no("artist", queue_length), value(track, "creator"));
	set(& queue, key_no("track", queue_length), value(track, "title"));
	set(& queue, key_no("album", queue_length), value(track, "album"));
	set(& queue, key_no("streamId", queue_length), value(track, "streamid"));
	set(& queue, key_no("timestamp", queue_length), timestamp);

	++queue_length;

	return !0;
}


/* Submit tracks from the queue. */
int submit() {
	char * body = NULL;
	const char * error;
	int retval = 0;

	if(!queue_length || subfork > 0)
		return 0;

	subfork = fork();

	if(subfork != 0)
		return !0;

	playfork = 0;
	enable(QUIET);

	signal(SIGTERM, SIG_IGN);

	body = rest("track.scrobble", & queue); // TODO: Check for errors!

	error = error_message(body);

	if(error) {
		fprintf(stderr, "Failed to scrobble track(s), error: %s. Query: %s\n", error, hash_query(& queue));
		retval = 1;
	}

	empty(& queue);
	free(body);

	exit(retval);
}


void subdead(int exitcode) {
	if(exitcode == 0) {
		queue_length = 0;
		empty(& queue);
	} else {
		fprintf(stderr, "Error while scrobbling track, will try again later.\n");
	}

	subfork = 0;
}


/* Write the tracks from the scrobble queue to a file. */
void dump_queue() {
	// TODO
	// const char * path = rcpath("scrobble-cache");

	// if(path != NULL) {
		// FILE * fd = fopen(path, overwrite ? "w" : "a+");
		// if(fd != NULL) {
			// unsigned n, x;
			// for(n = 0; n < queue_length; ++n) {
				// const char keys [] = "atirolbnm";
				// for(x = 0; x < sizeof(keys) - 1; ++x) {
					// char key[2] = { keys[x], 0 }, * encoded = NULL;
					// encode(value(& queue[n], key), & encoded);
					// fprintf(fd, "%c=%s;", keys[x], encoded);
					// free(encoded);
				// }
				// fputc(10, fd);
			// }
			// fclose(fd);
		// } else {
			// fprintf(stderr, "Couldn't open scrobble cache. %s.\n", ERROR);
		// }
	// } else {
		// fprintf(stderr, "Can't find suitable scrobble queue path.\n");
	// }

	empty(& queue);
}


void load_queue() {
	// TODO
	// const char * path = rcpath("scrobble-cache");

	// if(overwrite)
		// sliceq(queue_length);

	// if(path != NULL) {
		// FILE * fd = fopen(path, "r");
		// if(fd != NULL) {
			// while(!feof(fd)) {
				// char * line = NULL;
				// unsigned size = 0;

				// if(getln(& line, & size, fd) >= 2) {
					// unsigned n = 0;
					// char ** splt = split(line, ";\n", & n);
					// struct hash track;

					// memset(& track, 0, sizeof(struct hash));

					// while(n--) {
						// char key[2] = { 0 }, * value = NULL;
						// sscanf(splt[n], "%c=", & key[0]);
						// if(strchr("atirolbnm", key[0])) {
							// decode(splt[n] + 2, & value);
							// set(& track, key, value);
							// free(value);
						// }
						// free(splt[n]);
					// }

					// free(splt);

					// queue = realloc(queue, sizeof(struct hash) * (++queue_length));
					// assert(queue != NULL);
					// memcpy(& queue[queue_length - 1], & track, sizeof(struct hash));
				// }

				// if(line)
					// free(line);
			// }
			// fclose(fd);
		// }
	// }
}


/* Send "now playing notification" to Last.FM. */
void notify_now_playing(struct hash * track) {
	struct hash post = { 0, NULL };
	char * body;

	assert(track != NULL);

	set(& post, "artist", value(track, "creator"));
	set(& post, "track", value(track, "title"));
	set(& post, "album", value(track, "album"));

	body = rest("track.updateNowPlaying", & post);

	empty(& post);
	free(body);
}

static const char * key_no(const char * name, int n) {
	static char key[64];

	snprintf(key, sizeof(key), "%s[%d]", name, n);

	return key;
}
