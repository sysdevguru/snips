/*
 * $Header: /home/cvsroot/snips/ntpmon/main.c,v 1.1 2008/04/25 23:31:51 tvroon Exp $
 */ 

/*+ ntpmon
 * 	Network Time Protocol monitoring.
 *
 * This is a part of the snips package that monitors NTP strata of
 * remote hosts.
 *
 * Method :
 * 	Create a standard ntp control package with only the header(no data)
 *	Send it over a UDP socket.Then wait for a reply.
 *	The stratum value is contained in the data field of the reply.
 *	Stratum ranges from 0 to 16. Based on this calculate the error level.
 *
 * AUTHOR:
 *	Vikas Aggarwal, vikas@navya.com
 */	

/*
 * $Log: main.c,v $
 * Revision 1.1  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 22:44:25  vikas
 * For SNIPS
 *
 */ 

/* Copyright 2001, Netplex Technologies Inc., info@netplex-tech.com */

#include <stdio.h>
#include <fcntl.h>

#define _MAIN_
#include "snips.h"
#include "snips_funcs.h"
#include "snips_main.h"
#include "netmisc.h"
#include "ntpmon.h"
#include "event_utils.h"
#undef _MAIN_

/* We keep a linked list of all the devices that we poll and store the
 * various thresholds in this linked list.
 */
static struct device_info
{
  u_long wlevel, elevel, clevel;
  struct device_info *next;
} *device_info_list = NULL;	

/* function prototypes */
void set_functions();
void free_device_list(struct device_info **pslist);

int main(ac, av)
  int ac;
  char **av;
{
  set_functions();
  snips_main(ac, av);
  return(0);
}	/* end main() */

/*
 * Read the config file.Returns 1 on success, 0 on error.
 * Do other initializtions.
 * Format :
 * 	POLLINTERVAL  <time in seconds>
 * 	<hostname> <address> <Warn level> <Error Level> <Critical level>
 */
int readconfig()
{
  int fdout;
  char *configfile, *datafile;
  char record[BUFSIZ], *sender;
  char w1[MAXLINE], w2[MAXLINE], w3[MAXLINE], w4[MAXLINE], w5[MAXLINE];	
  FILE *pconfig ;
  EVENT v;				/* Defined in SNIPS.H		*/
  struct device_info *lastnode = NULL, *newnode;

  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm ;				/* no path in program name */
  else
    sender++ ;					/* skip leading '/' */

  if(device_info_list)
    free_device_list(&device_info_list);	/* In case rereading confg file */

  configfile = get_configfile();    datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR|O_CREAT|O_TRUNC)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile %s", prognm, datafile);
    perror (datafile);
    return (-1);
  }

  if ((pconfig = fopen(configfile, "r")) == NULL)
  {
    fprintf(stderr, "%s error readconfig() ", prognm) ;
    perror (configfile);
    return (-1);
  }
  
  init_event(&v);
  strncpy (v.sender, sender, sizeof(v.sender) - 1);
  strncpy (v.var.name, VARNM, sizeof (v.var.name) - 1);
  strncpy (v.var.units, VARUNITS, sizeof (v.var.units) - 1);

  while(fgets(record, BUFSIZ, pconfig) != NULL ) 
  {
    int rc;						/* return code	*/

    v.state = 0 ;				/* Init options to zero	*/
    *w1 = *w2 = *w3 = *w4 = *w5 = '\0' ;
    rc = sscanf(record,"%s %s %s %s %s", w1, w2, w3, w4, w5);
    if (rc == 0 || *w1 == '\0' || *w1 == '#')  /* Comment or blank 	*/
      continue;

    /* Looking for "POLLINTERVAL xxx" 	*/
    if (strncasecmp(w1, "POLLINTERVAL", 8) == 0)
    {
      char *p; 					/* for strtol */
      pollinterval = (u_long)strtol(w2, &p, 0) ;
      if (p == w2)
      {
	fprintf(stderr,"(%s): Error in format for POLLINTERVAL '%s'\n",
		prognm, w2) ;
	pollinterval = POLLINTERVAL ;		/* reset to default above */
      }
      
      continue;
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

    /* Only <name> <addr> <Wlevel> <Elevel> <Clevel>  remain */
    strncpy(v.device.name, w1, sizeof(v.device.name) - 1);
    strncpy(v.device.addr, w2, sizeof(v.device.addr) - 1);	/* no checks */
    
    if (get_inet_address(NULL, w2) < 0)	/* bad address */
    {
      fprintf(stderr,
	      "	(%s): Error in address '%s' for device '%s', ignoring\n",
	      prognm, w2, w1);
      continue ;
    }

    if (*w3 == '\0' || *w4 == '\0' || *w5 == '\0' )
    {
      fprintf(stderr, "%s: Warning levels not specified correctly\n", prognm);
      fprintf(stderr, "\t '%s'\n", record);
      continue;
    }

    /*
     * Create a device info node and add to the tail of linked list
     */
    newnode = (struct device_info *)calloc(1, sizeof(struct device_info));
    if(!newnode)
    {
      fprintf(stderr,"%s: Out of Memory ",prognm);
      return -1;
    }

    newnode->wlevel = strtoul(w3, NULL, 10);    
    newnode->elevel = strtoul(w4, NULL, 10);    
    newnode->clevel = strtoul(w5, NULL, 10);
    newnode->next = NULL;
	
    if(!device_info_list)
      device_info_list = newnode;		/* This is the first node (head) */
    else
      lastnode->next = newnode;
	  
    lastnode = newnode;

    write_event(fdout, &v);
  }	/* end: while			*/
  fclose(pconfig);
  close_datafile(fdout);

  if (!pollinterval)
    pollinterval = POLLINTERVAL ;		/* default value */

  return (1);
}		/* readconfig() */

