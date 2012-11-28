/*
	Copyright (C) 2006-2010 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#include "util.h"
#include "getln.h"
#include "md5.h"

#if (defined(TUXBOX) || defined(PPC))
#include <ctype.h>
#endif


void debug(__attribute__((unused)) char * format, ...) {
#ifdef DEBUG
	va_list argv;
	va_start(argv, format);

	vfprintf(stderr, format, argv);
#endif
}

/*
	Takes the path of a file as argument, reads the file and returns an array
	of strings containing the lines of the file.
*/
char ** slurp(const char * path) {
	char ** content = NULL;
	unsigned items = 0;
	FILE * fd;

	if((fd = fopen(path, "r")) != NULL) {
		char * line = NULL;
		unsigned size = 0;

		while(!feof(fd)) {
			if(!getln(& line, & size, fd))
				continue;

			if(strlen(line) > 1) {
				char * ptr;
				if((ptr = strrchr(line, 10)) != NULL)
					* ptr = (char) 0;

				if(strlen(line) > 0) {
					content = realloc(content, sizeof(char *) * (items + 2));

					assert(content != NULL);

					content[items] = strdup(line);
					assert(content[items] != NULL);

					content[++items] = NULL;
				}
			}
		}

		if(line)
			free(line);

		fclose(fd);
	}

	return content;
}


void spit(const char * path, const char * content) {
	FILE * fd = fopen(path, "w");

	assert(fd != NULL);

	fprintf(fd, "%s\n", content);
	fclose(fd);
}


/*
	Takes an array of strings and returns another array of strings with all
	duplicate strings removed. The old array is purged from memory.
*/
char ** uniq(char ** list) {
	char ** uniqlist = NULL;

	if(list != NULL) {
		int size = 0, n = 0;

		while(list[n] != NULL) {
			if(grep(uniqlist, list[n])) {
				free(list[n]);
				list[n] = NULL;
			} else {
				uniqlist = realloc(uniqlist, (sizeof(char *)) * (size + 2));

				assert(uniqlist != NULL);

				uniqlist[size++] = list[n];
				uniqlist[size] = NULL;
			}

			++n;
		}

		free(list);
	}

	return uniqlist;
}


int grep(char ** list, char * needle) {
	register unsigned x = 0;

	if(!needle)
		return 0;

	if(list != NULL) {
		while(list[x] != NULL) {
			if(!strcmp(list[x], needle))
				return !0;
			++x;
		}
	}

	return 0;
}

/* create a new buffer and populate with concatenation of all strings */
char * strjoin(const char *glue, ...)
{
	va_list ap;
	unsigned total, count, glue_len;
	const char *str;
	char *res;

	va_start(ap, glue);
	total = count = 0;
	while ((str = va_arg(ap, const char *))) {
		total += strlen(str);
		count ++;
	}
	va_end(ap);

	glue_len = glue ? strlen(glue) : 0;
	total += (count - 1) * glue_len;
	res = (char*)malloc(total + 1);
	if (!res)
		return NULL;

	va_start(ap, glue);
	total = 0;
	while ((str = va_arg(ap, const char *))) {
		if (total && glue_len) {
			memcpy(res + total, glue, glue_len);
			total += glue_len;
		}
		strcpy(res + total, str);
		total += strlen(str);
	}
	va_end(ap);

	return res;
}


#ifdef __STRNDUP__

char * strndup(const char * src, size_t len) {
	char * tmp = (char *) malloc(len + 1);

	if(tmp != NULL) {
		strncpy(tmp, src, len);
		tmp[len] = 0;
	}

	return tmp;
}

#endif

#if (defined(TUXBOX) || defined(PPC) || (defined (__SVR4) && defined (__sun)))

/* On a few system like a PPC based Dreambox libc does not include strcasestr ... */

char * strcasestr(const char * haystack, const char * needle) {
    int nlength;
    int mlength;

	assert(needle != NULL);
	assert(haystack != NULL);

	nlength = strlen(needle);

    if(nlength == 0)
		return NULL;

	mlength = strlen(haystack) - nlength + 1;

    /* If needle is bigger than haystack, can't match. */
    if(mlength < 0)
		return NULL;

    while(mlength) {
        /* See if the needle matches the start of the haystack. */
        int i;
        for(i = 0; i < nlength; i++) {
            if(tolower(haystack[i]) != tolower(needle[i]))
                break;
        }

        /* If it matched along the whole needle length, we found it. */
        if(i == nlength)
            return (char *) haystack;

        mlength--;
        haystack++;
    }

    return NULL;
}


#endif


const char const * plain_md5(const char * input) {
	const unsigned char * md5;
	static char plain[32 + 1] = { 0, };
	unsigned ndigit;

	md5 = MD5((const unsigned char *) input, strlen(input));

	for(ndigit = 0; ndigit < 16; ++ndigit)
		sprintf(2 * ndigit + plain, "%02x", md5[ndigit]);

	// debug("md5 password: <%s>\n", plain);

	return plain;
}
