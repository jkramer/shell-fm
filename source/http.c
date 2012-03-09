/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <sys/stat.h>

#include "hash.h"
#include "history.h"
#include "settings.h"
#include "getln.h"
#include "http.h"
#include "ropen.h"
#include "util.h"
#include "globals.h"


#ifndef USERAGENT
#define USERAGENT "Shell.FM/" PACKAGE_VERSION
#endif


char ** fetch(const char * url, FILE ** handle, const char * post, const char * type) {
	char ** resp = NULL, * host, * file, * port, * status = NULL, * line = NULL;
	char * connhost;
	char urlcpy[512 + 1];
	unsigned short nport = 80, chunked = 0;
	unsigned nstatus = 0, size = 0;
	signed valid_head = 0;
	FILE * fd;
	int useproxy;

	const char * head_format =
		"%s /%s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: " USERAGENT "\r\n"
		"Cookie: Session=%s\r\n"
		"Connection: close\r\n";

	const char * proxiedhead_format =
		"%s http://%s/%s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: " USERAGENT "\r\n"
		"Cookie: Session=%s\r\n";

	if(!batch && !enabled(QUIET))
		fputs("...\r", stderr);

	useproxy = haskey(& rc, "proxy");
	if(type == NULL)
		type = "application/x-www-form-urlencoded";

	if(handle)
		* handle = NULL;

	strncpy(urlcpy, url, sizeof(urlcpy) - 1);

	host = & urlcpy[strncmp(urlcpy, "http://", 7) ? 0 : 7];
	connhost = host;

	if(useproxy) {
		char proxcpy[512 + 1];
		memset(proxcpy, (char) 0, sizeof(proxcpy));
		strncpy(proxcpy, value(& rc, "proxy"), sizeof(proxcpy) - 1);
		connhost = & proxcpy[strncmp(proxcpy, "http://", 7) ? 0 : 7];
	}

	port = strchr(connhost, 0x3A);
	file = strchr(host, 0x2F);
	fflush(stderr);
	* file = (char) 0;
	++file;

	if(port && port < file) {
		char * ptr = NULL;
		* port = (char) 0;
		++port;
		nport = (unsigned short) strtol(port, & ptr, 10);
		if(ptr == port)
			nport = 80;
	}

	if(!(fd = ropen(connhost, nport)))
		return NULL;

	if(useproxy)
		fprintf(fd, proxiedhead_format, post ? "POST" : "GET", host,
				file ? file : "", host, value(& data, "session"));
	else
		fprintf(fd, head_format, post ? "POST" : "GET", file ? file : "", host, value(& data, "session"));

	if(post != NULL) {
		fprintf(fd, "Content-Type: %s\r\n", type);
		fprintf(fd, "Content-Length: %ld\r\n\r\n%s\r\n", (long) strlen(post), post);
	}

	fputs("\r\n", fd);
	fflush(fd);

	if(getln(& status, & size, fd) >= 12)
		valid_head = sscanf(status, "HTTP/%*f %u", & nstatus);

	if(nstatus != 200 && nstatus != 301 && nstatus != 302) {
		fshutdown(& fd);
		if(size) {
			if(valid_head != 2)
				fprintf(stderr, "Invalid HTTP: %s  from: %s\n", status, url);
			else
				fprintf(stderr, "HTTP Response: %s", status);

		}

		freeln(& status, & size);

		return NULL;
	}

	freeln(& status, & size);

	while(!0) {
		if(getln(& line, & size, fd) < 3)
			break;

		if(!strncasecmp(line, "Transfer-Encoding: chunked", 26))
			chunked = !0;

		if((nstatus == 301 || nstatus == 302) && !strncasecmp(line, "Location: ", 10)) {
			char newurl[512 + 1];
			memset(newurl, 0, sizeof(newurl));
			sscanf(line, "Location: %512[^\r\n]", newurl);
			fshutdown(& fd);

			return fetch(newurl, handle, post, type);
		}
	}

	freeln(& line, & size);

	if(handle) {
		* handle = fd;
		if(!batch && !enabled(QUIET))
			fputs("\r   \r", stderr);

		return NULL;
	}

	resp = (chunked) ? read_response_chunked(fd) : read_response(fd);

	fshutdown(& fd);

	if(!batch && !enabled(QUIET))
		fputs("\r   \r", stderr);
	return resp;
}

char ** read_response(FILE * fd) {
	char * line, ** resp = NULL;
	unsigned size, nline = 0;

	while(!feof(fd)) {
		line = NULL;
		size = 0;

		if(getln(& line, & size, fd)) {
			char * ptr = strchr(line, 10);

			if(ptr != NULL)
				* ptr = (char) 0;

			resp = realloc(resp, (nline + 2) * sizeof(char *));
			assert(resp != NULL);

			resp[nline] = line;
			resp[++nline] = NULL;
		} else if(size)
			free(line);
	}

	return resp;
}

