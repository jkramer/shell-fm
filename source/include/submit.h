
#ifndef SHELLFM_SUBMIT
#define SHELLFM_SUBMIT

extern int enqueue(struct hash *);
extern int submit();
extern void notify_now_playing(struct hash *);
extern void subdead(int);
extern void dump_queue();
extern void load_queue();

#endif
