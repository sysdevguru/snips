/*
 * $Header: /home/cvsroot/snips/nsmon/main.c,v 1.1 2008/04/25 23:31:51 tvroon Exp $
 */

/*+
 * Nameserver monitor. Sends a dns query to the specified host
 * and checks if the host returns the expected answer (authoritative
 * or non-authoritative).
 *
 */

/*
 * $Log: main.c,v $
 * Revision 1.1  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 22:38:19  vikas
 * For SNIPS
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */

#ifndef lint
 static char rcsid[] = "$RCSfile: main.c,v $ $Revision: 1.1 $ $Date: 2008/04/25 23:31:51 $" ;
#endif

#define _MAIN_				/* for 
global variables */
#include "snips.h"			/*	common structures	*/
#include "snips_funcs.h"
#include "snips_main.h"
#include "nsmon.h"			/* program specific defines	*/
#include "nsmon-local.h"
#include "netmisc.h"			/* get_inet_address */
#include "event_utils.h"
#undef _MAIN_

#include <string.h>			/* For strcat() definitions	*/
#include <stdlib.h>
#include <sys/file.h>
#include <signal.h>			/* For signal numbers		*/

/*
 * For each host being monitored, we keep a linked list of which domain
 * to query for and whether it should be queried for an AUTHORITATIVE
 * answer only. At the start of each polling cycle, we reset to the
 * beginning of the linked list
 */
struct device_info
{
  char *domain;		/* Domain name */
  int aa_wanted;	/* 1 if Authorative Answer required */
  struct device_info *next;
};

struct device_info *device_info_list = NULL;	/* linked list */
static int	maxseverity = E_CRITICAL ; 	/* Max severity of nsmon */

/* function prototypes */
void set_functions();
void free_device_list(struct device_info **si_list);

int main (ac, av)
  int ac;
  char **av;
{
  set_functions();
  snips_main(ac, av);
  return(0);
}
    
/*+ 
 * FUNCTION:
 * 	Brief usage
 */
int help ()
{
  snips_help();
  fprintf(stderr, "This program sends queries to nameservers to verify\n");
  fprintf(stderr, "their operational status.\n\n");
  return (1);
}


/*
 * readconfig()
 */

int readconfig()
{
  int auth_wanted;			/* Set to 1 if want auth answer */
  int fdout;
  char *sender;
  char record[BUFSIZ];
  char *querystr = QUERYDATA;		/* Query data string (domainname) */
  char *configfile, *datafile;		/* Filename of the configfile 	*/
  FILE *pconfig ;
  EVENT v;				/* Defined in SNIPS.H		*/
  struct device_info *lastnode = NULL, *newnode; /* for building linked list */

  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm ;				/* no path in program name */
  else
    sender++ ;				/* skip leading '/' */

  if(device_info_list)			/* if not null, free it up */
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
    fprintf(stderr, "%s error (readconfig) ", prognm) ;
    perror (configfile);
    return (-1);
  }

  init_event(&v);

  /*
   * in the following strncpy's, the NULL is already appended because
   * of the bzero, and copying one less than size of the arrays.
   */
  strncpy (v.sender, sender, sizeof(v.sender) - 1);
  strncpy (v.var.name, VARNM, sizeof (v.var.name) - 1);
  strncpy (v.var.units, VARUNITS, sizeof (v.var.units) - 1);

  while(fgets(record, BUFSIZ, pconfig) != NULL ) 
  {
    char w1[MAXLINE], w2[MAXLINE], w3[MAXLINE];	/* Confg words	*/
    int rc;						/* return code	*/

    v.state = 0 ;				/* Init options to zero	*/
    *w1 = *w2 = *w3 = '\0' ;
    rc = sscanf(record,"%s %s %s", w1, w2, w3);
    if (rc == 0 || *w1 == '\0' || *w1 == '#')  /* Comment or blank 	*/
      continue;

    if (strncmp(w1, "POLLINT", 7) == 0 || strncmp(w1, "pollint", 7) == 0)
    {
      char *p; 					/* for strtol */
      pollinterval = (u_long)strtol(w2, &p, 0) ;
      if (p == w2)
      {
	fprintf(stderr,"(%s): Error in format for POLLINTERVAL '%s'\n",
		prognm, w2) ;
	pollinterval = 0 ;		/* reset to default above */
      }
      continue ;
    }

    if (strncasecmp(w1,"DOMAINNAME",10) == 0)
    {
      querystr = (char *)malloc(strlen(w2) + 2);
      strcpy (querystr, w2);
      /* This will be 'attached' to all devices under this domain */

      if (debug)
	fprintf(stderr, "(debug): Querystr changed to  '%s'\n", 
		querystr);
	    
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

    /* These are <devicename> <addr> pairs. Put the 'domain name' being
     * monitored into the subdevice field.
     */
    strncpy(v.device.name, w1, sizeof(v.device.name) -1);
    strncpy(v.device.addr, w2, sizeof(v.device.addr) - 1);
    strncpy(v.device.subdev, querystr, sizeof(v.device.subdev) - 1);

    if (get_inet_address(NULL, w2) < 0)	/* bad address */
    {
      fprintf(stderr,
	      "(%s): Error in address '%s' for device '%s', ignoring\n",
	      prognm, w2, w1);
      continue ;
    }

    auth_wanted = 0;	/* Default AA not required */
    if (*w3 != '\0')	/* Some other keyword	*/
    {
      if (strcmp(w3, "test") == 0 || strcmp(w3, "TEST") == 0)
	v.state = v.state | n_TEST ;
      else
	/* 
	 * The word 'AUTH' can optionally be specified
	 * at the end of the device line
	 * This means that name query has to be authorative
	 */
	if(strcasecmp(w3, "AUTH") == 0 )
	{
	  auth_wanted = 1; /* Authorative reply wanted */
	  if(debug)
	    fprintf(stderr, "(debug)Authorative Answer for %s\n", w1);
	}
	else
	  fprintf(stderr,"%s: Ignoring unknown keyword- %s\n",prognm,w3);
    }
	
    /*
     * Create a device info node and add to the tail of linked list
     */
    newnode = (struct device_info *)calloc(1, sizeof(struct device_info));
    if(!newnode)
    {
      fprintf(stderr,"%s: Out of Memory ", prognm);
      return -1;
    }

    newnode->domain = querystr;
    newnode->aa_wanted = auth_wanted;	
    newnode->next = NULL;
	
    if(!device_info_list)		  /* This is the first node(head) */
      device_info_list = newnode;
    else
      lastnode->next = newnode;
    lastnode = newnode;

    write_event(fdout, &v);
  }	/* end: while */

  if(debug)
  {
    fprintf(stderr, "(debug)Devices are :\n");
    for(lastnode = device_info_list; lastnode; lastnode = lastnode->next)
      fprintf(stderr, "(debug)Dom: %s  %s\n",
	      lastnode->domain, (lastnode->aa_wanted)? "AA" : "Not AA");
  }
  fclose (pconfig);
  close_datafile(fdout);

  if (!pollinterval)
    pollinterval = POLLINTERVAL ;		/* default value */

  return(1);				/* All OK */

}		/* end:  readconfig()	*/

