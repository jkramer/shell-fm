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
#include <stdint.h>
#include <stdlib.h>

#include "ropen.h"
#include "util.h"
#include "settings.h"

static int tunnel(FILE * fd, const char * host, unsigned short port)
{
	uint8_t greeting[3] = { 0x05, 0x01, 0x00 };
	uint8_t request[7+255] = { 0x05, 0x01, 0x00, 0x03 };
	size_t len;

	debug("sending SOCKS greeting\n");
	len = strlen(host);
	if(len > 255 ||
	   fwrite(greeting, 1, 3, fd) != 3 ||
	   fread(greeting, 1, 2, fd) != 2 ||
	   greeting[0] != 0x05 ||
	   greeting[1] != 0x00) {
		fprintf(stderr, "SOCKS greeting failed\n");
		return -1;
	}

	request[4] = len;
	memcpy(request + 5, host, len);
	len += 5;
	*(uint16_t *)(request + len) = htons(port);
	len += 2;

	debug("requesting SOCKS tunnel to %s:%u\n", host, port);
	if(fwrite(request, 1, len, fd) != len ||
	   fread(request, 1, 5, fd) != 5 ||
	   request[0] != 0x05 ||
	   request[1] != 0x00) {
		fprintf(stderr, "SOCKS tunnel request failed with status %u\n", request[1]);
		return -1;
	}

	switch(request[3]) {
	case 0x01:
		len = 3 + 2;
		break;
	case 0x03:
		len = request[4] + 2;
		break;
	case 0x04:
		len = 15 + 2;
		break;
	default:
		fprintf(stderr, "invalid address type %u in SOCKS response\n", request[3]);
		return -1;
	}

	if(fread(request + 5, 1, len, fd) != len) {
		fprintf(stderr, "SOCKS response failed\n");
		return -1;
	}

	return 0;
}

FILE * ropen(const char * host, unsigned short port) {
	int sck;
	struct hostent * entry;
	struct sockaddr_in addr;
	FILE * fd;
	int usesocks;
	char proxcpy[512 + 1];
	char * p;
	const char * connhost;
	unsigned short connport = 0;

	if(!host || !port) {
		fputs("No host/port given.\n", stderr);
		return NULL;
	}

	connhost = host;
	connport = port;

	usesocks = haskey(& rc, "socks-proxy");
	if(usesocks) {
		strncpy(proxcpy, value(& rc, "socks-proxy"), sizeof(proxcpy) - 1);
		proxcpy[sizeof(proxcpy) - 1] = '\0';
		connhost = proxcpy;
		p = strchr(connhost, ':');
		if(p) {
			*p = '\0';
			connport = atoi(p + 1);
		}
		if(connport == 0)
			connport = 1080;
	}

	debug("connecting to %s:%u\n", connhost, connport);
	if(!(entry = gethostbyname(connhost)) || !entry->h_addr) {
		fprintf(stderr, "%s.\n", hstrerror(h_errno));
		return NULL;
	}

	if(-1 == (sck = socket(PF_INET, SOCK_STREAM, PF_UNSPEC))) {
		fprintf(stderr, "Couldn't create socket. %s.\n", strerror(errno));
		return NULL;
	}

	memset(& addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = * (unsigned *) entry->h_addr;
	addr.sin_port = htons(connport);
	addr.sin_family = PF_INET;

	if(connect(sck, (struct sockaddr *) & addr, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Connection to %s:%u failed. %s.\n", connhost, connport, strerror(errno));
		return NULL;
	}

	if(!(fd = fdopen(sck, "w+"))) {
		fprintf(stderr, "Couldn't create file handle. %s.\n", strerror(errno));
		shutdown(sck, SHUT_RDWR);
		close(sck);
	}

	if(usesocks) {
		if(tunnel(fd, host, port) == -1) {
			fshutdown(& fd);
		}
	}

	return fd;
}

void fshutdown(FILE ** fd) {
	shutdown(fileno(* fd), SHUT_RDWR);
	fclose(* fd);
	* fd = NULL;
}
