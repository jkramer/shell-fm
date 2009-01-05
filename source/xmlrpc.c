/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "md5.h"
#include "settings.h"
#include "http.h"

extern void purge(char **);

static void append(char **, unsigned *, const char *);
static void sparam(char **, unsigned *, const char *);
static void aparam(char **, unsigned *, const char **);
char * xmlize(const char *);

int xmlrpc(const char * method, const char * fmt, ...) {
	unsigned size = 1024, narg, x;
	const unsigned char * md5;
	va_list argv;
	int result = 0;

	char
		* xml = malloc(size),
		** resp, * url = "http://ws.audioscrobbler.com/1.0/rw/xmlrpc.php",
		md5hex[32 + 1] = { 0 }, tmp[32 + 8 + 1] = { 0 };

	assert(xml != NULL);

	const char
		* challenge = "Shell.FM",
		* xmlhead =
			"<?xml version=\"1.0\"?>\n"
			"<methodCall>\n"
			"\t<methodName>%s</methodName>\n"
			"\t<params>\n";


	/* generate password/challenge hash */
	snprintf(tmp, sizeof(tmp), "%s%s", value(& rc, "password"), challenge);
	md5 = MD5((unsigned char *) tmp, sizeof(tmp) - 1);
	for(x = 0; x < 16; ++x)
		sprintf(2 * x + md5hex, "%02x", md5[x]);

	va_start(argv, fmt);
	memset(xml, (char) 0, size);

	snprintf(xml, size, xmlhead, method);

	sparam(& xml, & size, value(& rc, "username"));
	sparam(& xml, & size, challenge);
	sparam(& xml, & size, md5hex);

	for(narg = 0; narg < strlen(fmt); ++narg) {
		const char * string, ** array;

		switch(fmt[narg]) {
			case 's': /* string */
				string = va_arg(argv, const char *);
				sparam(& xml, & size, string);
				break;

			case 'a': /* array */
				array = va_arg(argv, const char **);
				aparam(& xml, & size, array);
				break;
		}
	}

	append(& xml, & size, "\t</params>\n</methodCall>\n");

	if((resp = fetch(url, NULL, xml, "text/xml")) != NULL) {
		for(x = 0; resp[x]; ++x) {
			if(strstr(resp[x], ">OK<") != NULL)
				result = !0;
		}

		purge(resp);
	}

	free(xml);

	return result;
}


static void append(char ** string, unsigned * size, const char * appendix) {
	if(string && size && appendix) {
		char * copy;

		* size = strlen(* string) + strlen(appendix) + 1;
		copy = realloc(* string, * size);

		assert(copy != NULL);

		if(!copy)
			fputs("Out of memory!\n", stderr);
		else {
			strcat(copy, appendix);
			copy[(* size) - 1] = 0;
			* string = copy;
		}
	}
}


static void sparam(char ** xml, unsigned * size, const char * param) {
	if(xml && size && param) {
		char * encoded = xmlize(param);
		append(xml, size, "\t\t<param><value><string>");
		append(xml, size, encoded);
		append(xml, size, "</string></value></param>\n");
		free(encoded);
	}
}


static void aparam(char ** xml, unsigned * size, const char ** param) {
	if(xml && size) {
		unsigned n;

		append(xml, size, "\t\t<param><value><array><data>");

		for(n = 0; param && param[n] != NULL; ++n) {
			char * encoded = xmlize(param[n]);
			append(xml, size, "<value><string>");
			append(xml, size, encoded);
			append(xml, size, "</string></value>");
			free(encoded);
		}

		append(xml, size, "</data></array></value></param>\n");
	}
}

char * xmlize(const char * orig) {
	register unsigned i = 0, x = 0;
	char * encoded = calloc((strlen(orig) * 6) + 1, sizeof(char));

	assert(encoded != NULL);

	while(i < strlen(orig)) {
		if(strchr("&<>'\"", orig[i])) {
			snprintf(
					encoded + x,
					strlen(orig) * 6 - strlen(encoded) + 1,
					"&#x%02X;",
					(uint8_t) orig[i]
			);
			x += 6;
		}
		else {
			encoded[x++] = orig[i];
		}
		++i;
	}

	return encoded;
}
