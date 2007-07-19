
#ifndef SHELLFM_PROMPT
#define SHELLFM_PROMPT

struct prompt {
	const char * prompt;
	char * line, ** history;
	int (* callback)(char *, const unsigned);
};

extern char * readline(struct prompt *);
extern void canon(int);

#endif
