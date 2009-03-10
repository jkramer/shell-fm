
#ifndef SHELLFM_GLOBALS
#define SHELLFM_GLOBALS

/* for pid_t */
#include <unistd.h>

#include "hash.h"
#include "playlist.h"

/* Track, session and settings data structures. */
extern struct hash data, track;
extern struct playlist playlist;
extern struct hash rc;

/* Batch mode: no coloring, every line of output gets uncoditionally
   terminated by newline. */
extern int batch;

/* Forks. */
extern pid_t playfork, subfork;

extern char * currentStation; /* Name of the current station. */

extern unsigned submitting; /* Number of tracks currently submitted. */
extern time_t pausetime; /* Pause start time. */


extern unsigned flags;

#define STOPPED 0x1
#define DISCOVERY 0x2
#define CHANGED 0x4
#define RTP 0x8
#define QUIET 0x10
#define INTERRUPTED 0x20

#define enabled(n) (flags & n)
#define enable(n) (flags |= n)
#define disable(n) (flags &= ~n)
#define toggle(n) (flags ^= n)

extern char * nextstation;

#endif
