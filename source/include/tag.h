
#ifndef SHELLFM_TAG
#define SHELLFM_TAG

extern void tag(struct hash);
extern void stripslashes(char *);
extern void sendtag(char, char *, struct hash);
extern int tagcomplete(char *, const unsigned, int);
extern char * load_tags(char, struct hash *);

#endif
