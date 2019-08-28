/* $Header: /home/cvsroot/snips/pingmon/multiping/multiping.c,v 2.4 2008/04/25 23:31:51 tvroon Exp $ */

/*+ multiping.c
 *
 * Pings multiple sites at once.
 *
 *   -	if the list of hosts is too long, the command line can get
 *	truncated. So can specify the host list on stdin using '-'
 *   -	if you want the pkts to come from one of multiple IP addresses
 *	on a host, you can define the IP address in the environment
 *	variable SNIPS_LCLADDR
 *
 * This version of multiping maintained by:
 *
 *	Vikas Aggarwal, vikas@navya.com
 *
 * Derived from ping.c, original header attached below.
 */

/*
 * $Log: multiping.c,v $
 * Revision 2.4  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 2.3  2001/08/25 16:35:37  vikas
 * Now recieves packets during the interpacket delay in mpinger (earlier
 * only processed one incoming packet). Bug and patch by cbell@junknet.com
 *
 * Revision 2.2  2001/07/11 03:39:49  vikas
 * Needed include inet.h
 *
 * Revision 2.1  2001/07/08 23:17:31  vikas
 * Can specify the IP address of the source packets using SNIPS_LCLADDR
 * (patch submitted by jgreco@ns.sol.net)
 *
 */

/*
 *
 * "@(#)ping.c	5.9 (Berkeley) 5/12/91";
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility, measure
 * round-trip-delays and packet loss across network paths.
 * 
 * Original ping Author -
 *      Mike Muuss
 *      U. S. Army Ballistic Research Laboratory
 *      December, 1983
 * Modified at Uc Berkeley
 * Record Route and verbose headers - Phil Dykstra, BRL, March 1988.
 * ttl, duplicate detection - Cliff Frost, UCB, April 1989
 * Pad pattern - Cliff Frost (from Tom Ferrin, UCSF), April 1989
 * Wait for dribbles, option decoding, pkt compare - vjs@sgi.com, May 1989
 * Ping multiple sites simultaneously - Spencer Sun, Princeton/JvNC, June 1992
 * 
 * Status - Public Domain.  Distribution Unlimited. Bugs - More statistics
 * could always be gathered. This program has to run SUID to ROOT to access
 * the ICMP socket.
 *
 */
/*  Copyright 1997-2000, Netplex Technologies Inc., info@netplex-tech.com */

#ifndef lint
static char rcsid[] = "$Id: multiping.c,v 2.4 2008/04/25 23:31:51 tvroon Exp $";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <limits.h>		/* for LONG_MAX  */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>
#if defined(AIX) || defined(_AIX)
# include <sys/select.h>
#endif

#include <netinet/in_systm.h>	/* required for ip.h */
#include <netinet/in.h>
#include <arpa/inet.h>
#if defined(LINUX) || defined(linux)
# include "ip.h"
# include "ip_icmp.h"
#else
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/ip_var.h>	/* define's MAX_IPOPTLEN */
#endif	/* LINUX */

#include <math.h>

#include "multiping.h"

static int	finish_up = 0;	/* flag to indicate end */

int options;			/* F_* options */
int             max_dup_chk = MAX_DUP_CHK;
int             s = -1;		/* socket file descriptor */
u_char          outpack[MAXPACKET];
char            BSPACE = '\b';	/* characters written for flood */
char            DOT = '.';
unsigned short  ident;		/* process id to identify our packets */

/* counters */
long            npackets;	/* max packets to transmit */
long            ntransmitted;	/* number xmitted */
long            nreceived;      /* number received */
int             interval = 1;	/* interval between packets */

/* timing */
int             timing;		/* flag to do timing */
long            tmax = 0;	/* maximum round trip time */

char 		*prognm ;
char		*pr_addr();
const char *
_pr_addr6(
	struct sockaddr *addr,
	int addrlen);

void            wrapup(), stopit(), finish();

int		packlen ;
u_char		*packet;
int		max_sock = -1;

/* function prototypes */
void fill(char *bp, char *patp);
void pr_pack(char *buf, int cc, struct sockaddr_in *from);
void real_pinger(int which);
void tvsub(register struct timeval *out, register struct timeval *in);
void pr_iph(struct ip *ip);
void setup_sockaddr(char *addr, u_int rtableid);
void pr_retip(struct ip *ip);
void mpinger();
void pr_icmph(struct icmp *icp);
void usage();
int recv_packets(int rsock, struct timeval *ptimeout);
u_short in_cksum(u_short *addr, int len);
int numsites;			/* # of sites being polled */
int datalen;			/* data length */
destrec *dest[MAXREMOTE];	/* remote sites to ping */

/* ipv6 stuff */

int setsocket6();
void real_pinger6(int index);
int recv_packets6(struct timeval *ptimeout);

int isnumeric(const char *c);

