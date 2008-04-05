/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <assert.h>

#include "readline.h"
#include "interface.h"

static void delete(unsigned);

char * readline(struct prompt * setup) {
	int eoln = 0, seq = 0, histsize = 0, index = -1, changed = !0;
	static char line[1024];
	unsigned length = 0;

	assert(setup != NULL);

	/* Print prompt if present. */
	if(setup->prompt)
		fputs(setup->prompt, stderr);

	/* Initialize line (empty or with preset text if given). */
	memset(line, 0, sizeof(line));
	if(setup->line) {
		strncpy(line, setup->line, sizeof(line) - 1);
		length = strlen(line);
		fputs(line, stderr);
	}

	/* Count items in history. */
	for(histsize = 0; setup->history && setup->history[histsize]; ++histsize);
	index = histsize;

	canon(0);

	while(!eoln) {
		int key = fgetc(stdin);

		switch(key) {
			case 8: /* Backspace. */
			case 127: /* Delete. */
				/* We don't support moving the cursor from the end of the line
				 * so handle these the same. */
				if(length > 0) {
					delete(1);
					line[--length] = 0;
					changed = !0;
				}
				break;

			case 9: /* Tab. */
				/* Call the callback function for completion if present. */
				if(setup->callback != NULL) {
					if(setup->callback(line, sizeof(line), changed)) {
						delete(length);
						length = strlen(line);
						memset(line + length, 0, sizeof(line) - length);
						fprintf(stderr, "\r%s%s", setup->prompt, line);
					}
					changed = 0;
				}
				break;

			case 4: /* EOF (^D) */
			case 10: /* Line break. */
			case 13: /* Carriage return (who knows...) */
				eoln = !0;
				break;

			case 23: /* ^W */
				if(length > 0) {
					int alpha = isalpha(line[length - 1]);
					while(length > 0 && isalpha(line[length - 1]) == alpha) {
						line[--length] = 0;
						delete(1);
					}
					changed = !0;
				}

				break;

			case 27: /* Escape. */
				++seq;
				changed = !0;
				break;

			default:
				changed = !0;
				if(seq > 0) {
					if(seq < 2)
						++seq;
					else {
						seq = 0;
						switch(key) {
							case 65: /* Up. */
							case 66: /* Down. */
								if(histsize) {
									if(key == 66 && index < histsize - 1)
										++index;

									if(key == 65 && index > -1)
										--index;

									delete(length);
									memset(line, 0, length);
									length = 0;

									if(index > -1 && index < histsize) {
										strncpy(
											line,
											setup->history[index],
											sizeof(line) - 1
										);

										length = strlen(line);
										fputs(line, stderr);
									}
								}

								break;
						}
					}
				}
				else {
					if(length < sizeof(line)) {
						line[length++] = key;
						fputc(key, stderr);
					}
				}
		}
	}

	canon(!0);
	fputc(10, stderr);

	return line;
}

void canon(int enable) {
	struct termios term;
	tcgetattr(fileno(stdin), & term);

	term.c_lflag = enable
		? term.c_lflag | ICANON | ECHO
		: term.c_lflag & ~(ICANON | ECHO);

	tcsetattr(fileno(stdin), TCSANOW, & term);
}

static void delete(unsigned n) {
	while(n--)
		fputs("\b \b", stderr);
}
