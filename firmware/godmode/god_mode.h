// God mode — sweep-attack every network found, across both bands.
#ifndef GOD_MODE_H
#define GOD_MODE_H

#include <stdbool.h>

void god_mode_start(void);   // begin sweeping (first sweep re-scans)
void god_mode_stop(void);

// One attack sweep over all cached networks. Call repeatedly from a task.
// Returns the number of networks hit this sweep.
int  god_mode_process(void);

#endif // GOD_MODE_H
