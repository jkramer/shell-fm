/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
	
	Shell.FM - sckif.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <config.h>

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

extern unsigned getln(char **, unsigned *, FILE *);
extern int interactive;
extern unsigned discovery;
extern pid_t playfork;

static int ssck = -1;

int waitread(int, unsigned, unsigned);

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

void sckif(void) {
	if(ssck != -1 && waitread(ssck, 0, 0)) {
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
						(ptr = strchr(line, 13)) && (* ptr = 0);
						(ptr = strchr(line, 10)) && (* ptr = 0);
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
	char arg[1024] = { 0 };
	register unsigned ncmd;
	const char * known [] = {
		"play",
		"love",
		"ban",
		"skip",
		"rtp",
		"nortp",
		"quit",
		"info",
		"discovery",
		"stop"
	};

	for(ncmd = 0; ncmd < (sizeof(known) / sizeof(char *)); ++ncmd)
		if(!strncmp(known[ncmd], cmd, strlen(known[ncmd])))
			break;

	switch(ncmd) {
		case (sizeof(known) / sizeof(char *)):
			fprintf(fd, "Unknown command!\n");
			break;

		case 0:
			if(sscanf(cmd, "play %128[a-zA-Z0-9:/_ %,-]", arg) == 1) {
				char *station_str;
				decode(arg, & station_str);
				station(station_str);
				free(station_str);
			}
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

		case 7:
			fprintf(fd, "%s\n", meta(cmd + 5, 0));
			break;

		case 8:
			if (setdiscover(discovery = !discovery))
				fprintf(fd, "%s discovery mode.\n", discovery ? "Enabled" : "Disabled");
			else
				fprintf(fd, "Failed to %s discovery mode.\n", discovery ? "enable" : "disable");
			break;
		case 9:
			if(playfork)
				kill(playfork, SIGKILL);
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