int main(argc, argv)
  int             argc;
  char          **argv;
{
  extern int      errno, optind;
  extern char    *optarg;
  struct protoent *proto;
  register int    i;
  int             ch, hold, preload, dostdin, almost_done = 0;
  u_char         *datap;
  char		*lcladdr;
  const char	*errstr;
#ifdef IP_OPTIONS
  char            rspace[3 + 4 * NROUTES + 1];	/* record route space */
#endif
  int s = -1;		/* socket */
  struct timeval intvl;

  prognm = argv[0] ;
  numsites = 0;
  datalen = DEFDATALEN;

  dostdin = 0;
  bzero(dest, sizeof(destrec *) * MAXREMOTE);
  preload = 0;
  /* we store the time sent and the index into destrec[] inside pkt */
  datap = &outpack[8 + sizeof(struct timeval) + sizeof (int)];

  if (argc > 0 && strcmp(argv[argc-1], "-") == 0) {
    dostdin = 1;
    --argc;
  }
  while ((ch = getopt(argc, argv, "tRc:dfh:i:l:np:qrs:v")) != EOF) {
    switch (ch) {
      case 't':
        options |= F_TABULAR_OUTPUT;
        break;
      case 'c':
        npackets = atoi(optarg);
        if (npackets <= 0) {
	  fprintf(stderr, "%s: bad number of packets to transmit.\n", prognm);
	  exit(1);
        }
        break;
      case 'd':
        options |= F_SO_DEBUG;
        break;
      case 'f':
        if (getuid()) {  /* only root can use flood option */
	  fprintf(stderr, "%s: flood: Permission denied.\n", prognm); 
	  exit(1);
        }
        options |= F_FLOOD;
        setbuf(stdout, (char *) NULL);
        break;
      case 'i':			/* wait between sending packets */
        interval = atoi(optarg);
        if (interval <= 0) {
	  fprintf(stderr, "%s: bad timing interval.\n", prognm);
	  exit(1);
        }
        options |= F_INTERVAL;
        break;
      case 'l':
        preload = atoi(optarg);
        if (preload < 0) {
	  fprintf(stderr, "%s: bad preload value.\n", prognm);
	  exit(1);
        }
        break;
      case 'n':                 /* for pr_addr() */
        options |= F_NUMERIC;
        break;
      case 'p':			/* fill buffer with user pattern */
        options |= F_PINGFILLED;
        fill((char *) datap, optarg);
        break;
      case 'q':
        options |= F_QUIET;
        break;
      case 'R':                 /* record route */
        options |= F_RROUTE;
        break;
      case 'r':
        options |= F_SO_DONTROUTE;
        break;
      case 's':			/* size of packet to send */
        datalen = atoi(optarg);
        if (datalen > MAXPACKET) {
	  fprintf(stderr, "%s: packet size too large.\n", prognm);
	  exit(1);
        }
        if (datalen <= 0) {
	  fprintf(stderr, "%s: illegal packet size.\n", prognm);
	  exit(1);
        }
        break;
      case 'v':
        options |= F_VERBOSE;
        break;
      default:
        usage();
    }
  }
  argc -= optind;
  argv += optind;

  if (dostdin)			/* read list of hosts from stdin */
  {
    int  i;
    char thost[256];
    char mthost2[256];
    u_int rtableid;

    for (i = 0; fscanf(stdin, "%s %s", &thost) != EOF; ++i)
#if defined IRIS_VPOLLER && RT_TABLEID_MAX
      rtableid = (unsigned int) strtonum(mthost2, 0, RT_TABLEID_MAX, &errstr);
#else
      rtableid = -1;
#endif


    if (errstr) {
      fprintf(stderr, "adding two thosts (%s, %s)\n", thost, mthost2);
      setup_sockaddr(thost, 0);
      setup_sockaddr(mthost2, 0);
    } else {
      fprintf(stderr, "adding thost %s with rdomain %d\n", thost, rtableid);
      setup_sockaddr(thost, rtableid);
    }

    errstr = NULL;

    if (i <= 0) {
      fprintf(stderr, "ERROR: need at least one host\n");
      exit(1);
    }
  } else {
    if (argc < 1) usage();
    while (argc-- > 0) {
      if (argc) {
#if IRIS_VPOLLER 
     
#if RT_TABLEID_MAX
        u_int rtableid = (unsigned int) strtonum(*(argv+1), 0, RT_TABLEID_MAX, &errstr);
#elif FREEBSD
       u_int rtableid = (unsigned int) strtonum(*(argv+1), 0, INT_MAX, &errstr);
#endif
        setup_sockaddr(*argv, rtableid);
  	if (rtableid > 0 && isnumeric(*(argv+1))) {
	    argv += 2;
  	    argc--;
  	}
  	else {
  	    argv++;
  	}
#else
        u_int rtableid = -1;
        setup_sockaddr(*argv++, 0);
#endif
   
      } else {
        setup_sockaddr(*argv++, 0);
      }
    }
  }

  if (options & F_FLOOD && options & F_INTERVAL) {
    fprintf(stderr, "%s: -f and -i are incompatible options.\n", prognm);
    exit(1);
  }

  /* can we time transfer ? */
  if (datalen >= (sizeof(struct timeval) + sizeof(int)))
    timing = 1;
  packlen = datalen + MAXIPLEN + MAXICMPLEN;
  if (!(packet = (u_char *) malloc((u_int) packlen))) {
    fprintf(stderr, "%s: out of memory.\n", prognm);
    exit(1);
  }
  if (!(options & F_PINGFILLED))
    for (i = 8; i < datalen; ++i)
      *datap++ = i;

  ident = (unsigned short)(getpid() & 0xFFFF);	/* convert to short */

  if (!(proto = getprotobyname("icmp"))) {
    fprintf(stderr, "%s: unknown protocol icmp.\n", prognm);
    exit(1);
  }

  /* Create the ping socket(s) */
  for ( i = 0; i < numsites; i++) { 

    /* Each rtableid should have it's own socket because it has it's own sockopt */
    if (dest[i]->rtableid != -1) {
      dest[i]->sock = setupSocket();
#if IRIS_VPOLLER
#if FREEBSD
    if (setsockopt(dest[i]->sock, SOL_SOCKET, SO_SETFIB, &dest[i]->rtableid, sizeof(dest[i]->rtableid)) == -1)
#elif SO_RTABLE
      if (setsockopt(dest[i]->sock, SOL_SOCKET, SO_RTABLE, &dest[i]->rtableid, sizeof(dest[i]->rtableid)) == -1)
#endif
        printf("warning: setsockopt SO_RTABLE failed for rtable id = %d", dest[i]->rtableid);
#endif
    } else  {
      if (s < 0)
        s = setupSocket();
      dest[i]->sock = s;
    }
    max_sock = (dest[i]->sock > max_sock) ? dest[i]->sock : max_sock;
  }
//currently, no IPV6 support for LINUX
#ifndef LINUX2
  setsocket6();
#endif
  signal(SIGINT, wrapup);

  while (preload--)		/* fire off them quickies */
    mpinger();

  if (options & F_FLOOD) {
    intvl.tv_sec = 0;
    intvl.tv_usec = 10000;	/* 10 ms */
  }
  else {
    long t;		/* ms */
    /* Set t to interval in ms adjusted for time to send pkts */
    t = (interval * 1000) - (INTERPKTGAP * (numsites - 1));
    if (t < 0)
      t = 10;
    intvl.tv_sec  = (int) (t) / 1000;
    intvl.tv_usec = ((int) (t) % 1000) * 1000;
  }

  mpinger();			/* send the first ping */

  while (!finish_up) {

    int err = 0;

#ifdef XXX
    for (i = 0; i < numsites; i++) { 
      if (!dest[i]->isIPV6) { 
        /* recieve packets while waiting for next batch of pings */
        if (recv_packets(dest[i]->sock, &intvl) < 0) { /* fatal error */
          err = 1;
          break;	/* out of for() */
        }
      }
    }
#endif
    if (err) break; /*some fatal error happened. break out of while() */

    if (recv_packets(s, &intvl) < 0) {	/* fatal error */
      break;	/* out of while() */
    }
  	

#ifndef LINUX2
    if (recv_packets6(&intvl) < 0) {	/* fatal error */

      break;	/* out of while() */
    }
#endif

    if (!npackets || ntransmitted < npackets)
      mpinger();
    else {
#ifdef DEBUG2
      printf("all packets are sent - almost_done = %d, tmax = %d, nreceived = %d\n", almost_done, tmax, nreceived);
#endif
      if (almost_done) {
#ifdef DEBUG2
	printf("all done - getting out of loop, and out to finish routine\n");
#endif
        break;		/* second time around */
      }
      almost_done = 1;
      intvl.tv_usec = 0;
      if (tmax > 0 || nreceived > 0) {/* have recieved some packets back */
	intvl.tv_sec = 2 * tmax / 1000;
	if (!intvl.tv_sec)
	  intvl.tv_sec = 1;
        else if (intvl.tv_sec > MAXWAIT)
          intvl.tv_sec = MAXWAIT + 1;
      }
      else
	intvl.tv_sec = MAXWAIT + 1;
#ifdef DEBUG2
      printf("intvl.tv_sec = %d, intvl.tv_usec = %d\n", intvl.tv_sec, intvl.tv_usec);
#endif
    }
  }	/* end: while() */
#ifdef DEBUG2
 printf("finish line for multiping reached\n");
#endif
  finish();
  return(0);

}	/* end: main() */

