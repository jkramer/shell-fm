
#ifndef SHELLFM_SUBMIT
#define SHELLFM_SUBMIT

extern int enqueue(struct hash *);
extern int submit(const char *, const char *);
extern void ratelast(const char *);
extern void subdead(int);

#endif
