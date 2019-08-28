/* $Header: /home/cvsroot/snips/include/portmon.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $  */

#ifndef __portmon_h
# define __portmon_h

#include "snips.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>		/* for lseek */
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <netdb.h>


#define VARUNITS	"msecs"
#define POLLINTERVAL  	(60*15)		/* seconds between passes */

#define MAXSIMULCONNECTS	64	/* max simultaneous tcp connections */

#define RTIMEOUT  60			/* seconds for select() timeout */
#define READBUFLEN   1024		/* size of read buffer */

struct _response
{
  char  *response;
  int   severity;		/* max severity per response string */
  struct _response *next;
} ;

struct _harray
{
  char  *hname ;		/* Name for snips struct */
  char  *ipaddr ;		/* IP name (or address) */
  char  *writebuf ;		/* string to send remote host */
  char	*readbuf;		/* read data from remote host */
  char	*quitstr;		/* string to send to close connection */
  char	*rptr, *wptr;		/* temp read/write pointers */
  int   port ;		       	/* port number being tested */
  int   connseverity ;		/* max severity if cannot issue a connect */
  int	testseverity;		/* max severity from tests */
  int	fd;			/* open socket descriptor */
  int	status;			/* return value from tests */
  unsigned int	timeouts[3];	/* timeout in seconds for warn/err/crit */
  unsigned int	elapsedsecs;	/* elapsed secs for this host */
  struct _response  *responselist; /* chat strings + respective severity */
  struct _harray  *next;	/* linked list */
};

extern char *raw2newline();

#endif	/* ! __portmon_h */
