/*
 * $Header: /home/cvsroot/snips/tpmon/main.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * DESCRIPTION: throughput monitor
 *
 *   This program connects to the 'discard' port on a remote host and
 *   sends random data to the remote host and calculates the throughput
 *   (in bits per second).
 *
 *   For interfacing with 'snips', it opens the data file supplied by 
 *   the user and then creates a 'tpmon-output' file for use with netmon.
 *   It then directly reads and writes from this file.  If it gets SIGHUP,
 *   it rescans the data file before continuing.
 *
 *
 */

/* Copyright Netplex Technologies Inc. */

/*
 * $Log: main.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 04:00:25  vikas
 * thruput monitor for SNIPS v1.0
 *
 */

#ifndef lint
 static char rcsid[] = "$RCSfile: main.c,v $ $Revision: 1.1 $ $Date: 2008/04/25 23:31:52 $" ;
#endif

#include <string.h>			/* For strcat() definitions	*/
#include <stdlib.h>			/* For strtol() definitions     */
#include <sys/file.h>
#include <signal.h>			/* For signal numbers		*/
#include <arpa/nameser.h>

#define _MAIN_
#include "snips.h"			/*	common structures	*/
#include "snips_funcs.h"
#include "snips_main.h"
#include "tpmon.h"			/* program specific defines	*/
#include "netmisc.h"
#include "event_utils.h"
#undef _MAIN_
/*
 * We dont want the severity to escalate to CRITICAL since thruput might
 * not be a critical event.
 */
static int maxseverity = E_WARNING ; /* Max severity of events from tpmon */

/* function prototypes */
void set_functions();

int main (ac, av)
     int ac;
     char **av;
{
  set_functions();
  snips_main(ac, av);
  return(0);
}
    
/*+ 
** FUNCTION:
** 	Brief usage
**/
int help ()
{
  snips_help();
  fprintf(stderr, "\tThis program tests the throughput to devices by connecting\n");
  fprintf(stderr, "\tto the discard port (TCP port 9) and writing data into\n");
  fprintf(stderr, "\tit as fast as it can and measuring the transfer rate.\n");
  return (1);
}

/*
 *    All devices are set to UNINIT status.
 */

int readconfig()
{
  int fdout ;
  char record[BUFSIZ], *sender;
  char *configfile, *datafile;
  FILE *pconfig ;
  EVENT v;				/* Defined in SNIPS.H		*/

  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm ;				/* no path in program name */
  else
    sender++ ;				/* skip leading '/' */

  configfile = get_configfile();    datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR|O_CREAT|O_TRUNC)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile %s", prognm, datafile);
    perror (datafile);
    return (-1);
  }
  if ((pconfig = fopen(configfile, "r")) == NULL)
  {
    fprintf(stderr, "%s error (init_devices) ", prognm) ;
    perror (configfile);
    return (-1);
  }

  init_event(&v);
  strncpy (v.sender, sender, sizeof(v.sender) - 1);
  strncpy (v.var.name, VARNM, sizeof (v.var.name) - 1);
  strncpy (v.var.units, VARUNITS, sizeof (v.var.units) - 1);

  while(fgets(record, BUFSIZ, pconfig) != NULL ) 
  {
    char *p;		/* for strtol */
    u_long thresh;
    char w1[MAXLINE], w2[MAXLINE], w3[MAXLINE], w4[MAXLINE];
    /* Confg words	*/
    int rc;						/* return code	*/

    v.state = 0 ;				/* Init options to zero	*/
    *w1 = *w2 = *w3 = *w4 = '\0' ;
    /* line should look like "<name>  <IP#>  <threshold>  [TEST]" */
    rc = sscanf(record,"%s %s %s %s", w1, w2, w3, w4);
    if (rc == 0 || *w1 == '\0' || *w1 == '#')  /* Comment or blank 	*/
      continue;

    if (strncmp(w1, "POLLINT", 7) == 0 || strncmp(w1, "pollint", 7) == 0)
    {
      pollinterval = (u_long)strtol(w2, &p, 0) ;
      if (p == w2)
      {
	fprintf(stderr,"(%s): Error in format for POLLINTERVAL '%s'\n",
		prognm, w2) ;
	pollinterval = 0 ;		/* reset to default above */
      }
      continue ;
    }

    if (strcasecmp(w1, "rrdtool") == 0)
    {
      if (strcasecmp(w2, "on") == 0)
      {
#ifdef RRDTOOL
	++dorrd;
	if (debug > 1)
	  fprintf(stderr, "readconfig(): RRD enabled\n");
#else
	fprintf(stderr, "%s: RRDtool not supported\n", prognm);
#endif
      }
      continue;
    }

    strncpy(v.device.name, w1, sizeof(v.device.name) - 1);
    strncpy(v.device.addr, w2, sizeof(v.device.addr) - 1);	/* no checks */

    if (get_inet_address(NULL, w2) < 0)	/* bad address */
    {
      fprintf(stderr,
	      "(%s): Error in address '%s' for device '%s', ignoring\n",
	      prognm, w2, w1);
      continue ;
    }

    thresh = (u_long)strtol(w3, &p, 0);
    if (p == w3) { 			/* strtod() couldn't convert */
      fprintf(stderr,
	      "(%s): Error in format of threshold '%s' for device '%s'\n", 
	      prognm, w3, w1);
      fprintf(stderr,
	      "\t(should be a long integer representing bits per second)\n");
    } else
      v.var.threshold = thresh;
          	
    if (*w4 != '\0')			/* Some other keyword	*/
    {
      if (strcmp(w4, "test") == 0 || strcmp(w3, "TEST") == 0)
	v.state = v.state | n_TEST ;
      else
	fprintf(stderr, "%s: Ignoring unknown keyword- %s\n", prognm,w3);
    }

    write_event(fdout, &v);
  }					/* end: while			*/
  fclose (pconfig);			/* Not needed any more		*/    
  close_datafile(fdout);

  if (!pollinterval)
    pollinterval = POLLINTERVAL ;		/* default value */
  return(1);				/* All OK			*/
}		/* end:  init_devices()		*/

/*
 * Using generic_poll_devices() which calls dotest()
 */

u_long
dotest(hostname, hostaddr)
  char *hostname, *hostaddr;
{
  long tp;

  tp = (long)throughput(hostaddr, PORT_NUMBER, NUM_BYTES, BLOCKSIZE,
			  PATTERN, RUN_TIME, debug);

  if (tp < 0)
    tp = 0;
  return (tp /1000L);		/* convert bits to Kbits */
}

void set_functions()
{
  int help(), readconfig(), poll_devices();
  u_long dotest();

  set_help_function(help);
  set_readconfig_function(readconfig);
  set_test_function(dotest);
}