/*
 * On the first SIGINT, set npackets = ntransmitted so that the main
 * loop waits for any stragglers and ends the pinging.
 */
void
wrapup()
{
  int i;

  signal(SIGINT, stopit);		/* really quit on next interrupt */

  nreceived = 0;
  for (i = 0; i < numsites; i++)    	/* find max 'nreceived' */
    if (dest[i]->nreceived > nreceived)
      nreceived = dest[i]->nreceived;

  npackets = ntransmitted + 1;	/* forces wait before exit in main */
}

/*
 * called on signal (interrup). Just sets flag.
 */
void
stopit()
{
  finish_up = 1;
}

/*
 * mpinger -- front end to real_pinger() so that all sites get pinged with
 * each invocation of pinger().
 * Need to add a small delay to prevent the kernel overflow from packets.
 * We use 'select' to get small interpkt delays between the ping for each
 * site and utilize the waiting period to see if any responses came in.
 * If a valid packet comes in, the period waited is unknown, and hence,
 * we do another wait() to force a pause for the INTERPKTGAP time.
 */

void mpinger()
{
  int	i;
  struct timeval interpktdelay;

  interpktdelay.tv_sec = 0;
  interpktdelay.tv_usec = (INTERPKTGAP * 1000);	/* in millisecs */

  for (i = 0; i < numsites; i++) {
    if (dest[i]->isIPV6) {
#ifndef LINUX2
      real_pinger6(i);
      if (recv_packets6(&interpktdelay) < 0)
        continue;	/* ignore errors */
#endif
    } else {
      if (dest[i]->sock < 0) continue;		
      real_pinger(i);
     
      if (recv_packets(dest[i]->sock, &interpktdelay) < 0)
        continue;	/* ignore errors */
    }
  }	/* end:  for(numsites...)  */
	
  ++ntransmitted;
}

/*
 * real_pinger -- was 'pinger' in the original but pinger() is now a front
 * end to this one.
 * 
 * Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet will be
 * added on by the kernel.  The ID field is our UNIX process ID, and the
 * sequence number is an ascending integer.  The first 8 bytes of the data
 * portion are used to hold a UNIX "timeval" struct in VAX byte-order, to
 * compute the round-trip time.
 */
void real_pinger(which)
  int             which;       /* index into dest[] array, tells which */
                               /* site is being pinged */
{
  register struct icmp *icp;
  register int    cc;
  int             i;
  struct sockaddr_in *to;

  /* fill in the icmp headr */
  icp = (struct icmp *) outpack;
  icp->icmp_type = ICMP_ECHO;                   /* this is an echo request */
  icp->icmp_code = 0; 
  icp->icmp_cksum = 0;
  icp->icmp_seq = dest[which]->ntransmitted++;  /* sequence # */
  icp->icmp_id = ident;		                /* ID # == our pid */

  CLR(which, icp->icmp_seq % max_dup_chk);

  if (dest[which]->sock < 0) return;

  s = dest[which]->sock;
  /* record time at which we sent the packet out and index into destrec[] */
  if (timing) {
    gettimeofday((struct timeval *) & outpack[8], (struct timezone *) NULL);
    bcopy(&which, outpack + 8 + sizeof(struct timeval), sizeof(int));
  }

  cc = datalen + 8;		/* skips ICMP pkt portion */

  /* compute ICMP checksum here */
  icp->icmp_cksum = in_cksum((u_short *) icp, cc);

  to = &dest[which]->sockad;

  i = sendto(s, (char *) outpack, cc, 0, (struct sockaddr *)to, sizeof(struct sockaddr));

#ifdef DEBUG2
  fprintf(stderr, "    wrote %d chars for %s on socket %d, sent=%d, socket=\n", cc, inet_ntoa(to->sin_addr), s, i);
#endif

  if (i < 0 || i != cc) {
    if (i < 0)
      perror("ping: sendto");
  }
  if (!(options & F_QUIET) && options & F_FLOOD)
    write(STDOUT_FILENO, &DOT, 1);
}

/*
 * Recieve packets for a max of specified timeout interval. Return 0 on
 * timeout, -1 on some fatal read error from socket.
 */

int recv_packets(rsock, ptimeout)
  int rsock;			/* socket file descriptor */
  struct timeval *ptimeout;	/* how long to wait */
{
  int	fromlen, cc;
  struct sockaddr_in  from;
  struct timeval  timeout, now, start;
  fd_set       	rfds;
  static int counted = 0;
  gettimeofday(&start, NULL);

