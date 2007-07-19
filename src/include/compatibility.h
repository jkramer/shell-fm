/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_COMPATIBILITY
#define SHELLFM_COMPATIBILITY

#if !defined(__GNUC__) || __GNUC__ < 3
#define __UNUSED__
#else
#define __UNUSED__ __attribute__((unused))
#endif

extern const char * nextmatch(char **, char *);

#endif // SHELLFM_COMPATIBILITY
