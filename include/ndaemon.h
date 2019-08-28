/*
 * $Header: /home/cvsroot/snips/include/ndaemon.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $
 */

/*
 * ndaemon.h, contributed by Lydia Leong <lwl@sorcery.black-knight.org>
 */

/*
 * Application-specific stuff (extracted from snipstv.h)
 */

#include "snips.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/socket.h>		/* socket constants */
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <netdb.h>

#ifndef PAUSE
# define PAUSE 15		/* poll interval in seconds*/
#endif

#ifndef MAXLINELEN
# define MAXLINELEN 256
#endif

#ifndef max
# define max(a, b)	((a) > (b) ? (a) : (b))
#endif	/* endif max */

/* --------------------------------------------------------------------------
 * ndaemon-specific networking stuff.
 */

typedef struct descriptor_data DESC;
struct descriptor_data {
    int descriptor;
    int sev_level;
    char *input_text;
    char *output_text;
    DESC *next;
};

#define WALK_DESC_CHAIN(d,i) \
	for (d = client_descs_head, i = 0; \
	     (d != NULL) && (i < nclients); \
	     d = d->next, i++)