  while (1)
  {
    int n;	/* select return value */
    int i;

    FD_ZERO(&rfds);
    for (i=0; i<numsites; i++)
      FD_SET(dest[i]->sock, &rfds);

    gettimeofday(&now, NULL);

    timeout.tv_sec =  ptimeout->tv_sec - (now.tv_sec - start.tv_sec);
    timeout.tv_usec = ptimeout->tv_usec - (now.tv_usec - start.tv_usec);
    
    //making sure we never beyond 60 second wait.
    if (now.tv_sec - start.tv_sec > MAXWAIT * 60) {
      break;
    }
    while (timeout.tv_usec < 0) {
      timeout.tv_usec += 1000000;
      timeout.tv_sec--;
    }
    while (timeout.tv_usec >= 1000000) {
      timeout.tv_usec -= 1000000;
      timeout.tv_sec++;
    }
    if (timeout.tv_sec < 0)
      timeout.tv_sec = timeout.tv_usec = 0;
#ifdef DEBUG2
    fprintf(stderr, "(debug) %s: recv_packets timeout = %ld.%ld\n", prognm, timeout.tv_sec, timeout.tv_usec);
#endif

    if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
#ifdef DEBUG2
      printf("current recv_packet timeout occured, returning..\n");
#endif
      return (0);
    }

    n = select(max_sock + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);

    /* some error */
    if (n < 0)
    {
      if (errno == EINTR)
	continue;		/* interrupted, so wait again */

      fprintf(stderr, "%s: select() failed on socket %d -", prognm, max_sock);
      perror("");
      break;	/* fatal error, out of while */
    }

    /* check all sockets that are ready for reading */
    for (i=0; i<numsites; i++) {
      if (n > 0 && FD_ISSET(dest[i]->sock, &rfds))	/* recieve and print packet */
      {
        fromlen = sizeof(from);
        if ((cc = recvfrom(dest[i]->sock, (char *) packet, packlen, 0, (struct sockaddr *) &from, &fromlen)) < 0)
        {
          if (errno == EINTR || errno == EAGAIN)
            continue;
          fprintf(stderr, "%s: recvfrom() on fd %d -", prognm, dest[i]->sock);
          perror("");
          if (errno == EBADF || errno == ENOTSOCK)
          break;	/* fatal error, out of while */

          continue;
        }

        counted++;
        pr_pack((char *) packet, cc, &from);	/* process packet */
      } /* if (n > 0 )*/
    }	/* for */

  }	/* while 1 */

  /* here only upon error */
  return (-1);

}     /* recv_packets() */


/*
 * pr_pack -- Print out the packet, if it came from us.  This logic is
 * necessary because ALL readers of the ICMP socket get a copy of ALL ICMP
 * packets which arrive ('tis only fair).  This permits multiple copies of
 * this program to be run without having intermingled output (or
 * statistics!).
 */
void pr_pack(buf, cc, from)
  char           *buf;
  int             cc;
  struct sockaddr_in *from;
{
  register struct icmp *icp;
  register u_long l;
  register int    i, j;
  register u_char *cp, *dp;
  static int      old_rrlen;
  static char     old_rr[MAX_IPOPTLEN];
  struct ip      *ip;
  struct timeval  tv, *tp;
  long            triptime = 0;
  int             hlen, dupflag;

  /* record time when we received the echo reply */
  gettimeofday(&tv, (struct timezone *) NULL);

  /* Check the IP header */
  ip = (struct ip *) buf;
  hlen = ip->ip_hl << 2;
  if (cc < hlen + ICMP_MINLEN) {
    if (options & F_VERBOSE)
      fprintf(stderr, "%s: packet too short (%d bytes) from %s\n", prognm, cc,
	inet_ntoa(from->sin_addr));
    return;
  }
  /* Now the ICMP part */
  cc -= hlen;
  icp = (struct icmp *) (buf + hlen);
  if (icp->icmp_type == ICMP_ECHOREPLY)
  {				/* if ICMP ECHO type */
    int 	wherefrom;
    destrec	*dst;

    /* first see if this reply is addressed to our process */
    if (icp->icmp_id != ident)
      return;			/* 'Twas not our ECHO */

    /* if so, figure out which site it is in the dest[] array */
    if (timing)	/* easier using buf+hlen instead of icp to access wherefrom */
      bcopy(buf + hlen + 8 + sizeof(struct timeval), &wherefrom, sizeof(int));
    if (!timing || wherefrom >= numsites)
      for (wherefrom = 0; wherefrom < numsites; wherefrom++)
	/* Need to compare the individual fields and not entire structure */
	if ((dest[wherefrom]->sockad.sin_family == from->sin_family) &&    
	    (dest[wherefrom]->sockad.sin_port == from->sin_port) &&
	    (dest[wherefrom]->sockad.sin_addr.s_addr == from->sin_addr.s_addr))
	  break;
    if (wherefrom >= numsites)
    {
      fprintf(stderr,
	      "%s: received ICMP_ECHOREPLY from someone we didn't send to!?%d\n",
	      prognm, wherefrom);
      return;
    }

    if (TST(wherefrom, icp->icmp_seq % max_dup_chk)) {
    	//i think we should just leave process if that's duplicate?
    	return;
	}
    dst = dest[wherefrom];
    ++dst->nreceived;

    /* figure out round trip time, check min/max */
    if (timing) {
#ifndef icmp_data
      tp = (struct timeval *) & icp->icmp_ip;
#else
      tp = (struct timeval *) icp->icmp_data;
#endif
      tvsub(&tv, tp);
      triptime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
      dst->tsum += triptime;
      dst->tsum2 += triptime * triptime;
      if (triptime < dst->tmin)
	dst->tmin = triptime;
      if (triptime > dst->tmax)
	dst->tmax = triptime;
      if (triptime > tmax)
        tmax = triptime;
    }
    

    /* check for duplicate echoes. If a duplicate, check in case the host
     * name was repeated on the command line
     */
    if (TST(wherefrom, icp->icmp_seq % max_dup_chk)) {
      ++dst->nrepeats;
      --dst->nreceived;
      dst->tsum -= triptime;
      dst->tsum2 -= triptime * triptime;

      dupflag = 1;
    }
    else {
      SET(wherefrom, icp->icmp_seq % max_dup_chk);
      dupflag = 0;
    }

    if (options & F_QUIET)
      return;

    if (options & F_FLOOD)
      write(STDOUT_FILENO, &BSPACE, 1);
    else {
      printf("%d bytes from %s: icmp_seq=%u", cc,
	inet_ntoa(from->sin_addr),
        icp->icmp_seq);
      printf(" ttl=%d", ip->ip_ttl);
      if (timing)
	printf(" time=%ld ms", triptime);
      if (dupflag)
	printf(" (DUP!)");
      /* check the data. Note data when sent was at  8+time+index */
      cp = (u_char *) & icp->icmp_data[sizeof(struct timeval) + sizeof(int)];
      dp = &outpack[8 + sizeof(struct timeval) + sizeof(int)];
      for (i = sizeof(struct timeval) + sizeof(int);
	   i < datalen; ++i, ++cp, ++dp)
      {
	if (*cp != *dp) {
	  printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
	    i, *dp, *cp);
	  cp = (u_char *) & icp->icmp_data[0];
	  for (i = 8; i < datalen; ++i, ++cp) {
	    if ((i % 32) == 8)
	      printf("\n\t");
	    printf("%x ", *cp);
	  }
	  break;
	}
      }	/* for i < datalen */
    }	/* if-else options & F_FLOOD */
  }	/* if ICMP_ECHO */
  else
  {		/* We've got something other than an ECHOREPLY */
    if (!(options & F_VERBOSE))
      return;
    printf("%d bytes from %s: ", cc, pr_addr(from->sin_addr.s_addr));
    pr_icmph(icp);
  }

  /* Display any IP options */
  cp = (u_char *) buf + sizeof(struct ip);

  for (; hlen > (int) sizeof(struct ip); --hlen, ++cp)
    switch (*cp) {
    case IPOPT_EOL:
      hlen = 0;
      break;
    case IPOPT_LSRR:
      printf("\nLSRR: ");
      hlen -= 2;
      j = *++cp;
      ++cp;
      if (j > IPOPT_MINOFF)
	for (;;) {
	  l = *++cp;
	  l = (l << 8) + *++cp;
	  l = (l << 8) + *++cp;
	  l = (l << 8) + *++cp;
	  if (l == 0)
	    printf("\t0.0.0.0");
	  else
	    printf("\t%s", pr_addr(ntohl(l)));
	  hlen -= 4;
	  j -= 4;
	  if (j <= IPOPT_MINOFF)
	    break;
	  putchar('\n');
	}
      break;
    case IPOPT_RR:
      j = *++cp;		/* get length */
      i = *++cp;		/* and pointer */
      hlen -= 2;
      if (i > j)
	i = j;
      i -= IPOPT_MINOFF;
      if (i <= 0)
	continue;
      if (i == old_rrlen
	  && cp == (u_char *) buf + sizeof(struct ip) + 2
	  && !bcmp((char *) cp, old_rr, i)
	  && !(options & F_FLOOD)) {
	printf("\t(same route)");
	i = ((i + 3) / 4) * 4;
	hlen -= i;
	cp += i;
	break;
      }
      old_rrlen = i;
      bcopy((char *) cp, old_rr, i);
      printf("\nRR: ");
      for (;;) {
	l = *++cp;
	l = (l << 8) + *++cp;
	l = (l << 8) + *++cp;
	l = (l << 8) + *++cp;
	if (l == 0)
	  printf("\t0.0.0.0");
	else
	  printf("\t%s", pr_addr(ntohl(l)));
	hlen -= 4;
	i -= 4;
	if (i <= 0)
	  break;
	putchar('\n');
      }
      break;
    case IPOPT_NOP:
      printf("\nNOP");
      break;
    default:
      printf("\nunknown option %x", *cp);
      break;
    }
  if (!(options & F_FLOOD)) {
    putchar('\n');
    fflush(stdout);
  }
}

