/*
 * $Header: /home/cvsroot/snips/include/tpmon.h,v 1.0 2001/07/12 02:47:59 vikas Exp $
 */
#ifndef __tpmon_h
# define __tpmon_h

/*
 * The progam automatically fills in its own name in the EVENT.sender
 * field. If you want to over-ride this name with your own, then define
 * SENDER
 */
/* #define SENDER	"thruputmon"	/* About 16 characters possible */
/* #define CONFIGFILE	"../../etc/tpmon-confg"	/* in ETCDIR */

#define VARNM		"Thruput"	/* for EVENT.var.name field	*/
#define VARUNITS	"Kbps"		/* Units name */
#define POLLINTERVAL	(time_t)(2 * 3600)	/* interval between queries */

#define PORT_NUMBER	9		/* tcp discard port */
#define NUM_BYTES	0L		/* default num_blocks */
#ifdef BUFSIZ	/* in stdio.h */
#  define BLOCKSIZE 	BUFSIZ		/* default blocksize */
#endif /* BUFSIZ */
#define PATTERN		NULL		/* default pattern == none == random */
#define RUN_TIME	30		/* default secs of test run */
  /* see below for the effects of having both time and blocksize non-zero */

/*
 * throughput -- finds throughput of a remote device
 *
 * parameters:
 *   addr is the address of the device to be tested.  It can be either
 *     a name (e.g. phoenix.princeton.edu) or an IP # (e.g. 128.112.128.43)
 *   port is the port number, and defaults to 9 (discard port)
 *   numblocks is the number of blocks to send
 *   blocksize is the blocksize to be used
 *   pattern is a pattern of bytes which will be used to fill the data block
 *     (set it to NULL for random characters)
 *   time is the duration of the test in seconds
 *   verbose enables verbose output mode
 *
 *   if both numblocks and time are nonzero, then the test will last until
 *   either numblocks blocks have been sent, or time seconds have elapsed,
 *   whichever comes first
 *
 * return values:
 *   a double representing the throughput in bits per second
 *   -1.0 if an error occured
 */

double throughput(/* char *addr, short int port, long numbytes, int blocksize,
                  char *pattern, int time, int verbose */);

#endif /* __tpmon_h */
