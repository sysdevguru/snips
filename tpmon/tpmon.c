/*
 * $Header: /home/cvsroot/snips/tpmon/tpmon.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/* FUNCTION:
 *	Connects to the discard socket and pushes data through the
 * connection to measure throughput. This module contains the  throughput()
 * function.  See tpmon.h for calling parameters.
 *
 * Author:
 *	Original version: S. Spencer Sun, JvNCnet 1992
 * 	This version maintained by Vikas Aggarwal, vikas@navya.com
 */

/*
Copyright 1992 JvNCnet, Princeton University

Permission to use, copy, modify and distribute this software and its
documentation for any purpose is hereby granted without fee, provided
that the above copyright notice appear in all copies and that both
that copyright notice and this permission notice appear in supporting
documentation, and that the name of JvNCnet or Princeton University
not be used in advertising or publicity pertaining to distribution of
the software without specific, written prior permission.  Princeton
University makes no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.

PRINCETON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
DAMAGES WHATSOEVER, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA
OR PROFITS, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*+
 *
 * $Log: tpmon.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/12 02:47:17  vikas
 * Initial revision
 *
 *
 * Rewrite so that it does not use the ugly 'setjmp' call, and uses
 * snips library   -vikas@navya.com 1994
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <time.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include "osdefs.h"		/* definition of RAND */

extern char *prognm;
extern int errno;
static int stoptest;		/* flag used by signal handler */

/*
 * setup_sockaddr -- given address (e.g. phoenix.princeton.edu) or an
 *	IP # (e.g. 128.112.120.1), fills in the struct sockaddr_in
 *	passed to it, with the port number requested
 *
 * returns 0 on success, -1 on error
 */
int
setup_sockaddr(addr, to, port)
  char *addr;
  struct sockaddr_in *to;
  short int port;
{
  int rtn = 0;

  bzero((char *)to, sizeof(struct sockaddr_in));
  to->sin_family = AF_INET;
  to->sin_port = htons(port);
  if (isdigit (*addr))
  {
      to->sin_addr.s_addr = inet_addr(addr);
      if (to->sin_addr.s_addr == -1)
	rtn = -1 ;
  }
  else
  {
      struct hostent *hp ;
      if ((hp = gethostbyname(addr)) == NULL)
      {
	  fprintf(stderr, "%s: unknown host: %s\n", prognm, addr);
	  rtn = -1;
      }
      else
#ifdef h_addr	/* in netdb.h */
	bcopy((char *)hp->h_addr, (char *)&to->sin_addr, hp->h_length);
#else
	bcopy((char *)hp->h_addr_list[0], (char *)&to->sin_addr, hp->h_length);
#endif
  }
  return rtn;
}

/*
 * make_buffer -- creates a buffer of size nbytes and fills it with
 * 	the pattern passed to it, or random chars if pattern is NULL
 */
char *
make_buffer(nbytes, pattern)
  int nbytes;
  char *pattern;
{
  char *buf;
  int plen;
  int i;

  if ((buf = (char *)malloc(nbytes+1)) != NULL) {
    if (pattern == NULL) {
      plen = 0;
      SRANDOM(getpid() ^ time((time_t *)0));
    } else
      plen = strlen(pattern);
    for (i = 0; i < nbytes; i++) {
      if (pattern == NULL)
        buf[i] = RANDOM() % 256;
      else
        buf[i] = pattern[i % plen];
    }
  }
  return buf;
}

/*
 * throughput -- parameters and usage are described in tp.h and tpmon.h
 */
