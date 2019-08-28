/*
* $Header: /home/cvsroot/snips/include/pingmon.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $
*/

#ifndef __pingmon_h
# define __pingmon_h

/*
 * The mode is automatically detected from the 'prognm' and the
 * value of 'ping'
 */
#define IPPING_MODE		1
#define IPMULTIPING_MODE	2
#define RPCPING_MODE		3
#define OSIPING_MODE		4

/* Set the values of the PING path's. */
#ifndef IPPING
# define IPPING		"/usr/local/snips/bin/multiping"  /* or /sbin/ping */
#endif
#ifndef OSIPING
# define OSIPING	"osiping"		/* /usr/sunlink/bin/osiping */
#endif
#ifndef RPCPING
# define RPCPING	"/usr/local/snips/bin/rpcping"
#endif


/* number of devices to ping at a time in multiping */
#define IPMULTIPING_BATCHSIZE	64

/*
 * Default values for IP and OSI pings
 */
#define	DATALEN		100	/* Size of ping packets	(bytes) */
#define	NPACKETS	5	/* Number of ping packets sent */
#define PING_THRES      3	/* Down if less than this recieved */
#define RTT_THRES	1000	/* 1 second */
#define POLLINTERVAL	180	/* secs between starting next poll */
#define RPCTIMEOUT	5	/* timeout for rpcping in secs */

#define IPVARNM		"ICMP-ping"	/* for EVENT.var.name field	*/
#define IPVARUNITS	"Pkts"		/* Units name */
#define RTTVARNM	"ICMP-RTT"	/* for EVENT.var.name field	*/
#define RTTVARUNITS	"msecs"		/* Units name */
#define RPCVARNM	"Portmapper"
#define RPCVARUNITS	"Status"
#define OSIVARNM	"OSI-ping"
#define OSIVARUNITS	"Pkts"

/*
 * Global variables
 */
#ifdef _MAIN_
# define EXTERN
#else
# define EXTERN extern
#endif
EXTERN int  mode;			/* IPMODE or RPCMODE */
EXTERN int  npackets;
EXTERN int  pktsize;
EXTERN int  batchsize;
EXTERN char *ping;
#undef EXTERN

float *pingmon (char **devices);

#endif	/* ! __pingmon_h */
