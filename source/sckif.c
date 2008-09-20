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

struct hash track;

static int stcpsck = -1, sunixsck = -1;
static int waitread(int, unsigned, unsigned);

#define REPLYBUFSIZE 1024

int tcpsock(const char * ip, unsigned short port) {
	static const int one = 1;
	struct sockaddr_in host;
	struct hostent * hostent;

	if(!ip || !port)
		return 0;

	if(-1 == (stcpsck = socket(AF_INET, SOCK_STREAM, PF_UNSPEC))) {
		fputs("Failed to create socket.\n", stderr);
		return 0;
	}

	if(!(hostent = gethostbyname(ip))) {
		fprintf(stderr, "Failed to lookup host. %s.\n", hstrerror(h_errno));
		return 0;
	}

	host.sin_family = PF_INET;
	host.sin_port = htons(port);
	host.sin_addr.s_addr = * (unsigned *) hostent->h_addr;

	if(-1 == setsockopt(stcpsck, SOL_SOCKET, SO_REUSEADDR, & one, sizeof one))
		fprintf(stderr, "Couldn't make socket re-usable. %s\n", strerror(errno));

	if(bind(stcpsck, (struct sockaddr *) & host, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Failed to bind socket. %s.\n", strerror(errno));
		return 0;
	}

	listen(stcpsck, 2);

	return !0;
}


int unixsock(const char * path) {
	struct sockaddr_un host;

	if(path == NULL)
		return 0;


	if(!access(path, F_OK)) {
		fprintf(stderr, "%s already existing. UNIX socket not created.\n", path);
		return 0;
	}


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


void rmsckif(void) {
	if(stcpsck > 0) {
		close(stcpsck);
		stcpsck = -1;
	}

	if(sunixsck > 0) {
		close(sunixsck);
		sunixsck = -1;
	}
}


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

void execcmd(const char * cmd, char * reply) {
	char arg[1024], * ptr;
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
		case (sizeof(known) / sizeof(char *)):
			strncpy(reply, "ERROR", REPLYBUFSIZE);
			break;

		case 0:
			if(sscanf(cmd, "play %128[a-zA-Z0-9:/_ %,-]", arg) == 1) {
				char * url;
				decode(arg, & url);
				station(url);
				free(url);
			}
			break;

		case 1:
			rate("L");
			break;

		case 2:
			rate("B");
			break;

		case 3:
			rate("S");
			break;

		case 4:
			exit(EXIT_SUCCESS);

		case 5:
			if(* (cmd + 5))
				strncpy(reply, meta(cmd + 5, 0, & track), REPLYBUFSIZE);
			else if(haskey(& rc, "np-file-format"))
				strncpy(
					reply,
					meta(value(& rc, "np-file-format"), 0, & track),
					REPLYBUFSIZE
				);

			break;

		case 6:
			if(playfork) {
				if(pausetime) {
					kill(playfork, SIGCONT);
				}
				else {
					time(& pausetime);
					kill(playfork, SIGSTOP);
				}
			}
			break;

		case 7:
			toggle(DISCOVERY);
			break;

		case 8:
			if(sscanf(cmd, "tag-artist %128s", arg) == 1)
				sendtag('a', arg, track);
			break;

		case 9:
			if(sscanf(cmd, "tag-album %128s", arg) == 1)
				sendtag('l', arg, track);
			break;

		case 10:
			if(sscanf(cmd, "tag-track %128s", arg) == 1)
				sendtag('t', arg, track);
			break;

		case 11:
			if((ptr = oldtags('a', track)) != NULL) {
				strncpy(reply, ptr, REPLYBUFSIZE);
				free(ptr);
				ptr = NULL;
			}
			break;

		case 12:
			if((ptr = oldtags('l', track)) != NULL) {
				strncpy(reply, ptr, REPLYBUFSIZE);
				free(ptr);
				ptr = NULL;
			}
			break;

		case 13:
			if((ptr = oldtags('t', track)) != NULL) {
				strncpy(reply, ptr, REPLYBUFSIZE);
				free(ptr);
				ptr = NULL;
			}
			break;

		case 14:
			if(playfork) {
				enable(STOPPED);
				kill(playfork, SIGUSR1);
			}
			break;
	}
}

static int waitread(int fd, unsigned sec, unsigned usec) {
	fd_set readfd;
	struct timeval tv;

	FD_ZERO(& readfd);
	FD_SET(fd, & readfd);

	tv.tv_sec = sec;
	tv.tv_usec = usec;
	
	return (select(fd + 1, & readfd, NULL, NULL, & tv) > 0);
}
