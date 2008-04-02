/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ropen.h"

FILE * ropen(const char * host, unsigned short port) {
	int sck;
	struct hostent * entry;
	struct sockaddr_in addr;
	FILE * fd;

	if(!host || !port) {
		fputs("No host/port given.\n", stderr);
		return NULL;
	}
	
	if(!(entry = gethostbyname(host)) || !entry->h_addr) {
		fprintf(stderr, "%s.\n", hstrerror(h_errno));
		return NULL;
	}

	if(-1 == (sck = socket(PF_INET, SOCK_STREAM, PF_UNSPEC))) {
		fprintf(stderr, "Couldn't create socket. %s.\n", strerror(errno));
		return NULL;
	}

	memset(& addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = * (unsigned *) entry->h_addr;
	addr.sin_port = htons(port);
	addr.sin_family = PF_INET;

	if(connect(sck, (struct sockaddr *) & addr, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Connection failed. %s.\n", strerror(errno));
		return NULL;
	}

	if(!(fd = fdopen(sck, "w+"))) {
		fprintf(stderr, "Couldn't create file handle. %s.\n", strerror(errno));
		shutdown(sck, SHUT_RDWR);
		close(sck);
	}

	return fd;
}

void fshutdown(FILE ** fd) {
	shutdown(fileno(* fd), SHUT_RDWR);
	fclose(* fd);
	* fd = NULL;
}
