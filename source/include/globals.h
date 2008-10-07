
#ifndef SHELLFM_GLOBALS
#define SHELLFM_GLOBALS

#include "hash.h"
#include "playlist.h"

/* Track, session and settings data structures. */
extern struct hash data, rc;
extern struct playlist playlist;

/* Batch mode: no coloring, every line of output gets uncoditionally
   terminated by newline. */
extern int batch;

/* Forks. */
extern pthread_t playthread, subthread;

/* Pause mutex. */
extern pthread_mutex_t paused;

extern char * currentstation; /* Name of the current station. */

extern unsigned submitting; /* Number of tracks currently submitted. */
extern time_t changetime, pausetime, pauselength; /* Timers. */


extern unsigned flags;

#define STOPPED 0x1
#define DISCOVERY 0x2
#define CHANGED 0x4
#define RTP 0x8
#define QUIET 0x10

#define enabled(n) (flags & n)
#define enable(n) (flags |= n)
#define disable(n) (flags &= ~n)
#define toggle(n) (flags ^= n)

extern char * nextstation;

#endif
