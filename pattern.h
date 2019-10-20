#ifndef __PATTERN_H__
#define __PATTERN_H__

#include "support.h"

/* finds all files matching given pattern and enqueues them
 * to alphabetically sorted queue q
 * returns the number of matches */
int pattern_filename_gen (char *pat, struct llist_s **q);

/* returns 1 if string needle matches the pattern pat
 * 0 otherwise */
int pattern_match (char *pat, char *needle);

#endif
