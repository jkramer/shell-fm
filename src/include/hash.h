/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_HASH
#define SHELLFM_HASH

struct pair {
	char * key, * value;
};

struct hash {
	unsigned size;
	struct pair * content;
};

extern void set(struct hash *, const char *, const char *);
extern const char * value(struct hash *, const char *);
extern void unset(struct hash *, const char *);
extern int haskey(struct hash *, const char *);
extern void empty(struct hash *);

#endif
