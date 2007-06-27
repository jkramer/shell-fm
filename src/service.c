/*
	Shell.FM - service.c
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
#include <signal.h>

#include "hash.h"
#include "http.h"
#include "play.h"
#include "settings.h"
#include "md5.h"
#include "history.h"
#include "service.h"
#include "playlist.h"

extern int stationChanged, discovery, stop;

struct hash data; /* Warning! MUST be bzero'd ASAP or we're all gonna die! */
extern struct hash track;

pid_t playfork = 0; /* PID of the decoding & playing process, if running */
struct playlist playlist;

char * currentStation = NULL;


int authenticate(const char * username, const char * password) {
	const unsigned char * md5;
	char hexmd5[32 + 1] = { 0 }, url[512] = { 0 }, ** response;
	char * encuser = NULL;
	unsigned ndigit, i = 0;
	const char * session, * fmt =
		"http://ws.audioscrobbler.com/radio/handshake.php"
		"?version=0.1"
		"&platform=linux"
		"&username=%s"
		"&passwordmd5=%s"
		"&debug=0"
		"&language=en";
	
	memset(& data, 0, sizeof(struct hash));
	
	/* create the hash, then convert to ASCII */
	md5 = MD5((const unsigned char *) password, strlen(password));
	for(ndigit = 0; ndigit < 16; ++ndigit)
		sprintf(2 * ndigit + hexmd5, "%02x", md5[ndigit]);

	set(& rc, "password", hexmd5);
	
	/* escape username for URL */
	encode(username, & encuser);

	/* put handshake URL together and fetch initial data from server */
	snprintf(url, sizeof(url), fmt, encuser, hexmd5);
	free(encuser);

	response = fetch(url, NULL, NULL);
	if(!response) {
		fputs("No response.\n", stderr);
		return 0;
	}
	
	while(response[i]) {
		char key[64] = { 0 }, val[256] = { 0 };
		sscanf(response[i], "%63[^=]=%255[^\r\n]", key, val);
		set(& data, key, val);
		free(response[i++]);
	}
	free(response);

	session = value(& data, "session");
	if(!session || !strcmp(session, "FAILED")) {
		fputs("Authentication failed.\n", stderr);
		unset(& data, "session");
		return 0;
	}
	
	return !0;
}


int station(const char * stationURL) {
	char url[512] = { 0 }, * encodedURL = NULL, ** response;
	unsigned i = 0, retval = !0, regular = !0;
	const char * fmt;	
	const char * types[4] = {"play", "preview", "track", "playlist"};

	freelist(& playlist);

	if(!haskey(& data, "session")) {
		fputs("Not authenticated, yet.\n", stderr);
		return 0;
	}

	if(!stationURL)
		return 0;

	if(!strncasecmp(stationURL, "lastfm://", 9))
		stationURL += 9;

	/* Check if it's a static playlist of tracks or track previews. */
	for(i = 0; i < 4; ++i)
		if(!strncasecmp(types[i], url, strlen(types[i]))) {
			regular = 0;
			break;
		}

	/*
		If this is not a special "one-time" stream, it's a regular radio
		station and we request it using the good old /adjust.php URL.
		If it's not a regular stream, the reply of this request already is
		a XSPF playlist we have to parse.
	*/
	if(regular) {
		playlist.continuous = !0;
		fmt = "http://ws.audioscrobbler.com/radio/adjust.php?session=%s&url=%s";
	} else {
		playlist.continuous = 0;
		fmt =
			"http://ws.audioscrobbler.com/1.0/webclient/getresourceplaylist.php"
			"?sk=%s&url=%s&desktop=1";
	}

	encode(stationURL, & encodedURL);
	snprintf(url, sizeof(url), fmt, value(& data, "session"), encodedURL);
	free(encodedURL);

	if(!(response = fetch(url, NULL, NULL)))
		return 0;

	if(regular) {
		for(i = 0; response[i]; ++i) {
			char status[64] = { 0 };
			if(sscanf(response[i], "response=%63[^\r\n]", status) > 0)
				if(!strncmp(status, "FAILED", 6))
					retval = 0;
			free(response[i]);
		}
		free(response);
		
		if(!retval) {
			printf("Sorry, couldn't set station to %s.\n", stationURL);
			return 0;
		}

		expand(& playlist);
	} else {
		char * xml = NULL;
		unsigned length = 0;

		for(i = 0; response[i]; ++i) {
			xml = realloc(xml, sizeof(char) * (length + strlen(response[i]) + 1));
			strcpy(xml + length, response[i]);
			length += strlen(response[i]);
			xml[length] = 0;
		}

		freelist(& playlist);
		if(!parsexspf(& playlist, xml))
			retval = 0;
		free(xml);
	}
	
	stationChanged = !0;
	histapp(stationURL);

	if(currentStation)
		free(currentStation);

	currentStation = strdup(stationURL);

	if(retval && playfork) {
		stop = !0;
		kill(playfork, SIGKILL);
	}

	return retval;
}


/*
	Function: control

	Send control command to last.fm to skip/love/ban the currently played track
	or enable/disable recording tracks to profile.

	$0 = (const char *) one of "love", "ban", "rtp" and "nortp"
	
	return value: non-zero on success, zero on error
*/
int control(const char * cmd) {
	char url[512] = { 0 }, ** response;
	unsigned i = 0, retval = 0;
	const char * fmt =
		"http://ws.audioscrobbler.com/radio/control.php"
		"?session=%s"
		"&command=%s"
		"&debug=0";

	snprintf(url, sizeof(url), fmt, value(& data, "session"), cmd);
	if(!(response = fetch(url, NULL, NULL)))
		return 0;

	while(response[i]) {
		if(!strncmp(response[i], "response=OK", 11))
			retval = !0;

		free(response[i++]);
	}

	free(response);

	if(!strncmp("ban", cmd, 3) && retval && playfork)
		kill(playfork, SIGKILL);

	return retval;
}


int setdiscover(int enable) {
	char url[512] = { 0 };
	snprintf(url, sizeof(url), "lastfm://settings/discovery/%s", enable ? "on" : "off");
	return station(url);
}

int play(struct playlist * list) {
	assert(list != NULL);

	if(!list->left)
		return 0;

	if(playfork) {
		kill(playfork, SIGKILL);
		return !0;
	}

	playfork = fork();

	empty(& track);

	if(playfork) {
		unsigned i;
		char * keys [] = {
			"creator", "title", "album", "duration",
			"station", "lastfm:trackauth"
		};

		for(i = 0; i < (sizeof(keys) / sizeof(char *)); ++i)
			set(& track, keys[i], value(& list->track->track, keys[i]));
	} else {
		FILE * fd = NULL;
		const char * location = value(& list->track->track, "location");
		if(location != NULL) {
			fetch(location, & fd, NULL);

			empty(& data);
			freelist(list);

			if(fd != NULL)
				playback(fd);

			empty(& rc);
		}
		exit(0);
	}

	return !0;
}
