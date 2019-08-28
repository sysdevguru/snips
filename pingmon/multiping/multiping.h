/* $Header: /home/cvsroot/snips/pingmon/multiping/multiping.h,v 2.1 2001/07/14 02:41:28 vikas Exp $ */
/*
 * multiping.h -- header file for multiping.c
 */


#ifndef __MULTIPING_H
# define __MULTIPING_H

#define	DEFDATALEN	(64 - 8)	/* default data length, min 56 */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(65536 - 60 - 8)/* max packet size */
#define	MAXWAIT		10		/* max seconds to wait for response */
#define	NROUTES		9		/* number of record route slots */

/*
 * Macros for bitwise operations, for use in maintaining and
 * checking the duplicate table
 */
#define	A(x, bit)	(dest[x]->rcvd_tbl[(bit)>>3]) /* idtfy byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(x, bit)	(A((x), (bit)) |= B(bit))
#define	CLR(x, bit)	(A((x), (bit)) &= (~B(bit)))
#define	TST(x, bit)	(A((x), (bit)) & B(bit))


/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 *
 * MAXREMOTE is the max # of systems you can ping simultaneously.  This
 * number can actually be arbitrarily large but your system performance
 * will begin to suffer.  You will probably never ping anywhere close to
 * 128 sites simultaneously anyway.
 *
 * INTERPKTGAP is the number of milliseconds between a ping for each
 * site. This prevents packets from killing the network by sending a
 * ping to N sites at the same time. Note that the time between a ping
 * to the same site is still 'interval'-secs. (typically 1 sec).
 */
#define	MAX_DUP_CHK	8192
#define MAXREMOTE	1024
#define INTERPKTGAP	50	/* milliseconds */

/*
 * Define a structure to keep track internally of the various remote sites
 */
#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN  128
#endif
typedef struct destrec {     /* in earlier revisions, destrec.sockad was a */
  struct sockaddr_in sockad; /* struct sockaddr, in case you have problems */
  struct sockaddr_in6 sockad6;
  char *rcvd_tbl;	/* bits to indicate which seq no. have been rcvd */
  long nreceived, 	/* # packets we got back */
       nrepeats, 	/* # packets duplicated */
       ntransmitted;	/* # packets xmitted */

   double       tmin,  		/* minimum round-trip time */
       tmax;		/* max RTT */
  double tsum;		/* sum of all RTT's */
  double tsum2;
  char hostname[MAXHOSTNAMELEN];
  u_int rtableid;
  int   sock; /* individual socket added - SNIPS-3. sites with rtableid will have separate sockets.*/
  char isIPV6;
} destrec;


/*
 * Flags for the various command-line options
 */

#define	F_FLOOD			0x0001
#define	F_INTERVAL		0x0002
#define	F_NUMERIC		0x0004
#define	F_PINGFILLED		0x0008
#define	F_QUIET			0x0010
#define	F_RROUTE		0x0020
#define	F_SO_DEBUG		0x0040
#define	F_SO_DONTROUTE		0x0080
#define	F_VERBOSE		0x0100
#define F_TABULAR_OUTPUT	0x0200

/* Operating System specific definitions */
#if defined(SYSV) ||  defined(SVR4) || defined(__svr4__)
# define bzero(b,n)	memset(b,0,n)
# define bcmp(a,b,n)	memcmp(a,b,n)
# define bcopy(a,b,n)	memcpy(b,a,n)
#endif

#ifndef MAX_IPOPTLEN	/* on Linux */
# define MAX_IPOPTLEN  4096
#endif

#ifndef ICMP_MINLEN	/* on Linux */
# define ICMP_MINLEN
#endif

#endif /* __MULTIPING_H */
