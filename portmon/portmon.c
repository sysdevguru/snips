/* #define DEBUG		/* */

/* $Header: /home/cvsroot/snips/portmon/portmon.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $  */

/*+ 		portmon.c
 * FUNCTION:
 * 	Monitor TCP ports and reponses for SNIPS. It can send a string
 * and then parse the responses against a list. Each response can be
 * assigned a severity. Checks simple port connectivity if given a NULL
 * response string.
 *
 * Features:
 *	1) Uses non-blocking reads, hence a single process can
 *	   monitor upto 64 hosts in one pass (can be increased,
 *	   if you have a new OS and have a high MAXFD). Very
 *	   fast and efficient.
 *	2) Responses can contain a '\0' in the data stream.
 *	3) Can test connectivity to port, time to response.
 *
 * AUTHOR:
 *  	Vikas Aggarwal, -vikas@navya.com
 */

/*
 * $Log: portmon.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 22:48:28  vikas
 * For SNIPS
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */

/*  */
#ifndef lint
static char rcsid[] = "$Id: portmon.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $" ;
#endif

#include "portmon.h"
#include "netmisc.h"
#include <stdlib.h>

extern char	*skip_spaces() ;		/* in libsnips */
extern char	*Strcasestr();			/* like strstr() */

/* function prototypes */
int NBconnect(int s, char *host, int port);
int send_hoststring(int sock, struct _harray *h);
int process_host(int sock, struct _harray *h);
int check_resp(char *readstr, struct _response *resarr);

/*
 * This function opens a bunch of non-blocking connects to the remote
 * devices and calls process_host() as each socket becomes ready.
 * Handles multiple devices in parallel.
 */
