/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>
#include <sys/un.h>

#include "sckif.h"
#include "http.h"
#include "service.h"
#include "interface.h"
#include "hash.h"
#include "submit.h"
#include "getln.h"
#include "tag.h"

#include "globals.h"

static int stcpsck = -1, sunixsck = -1;
static int waitread(int, unsigned, unsigned);
void oldtagstring(char *, unsigned, char);

#define REPLYBUFSIZE 1024

/* Open a TCP port for remote control. */
int tcpsock(const char * ip, unsigned short port) {
	static const int one = 1;
	struct sockaddr_in host;
	struct hostent * hostent;

	if(!ip || !port)
		return 0;

	/* Create socket. */
	if(-1 == (stcpsck = socket(AF_INET, SOCK_STREAM, PF_UNSPEC))) {
		fputs("Failed to create socket.\n", stderr);
		return 0;
	}

	/* Look up local address to bind socket to. */
	if(!(hostent = gethostbyname(ip))) {
		fprintf(stderr, "Failed to lookup host. %s.\n", hstrerror(h_errno));
		return 0;
	}

	host.sin_family = PF_INET;
	host.sin_port = htons(port);
	host.sin_addr.s_addr = * (unsigned *) hostent->h_addr;

	/* Prepare socket for incoming connections and make it re-usable. */
	if(-1 == setsockopt(stcpsck, SOL_SOCKET, SO_REUSEADDR, & one, sizeof one))
		fprintf(stderr, "Couldn't make socket re-usable. %s\n", strerror(errno));

	if(bind(stcpsck, (struct sockaddr *) & host, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Failed to bind socket. %s.\n", strerror(errno));
		return 0;
	}

	listen(stcpsck, 2);

	return !0;
}


/* Create a local (UNIX) socket for locale "remote" control. */
int unixsock(const char * path) {
	struct sockaddr_un host;

	if(path == NULL)
		return 0;


	if(!access(path, F_OK))
		unlink(path);


	if(-1 == (sunixsck = socket(AF_UNIX, SOCK_STREAM, PF_UNSPEC))) {
		fputs("Failed to create socket.\n", stderr);
		return 0;
	}


	memset(& host, 0, sizeof(struct sockaddr_un));
	strncpy(host.sun_path, path, sizeof(host.sun_path) - 1);
	host.sun_family = AF_UNIX;


	if(bind(sunixsck, (struct sockaddr *) & host, sizeof(struct sockaddr_un))) {
		fprintf(stderr, "Failed to bind socket. %s.\n", strerror(errno));
		return 0;
	}

	listen(sunixsck, 2);

	return !0;
}


/* Clean up sockets. */
void rmsckif(void) {
	if(stcpsck > 0) {
		close(stcpsck);
		stcpsck = -1;
	}

	if(sunixsck > 0) {
		close(sunixsck);
		unlink(value(& rc, "unix"));

		sunixsck = -1;
	}
}


/* Handle input on UNIX or TCP socket. */
void sckif(int timeout, int sck) {

	/* If the given socket is -1, try both sockets, unix and tcp. */
	if(sck == -1) {
		if(stcpsck != -1)
			sckif(timeout, stcpsck);

		if(sunixsck != -1)
			sckif(timeout, sunixsck);

		return;
	}

	signal(SIGPIPE, SIG_IGN);

	if(waitread(sck, timeout, 0)) {
		struct sockaddr client;
		socklen_t scksize = sizeof(struct sockaddr);

		int csck = accept(sck, & client, & scksize);

		if(-1 != csck) {
			if(waitread(csck, 0, 100000)) {
				FILE * fd = fdopen(csck, "r");

				if(fd) {
					char * line = NULL, * ptr = NULL, reply[REPLYBUFSIZE];
					unsigned size = 0;

					getln(& line, & size, fd);

					if(line && size > 0 && !ferror(fd)) {
						if((ptr = strchr(line, 13)) || (ptr = strchr(line, 10)))
							* ptr = 0;

						execcmd(line, reply);
					}

					if(line)
						free(line);

					if(strlen(reply)) {
						write(csck, reply, strlen(reply));
					}

					fclose(fd);
				}
			}

			shutdown(csck, SHUT_RDWR);
			close(csck);
		}
	}
}


/* Parse and execute a command received on a socket. */
void execcmd(const char * cmd, char * reply) {
	char arg[1024];
	unsigned ncmd;
	const char * known [] = {
		"play",
		"love",
		"ban",
		"skip",
		"quit",
		"info",
		"pause",
		"discovery",
		"tag-artist",
		"tag-album",
		"tag-track",
		"artist-tags",
		"album-tags",
		"track-tags",
		"stop",
	};

	memset(arg, 0, sizeof(arg));
	memset(reply, 0, REPLYBUFSIZE);

	for(ncmd = 0; ncmd < (sizeof(known) / sizeof(char *)); ++ncmd) {
		if(!strncmp(known[ncmd], cmd, strlen(known[ncmd])))
			break;
	}

	switch(ncmd) {
		/* Send error if the command isn't known. */
		case (sizeof(known) / sizeof(char *)):
			strncpy(reply, "UNKNOWN COMMAND", REPLYBUFSIZE);
			break;

		/* Play a station. */
		case 0:
			if(sscanf(cmd, "play %128[a-zA-Z0-9:/_ %,-]", arg) == 1) {
				char * url;
				decode(arg, & url);
				station(url);
				free(url);
			}
			break;

		/* Love track. */
		case 1:
			rate("L");
			break;

		/* Ban track. */
		case 2:
			rate("B");
			break;

		/* Skip track. */
		case 3:
			rate("S");
			break;

		/* Quit. */
		case 4:
			exit(EXIT_SUCCESS);

		/* Evaluate a given format string and return it. */
		case 5:
			if(* (cmd + 5))
				strncpy(reply, meta(cmd + 5, 0, & playlist.track->track), REPLYBUFSIZE);
			else if(haskey(& rc, "np-file-format"))
				strncpy(
					reply,
					meta(value(& rc, "np-file-format"), 0, & playlist.track->track),
					REPLYBUFSIZE
				);

			break;

		/* Pause. */
		case 6:
			if(playthread) {
				if(pausetime) {
					pauselength += time(NULL) - pausetime;
					pausetime = 0;

					pthread_mutex_unlock(& paused);
				}
				else {
					time(& pausetime);
					pthread_mutex_lock(& paused);
				}
			}
			break;

		/* Toggle discovery mode. */
		case 7:
			toggle(DISCOVERY);
			break;

		/* Tag artist of the currently played track. */
		case 8:
			if(sscanf(cmd, "tag-artist %128s", arg) == 1)
				sendtag('a', arg, playlist.track->track);
			break;

		/* Tag album of the currently played track. */
		case 9:
			if(sscanf(cmd, "tag-album %128s", arg) == 1)
				sendtag('l', arg, playlist.track->track);
			break;

		/* Tag currently played track. */
		case 10:
			if(sscanf(cmd, "tag-track %128s", arg) == 1)
				sendtag('t', arg, playlist.track->track);
			break;

		/* Get artist tags. */
		case 11:
			oldtagstring(reply, REPLYBUFSIZE, 'a');
			break;

		/* Get album tags. */
		case 12:
			oldtagstring(reply, REPLYBUFSIZE, 'l');
			break;

		/* Get track tags. */
		case 13:
			oldtagstring(reply, REPLYBUFSIZE, 't');
			break;

		/* Stop station. */
		case 14:
			if(playthread) {
				enable(STOPPED);
				pthread_kill(playthread, SIGUSR1);
			}
			break;
	}
}


/* Wait for incoming data on the given file descriptor. */
static int waitread(int fd, unsigned sec, unsigned usec) {
	fd_set readfd;
	struct timeval tv;

	FD_ZERO(& readfd);
	FD_SET(fd, & readfd);

	tv.tv_sec = sec;
	tv.tv_usec = usec;
	
	return (select(fd + 1, & readfd, NULL, NULL, & tv) > 0);
}


/* Get old tags and copy the string into the reply buffer. */
void oldtagstring(char * reply, unsigned size, char type) {
	char * tagstring;

	if((tagstring = oldtags(type, playlist.track->track)) != NULL) {
		strncpy(reply, tagstring, size);
		free(tagstring);
	}
}
