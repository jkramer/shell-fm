/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

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

#include <openssl/md5.h>

#include "include/hash.h"
#include "include/http.h"
#include "include/play.h"
#include "include/settings.h"

extern int stationChanged;

struct hash data; /* Warning! MUST be bzero'd ASAP or we're all gonna die! */
pid_t playfork = 0; /* PID of the decoding & playing process, if running */

char * currentStation = NULL;


/*
	Function: handshake
	
	Authenticate and create session by sending username and
	password to the handshake page.
	
	$0 = (const char *) username
	$1 = (const char *) password

	return value: non-zero on success, zero on error
*/
int handshake(const char * username, const char * password) {
	unsigned char * md5;
	char hexmd5[32 + 1] = { 0 }, url[512] = { 0 }, ** response;
	unsigned ndigit, i = 0;
	const char * session, * fmt =
		"http://ws.audioscrobbler.com/radio/handshake.php"
		"?version=0.1"
		"&platform=linux"
		"&username=%s"
		"&passwordmd5=%s"
		"&debug=0";
	
	memset(& data, 0, sizeof(struct hash));
	
	/* let openSSL create the hash, then convert to ASCII */
	md5 = MD5((unsigned char *) password, strlen(password), NULL);
	for(ndigit = 0; ndigit < 16; ++ndigit)
		sprintf(2 * ndigit + hexmd5, "%02x", md5[ndigit]);

	set(& rc, "password", hexmd5);
	
	/* put handshake URL together and fetch initial data from server */
	snprintf(url, sizeof(url), fmt, username, hexmd5);
	response = fetch(url, NULL, NULL, NULL);
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

/*
	Function: station
	
	Tell last.fm to stream the station specified by the given
	station identifier (an URL starting with "lastfm://").

	$0 = (const char *) radio URL ("lastfm://...")

	return value: non-zero on success, zero on error
*/
int station(const char * stationURL) {
	char url[512] = { 0 }, * encodedURL = NULL, ** response;
	unsigned i = 0, retval = !0;
	const char * fmt =
    "http://ws.audioscrobbler.com/radio/adjust.php"
		"?session=%s"
		"&url=%s"
		"&debug=0";
	
	if(!haskey(& data, "session")) {
		fputs("Not authenticated, yet.\n", stderr);
		return 0;
	}

	if(!stationURL)
		return 0;
	
	encode(stationURL, & encodedURL);
	snprintf(url, sizeof(url), fmt, value(& data, "session"), encodedURL);
	free(encodedURL);

	response = fetch(url, NULL, NULL, NULL);
	while(response[i]) {
		char status[64] = { 0 };
		if(sscanf(response[i], "response=%63[^\r\n]", status) > 0)
			if(!strncmp(status, "FAILED", 6))
				retval = 0;
		free(response[i++]);
	}
	free(response);
	
	retval
		? (stationChanged = !0)
		: printf("Sorry, couldn't set station to %s.\n", stationURL);

	if(!playfork) {
		pid_t pid = fork();
		if(pid)
			playfork = pid;
		else {
			int i;
			FILE * fd = NULL;
			for (i = 3; i < FD_SETSIZE; i++)
				close(i);
			signal(SIGINT, SIG_IGN);
			fetch(value(& data, "stream_url"), & fd, NULL, NULL);
			playback(fd);
			fclose(fd);
			exit(EXIT_SUCCESS);
		}
	}

	if(currentStation)
		free(currentStation);

	currentStation = strdup(stationURL);

	return retval;
}

/*
	Function: update

	Fetch meta data of currently streamed song from last.fm and
	store them in the hash structure given by reference.

	$0 = pointer to hash structure

	return value: non-zero on success, zero on error
*/
int update(struct hash * track) {
	const char * fmt =
		"http://ws.audioscrobbler.com/radio/np.php?session=%s&debug=0";
	char url[512] = { 0 }, ** response;
	unsigned i = 0;
	
	assert(track);
	snprintf(url, sizeof(url), fmt, value(& data, "session"));
	
	if(!(response = fetch(url, NULL, NULL, NULL)))
		return 0;

	while(response[i]) {
		char key[64] = { 0 }, value[256] = { 0 };
		if(sscanf(response[i], "%63[^=]=%255[^\r\n]", key, value) > 0)
			set(track, key, value);
		free(response[i++]);
	}
	free(response);

	return !0;
}

/*
	Function: control

	Send control command to last.fm to skip/love/ban the currently played track
	or enable/disable recording tracks to profile.

	$0 = (const char *) one of "love", "skip", "ban", "rtp" and "nortp"
	
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
	if(!(response = fetch(url, NULL, NULL, NULL)))
		return 0;

	while(response[i]) {
		if(!strncmp(response[i], "response=OK", 11))
			retval = !0;
		free(response[i++]);
	}
	free(response);

	return retval;
}

int setdiscover(int enable) {
	char url[512] = { 0 };
	snprintf(url, sizeof(url), "lastfm://settings/discovery/%s", enable ? "on" : "off");
	return station(url);
}