/*
 * This func. calls ntpmon, gets the stratum  for
 * every device and writes the output
 */

int poll_devices()
{
  EVENT v;			    	/* described in snips.h		*/
  int status, fdout, bufsize;
  char *datafile;
  struct device_info *curnode;		/* current device being processed */

  datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile %s", prognm, datafile);
    perror (datafile);
    return (-1);
  }
  /* 
   * until end of the file or erroneous data... one entire pass
   */

  curnode = device_info_list;	/* Point to the head of Linked List */

  while ( (bufsize = read_event(fdout, &v)) == sizeof(v) ) 
  {
   int maxseverity, stratum;
   u_long thres;

   stratum = ntpmon(v.device.addr);
   status = calc_status(stratum, curnode->wlevel, curnode->elevel,
			curnode->clevel, 1, &thres, &maxseverity) ;

   if (debug)
   {
     fprintf(stderr, "(debug) %s: NTP status is %s, stratum=%d\n",
	     prognm, status ? "UP" : "DOWN", stratum);
     fflush(stderr);
   }

   update_event(&v, status, (u_long)stratum, thres, maxseverity);
   rewrite_event(fdout, &v);

   curnode = curnode->next;

   if (do_reload)		/* recvd SIGHUP */
     break;
     
  }	/* end of:    while (read..)	*/
    
  /**** Now determine why we broke out of the above loop *****/
  close_datafile(fdout);
  if (bufsize >= 0)			/* reached end of file or break */
    return (1);
  else {				/* error in output data file */
    fprintf (stderr, "%s: Insufficient read data in output file", prognm);
    return (-1);
  }

}	/* end of:  poll_devices		*/

int help()
{
  snips_help();
  fprintf(stderr, "\
  This program checks the NTP stratum of the hosts being monitored and\n\
  escalates the severity of the device if the NTP stratum drops (gets\n\
  worse) below the configured levels.\n\n");
}

void free_device_list(pslist)
  struct device_info **pslist;
{
  struct device_info *curptr, *nextptr;

  for(curptr = *pslist; curptr ; curptr = nextptr)
  {
    nextptr = curptr->next;		/* store next cell before freeing */
    free(curptr);
  }

  *pslist = NULL;
}	/* end free_device_list() */

/*
 * Override the library functions since we are using snips_main()
 */
void set_functions()
{
  int help();
  int readconfig();
  int poll_devices();
 
  set_help_function(help);
  set_readconfig_function(readconfig);
  set_polldevices_function(poll_devices);

}