int checkports(hv, nhosts, rtimeout)
  struct _harray  *hv[];	/* list of device structures */
  int nhosts;
  int rtimeout;			/* read timeout in secs */
{
  int i, nleft;
  int maxfd = 0;
  int sockarray[MAXSIMULCONNECTS];
  time_t elapsedsecs = 0;
  fd_set rset, wset;

  if (nhosts <= 0) return;

  FD_ZERO(&rset); FD_ZERO(&wset);
  nleft = nhosts;

  if (debug > 1) fprintf(stderr, "Testing %d devices simultaneously\n", nhosts);

  /* issue a non-blocking connect to each host */
  for (i = 0; i < nhosts; ++i)
  {
    /* init the array for each host */
    hv[i]->status = 0;	/* assume down */
    hv[i]->testseverity = hv[i]->connseverity;	/* sev if cannot connect */
    hv[i]->wptr = hv[i]->writebuf; hv[i]->rptr = hv[i]->readbuf;
    *(hv[i]->readbuf) = '\0';
    hv[i]->elapsedsecs = 0;

    if ((sockarray[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("socket");
      return (-1);	/* fatal error */
    }

    /* NBconnect() will return zero/positive if it gets an immediate
     * connection, else it will set errno.
     */
    if (NBconnect(sockarray[i], hv[i]->ipaddr, hv[i]->port) < 0 &&
	errno != EINPROGRESS)
    {		/* some error */
      if (debug)
	fprintf(stderr, "connect() failed for %s:%d- %s\n",
		hv[i]->ipaddr, hv[i]->port, (char *)strerror(errno) );
      close(sockarray[i]);
      --nleft;
    }
    else
    {
	FD_SET(sockarray[i], &rset);
	FD_SET(sockarray[i], &wset);
	if (sockarray[i] > maxfd) maxfd = sockarray[i];
    }

  }	/* for (i = 1..nhosts) */

  /* now do a select on all the sockets, and process each one */
  while (nleft > 0) {
    int n;
    fd_set rs, ws;
    clock_t curtime;
    struct timeval tval;
    struct timeval start, end;

    rs = rset; ws = wset;
    curtime = clock();//time(NULL);
    gettimeofday(&start, NULL);
    if (elapsedsecs/1000000.0 >= rtimeout)
    {
      for (i = 0; i < nhosts; ++i)
	close(sockarray[i]);
      return 0;
    }
    
    
    tval.tv_sec = rtimeout - elapsedsecs/1000000.0;
    tval.tv_usec = 0;
#ifdef DEBUG
    if (debug > 2)
      fprintf(stderr,
	      "\ncheckports() calling select(), timeout %ld, nleft= %d\n",
	      tval.tv_sec/1000.0, nleft);
#endif
    if ( (n = select(maxfd + 1, &rs, &ws, NULL, &tval)) <= 0)
    {
      if (n < 0 && errno != EINTR)
      {
	perror("select()");
	return (-1);	/* fatal ?? */
      }
      else
      {
	if (debug) fprintf(stderr, "timeout for connect()\n");
	for (i = 0; i < nhosts; ++i)
	  close(sockarray[i]);
	break;	/* out of while */
      }
    }	/* if (n = select) */

    gettimeofday(&end, NULL);
    elapsedsecs += (end.tv_usec - start.tv_usec);	/* stop the clock */

#ifdef DEBUG
    if (debug > 2)
      fprintf(stderr, "select() returned %d, elapsed msecs = %d\n",
	      n, elapsedsecs/1000.0);
#endif
    /* see which socket is ready from the select() and process it */
    for (i = 0; i < nhosts; ++i) 
    {
      if ( FD_ISSET(sockarray[i], &rs) || FD_ISSET(sockarray[i], &ws) )
      {
	int error;
	int len;
	len = sizeof(error);
	if (getsockopt(sockarray[i], SOL_SOCKET, SO_ERROR, (void *)&error,
		       &len) < 0 || error != 0)
	{	/* some error in connect() */
	  if (debug)
	  {
	    fprintf(stderr, "connect() failed to %s:%d- ", 
		    hv[i]->hname, hv[i]->port);
	    if (error) fprintf(stderr, "%s\n", (char *)strerror(errno) );
	    else  fprintf(stderr, "%s\n", (char *)strerror(errno));
	  }
	  FD_CLR(sockarray[i], &rset); FD_CLR(sockarray[i], &wset);
	  close(sockarray[i]);	/* ensure this is closed */
	  --nleft;
	  if (debug > 2)
	    fprintf(stderr, "DONE #%d. %s:%d (%s)\n",
		    i, hv[i]->hname, hv[i]->port, hv[i]->ipaddr);
	}
	else	/* ready for reading/writing and connected */
	{
	  hv[i]->elapsedsecs = elapsedsecs/1000.0;	/* store elapsed time */
    fprintf(stderr, "%lld elapsed msecs\n",    hv[i]->elapsedsecs);

	  if (FD_ISSET(sockarray[i], &ws))
	    send_hoststring(sockarray[i], hv[i]);

	  if (FD_ISSET(sockarray[i], &rs) || hv[i]->responselist == NULL)
	    if ( process_host(sockarray[i], hv[i]) == 1 )
	    {
	      FD_CLR(sockarray[i], &rset); FD_CLR(sockarray[i], &wset);
	      close(sockarray[i]);	/* ensure this is closed */
	      --nleft;
	      if (debug > 2)
		fprintf(stderr, "DONE #%d. %s:%d (%s)\n",
			i, hv[i]->hname, hv[i]->port, hv[i]->ipaddr);
	    }

	  if (hv[i]->wptr == NULL || *(hv[i]->wptr) == '\0')
	    FD_CLR(sockarray[i], &wset);		/* nothing to write */
	}	/* if else getsockopt() */

      }	/* if-else FD_ISSET */

    }	/* for() */

  }	/* while(nleft) */

  /* here if timeout or all the connects have been processed */

  if (nleft && debug > 1)
    fprintf(stderr, " %d devices unprocessed (no response)\n", nleft);

  for (i = 0; i < nhosts; ++i)		/* close any open sockets */
    close(sockarray[i]);

  return (0);

}	/* check_ports() */

/*+
 * Send a string to a host
 */
int send_hoststring(sock, h)
  int sock;		/* connected socket */
  struct _harray *h;
{
  int n;

  if (debug)
    fprintf(stderr, " (debug) send_hoststring('%s:%d')\n", h->hname, h->port);

  /* if socket is non-blocking, undo this flag */
/*  sflags = fcntl(sock, F_GETFL, 0);		/*  */
/*  fcntl(sock, F_SETFL, sflags ^ O_NONBLOCK);	/* set blocking */
/*  fcntl(sock, F_SETFL, sflags | O_NONBLOCK);	/* set nonblocking */

  /* return if nothing to send */
  if (h->wptr == NULL || *(h->wptr) == '\0')
    return 1;

  /* send our string */
  if ( (n = write(sock, h->wptr, strlen(h->wptr))) < 0)
  {
    if (errno == EWOULDBLOCK)
      return 0;
#ifdef EAGAIN		/* sysV ? */
    if (errno == EAGAIN)
      return 0;
#endif
    if (debug)
      fprintf(stderr, 
	      "Error in write() for '%s:%d' - %s\n", h->hname, h->port,
	      (char *)strerror(errno));
    close (sock);
    h->testseverity = h->connseverity;
    h->status = 0;	/* mark as down */
    return(1);	/* finished testing */
  }	/* end if (couldn't write)  */

  (h->wptr) += n;
  if (*(h->wptr) != '\0')
  {
#ifdef DEBUG
    if (debug > 1)
      fprintf(stderr, "  (debug) Host %s:%d- sent %d bytes\n",
	      h->hname, h->port, n);
#endif
    return 0;
  }

  if (debug)
    fprintf(stderr, "  (debug) Host %s:%d- Sent string '%s'\n",
	    h->hname, h->port, h->writebuf) ;

  return 1;

}	/* send_hoststring() */
  
/*+ 
 * FUNCTION:
 * 	Checks the port for the structure _harray passed to it.
 * Return value is '1' if finished processing, '0' if yet to finish.
 */
int process_host(sock, h)
  int sock;		/* connected socket */
  struct _harray *h;
{
  int i, n;
  int buflen, maxsev;
  register char *r, *s;

  if (debug)
    fprintf(stderr, " (debug) process_host('%s:%d')\n", h->hname, h->port);

  if (h->responselist == NULL)		/* no responses to check */
    if (h->wptr == NULL || *(h->wptr) == '\0')	/* done sending */
    {
      if (debug > 1)
	fprintf(stderr,
		" process_host(%s:%d) nothing to send/recieve, done\n",
		h->hname, h->port);
      h->testseverity = E_INFO;
      h->status = 1;		  	/* device up */
      if (h->quitstr && *(h->quitstr))
	write(sock, h->quitstr, strlen(h->quitstr));
      close (sock);
      return(1);	/* done testing */
    }

  /* now fill any remaining read buffer space we have */
  buflen = h->readbuf + READBUFLEN - h->rptr;	/* amount we can read*/
  n = read(sock, h->rptr, buflen - 1);
#ifdef DEBUG  	/* */
  if (debug > 2)
    fprintf(stderr, "  read %d bytes from %s:%d\n", n, h->hname, h->port);
#endif	/* */
  if (n < 0)
  {		/* read() error */
    if (errno == EWOULDBLOCK)
      return (0);
#ifdef EAGAIN
    if (errno == EAGAIN)
      return 0;
#endif
    if (debug)
      fprintf(stderr, 
	      "Error in read() for '%s:%d' - %s\n", h->hname, h->port,
	      (char *)strerror(errno));
    close (sock);
    h->testseverity = h->connseverity;
    h->status = 0;	/* mark as down */
    return(1);		/* finished testing */
  }	/* end if (n < 0)  */

  /* if n==0, then we have read end of file, so do a final check_resp() */

  /* replace any \0 in the stream with a \n */
  for (i = 0; i < n ; ++i)
    if ((h->rptr)[i] == '\0')
      (h->rptr)[i] = '\n';

  (h->rptr) += n;		/* increment pointer */
  *(h->rptr) = '\0';

  if (n > 0 && (r = (char *)strrchr(h->readbuf, '\n')) == NULL)  /* no \n */
  {
    if ( n < (buflen - 1) )	/* remaining empty buffer space */
      return (0);		/* need to continue reading */
    else			/* filled buffer, but no \n yet */
      r = h->rptr - 32;		/* set to about 32 chars back from end */
  }

  /* here if end-of-file or found a newline */
  maxsev = check_resp(h->readbuf, h->responselist);

  if (maxsev == -1)
  {	/* no match in response list */
    if (n == 0)		/* end of file, so we are done */
      maxsev = h->connseverity;
    else
    {
      for (++r, s = h->readbuf; *r; )
	*s++ = *r++;	/* shift stuff after \n to start of readbuf */
      *s = '\0';	       	/* lets be safe */
      h->rptr = s;		/* point to next location to be read */
      return 0;			/* still not done */
    }
  }
  else
  {			/* Here if we found a match in check_resp() */
    if (h->timeouts[0] != 0)
    {				/* we are checking port speed */
      if (debug > 1)
	fprintf(stderr,"  (debug) elapsed time= %ld msecs\n", h->elapsedsecs);
      if (h->elapsedsecs < h->timeouts[0])	maxsev = E_INFO;
      else if (h->elapsedsecs < h->timeouts[1])	maxsev = E_WARNING;
      else if (h->elapsedsecs < h->timeouts[2])	maxsev = E_ERROR;
      else maxsev = E_CRITICAL;
    }
  }

  if (debug)
    fprintf(stderr," (debug) process_host(%s:%d): returning severity %d\n",
	    h->hname, h->port, maxsev);
  if ( (h->testseverity = maxsev) == E_INFO )
    h->status = 1;
  else
    h->status = 0;

  if (h->quitstr && *(h->quitstr))
    write(sock, h->quitstr, strlen(h->quitstr));
  close(sock);

  return(1);	/* done processing */

}	/* end:  process_host() */

/*+ 
 * FUNCTION:
 * 	Check the list of responses using Strcasestr()
 */
int check_resp(readstr, resarr)
  char *readstr;
  struct _response  *resarr ;
{
  struct _response *r;

  if (debug > 1)
    fprintf(stderr, "  (debug) check_resp() '%s'\n", readstr);

  for (r = resarr; r ; r = r->next)
  {
    if (r->response == NULL)
      continue;
    if (Strcasestr(readstr, r->response) != NULL)
    {
#ifdef DEBUG
      if (debug > 1)
	fprintf(stderr,"   (debug) check_resp(): Matched '%s'\n", r->response);
#endif
      return(r->severity);
    }
  }	/* for() */

  if (debug)
    fprintf (stderr, "  check_resp(): No response matched for device\n");

  return(-1);		/* No response matched given list */

}	/* check_resp()  */

/*
 * Start non-blocking connect to remote host
 */
int NBconnect(s, host, port)
  int s;	/* socket */
  char *host;
  int port;
{
  int sflags;
  char *lcladdr;
  struct sockaddr_in sin, hostaddr;

  /*
   * Sometimes, you really want the data to come from a specific IP addr
   * on the machine, i.e. for firewalling purposes
   */

  if ( (lcladdr = (char *)getenv("SNIPS_LCLADDR")) ||
       (lcladdr = (char *)getenv("NOCOL_LCLADDR")) )
  {
    bzero((char *)&hostaddr, sizeof(hostaddr));
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_addr.s_addr = inet_addr(lcladdr);
    if (bind(s, (struct sockaddr *)&hostaddr, sizeof(hostaddr)) < 0) {
      perror("bind");
    }
  }
  bzero((char *)&hostaddr, sizeof(hostaddr));

  if (get_inet_address(&sin, host) < 0)
  {
    fprintf(stderr, "inet_addr() failed for ");
    perror(host);
    return (-1);
  }
  sin.sin_port = htons(port);

  sflags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, sflags | O_NONBLOCK);
  return (connect(s, (struct sockaddr *)&sin, sizeof(sin)));

} 

