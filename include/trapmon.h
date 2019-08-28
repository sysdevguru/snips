/*
 * $Header: /home/cvsroot/snips/include/trapmon.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $
 */
#ifndef __trapmon_h
# define __trapmon_h

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

/*
 * Describe all the possible traps here and their severity levels here.
 * Note that the location of the traps in the array depends on the int
 * value assigned to the various trap types in 'snmp.h'
 */

struct t_desc {
    char  *tname ;		/* Trap name */
    int   tseverity ;		/* Trap severity as described in snips.h */
    char  *tseverity_str ;	/* Severity description strings */
    int   state ;		/* Operation status n_UP, n_DOWN, n_UNKNOWN */
    int   loglevel ;		/* For snipslogd logging level */
} ;

struct t_desc trap_desc[] = {
  { "Cold_Start", E_WARNING, "Warning", n_UP, E_WARNING },	/* trap 0 */
  { "Warm_Start", E_WARNING, "Warning", n_UP, E_WARNING },	/* trap 1 */
  { "Link_Down",  E_ERROR,   "ERROR", n_DOWN, E_ERROR},		/* trap 2 */
  { "Link_Up",    E_ERROR,   "ERROR", n_UP, E_ERROR},		/* trap 3 */
  { "Auth_Failure", E_WARNING, "Warning", n_UP, E_WARNING},	/* trap 4 */
  { "EGP_Peer_Loss", E_CRITICAL, "CRITICAL", n_DOWN, E_CRITICAL}, /* trap 5 */
  { "Enterprise", E_WARNING, "Warning", n_UNKNOWN, E_WARNING},	/* trap 6 */
  { "Unknown_Type", E_WARNING, "Warning", n_UNKNOWN, E_WARNING}	/* trap 7 */
} ;

/*
 * Timeout for each EVENT in seconds - currently set to 30 minutes.
 * Trap events older than this will be deleted from the data file.
 */
#define TIME_TO_LIVE	(30*60)		/* convert to seconds */

#define MAX_PATH_LEN	256			/* Longest path expected */
#define VARUNITS	"Trap"			/* SNIPS event.var.units */
#define MAX_TRAPS	1024			/* Max # of unresolved traps */

int		numtraps;			/* # of outstanding EVENTs */
time_t		expire_at[MAX_TRAPS];		/* When this EVENT expires */

#endif	/* ! __trapmon_h */
