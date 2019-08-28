/* $Header: /home/cvsroot/snips/lib/eventlog.c,v 1.2 2008/04/25 23:31:50 tvroon Exp $ */

/*
 * A small library of functions for use in connecting to the SNIPS event
 * logging daemon  (snipslogd).
 *
 */

/*+
 * $Log: eventlog.c,v $
 * Revision 1.2  2008/04/25 23:31:50  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.1  2001/07/28 01:47:16  vikas
 * Now converts host endian to network byte order (marya@st.jip.co.jp)
 *
 * Revision 1.0  2001/07/08 22:19:38  vikas
 * Lib routines for SNIPS
 *
 */

/* Copyright 2001, Netplex Technologies, Inc. */

#ifndef lint
 static char rcsid[] = "$Id: eventlog.c,v 1.2 2008/04/25 23:31:50 tvroon Exp $" ;
#endif

#include <stdio.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "netmisc.h"
#include "snipslogd.h"
#include "snips_funcs.h"
#include "eventlog.h"

/*
 * The logfile descriptor:   -2 if it hasn't been opened at all, and
 * 	-1 if openeventlog()  failed.
 */
static int logFds[SNIPS_HOST_MAX_COUNT];
static int logfd;
static char *s;
static int eventLogInitialized = 0;
static int delaycount;
static time_t	closetime;

/*
 * Opens a socket to the snips logging daemon.
 * Returns 0 on success, or prints an error message to stderr
 * and returns -1 on failure. ON failure, it MUST change logfd
 * to a value of -1.
 */
int openeventlog()
{
  struct sockaddr_in	sin;
  struct servent	*sp;

  if (logfd >= 0)	/* connection already open */
    return(1);

  if ( (logfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("openeventlog: socket() ");
    logfd = -1 ;
    return(-1);
  }
    
  bzero(&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  //s = (char **)get_snips_loghosts();


  if (get_inet_address(&sin, s) < 0) {
    logfd = -1;
    return(-1);
  }


#ifdef DEBUG
  fprintf(stderr, "openeventlog: logging to %s\n", inet_ntoa(sin.sin_addr)) ;
#endif

  /* Figure out what port number to use */
  if ((sp = getservbyname(SNIPSLOG_SERVICE, "udp")) == NULL &&
      (sp = getservbyname(NLOG_SERVICE, "udp")) == NULL)
    sin.sin_port = htons(SNIPSLOG_PORT);
  else
    sin.sin_port = sp->s_port;
    
  if (connect(logfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("client: AF_INET connect() failed");
    logfd = -1;
    return(-1);
  }

#ifdef DEBUG
    fprintf(stderr, "openeventlog: opened INET socket %d\n", logfd);
#endif
    
    return(0);
}

/*
 * Writes an EVENT structure to the snips logging daemon.
 * The connection must have been already created with openeventlog().
 * If logfd = -2, then assumes that the user erred and never called
 * openeventlog(), so it does it for him.
 * Returns 0 on success, or prints an error message to stderr
 * and returns -1  on failure.
 *
 * NOTE: since the socket is of the connectionless type, logfd might actually
 *	be okay, but the logging daemon might not be up.
 */
#define RETRY_REOPEN	1*60	/* seconds before trying to reopen logfd */



//function to send packet to a host
int eventLogToHost(vin)
EVENT *vin;
{
    int bytesleft, retval;
    EVENT vbuf;
    EVENT *v = &vbuf;

    if (logfd == -2)	/* try to open if after RETRY_REOPEN interval */
    {
        time_t curtime = time((time_t *) NULL);
        if ((curtime - closetime) > RETRY_REOPEN)
            openeventlog();
    }
    
    if (logfd < 0)    	/* Silently fail if no connection could be opened */
        return(-1);
    
    htonevent(vin, v);		/* convert to network endian */
    
    for (bytesleft=sizeof(*v); bytesleft > 0; bytesleft-=retval)
        if ((retval=write(logfd, (char *)v + (sizeof(*v) - bytesleft), bytesleft)) < 0)
        {
#ifdef DEBUG
            perror("eventlog: write() failed");
#endif /* DEBUG */
            closeeventlog();	/* no point keeping it open */
            return(-1);
        }
    
     return(0);

}

//initialize the sockets
void eventLogInitialization()
{
    int index;
    
    for (index  = 0; index < SNIPS_HOST_MAX_COUNT; index++)
        logFds[index] = -2;

}

int eventlog(vin)
  EVENT *vin;			/* in host endian format */
{
 
  int logResult = 0;
  int failed = 0;
  int index;
  //initialize each logFd
  if (!eventLogInitialized)
  {
      eventLogInitialized = 1;
      eventLogInitialization();
  }
    
  //for each host, we will do event logging
    for (index  = 0; index < SNIPS_HOST_MAX_COUNT; index++)
    {
        s = get_snips_loghosts()[index];
        
        if (s)
        {
    				logfd = logFds[index];
            logResult = eventLogToHost(vin);
            logFds[index] = logfd;
        
            if (logResult < 0)
                failed = 1;
        }
    }

    if ((++delaycount % 50) == 0)	/* small delay after X loggings */
    {
        delaycount = 0 ;
        sleep(1);
    }
    
    //if failed
    if (failed)
        return (-1);
        
    return (0);

  
}	/* eventlog() */

/*
 * Closes the current connection to the snips logging daemon.
 */
int closeeventlog()
{
  if (logfd < 0)
    return(-1);
    
  if (close(logfd) < 0) {
    perror("closeeventlog: close() failed");
    return(-1);
  }

  /*
   * Reset things cleanly so we can call openeventlog() later if we want.
   * Store the time that we closed the file desc so that we don't try
   * and reopen too many times in 'openeventlog'
   */
  closetime = time((time_t *) NULL);
  logfd = -2;
  return(0);
}

/*
 * Convert EVENT values from host to network byte order
 */
void htonevent(f,t)
  EVENT *f;
  EVENT *t;
{
  if (f != t)
    bcopy(f,t,sizeof(*f));
  t->var.value     = htonl(f->var.value);
  t->var.threshold = htonl(f->var.threshold);
  t->eventtime     = htonl(f->eventtime);
  t->polltime      = htonl(f->polltime);
  t->op            = htonl(f->op);
  t->id            = htonl(f->id);
}

/*
 * Convert EVENT values from network to host byte order
 */
void ntohevent(f,t)
  EVENT *f;
  EVENT *t;
{
  if (f != t)
    bcopy(f,t,sizeof(*f));
  t->var.value     = ntohl(f->var.value);
  t->var.threshold = ntohl(f->var.threshold);
  t->eventtime     = ntohl(f->eventtime);
  t->polltime      = ntohl(f->polltime);
  t->op            = ntohl(f->op);
  t->id            = ntohl(f->id);
}
