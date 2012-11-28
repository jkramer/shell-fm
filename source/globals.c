
#include <time.h>

#include "globals.h"

unsigned flags = RTP;
time_t change_time = 0, pausetime = 0;
char * nextstation = NULL;

int batch = 0, error = 0, delayquit = 0;
