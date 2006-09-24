/*
	vim:syntax=c tabstop=2 shiftwidth=2
	
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
#include <sys/stat.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>

#include "include/sckif.h"
#include "include/service.h"

extern unsigned getln(char **, unsigned *, FILE *);
extern int interactive;

static int ssck = -1;

int waitread(int, unsigned, unsigned);

int mksckif(const char * ip, unsigned short port) {
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

void sckif(void) {
	if(ssck != -1 && waitread(ssck, 0, 500)) {
		struct sockaddr_in client;
		socklen_t scksize = sizeof(struct sockaddr_in);

		int csck = accept(ssck, & client, & scksize);
		if(-1 != csck) {
			if(waitread(csck, 0, 500)) {
				FILE * fd = fdopen(csck, "r+");

				if(fd) {
					char * line = NULL, * ptr = NULL;
					unsigned size = 0;
					getln(& line, & size, fd);

					if(line && size > 0) {
						(ptr = strchr(line, 13)) && (* ptr = 0);
						(ptr = strchr(line, 10)) && (* ptr = 0);
						execcmd(line);
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

void execcmd(const char * cmd) {
	char arg[1024] = { 0 };
	register unsigned ncmd;
	const char * known [] = {
		"play",
		"love",
		"ban",
		"skip",
		"rtp",
		"nortp",
		"quit"
	};

	for(ncmd = 0; ncmd < (sizeof(known) / sizeof(char *)); ++ncmd)
		if(!strncmp(known[ncmd], cmd, strlen(known[ncmd])))
			break;

	switch(ncmd) {
		case (sizeof(known) / sizeof(char *)):
			break;

		case 0:
			if(sscanf(cmd, "play %128[a-zA-Z0-9:/_ -,-]", arg) == 1)
				station(arg);
			break;

		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			control(known[ncmd]);
			break;

		case 6:
			exit(EXIT_SUCCESS);
	}
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