/* Re-assemble a chunked response */
char ** read_response_chunked(FILE * fd) {
	char * line, * pos = NULL, * ptr, * chunks = NULL, ** resp = NULL;
	unsigned size, chunk_size, response_size = 0, nlines = 0;

	/* Read all chunks */
	while(!feof(fd)) {
		line = NULL;
		size = 0;

		if(getln(& line, & size, fd)) {
			ptr = strchr(line, 10);

			if(ptr != NULL)
				* ptr = (char) 0;

			/* chunk_size = 0 means no more chunks follow */
			if (sscanf(line, "%x", &chunk_size) == 1 && chunk_size != 0) {
				response_size += chunk_size;

				chunks = realloc(chunks, response_size * sizeof(char));
				assert(chunks != NULL);

				if (pos == NULL)
					pos = chunks;

				/* We don't have to worry about reading the CRLF after
				 * chunks, since getln() will read past them */
				if (fread(pos, 1, chunk_size, fd) != chunk_size)
				{
					debug("Couldn't read chunks.\n");
					break;
				}

				pos += chunk_size;
			} else if (chunk_size == 0)
				break; /* Ignore trailing headers */
		} else if(size)
			free(line);
	}

	/* Break response into lines */
	pos = chunks;
	while (pos != NULL && pos < (chunks + response_size)) {
		resp = realloc(resp, (nlines + 2) * sizeof(char *));
		assert(resp != NULL);

		resp[nlines] = pos;
		resp[++nlines] = NULL;

		pos = strchr(pos, '\n');
		if (pos != NULL)
			*pos = '\0';

		/* Set the pointer to just after the previous line or NULL if at the end */
		pos = (pos != NULL && pos < (chunks + response_size)) ? pos + 1 : NULL;
	}

	return resp;
}

unsigned encode(const char * orig, char ** encoded) {
	register unsigned i = 0, x = 0;

	* encoded = calloc((strlen(orig) * 3) + 1, sizeof(char));

	assert(* encoded != NULL);

	while(i < strlen(orig)) {
		if(isalnum(orig[i]) || orig[i] == ' ')
			(* encoded)[x++] = orig[i];
		else {
			snprintf(
					(* encoded) + x,
					strlen(orig) * 3 - strlen(* encoded) + 1,
					"%%%02X",
					(uint8_t) orig[i]
			);
			x += 3;
		}
		++i;
	}

	for(i = 0; i < x; ++i)
		if((* encoded)[i] == ' ')
			(* encoded)[i] = '+';

	return x;
}

unsigned decode(const char * orig, char ** decoded) {
	register unsigned i = 0, x = 0;
	const unsigned len = strlen(orig);

	* decoded = calloc(len + 1, sizeof(char));

	assert(decoded != NULL);

	while(i < len) {
		if(orig[i] != '%')
			(* decoded)[x] = orig[i];
		else {
			unsigned hex;
			if(sscanf(orig + i, "%%%02x", & hex) != 1)
				(* decoded)[x] = orig[i];
			else {
				(* decoded)[x] = (char) hex;
				i += 2;
			}
		}

		++i;
		++x;
	}

	* decoded = realloc(* decoded, (x + 1) * sizeof(char));

	assert(decoded != NULL);

	for(i = 0; i < x; ++i)
		if((* decoded)[i] == '+')
			(* decoded)[i] = ' ';

	return x;
}

void freeln(char ** line, unsigned * size) {
	if(size)
		* size = 0;

	if(line && * line) {
		free(* line);
		* line = NULL;
	}
}


void unhtml(char * html) {
	unsigned i;
	const char * codes [] = {
		"&amp;", "&",
		"&lt;", "<",
		"&gt;", ">",
		"&nbsp;", " ",
		"&quot;", "\"",
		"&apos;", "\'",
	};

	for(i = 0; i < (sizeof(codes) / sizeof(char *)); i += 2) {
		unsigned length = strlen(codes[i]);
		char * ptr;
		while((ptr = strcasestr(html, codes[i])) != NULL) {
			* ptr = codes[i + 1][0];
			memmove(ptr + 1, ptr + length, strlen(ptr + length));
			* (ptr + strlen(ptr) - length + 1) = 0;
		}
	}
}


const char * makeurl(const char * fmt, ...) {
	static char url[512];
	const char * src = fmt;
	char * ptr = NULL;
	unsigned pos = 0;
	va_list argv;

	va_start(argv, fmt);

	memset(url, 0, sizeof(url));

	while(* src && pos < sizeof(url) - 1) {
		if(* src != '%')
			url[pos++] = * (src++);
		else if(* (src + 1)) {
			switch(* (++src)) {
				case '%':
					url[pos] = '%';
					break;

				case 's':
					encode(va_arg(argv, char *), & ptr);
					pos += snprintf(& url[pos], sizeof(url) - pos, "%s", ptr);
					free(ptr);
					ptr = NULL;
					break;

				case 'i':
					pos += sprintf(& url[pos], "%d", va_arg(argv, int));
					break;

				case 'u':
					pos += sprintf(& url[pos], "%u", va_arg(argv, unsigned));
					break;
			}
			++src;
		}
	}

	return url;
}

char ** cache(const char * url, const char * name, int refresh) {
	time_t expiry = 60 * 60 * 24;
	char path[4096];

	if(haskey(& rc, "expiry"))
		expiry = atoi(value(& rc, "expiry"));

	memset(path, (char) 0, sizeof(path));
	strncpy(path, rcpath("cache"), sizeof(path));
	if(access(path, W_OK | X_OK))
		mkdir(path, 0700);

	snprintf(path, sizeof(path), "%s/%s", rcpath("cache"), name);

	if(!refresh) {
		if(access(path, R_OK | W_OK))
			refresh = !0;
		else {
			time_t now = time(NULL);
			struct stat status;

			stat(path, & status);
			if(status.st_mtime < now - expiry)
				refresh = !0;
		}
	}

	if(!refresh)
		return slurp(path);
	else {
		char ** data = fetch(url, NULL, NULL, NULL);
		if(data) {
			FILE * fd = fopen(path, "w");
			if(fd != NULL) {
				unsigned line = 0;
				while(data[line])
					fprintf(fd, "%s\n", data[line++]);
				fclose(fd);
			} else {
				fputs("Couldn't write cache.\n", stderr);
			}
		}

		return data;
	}
}
