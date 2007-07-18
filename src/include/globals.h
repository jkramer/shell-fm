
#ifndef SHELLFM_GLOBALS
#define SHELLFM_GLOBALS

#include "hash.h"
#include "playlist.h"

/* Track, session and settings data structures. */
extern struct hash data, track;
extern struct playlist playlist;
extern struct hash rc;

/* Forks. */
extern pid_t playfork, subfork;

extern char * currentStation; /* Name of the current station. */

extern float avglag; /* Average lag. */
extern unsigned submitting; /* Number of tracks currently submitted. */
extern time_t pausetime; /* Pause start time. */
extern int
	discovery, /* Discovery mode switch. */
	record, /* RTP switch (Record To Profile). */
	stopped, /* Radio stopped? */
	stationChanged; /* Did station change? */

#endif