double
throughput(addr, port, numbytes, blocksize, pattern, time, verbose)
  char *addr, *pattern;
  long numbytes;
  int blocksize, time, verbose;
  short int port;
{
  int i ; 	
  int s;			/* socket fd */
  struct sockaddr_in sockad;	/* our destination */
  struct hostent *hp;
  static char *buf = NULL;	/* data buffer */
  char error[128];
  long written,			/* actual # bytes written by write() */
    bytessent,	 		/* bytes sent successfully */
    badwrites;			/* write() returned something != blocksize */
  struct timeval tbeg, tend;    /* for timing purposes */
  double bits, secs, tp;	/* for calculating times */
  void stoptesting();


  if (verbose) {
    if (time) 
      printf("%s: (throughput) sending for %d seconds\n", prognm, time);
    else
      printf("%s: (throughput) sending %.1fk bytes\n", prognm,
        (float)numbytes/1024.0);
  }
  /* this lets us put the program name in perror() messages */
  sprintf(error, "%s: ", prognm);

  /* first fill up the buffer... */
  if (buf == NULL && (buf = make_buffer(blocksize, pattern)) == NULL) {
    fprintf(stderr, "%s: out of memory in make_buffer\n", prognm);
    return (double)-1.0;
  }

  if (setup_sockaddr(addr, &sockad, port) < 0)
  {
    close(s);
    return (double)-1.0;
  }
  if (verbose) {
    /* find out remote device's full name for printing out later */
    hp = gethostbyaddr((char *)&sockad.sin_addr, sizeof(struct in_addr),
		       AF_INET);
    if (hp == NULL) {
	strcat(error, "gethostbyaddr");
	perror(error);
	close (s);
	return (double)-1.0;
    }
    addr = hp->h_name;
    printf("Connecting to %s [%s], port %d...\n", inet_ntoa(sockad.sin_addr),
      addr, ntohs(sockad.sin_port));
  }    

  if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    strcat(error, "socket");
    perror(error);
    return (double)-1.0;
  }
  if (connect(s, (struct sockaddr *)&sockad, sizeof(sockad)) < 0) {
    strcat(error, "connect to ");
    strcat(error, inet_ntoa(sockad.sin_addr));
    perror(error);
    close(s);
    return (double)-1.0;
  }
#ifdef BSD
  if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | FNDELAY) < 0) {
#else	/* SYS5 systems */
  if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NDELAY) < 0) {
#endif
      strcat(error, "fcntl");
      perror(error);
      close(s);
      return (double)-1.0;
  }

  /* if timing, set up the SIGALRM and set keepgoing to 1 for the loop */
  signal(SIGINT, stoptesting);
  if (time) {
      signal(SIGALRM, stoptesting);
      alarm(time);
  }
    
  /* ready to go... */
  bytessent = badwrites = 0;
  stoptest = 0;
  gettimeofday(&tbeg, (struct timezone *)NULL);
  do {
      if ((written = write(s, buf, blocksize)) < 0) {
	  if ((errno == EWOULDBLOCK) || (errno == EINTR))
	    continue;
	  else
	    ++badwrites;
      } else
        bytessent += written;
  } while (!stoptest && (time || (bytessent < numbytes)) );


  /*
   * now ignore SIGALRM, just in case both time and numblocks were
   * nonzero and we wrote out 'numbytes' bytes before 'time' seconds
   * expired.  Also alarm(0) to cancel any pending alarms.
   */
  signal(SIGALRM, SIG_IGN);
  signal(SIGINT, SIG_DFL);
  alarm(0);

  /* ok, we're done, how long did it really take? */
  gettimeofday(&tend, (struct timezone *)NULL);
  bits = (double)bytessent * (double)8.0;
  secs = (double)(tend.tv_sec - tbeg.tv_sec) +
         (double)(tend.tv_usec - tbeg.tv_usec) / (double)1000000.0;
  tp = bits / secs;

  if (verbose) 
    printf("Successfully sent %.1fk in %4.3g seconds (%d bad writes)\n",
      (float)(bytessent) / 1024.0, secs, badwrites);
  if ((close(s) < 0) && verbose) {
    strcat(error, "couldn't close socket [");
    strcat(error, inet_ntoa(sockad.sin_addr));
    strcat(error, "] (non-fatal)");
    perror(error);
  }
  return tp;
}

/*
 * stoptesting -- hack to break out of while loop in throughput()
 * 	activated by SIGALRM
 */
void stoptesting()
{
    stoptest = 1;
}
