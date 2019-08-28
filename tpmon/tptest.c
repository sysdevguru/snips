/*
 * $Header: /home/cvsroot/snips/tpmon/tptest.c,v 1.0 2001/07/09 04:00:25 vikas Exp $
 */

/*
 * Thruput tester - standalone version.
 * tptest.c -- testing stub program for tpmon.c
 *
 * AUTHOR
 *	S. Spencer Sun, spencer@phoenix.princeton.edu, June 1992 for SNIPS
 *
 *
 * $Log: tptest.c,v $
 * Revision 1.0  2001/07/09 04:00:25  vikas
 * thruput monitor for SNIPS v1.0
 *
 */

#include <stdio.h>

#include "tptest.h"

char *prognm;

void
usage()
{
  fprintf(stderr, "Usage: %s [ -b numbytes ] [ -s blocksize ] [ -t secs ]\n",
    prognm);
  fprintf(stderr,
    "\t[ -p fillpattern ] [ -P port# ] [ -v ] host [ host ... ]\n\n");
  fprintf(stderr,
    "\tIf both -b and -t are used, then the -b part is ignored.\n");
}

int
main(argc, argv)
  int argc;
  char *argv[];
{
  extern char *optarg;
  extern int optind;

  int ch, blocksize, time, verbose;
  long numbytes;
  short int port;
  char *pattern;
  double tp;

  prognm=argv[0];
  numbytes = 0x00400000;
  blocksize = 1024;
  time = 0;
  port = 9;
  pattern = NULL;
  verbose = 0;

  while ((ch = getopt(argc, argv, "b:P:p:s:t:v")) != EOF) {
    switch(ch) {
      case 'b':
        numbytes = atol(optarg);
        if (numbytes < 0) {
          fprintf(stderr,
            "%s: error in numbytes parameter to -b flag, must be >= 0\n",
            prognm);
          return 1;
        }
        break;
      case 'p':
        pattern = optarg;
        break;
      case 'P':
        port = (short int)atoi(optarg);
        break;
      case 's':
        blocksize = atoi(optarg);
        if (blocksize < 1) {
          fprintf(stderr, "%s: blocksize parameter to -s must be non-zero\n");
          return 1;
        }
      case 't':
        time = atoi(optarg);
        if (time < 1) {
          fprintf(stderr, "%s: time must be at least 1 second\n", prognm);
          return 1;
        }
        break;
      case 'v':
        verbose = 1;
        break;
      default:
        usage();
        return 1;
        break;
    }
  }
  argc -= optind;
  if (argc < 1) {
    fprintf(stderr, "%s: no hosts were specified\n", prognm);
    usage();
    return 1;
  }
  argv += optind;
  do {
    tp = throughput(*argv, port, numbytes, blocksize, pattern, time, verbose);
    if (tp >= 0)
      printf("Throughput for %s is %7.5g bps\n", *argv, tp);
    ++argv;
  } while (--argc);
  return 0;
}
