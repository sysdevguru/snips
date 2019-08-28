/* $Header: /home/cvsroot/snips/include/snipslogd.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $ */


#ifndef __snipslogd_h
# define __snipslogd_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "snips.h"

/* For extracting the snipslog port from /etc/services */
#ifndef SNIPSLOG_SERVICE
# define SNIPSLOG_SERVICE	"snipslog"
# define NLOG_SERVICE		"noclog"	/* nocol compatibility */
#endif

/* Port in case not available from /etc/services */
#ifndef SNIPSLOG_PORT
# define SNIPSLOG_PORT		5354
#endif

#define READ_TIMEOUT		5	/* secs before socket read timeout */

#endif  /* !__snipslogd_h  */
