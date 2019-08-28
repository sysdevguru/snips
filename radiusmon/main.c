/* $Header: /home/cvsroot/snips/radiusmon/main.c,v 1.2 2008/04/25 23:31:52 tvroon Exp $
 */ 

/*+ radiusmon
 * 	RADIUS monitor for SNIPS.
 *
 * Method :
 * 	Create a radius packet and send to server.
 *
 * Originally posted by Claranet Ltd., UK rewritten to avoid copyright
 * issues. Some portions of the code from the Livingston public domain
 * software.
 *
 * 	Vikas Aggarwal, vikas@navya.com
 */

/*
 * $Log: main.c,v $
 * Revision 1.2  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.1  2001/08/05 09:13:42  vikas
 * Added support for using NAS_PORT_TYPE. Needed by some radius servers.
 *
 * Revision 1.0  2001/07/08 22:51:22  vikas
 * For SNIPS
 *
 */ 

#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define _MAIN_
#include "snips.h"
#include "snips_funcs.h"
#include "snips_main.h"
#include "radiusmon.h"
#include "netmisc.h"
#include "event_utils.h"
#undef _MAIN_

/* We keep a linked list of all the devices that we poll and store the
 * various thresholds in this linked list.
 */
struct device_info
{
  int port;
  char *secret;
  char *user;
  char *pass;		/* password */
  int  timeout;		/* in secs */
  int  retries;
  int  porttype;	/* normally -1, indicating not sent */
  struct device_info *next;
}  *device_info_list = NULL;	

/* function prototypes */
void set_functions();
int readconfig();
void free_device_list(struct device_info **si_list);


int main(argc, argv)
  int argc;
  char **argv;
{
  set_functions();
  snips_main(argc, argv);
  return(0);
}


/*
 * Read the config file.Returns 1 on success, 0 on error.
 * Do other initializtions.
 * Format :
 *   POLLINTERVAL  <time in seconds>
 *   hostname addr port secret username password [retry] [ntries] [port_type]
 */
int readconfig()
{
  int fdout;
  char *configfile, *datafile;
  char record[BUFSIZ], *sender;
  char w1[MAXLINE], w2[MAXLINE], w3[MAXLINE], w4[MAXLINE], w5[MAXLINE],
    w6[MAXLINE], w7[MAXLINE], w8[MAXLINE], w9[MAXLINE];
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
    int rc;						/* return code	*/

    v.state = 0 ;				/* Init options to zero	*/
    *w1 = *w2 = *w3 = *w4 = *w5 = *w6 = *w7 = *w8 = *w9 = '\0' ;
    rc = sscanf(record, "%s %s %s %s %s %s %s %s %s",
		w1, w2, w3, w4, w5, w6, w7, w8, w9);
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
    
    if (strcasecmp(w1, "RRDTOOL") == 0)
    {
      if (strcasecmp(w2, "ON") == 0)
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

    /* Only <name> <addr> <port> <secret> <username> <password>  remain */
    strncpy(v.device.name, w1, sizeof(v.device.name) - 1);
    strncpy(v.device.addr, w2, sizeof(v.device.addr) - 1);	/* no checks */
    
    if (get_inet_address(NULL, w2) < 0)	/* bad address */
    {
      fprintf(stderr,
	      "	(%s): Error in address '%s' for device '%s', ignoring\n",
	      prognm, w2, w1);
      continue ;
    }

    if (*w3 == '\0' || *w4 == '\0' || *w5 == '\0' || *w6 == '\0')
    {
      fprintf(stderr, "%s: invalid number of fields\n", prognm);
      fprintf(stderr, "\t '%s'\n", record);
      continue;
    }
    if (atoi(w3) <= 0) {
      fprintf(stderr, "%s: invalid port number '%s', skipping\n", prognm, w3);
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

    newnode->port = atoi(w3);
    newnode->secret = Strdup(w4);    
    newnode->user = Strdup(w5);    
    newnode->pass = Strdup(w6);
    newnode->timeout  = (*w7 == '\0') ?  0 : atoi(w7);
    newnode->retries  = (*w8 == '\0') ?  0 : atoi(w8);
    newnode->porttype = (*w9 == '\0') ? -1 : atoi(w9);
    newnode->next = NULL;
	
    if(!device_info_list)
      device_info_list = newnode;	/* This is the first node (head) */
    else
      lastnode->next = newnode;
	  
    lastnode = newnode;

    write_event(fdout, &v);
  }	/* end: while	*/

  fclose(pconfig);
  close_datafile(fdout);

  if (!pollinterval)
    pollinterval = POLLINTERVAL ;		/* default value */

  return (1);
}		/* readconfig() */

/*
 * This function calls radiusmon and gets the status of the radius server.
 */
unsigned long
dotest(hostname, hostaddr)
  char *hostname, *hostaddr;
{
  int value;
  static struct device_info *curnode;	/* current device being processed */

  if (curnode == NULL)
    curnode = device_info_list;		/* rewind to start of list */

  value = radiusmon(hostaddr, curnode->port, curnode->secret,
		      curnode->user, curnode->pass, curnode->timeout,
		      curnode->retries, curnode->porttype);

  curnode = curnode->next;
  return ( (u_long)value );

}	/* dotest() */

int help()
{
  snips_help();
}

void free_device_list(si_list)
  struct device_info **si_list;
{
  struct device_info *curptr, *nextptr;

  for(curptr = *si_list; curptr ; curptr = nextptr)
  {
    nextptr = curptr->next;		/* store next cell before freeing */
    free(curptr);
  }

  *si_list = NULL;
}	/* end free_device_list() */

void set_functions()
{
  int help();
  int readconfig();
  u_long dotest();
 
  set_help_function(help);
  set_readconfig_function(readconfig);
  set_test_function(dotest);

}