/*  */

/*
 * Makes one pass over all the hosts.
 */

/* #defines for finish_status */
#define REACHED_EOF 1
#define READ_ERROR 2

int poll_devices()
{
  EVENT v;			    	/* described in snips.h	*/
  int status, fdout, bufsize;
  char *datafile;
  struct device_info *node = NULL;	/* Pointer to the device info LL */

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
  node = device_info_list;	/* Point to the head of Linked List */

  while ( (bufsize = read_event(fdout, &v)) == sizeof(v) )
  {
    switch(nsmon(v.device.addr, node->domain, QUERYTYPE, QUERYTIMEOUT,
		 node->aa_wanted, debug)) 
    {
    case ALL_OK:
      status = 1;			/* '1' for all OK */
      break;

    case NOT_AUTHORITATIVE:
      if (!(node->aa_wanted)) 
	status = 1;		/* Auth answ was not required anyway, so OK */
      else
	status = 0;
      break;

    case NS_ERROR:
    case NO_NSDATA:
    default:
      status = 0;
      break;
    } /* switch */

    if (debug)
    {
      fprintf(stderr, "(debug) %s: Nameserver status is %d/%s\n",
	      prognm, status, status ? "UP" : "DOWN" );
      fflush(stderr);
    }

    update_event(&v, status, /*VALUE*/ (u_long)status, v.var.threshold, 
                 maxseverity);
    rewrite_event (fdout, &v);

    node = node->next;

    if (do_reload)		/* recieved HUP signal */
      break;
  }	/* end of:    while (read..)	*/
    
  /**** Now determine why we broke out of the above loop *****/

  close_datafile(fdout);
  if (bufsize >= 0)			/* reached end of the file */
    return (1);
  else {				/* error in output data file */
    fprintf (stderr, "%s: Insufficient read data in output file", prognm);
    return (-1);
  }

}	/* end poll_devices() */

/*
 * free the linked list. Since two or more nodes might point to the same
 * domain name, take care to free up only once
 */

void free_device_list(si_list)
  struct device_info **si_list;
{
  struct device_info *curptr, *nextptr;
  char *prev_domain = NULL;

  for(curptr = *si_list; curptr ; curptr = nextptr)
  {
    if(prev_domain != curptr->domain)
    {
      /* 
       * Need to free the domain names also.
       * This can be tricky, as number of nodes have 'domain' pointing to
       * the same place.Hence, should be freed only once.
       */
      free(curptr->domain);
      prev_domain = curptr->domain;	/* store location of domain name */
    }
    nextptr = curptr->next;		/* store next cell before freeing */
    free(curptr);
  }
  *si_list = NULL;
}	/* end free_device_list() */

/*
 * set default functions for calling snips_main().
 */
void set_functions()
{
  int help(), readconfig(), poll_devices();

  set_help_function(help);
  set_readconfig_function(readconfig);
  set_polldevices_function(poll_devices);
}
