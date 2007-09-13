
#ifndef SHELLFM_SUBMIT
#define SHELLFM_SUBMIT

extern int enqueue(struct hash *);
extern int submit(const char *, const char *);
extern void subdead(int);
extern void dumpqueue(int);
extern void loadqueue(int);

#endif