/*
 * in_cksum -- Checksum routine for Internet Protocol family headers (C
 * Version)
 */
u_short in_cksum(addr, len)
  u_short        *addr;
  int             len;
{
  register int    nleft = len;
  register u_short *w = addr;
  register int    sum = 0;
  u_short         answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the carry
   * bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(u_char *) (&answer) = *(u_char *) w;
    sum += answer;
  }
  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
  sum += (sum >> 16);		/* add carry */
  answer = ~sum;		/* truncate to 16 bits */
  return (answer);
}

/*
 * tvsub -- Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
void tvsub(out, in)
  register struct timeval *out, *in;
{
  if ((out->tv_usec -= in->tv_usec) < 0) {
    --out->tv_sec;
    out->tv_usec += 1000000;
  }
  out->tv_sec -= in->tv_sec;
}

/*
 * In case anyone's using a program that depends on the output being
 * in the non-tabular format
 */
void
output_old_style()
{
  int             i;
  destrec        *dp;

  for (i = 0; i < numsites; i++) {
    dp = dest[i];
    printf("--- %s ping statistics ---\n",pr_addr(dp->sockad.sin_addr.s_addr));
    printf("%ld packets transmitted, ", dp->ntransmitted);
    printf("%ld packets received, ", dp->nreceived);
    if (dp->nrepeats)
      printf("+%ld duplicates, ", dp->nrepeats);
    if (dp->ntransmitted)
    {
      if (dp->nreceived > dp->ntransmitted)
	printf("-- somebody's printing up packets!");
      else
	printf("%d%% packet loss",
		      (int) (((dp->ntransmitted - dp->nreceived) * 100) /
			     dp->ntransmitted));
    }
    putchar('\n');
    if (dp->nreceived && timing)
      printf("round-trip min/avg/max = %.2f/%.2f/%.2f ms\n",
		    dp->tmin, dp->tsum / (dp->nreceived + dp->nrepeats),
		    dp->tmax);
  }
}

void
output_new_style()
{
  int             i;
  destrec        *dp;
  long            sent, rcvd, rpts, tmin, tsum, tmax;

  printf("-=-=- PING statistics -=-=-\n");
  printf("                                      Number of Packets");
  if (timing)
    printf("         Round Trip Time\n");
  printf("Remote Site                     Sent    Rcvd    Rptd   Lost ");
  if (timing)
    printf("    Min   Avg   Max   StD");
  printf("\n-----------------------------  ------  ------  ------  ----");
  if (timing)
    printf("    ----  ----  ----  ----");
  sent = rcvd = rpts = tmax = tsum = 0;
  tmin = LONG_MAX;
  for (i = 0; i < numsites; i++) {
    dp = dest[i];
    printf("\n%-29.29s  %6ld  %6ld  %6ld%c %3ld%%",
      /*(dp->isIPV6) ? _pr_addr6((struct sockaddr *)&dp->sockad6, sizeof(dp->sockad6)):*/pr_addr(dp->sockad.sin_addr.s_addr),
      dp->ntransmitted, dp->nreceived, dp->nrepeats,
      (dp->nreceived > dp->ntransmitted) ? '!' : ' ',
      (long) (((dp->ntransmitted - dp->nreceived) * 100) / dp->ntransmitted));
    sent += dp->ntransmitted;
    rcvd += dp->nreceived;
    rpts += dp->nrepeats;
    if (timing)
    {
      if (dp->nreceived)
      {
      	double x = (((float) dp->tsum2) / dp->nreceived) - pow(((float) dp->tsum) / dp->nreceived, 2);
        double stdev = sqrt(abs(x));
        	
        if (isnan(stdev) || isinf(stdev))
        	stdev = 0;
        	
        printf("    %4.2f  %4.2f  %4.2f  % 4.1f", dp->tmin,
          dp->tsum / dp->nreceived, dp->tmax, stdev);
        if (dp->tmin < tmin)
          tmin = dp->tmin;
        if (dp->tmax > tmax)
          tmax = dp->tmax;
        tsum += dp->tsum;
      } else        
        printf("       0     0     0");
    }
  }
  printf("\n-----------------------------  ------  ------  ------  ----");
  if (timing)
    printf("    ----  ----  ----  ----");
  if (numsites > 1) {
    printf("\nTOTALS                         %6ld  %6ld  %6ld  %3ld%%",
      sent, rcvd, rpts, (long) (((sent - rcvd) * 100) / sent));
    if (timing)
    {
      if (rcvd)		/* avoid divide by zero */
	printf("    %4ldk  %4ldk  %4ldk", tmin, tsum / rcvd, tmax);
      else
        printf("       0     0     0");
    }

  }
  putchar('\n');
  exit(0);
}

