/*
	vim:syntax=c tabstop=2 shiftwidth=2

	Shell.FM - http.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include <sys/socket.h>

#include "include/version.h"

extern FILE * ropen(const char *, unsigned short);
extern void fshutdown(FILE *);

char ** fetch(char * const url, FILE ** pHandle) {
	char ** resp = NULL, * host, * file, * port;
	unsigned short nport = 80;
	unsigned nline = 0, size = 0, nstatus = 0, length;
	char * status = NULL;
	FILE * fd;

	if(pHandle)
		* pHandle = NULL;

	host = & url[strncmp(url, "http://", 7) ? 0 : 7];
	port = strchr(host, 0x3A);
	file = strchr(port ? port : host, 0x2F);
	* (file++) = (char) 0;
	if(port) {
		char * ptr = NULL;
		nport = strtol(port, & ptr, 10);
		(ptr == port) && (nport = 80);
		* port = (char) 0;
	}

	if(!(fd = ropen(host, nport)))
		return NULL;
	
	fprintf(fd, "GET /%s HTTP/1.1\r\n", file ? file : "");
	fprintf(fd, "Host: %s\r\n", host);
	fprintf(fd, "User-Agent: Shell-FM v0.1.2\r\n\r\n");
	fflush(fd);

	length = getline(& status, & size, fd);
	if(length >= 12)
		sscanf(status, "HTTP/%*f %u", & nstatus);

	if(nstatus != 200 && nstatus != 301) {
		fshutdown(fd);
		fprintf(stderr, "%s", status);
		return NULL;
	}

	while(!0) {
		char * line = NULL;
		size = 0;
		if(getline(& line, & size, fd) <= 2) {
			if(size)
				free(line);
			break;
		}
		if(nstatus == 301 && !strncasecmp(line, "Location: ", 10)) {
			char newurl[512 + 1] = { 0 };
			sscanf(line, "Location: %512[^\r\n]", newurl);
			free(line);
			fshutdown(fd);
			return fetch(newurl, pHandle);
		}
		free(line);
	}

	if(pHandle) {
		* pHandle = fd;
		return NULL;
	}
	
	while(!feof(fd)) {
		resp = realloc(resp, (nline + 2) * sizeof(char *));
		assert(resp != NULL);

		resp[nline] = NULL, size = 0;
		getline(& resp[nline], & size, fd);
		resp[++nline] = NULL;
	}
	
	fshutdown(fd);
	return resp;
}

unsigned encode(const char * orig, char ** encoded) {
	register unsigned i = 0, x = 0;
	* encoded = calloc((strlen(orig) * 3) + 1, sizeof(char));
	memset(* encoded, 0, (strlen(orig) * 3) + 1);
	while(i < strlen(orig)) {
		if(isalnum(orig[i]))
			(* encoded)[x++] = orig[i];
		else {
			sprintf((* encoded) + x, "%%%02x", orig[i]);
			x += 3;
		}
		++i;
	}
	return x;
}
