/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
	
	Shell.FM - sckif.c
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>

#include "sckif.h"
#include "http.h"
#include "service.h"
#include "interface.h"
#include "hash.h"
#include "submit.h"
#include "getln.h"

extern unsigned discovery;
extern pid_t playfork;

extern struct hash rc;

static int ssck = -1;

int waitread(int, unsigned, unsigned);
extern void rate(const char *);


int mksckif(const char * ip, unsigned short port) {
	static const int one = 1;
	struct sockaddr_in host;
	struct hostent * hostent;

	if(!ip || !port)
		return 0;

	if(-1 == (ssck = socket(AF_INET, SOCK_STREAM, PF_UNSPEC))) {
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

	if (-1 == setsockopt(ssck, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one))
		fprintf(stderr, "Failed to set SO_REUSEADDR socket option. %s\n", strerror(errno));

	if(bind(ssck, (struct sockaddr *) & host, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Failed to bind socket. %s.\n", strerror(errno));
		return 0;
	}

	listen(ssck, 0x2);

	return !0;
}

void rmsckif(void) {
	if(ssck > 0) {
		close(ssck);
		ssck = -1;
	}
}

void sckif(int timeout) {
	if(ssck != -1 && waitread(ssck, timeout, 0)) {
		struct sockaddr_in client;
		socklen_t scksize = sizeof(struct sockaddr_in);

		int csck = accept(ssck, & client, & scksize);
		if(-1 != csck) {
			if(waitread(csck, 0, 100000)) {
				FILE * fd = fdopen(csck, "r+");

				if(fd) {
					char * line = NULL, * ptr = NULL;
					unsigned size = 0;
					getln(& line, & size, fd);

					if(line && size > 0) {
						if((ptr = strchr(line, 13)) != NULL)
							* ptr = 0;

						if((ptr = strchr(line, 10)) != NULL)
							* ptr = 0;

						execcmd(line, fd);
					}

					if(line)
						free(line);

					fclose(fd);
				}
			}
			shutdown(csck, SHUT_RDWR);
			close(csck);
		}
	}
}

void execcmd(const char * cmd, FILE * fd) {
	char arg[1024];
	register unsigned ncmd;
	const char * known [] = {
		"play",
		"love",
		"ban",
		"skip",
		"quit",
		"info",
	};

	memset(arg, sizeof(arg), 0);

	for(ncmd = 0; ncmd < (sizeof(known) / sizeof(char *)); ++ncmd)
		if(!strncmp(known[ncmd], cmd, strlen(known[ncmd])))
			break;

	switch(ncmd) {
		case (sizeof(known) / sizeof(char *)):
			fprintf(fd, "Unknown command!\n");
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
		case 2:
			control(known[ncmd]);
			break;

		case 3:
			if(playfork) {
				rate("S");
				kill(playfork, SIGKILL);
			}
			break;

		case 4:
			exit(EXIT_SUCCESS);

		case 5:
			snprintf(arg, sizeof(arg), "%s", meta(cmd + 5, 0));
			if(!strlen(arg) && haskey(& rc, "np-file-format"))
				snprintf(arg, sizeof(arg), "%s", meta(value(& rc, "np-file-format"), 0));
			fprintf(fd, "%s\n", arg);
			break;
	}
	fflush(fd);
}

int waitread(int fd, unsigned sec, unsigned usec) {
	fd_set readfd;
	struct timeval tv;

	FD_ZERO(& readfd);
	FD_SET(fd, & readfd);

	tv.tv_sec = sec;
	tv.tv_usec = usec;
	
	return (select(fd + 1, & readfd, NULL, NULL, & tv) > 0);
}