/*
 * finish -- Print out statistics, and give up.
 */
void
finish()
{
  signal(SIGINT, SIG_IGN);
  putchar('\n');
  if (!(options & F_TABULAR_OUTPUT))
    output_old_style();
  else
    output_new_style();
  exit(0);
}

#ifdef notdef
static char    *ttab[] = {
  "Echo Reply",			/* ip + seq + udata */
  "Dest Unreachable",		/* net, host, proto, port, frag, sr + IP */
  "Source Quench",		/* IP */
  "Redirect",			/* redirect type, gateway, + IP  */
  "Echo",
  "Time Exceeded",		/* transit, frag reassem + IP */
  "Parameter Problem",		/* pointer + IP */
  "Timestamp",			/* id + seq + three timestamps */
  "Timestamp Reply",		/* " */
  "Info Request",		/* id + sq */
  "Info Reply"			/* " */
};
#endif

/*
 * pr_icmph -- Print a descriptive string about an ICMP header.
 */
void pr_icmph(icp)
  struct icmp    *icp;
{
  switch (icp->icmp_type) {
  case ICMP_ECHOREPLY:
    printf("Echo Reply\n");
    /* XXX ID + Seq + Data */
    break;
  case ICMP_UNREACH:
    switch (icp->icmp_code) {
    case ICMP_UNREACH_NET:
      printf("Destination Net Unreachable\n");
      break;
    case ICMP_UNREACH_HOST:
      printf("Destination Host Unreachable\n");
      break;
    case ICMP_UNREACH_PROTOCOL:
      printf("Destination Protocol Unreachable\n");
      break;
    case ICMP_UNREACH_PORT:
      printf("Destination Port Unreachable\n");
      break;
    case ICMP_UNREACH_NEEDFRAG:
      printf("frag needed and DF set\n");
      break;
    case ICMP_UNREACH_SRCFAIL:
      printf("Source Route Failed\n");
      break;
    default:
      printf("Dest Unreachable, Bad Code: %d\n",
		    icp->icmp_code);
      break;
    }
    /* Print returned IP header information */
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_SOURCEQUENCH:
    printf("Source Quench\n");
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_REDIRECT:
    switch (icp->icmp_code) {
    case ICMP_REDIRECT_NET:
      printf("Redirect Network");
      break;
    case ICMP_REDIRECT_HOST:
      printf("Redirect Host");
      break;
    case ICMP_REDIRECT_TOSNET:
      printf("Redirect Type of Service and Network");
      break;
    case ICMP_REDIRECT_TOSHOST:
      printf("Redirect Type of Service and Host");
      break;
    default:
      printf("Redirect, Bad Code: %d", icp->icmp_code);
      break;
    }
    printf("(New addr: 0x%08lx)\n", (long unsigned int)icp->icmp_gwaddr.s_addr);
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_ECHO:
    printf("Echo Request\n");
    /* XXX ID + Seq + Data */
    break;
  case ICMP_TIMXCEED:
    switch (icp->icmp_code) {
    case ICMP_TIMXCEED_INTRANS:
      printf("Time to live exceeded\n");
      break;
    case ICMP_TIMXCEED_REASS:
      printf("Frag reassembly time exceeded\n");
      break;
    default:
      printf("Time exceeded, Bad Code: %d\n",
		    icp->icmp_code);
      break;
    }
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_PARAMPROB:
    printf("Parameter problem: pointer = 0x%02x\n",
		  icp->icmp_hun.ih_pptr);
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_TSTAMP:
    printf("Timestamp\n");
    /* XXX ID + Seq + 3 timestamps */
    break;
  case ICMP_TSTAMPREPLY:
    printf("Timestamp Reply\n");
    /* XXX ID + Seq + 3 timestamps */
    break;
  case ICMP_IREQ:
    printf("Information Request\n");
    /* XXX ID + Seq */
    break;
  case ICMP_IREQREPLY:
    printf("Information Reply\n");
    /* XXX ID + Seq */
    break;
#ifdef ICMP_MASKREQ
  case ICMP_MASKREQ:
    printf("Address Mask Request\n");
    break;
#endif
#ifdef ICMP_MASKREPLY
  case ICMP_MASKREPLY:
    printf("Address Mask Reply\n");
    break;
#endif
  default:
    printf("Bad ICMP type: %d\n", icp->icmp_type);
  }
}

/*
 * pr_iph -- Print an IP header with options.
 */
void pr_iph(ip)
  struct ip      *ip;
{
  int             hlen;
  u_char         *cp;

  hlen = ip->ip_hl << 2;
  cp = (u_char *) ip + 20;	/* point to options */

  printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst Data\n");
  printf(" %1x  %1x  %02x %04x %04x",
		ip->ip_v, ip->ip_hl, ip->ip_tos, ip->ip_len, ip->ip_id);
  printf("   %1x %04x", ((ip->ip_off) & 0xe000) >> 13,
		(ip->ip_off) & 0x1fff);
  printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ip->ip_sum);
  printf(" %s ", inet_ntoa(ip->ip_src));
  printf(" %s ", inet_ntoa(ip->ip_dst));
  /* dump and option bytes */
  while (hlen-- > 20) {
    printf("%02x", *cp++);
  }
  putchar('\n');
}

