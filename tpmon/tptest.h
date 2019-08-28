/*
 * $Header: /home/cvsroot/snips/tpmon/tptest.h,v 1.0 2001/07/09 04:00:25 vikas Exp $
 */

/*
 * tptest.h -- #include this file to interface with tpmon.c
 *
 * This header file is NOT part of the tpmon/netmon package.  It is
 * simply an interface file that you can #include to use the
 * throughput() routine, should you wish to use it in any of your
 * own programs.
 *
 * See tptest.c and 'make tptest' for an example.
 *
 * === NOTE === tpmon.c expects an extern char *prognm which points to
 * the program's name, so that it can stick this in error messages
 */

#ifndef __TP_H__
#define __TP_H__

/* these variables are exported in case you're interested in them */

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

double throughput(/* char *addr, short int port, int numblocks, int blocksize,
                  char *pattern, int time, int verbose */);

#endif