/*
 * pr_addr -- Return an ascii host address as a dotted quad and optionally
 * with a hostname.
 */
char           *
pr_addr(l)
  u_long          l;
{
  struct hostent *hp;
  static char     buf[80];
  struct in_addr tmp_inaddr;	/* tmp struct for inet_ntoa */

  tmp_inaddr.s_addr = l;
  if ((options & F_NUMERIC) ||
      !(hp = gethostbyaddr((char *) &l, 4, AF_INET)))
    sprintf(buf, "%s", inet_ntoa(tmp_inaddr));
  else
    sprintf(buf, "%s (%s)", hp->h_name, inet_ntoa(tmp_inaddr));
  return (buf);
}

/*
 * pr_retip -- Dump some info on a returned (via ICMP) IP packet.
 */
void pr_retip(ip)
  struct ip      *ip;
{
  int             hlen;
  u_char         *cp;

  pr_iph(ip);
  hlen = ip->ip_hl << 2;
  cp = (u_char *) ip + hlen;

  if (ip->ip_p == 6)
    printf("TCP: from port %u, to port %u (decimal)\n",
		  (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
  else if (ip->ip_p == 17)
    printf("UDP: from port %u, to port %u (decimal)\n",
		  (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
}

void fill(bp, patp)
  char           *bp, *patp;
{
  register int    ii, jj, kk;
  int             pat[16];
  char           *cp;

  for (cp = patp; *cp; cp++)
    if (!isxdigit(*cp)) {
      fprintf(stderr, "%s: patterns must be specified as hex digits.\n",prognm);
      exit(1);
    }
  ii = sscanf(patp,
	      "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	      &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	      &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	      &pat[13], &pat[14], &pat[15]);

  if (ii > 0)
    for (kk = 0; kk <= MAXPACKET - (8 + ii); kk += ii)
      for (jj = 0; jj < ii; ++jj)
	bp[jj + kk] = pat[jj];
  if (!(options & F_QUIET)) {
    printf("PATTERN: 0x");
    for (jj = 0; jj < ii; ++jj)
      printf("%02x", bp[jj] & 0xFF);
    printf("\n");
  }
}

/*
 * allocates a destrec and initializes its fields
 */ 
destrec *dest_malloc()
{
  destrec *p;

  p = (destrec *)malloc(sizeof(destrec));
  if ((p == NULL) || ((p->rcvd_tbl = (char *)malloc(MAX_DUP_CHK / 8)) == NULL))
  {
    fprintf(stderr, "multiping: dest_malloc: out of memory\n");
    exit(1);
  }
  bzero(p->rcvd_tbl, MAX_DUP_CHK/8);
  p->nreceived = p->nrepeats = p->ntransmitted = (char)0;
  p->hostname[0] = '\0';
  p->tmax = p->tsum = 0;
  p->tmin = SHRT_MAX;
  return p;
}  

/**
 sets up host addr for ipv6 host

**/
int setup_sockaddr6(addr, rtableid, dst)
  char *addr;
  u_int rtableid;
  destrec *dst;
{
  struct addrinfo hints;
  struct addrinfo *res;

  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_RAW;
  hints.ai_protocol = IPPROTO_ICMPV6;

  int ret_ga = getaddrinfo(addr, NULL, &hints, &res);

  if (ret_ga) {
    //point to go for ipv4 version
    fprintf(stderr, "ping6: %s\n", gai_strerror(ret_ga));
    return 0;
  }

  if (!res->ai_addr) {
    //point to go for ipv4 ping
    return 0;
  }
	
  if (res->ai_canonname)
    strncpy(dst->hostname, res->ai_canonname, MAXHOSTNAMELEN-1);
  else
    strncpy(dst->hostname, addr, MAXHOSTNAMELEN-1);

  memcpy(&dst->sockad6, res->ai_addr, res->ai_addrlen);
  dst->isIPV6 = 1;
  return 1;
}


/*
 * Takes a host name like 'phoenix.princeton.edu' or an IP address
 * like '128.112.120.1' and creates an entry for that host in the
 * dest[] array.
 */
void setup_sockaddr(addr, rtableid)
  char *addr;
  u_int rtableid;
{
  destrec *dst;
  struct sockaddr_in *to;
  struct hostent *hp;

  dst = dest_malloc();
  dst->rtableid = rtableid;
  dst->isIPV6 = 0;
  bzero((char*)&(dst->sockad), sizeof(struct sockaddr_in));
  to = &dst->sockad;
  to->sin_family = PF_INET;
  to->sin_addr.s_addr = inet_addr(addr);


  if (to->sin_addr.s_addr == 0) {
    fprintf(stderr, "ping: unknown host %s\n", addr);
    dst->sock = -1;
    dst->ntransmitted = dst->nreceived = dst->nrepeats = 0;
    dest[numsites++] = dst;
    return;
  }
#ifdef INADDR_NONE
  else if (to->sin_addr.s_addr != INADDR_NONE)
#else
  else if (to->sin_addr.s_addr != (u_int)-1)
#endif
    strcpy(dst->hostname, addr);
  else {
    hp = gethostbyname(addr);
    if (hp == NULL) {
    	if (!setup_sockaddr6(addr, rtableid, dst)) {
           fprintf(stderr, "ping: unknown host %s\n", addr);
           dst->sock = -1;
           dst->ntransmitted = dst->nreceived = dst->nrepeats = 0;
           dest[numsites++] = dst;
           return;
       }
    }
    
    if (dst->isIPV6 == 0 ) {
      to->sin_family = hp->h_addrtype;
      bcopy(hp->h_addr, (caddr_t)&to->sin_addr, hp->h_length);
      strncpy(dst->hostname, hp->h_name, MAXHOSTNAMELEN-1);
    }
  }

  dest[numsites++] = dst;
  
  if (dst->isIPV6) {
    if (dst->sockad6.sin6_family == PF_INET6 ||  dst->sockad6.sin6_family == AF_INET6 )
      printf("PING %s: %d data bytes (rtableid %d)\n", _pr_addr6((struct sockaddr *)&dst->sockad6, sizeof(dst->sockad6)), datalen, dst->rtableid);
    else
      printf("PING %s: %d data bytes (rtableid %d)", dst->hostname, datalen, dst->rtableid);
  } else {
    if (to->sin_family == PF_INET )
      printf("PING %s (%s): %d data bytes (rtableid %d)\n", pr_addr(to->sin_addr.s_addr), inet_ntoa(to->sin_addr), datalen, dst->rtableid);
    else
      printf("PING %s: %d data bytes (rtableid %d)", dst->hostname, datalen, dst->rtableid);
  }

  if (numsites > MAXREMOTE) {
    fprintf(stderr, "multiping: Error- number of hosts exceeds MAX %d\n", MAXREMOTE);
    exit (1);
  }
}

/*
 * Takes a host name like 'phoenix.princeton.edu' or an IP address
 * like '128.112.120.1' and creates an entry for that host in the
 * dest[] array.
 */
/*void setup_sockaddr(addr, rtableid)
  char *addr;
  u_int rtableid;
{
  destrec *dst;
  struct sockaddr_in *to;
  struct hostent *hp;

  dst = dest_malloc();
  dst->rtableid = rtableid;

  bzero((char*)&(dst->sockad), sizeof(struct sockaddr_in));
  to = &dst->sockad;
  to->sin_family = PF_INET;
  to->sin_addr.s_addr = inet_addr(addr);
  if (to->sin_addr.s_addr == 0) {
      fprintf(stderr, "ping: unknown host %s\n", addr);
      exit (1);
  }


#ifdef INADDR_NONE
  else if (to->sin_addr.s_addr != INADDR_NONE)
#else
  else if (to->sin_addr.s_addr != (u_int)-1)
#endif
    strcpy(dst->hostname, addr);
  else {
    hp = gethostbyname(addr);
    if (hp == NULL) {
      fprintf(stderr, "ping: unknown host %s\n", addr);
      exit(1);
    }
    to->sin_family = hp->h_addrtype;
    bcopy(hp->h_addr, (caddr_t)&to->sin_addr, hp->h_length);
    strncpy(dst->hostname, hp->h_name, MAXHOSTNAMELEN-1);
  }
  dest[numsites++] = dst;
  if (to->sin_family == PF_INET)
     printf("PING %s (%s): %d data bytes (rtableid %d)\n", pr_addr(to->sin_addr.s_addr),
           inet_ntoa(to->sin_addr), datalen, dst->rtableid);
   else
     printf("PING %s: %d data bytes (rtableid %d)", dst->hostname, datalen, dst->rtableid);

  if (numsites > MAXREMOTE)
  {
      fprintf(stderr, "multiping: Error- number of hosts exceeds MAX %d\n",
	      MAXREMOTE);
      exit (1);
  }
}	/* setup_sockaddr() */

void usage()
{
  fprintf(stderr, "usage: %s [-Rdfnqrtv] [-c count] [-i wait] [-l preload]\n",
	  prognm);
  fprintf(stderr, "            [-p pattern] [-s packetsize] host|-\n\n");
  fprintf(stderr, "       Options:\n");
  fprintf(stderr, "       R: ICMP record route\n");
  fprintf(stderr, "       d: set SO_DEBUG socket option\n");
  fprintf(stderr, "       f: 'flood' mode\n");
  fprintf(stderr, "       n: force addresses to be displayed in numeric format\n");
  fprintf(stderr, "       r: set SO_DONTROUTE socket option\n");
  fprintf(stderr, "       t: show results in tabular form\n");
  fprintf(stderr, "       v: verbose mode for ICMP stuff\n");
  fprintf(stderr, "    If hostname is '-', you can specify the list of hosts on stdin\n");

  exit(1);
}


/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
const char *
_pr_addr6(addr, addrlen)
	struct sockaddr *addr;
	int addrlen;
{
	static char buf[NI_MAXHOST];
	int flag;

#ifdef NI_WITHSCOPEID
	flag = NI_WITHSCOPEID;
#else
	flag = 0;
#endif

	if (getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0, flag) == 0)
		return (buf);
	else
		return "?";
}


int setupSocket()
{
  int sock;
  extern int errno, optind;
  extern char *optarg;
  struct timeval intvl;
  struct protoent *proto;
  register int i;
  int ch, hold, preload, dostdin, almost_done = 0;
  u_char *datap;
  char*lcladdr;
  const char *errstr;
#ifdef IP_OPTIONS
  char rspace[3 + 4 * NROUTES + 1];	/* record route space */
#endif
 
  if (!(proto = getprotobyname("icmp"))) {
    fprintf(stderr, "%s: unknown protocol icmp.\n", prognm);
    exit(1);
  }
  if ((sock = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
    perror("ping: socket");
    exit(1);
  }

  /*
   * Sometimes, you really want the data to come from a specific IP addr
   * on the machine, i.e. for firewalling purposes
   */
 
  if ( (lcladdr = getenv("SNIPS_LCLADDR")) ||
       (lcladdr = getenv("NOCOL_LCLADDR")) )
  {
    struct sockaddr_in hostaddr;

    bzero((char *)&hostaddr, sizeof(hostaddr));
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_addr.s_addr = inet_addr(lcladdr);
    if (bind(sock, (struct sockaddr *)&hostaddr, sizeof(hostaddr)) < 0) {
      perror("bind");
      return -1;
    }
  }

  hold = 1;	/* use temporarily for setsockopt() */
  if (options & F_SO_DEBUG)
    setsockopt(sock, SOL_SOCKET, SO_DEBUG, (char *) &hold, sizeof(hold));
  if (options & F_SO_DONTROUTE)
    setsockopt(sock, SOL_SOCKET, SO_DONTROUTE, (char *) &hold, sizeof(hold));


  /* record route option */
  if (options & F_RROUTE) {
#ifdef IP_OPTIONS
    rspace[IPOPT_OPTVAL] = IPOPT_RR;
    rspace[IPOPT_OLEN] = sizeof(rspace) - 1;
    rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
    if (setsockopt(sock, IPPROTO_IP, IP_OPTIONS, rspace, sizeof(rspace)) < 0) {
      perror("ping: record route");
      return -1;
    }
#else
    fprintf(stderr,
      "%s: record route not available in this implementation.\n", prognm);
    return -1;
#endif				/* IP_OPTIONS */
  }
  /*
   * When pinging the broadcast address, you can get a lot of answers. Doing
   * something so evil is useful if you are trying to stress the ethernet, or
   * just want to fill the arp cache to get some stuff for /etc/ethers.
   */
  hold = 48 * 1024;
  setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &hold, sizeof(hold));
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *) &hold, sizeof(hold));

  return sock;
}


/*! figure out if given string is a number*/
int isnumeric(const char *str) {
    char *ch = str;
    while (*ch) {
        if (!isdigit(*ch)) {
            return 0;
        }
        ch++;
    }
    return 1;
}
