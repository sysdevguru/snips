/* $Header: /home/cvsroot/snips/pingmon/main.c,v 1.1 2008/04/25 23:31:51 tvroon Exp $ */ 

/*+ pingmon
 * Monitor devices using ICMP ping or RPCping.
 *
 * Determine's mode (RPC or ICMP) using the program name (ippingmon or
 * rpcpingmon). Determines if MULTIPING icmp mode if PING is set to
 * 'multiping'.
 *
 *
 * AUTHOR:
 *	Vikas Aggarwal, vikas@navya.com
 *
 */	

/*
 * $Log: main.c,v $
 * Revision 1.1  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 22:56:07  vikas
 * For SNIPS
 *
 *
 * Revision 1.2 2015/02/16  Munir Ahmed
 * changes added for SNIPS-13
 */

/* Copyright 2001 Netplex Technologies Inc., info@netplex-tech.com */

#ifndef lint
 static char rcsid[] = "$RCSfile: main.c,v $ $Revision: 1.1 $ $Date: 2008/04/25 23:31:51 $" ;
#endif

#define _MAIN_
#include "snips.h"
#include "snips_funcs.h"
#include "snips_main.h"
#include "pingmon.h"
#include "netmisc.h"
#include "event_utils.h"
#undef _MAIN_

#include <stdlib.h>
#include <string.h>
#define MAX_EXTENSION_STR_LEN 8

char fileExtension[MAX_EXTENSION_STR_LEN];
/* We keep a linked list of all the devices that we poll and store the
 * various thresholds in this linked list.
 */
static struct device_info {
  u_long  pkt_wlevel, pkt_elevel, pkt_clevel;
  u_long  rtt_wlevel, rtt_elevel, rtt_clevel;
  u_int   rtableid;
  struct device_info  *next;
} *device_info_list;

extern char *strcat();

/* function prototypes */
void set_functions();
void free_device_list(struct device_info **pslist);
int help();

int main(ac, av)
  int ac;
  char **av;
{
  pktsize = DATALEN;			/* set default values */
  npackets = NPACKETS;
  batchsize = 1;

  set_functions();
  snips_main(ac, av);
  return(0);

}	/* end main() */


char *get_bare_prognm() {
  char *sender = prognm;

  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm;                           /* no path in program name */
  else
    sender++;                                  /* skip leading '/' */

  return sender;
}



/*
   this function returns
   1 if config file ends with an even number,
   0 if config file ends with an odd number,
   2 if no number found in config filename.
 
 */

int hasEvenNumber(const char *configFileName)
{
    char *character = configFileName;
    char number_str[33];
    int index = 0;
    
    if (!configFileName) return 2;
    
    memset(number_str, 0x00, 33);
    
    while (*character)
    {
        if (isdigit(*character))
            number_str[index++] = *character;
        character++;
    }
    
    if (strlen(number_str))
    {
        int number = atoi(number_str);
        if (number % 2 == 0 ) return 1;
        else return 0;
    }
    
    return 2;
}


/*
 *  this function returns 1 if config file or program name ends with an even number (or no number was found in config filename)
 *  otherwise returns 0.
 */

int isJsonOrBackup()
{
    //check the process name.
    int retVal = hasEvenNumber(get_bare_prognm());
    if (retVal < 2)
        return retVal;
    
    //check the config file
    retVal = hasEvenNumber(get_configfile());
    if (retVal < 2)
        return retVal;
    
    //if no number was found, then only create json file.
    //should we quit instead?
    return 1;
}




/*
 * Read the config file.Returns 1 on success, 0 on error.
 * Do other initializtions.
 */
int readconfig()
{
  int fdout;
  int pkt_defwthres, pkt_defethres, pkt_defcthres; /* default thresholds */
  int rtt_defwthres, rtt_defethres, rtt_defcthres; /* default thresholds */
  int hostcnt = 0, lineno = 0;
  char *configfile, *datafile;
  char record[BUFSIZ], *sender;
  char *varnm, *varunits;
  char *words[10];		/* 10 temporary words */
  FILE *pconfig ;
  EVENT v;				/* Defined in SNIPS.H		*/
  struct device_info *lastnode = NULL, *newnode;

  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm ;				/* no path in program name */
  else
    sender++ ;					/* skip leading '/' */

  if(device_info_list)
    free_device_list(&device_info_list);	/* In case rereading confg file */

  configfile = get_configfile();
  datafile = get_datafile();
    
  if (isJsonOrBackup(configfile))
  {
      strcpy(fileExtension, "json");
      fileExtension[4] = 0;
  }
  else
  {
      strcpy(fileExtension, "backup");
      fileExtension[6] = 0;

  }
    
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
  
  rtt_defwthres = rtt_defethres = rtt_defcthres = RTT_THRES;

  /*
   * Set mode based on the program name. Detect if multiping by value
   * of 'ping'. Note that ping can also be read from the config file
   * so we test for multiping below.
   */
  if (strstr(sender, "rpcping")) {
    mode = RPCPING_MODE;
    ping = RPCPING;
    varnm = RPCVARNM;
    varunits = RPCVARUNITS;
  }
  else if (strstr(sender, "osiping")) {
    mode = OSIPING_MODE;
    ping = OSIPING;
    varnm = OSIVARNM;
    varunits = OSIVARUNITS;
  }
  else {
    mode = IPPING_MODE;
    ping = IPPING;
    varnm = IPVARNM;
    varunits = IPVARUNITS;
  }

  if (mode == RPCPING_MODE)
    pkt_defwthres = pkt_defethres = pkt_defcthres = 0;	/* always zero */
  else
    pkt_defwthres = pkt_defethres = pkt_defcthres = PING_THRES;/* pkts RECVD */
  /*
   * Fill in the static data stuff
   */
  init_event(&v);
  /*
   * in the following strncpy's, the NULL is already appended because
   * of the bzero, and copying one less than size of the arrays.
   */
  strncpy (v.sender, sender, sizeof(v.sender) - 1);
  strncpy (v.var.name, varnm, sizeof (v.var.name) - 1);
  strncpy (v.var.units, varunits, sizeof (v.var.units) - 1);

  while(fgets(record, BUFSIZ, pconfig) != NULL) 
  {
    register int i;
    int pkt_wthres, pkt_ethres, pkt_cthres;	/* thres per device */
    int rtt_wthres, rtt_ethres, rtt_cthres;	/* thres per device */
    u_int rtableid;                             /* rtableid per device */
    char *line;

    ++lineno;
    line = record;
    while (*line == ' ' || *line == '\t')
      ++line;
    if (*line == '#')
      continue;

    v.rating = 0 ;				/* Init options to zero	*/
    bzero(words, sizeof words);
    for (i = 0; i < 10; ++i) {
      words[i] = (i == 0) ? (char *)strtok(line, " \t\n\r\0") : (char *)strtok(NULL, " \t\n\r\0");
    if (words[i] == NULL)
      break;
    }
    if (words[0] == NULL || *(words[0]) == '\0' || *(words[0]) == '#')
      continue;		/* comment or blank */

    if (words[1] == NULL)
    {
      fprintf(stderr, "(%s) [%d]: Invalid config line (no argument to %s\n",
	      prognm, lineno, words[0]);
      continue;
    }

    /* Looking for "POLLINTERVAL xxx" 	*/
    if (!strncasecmp(words[0], "POLLINTERVAL", 8))
    {
      char *p; 					/* for strtol */
      pollinterval = (u_long)strtoul(words[1], &p, 0) ;
      if (p == words[1])
      {
	fprintf(stderr,"(%s) [%d]: Error in format for POLLINTERVAL '%s'\n",
		prognm, lineno, words[1]) ;
	pollinterval = POLLINTERVAL ;		/* reset to default above */
      }
      continue;
    }

    if (!strcasecmp(words[0], "RRDTOOL"))
    {
      if (words[1] && !strcasecmp(words[1], "on"))
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

    if (!strcasecmp(words[0], "PINGPATH"))
    {
      if (words[1] && *(words[1]))
	ping = (char *)Strdup(words[1]);
      else
	fprintf(stderr, "readconfig() [%d]: Missing PINGPATH\n", lineno);
      continue;
    }

    if (!strcasecmp(words[0], "PACKETSIZE"))
    {
      if ( words[1] && (pktsize = atoi(words[1])) <= 0)
	pktsize = DATALEN;
      continue;
    }

    if (!strcasecmp(words[0], "NPACKETS"))
    {
      if ( words[1] && (npackets = atoi(words[1])) <= 0)
        npackets = NPACKETS;
      continue;
    }

    if (!strcasecmp(words[0], "THRESHOLDS") || !strcasecmp(words[0], "PKT_THRESHOLDS"))
    {
      if ( words[1] == NULL || (pkt_defwthres = atoi(words[1])) <= 0 )
	pkt_defwthres = PING_THRES;
      if ( words[2] == NULL || (pkt_defethres = atoi(words[2])) <= 0 )
	pkt_defethres = PING_THRES;
      if ( words[3] == NULL || (pkt_defcthres = atoi(words[3])) <= 0 )
	pkt_defcthres = PING_THRES;

      continue;
    }

    if (!strcasecmp(words[0], "RTT_THRESHOLDS") || !strcasecmp(words[0], "RTTTHRESHOLDS"))
    {
      if ( words[1] == NULL || (rtt_defwthres = atoi(words[1])) <= 0 )
	rtt_defwthres = PING_THRES;
      if ( words[2] == NULL || (rtt_defethres = atoi(words[2])) <= 0 )
	rtt_defethres = PING_THRES;
      if ( words[3] == NULL || (rtt_defcthres = atoi(words[3])) <= 0 )
	rtt_defcthres = PING_THRES;

      continue;
    }

    /* Only <name> <addr> [<Wlevel> <Elevel> <Critlevel>]  remain */
    strncpy(v.device.name, words[0], sizeof(v.device.name) - 1);
    strncpy(v.device.addr, words[1], sizeof(v.device.addr) - 1);	/* no checks */
    
    if (get_inet_address(NULL, words[1]) < 0)	/* bad address */
    {
      fprintf(stderr,
	      "	(%s) [%d]: Error in address '%s' for device '%s', ignoring\n",
	      prognm, lineno, words[1], words[0]);
      continue ;
    }

    if ( words[2] == NULL || (pkt_wthres = atoi(words[2])) <= 0 )
      pkt_wthres = pkt_defwthres;
    if ( words[3] == NULL || (pkt_ethres = atoi(words[3])) <= 0 )
      pkt_ethres = pkt_defethres;
    if ( words[4] == NULL || (pkt_cthres = atoi(words[4])) <= 0 )
      pkt_cthres = pkt_defcthres;
    if ( words[5] == NULL || (rtt_wthres = atoi(words[5])) <= 0 )
      rtt_wthres = rtt_defwthres;
    if ( words[6] == NULL || (rtt_ethres = atoi(words[6])) <= 0 )
      rtt_ethres = rtt_defethres;
    if ( words[7] == NULL || (rtt_cthres = atoi(words[7])) <= 0 )
      rtt_cthres = rtt_defcthres;

    if ( words[8] == NULL || (rtableid = atoi(words[8])) <= 0 )
      rtableid = 0;

    v.device.rtableid = rtableid;

    if (debug > 1)
    {
      fprintf(stderr, "debug: Pkt loss thresholds for %s = %d %d %d\n",
	      words[1], pkt_wthres, pkt_ethres, pkt_cthres);
      if (mode != RPCPING_MODE)
	fprintf(stderr, "debug: RTT thresholds for %s = %d %d %d\n",
		words[1], rtt_wthres, rtt_ethres, rtt_cthres);
      if (rtableid != 0)
        fprintf(stderr, "debug: rtableid for %s = %d\n", words[1], rtableid);
    }
    if (pkt_wthres >= npackets || pkt_ethres >= npackets || 
	pkt_cthres >= npackets)
    {
      fprintf(stderr,
	      "readconfig [%d] Threshold pkt loss exceeds number of packets %d,  resetting to 0\n", lineno, npackets);
      pkt_wthres = pkt_ethres = pkt_cthres = 0;
    }
    /*
     * Create a device info node and add to the tail of linked list
     */
    newnode = (struct device_info *)calloc(1, sizeof(struct device_info));
    if(!newnode)
    {
      fprintf(stderr,"%s [%d]: Out of Memory ", prognm, lineno);
      return -1;
    }

    newnode->pkt_wlevel = (u_long)pkt_wthres;;
    newnode->pkt_elevel = (u_long)pkt_ethres;    
    newnode->pkt_clevel = (u_long)pkt_cthres;
    newnode->rtt_wlevel = (u_long)rtt_wthres;;
    newnode->rtt_elevel = (u_long)rtt_ethres;    
    newnode->rtt_clevel = (u_long)rtt_cthres;
    newnode->rtableid   =         rtableid;
    newnode->next = NULL;
	
    if(!device_info_list)
      device_info_list = newnode;		/* This is the first node (head) */
    else
      lastnode->next = newnode;
	  
    lastnode = newnode;
    ++hostcnt;

    write_event(fdout, &v);
    /* for ip mode, write out another event for round-trip times RTT */
    if (mode != RPCPING_MODE)
    {
      strncpy (v.var.name, RTTVARNM, sizeof (v.var.name) - 1);
      strncpy (v.var.units, RTTVARUNITS, sizeof (v.var.units) - 1);
      write_event(fdout, &v);
      strncpy (v.var.name, varnm, sizeof (v.var.name) - 1);
      strncpy (v.var.units, varunits, sizeof (v.var.units) - 1);
    }
  }	/* end: while */
  fclose(pconfig);
  close_datafile(fdout);

  if (mode == IPPING_MODE && strstr(ping, "multiping")) {
    mode = IPMULTIPING_MODE;
    batchsize = IPMULTIPING_BATCHSIZE;
  }


  if (debug)
    fprintf(stderr, "Total device count = %d\n", hostcnt);
  if (debug > 1)
    fprintf(stderr,
	    "Mode= %d, ping= %s, npackets= %d, pktsize=%d\n",
	    mode, ping, npackets, pktsize);
  if (!pollinterval)
    pollinterval = POLLINTERVAL ;		/* default value */

  return (1);
}		/* readconfig() */

/*
 * This func. calls pingmon, gets the number of packets lost for
 * every device and writes the output datafile. It is slightly different
 * from 'regular' poll_devices since it works with batches of events at
 * a time for efficiency (multiping).
 * For IPping, we have to handle the extra event per device that we have
 * inserted for measuring RTT.
 */

int poll_devices()
{
  int buflen, fdout,  finish = 0;
  float *pvalues;			/* array of return values */
  char *datafile;
  static char **devices = NULL;
  static EVENT *varray = NULL;
  struct device_info *curnode;	/* current device being processed */

  datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile %s", prognm, datafile);
    perror (datafile);
    return (-1);
  }

  if (batchsize <= 0)
    batchsize = 1;	/* prevent being stuck in a loop */

  if (devices == NULL)
  {
    devices = (char **)malloc(sizeof (char *) * (batchsize+1));
    varray = (EVENT *)malloc(sizeof(EVENT) * (batchsize+1) * 2);
    if (devices == NULL || varray == NULL) {
      perror("malloc() for devices/events");
      return (-1);
    }
  }
    
  /* 
   * until end of the file or erroneous data... one entire pass
   */

  curnode = device_info_list;	/* Reset to the head of linked list */

  while (!finish)
  {
    register int i, j;
    int nevents;
    int maxseverity, status;
    u_long thres;
    EVENT *pv;

    devices[0] = NULL;		/* reset every time */
    pv = varray;		/* point to array of events */

    if (mode != RPCPING_MODE)
      buflen = read_n_events (fdout, pv, batchsize * 2);
    else
      buflen = read_n_events (fdout, pv, batchsize);

    if (buflen <= 0)
      break;
    nevents = (int) (buflen / sizeof(EVENT));	/* will be 2x in IP mode */

    for (i=0, j=0; i < nevents; ++i, ++pv) {
      if (mode != RPCPING_MODE  &&  i % 2 == 1)
	continue;	/* skip rtt entry */

      if (pv->device.rtableid) {
        devices[j] = (char *) malloc(256);
        snprintf(devices[j], 255, "%s %d", pv->device.addr, pv->device.rtableid);
        j++;
      } else {
        devices[j++] = pv->device.addr;
      }
    }
    devices[j] = NULL;

    pvalues = (float *)pingmon(devices);	/* returns array of return values */

      if (pvalues == NULL)
        return (-1);             /* fatal error */
      pv = varray;
 
    // json init, fopen, preparation
    json_init_extension(get_bare_prognm(), "snipsippingmon", fileExtension);
    // end json


    for (i = 0; i < nevents / (mode != RPCPING_MODE ? 2 : 1); ++i, ++pv)
    {
      status = calc_status(pvalues[5*i+0], curnode->pkt_wlevel,
                          curnode->pkt_elevel, curnode->pkt_clevel,
                          /*incr*/0, &thres, &maxseverity) ;
      update_event(pv, status, /* VALUE */ pvalues[5*i+0], thres, maxseverity);
      if (debug > 1)
      {
       fprintf(stderr,
               "(debug) %s: Device %s (%s) var %s status is %s (val=%lu)\n",
               prognm, pv->device.name, pv->device.addr, pv->var.name,
               status ? "UP" : "DOWN",  (u_long)pvalues[5*i+0]);
       fflush(stderr);
      }
      if (mode != RPCPING_MODE)        /* IP ping, so do RTT */
      {
       ++pv;
       status = calc_status(pvalues[5*i+1], curnode->rtt_wlevel,
                            curnode->rtt_elevel, curnode->rtt_clevel,
                            /*incr*/1, &thres, &maxseverity) ;
       update_event(pv, status, /* VALUE */ pvalues[5*i+1], thres, maxseverity);

        float npackets_f = (float)npackets;
        
        json_log(pv->device.name, 6,
            "pkts_sent",  (char *)&npackets_f,                                                  
            "pkts_recvd", (char *)&(pvalues[5*i+0]),                          
            "rtt_min",    (char *)(pvalues[5*i+0] > 0.00 ? &pvalues[5*i+1] : NULL),
            "rtt_avg",    (char *)(pvalues[5*i+0] > 0.00 ? &pvalues[5*i+2] : NULL),
            "rtt_max",    (char *)(pvalues[5*i+0] > 0.00 ? &pvalues[5*i+3] : NULL),
            "rtt_stddev", (char *)(pvalues[5*i+0] > 0.00 ? &pvalues[5*i+4] : NULL) 
            );

       if (debug > 1)
       {
         fprintf(stderr,
                 "(debug) %s: Device %s (%s) var %s status is %s (val=%lu)\n",
                 prognm, pv->device.name, pv->device.addr, pv->var.name,
                 status ? "UP" : "DOWN",  (u_long)pvalues[5*i+1]);
         fflush(stderr);
       }

        // Extra fields...
      }


      curnode = curnode->next;
    }  /* end for() */

    json_close();

    rewrite_n_events (fdout, varray, nevents);
  
    if (do_reload)		/* recvd SIGHUP */
      break;
     
  }	/* end of:    while (!finish)	*/
    
  /**** Now determine why we broke out of the above loop *****/
  close_datafile(fdout);

  if (buflen >= 0)			/* reached end of file or break */
    return (1);
  else {				/* error in output data file 	*/
    fprintf (stderr, "%s: Insufficient data in output file ", prognm);
    perror("read()");
    return (-1);
  }

}	/* end of:  poll_devices		*/

int help()
{
  snips_help();
  fprintf(stderr, "\
  This program sends ICMP (or RPC) pings to the devices being monitored\n\
  and escalates their severity if the configured number of packets are\n\
  lost\n\n");
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
 * override the library functions since we are calling snips_main()
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
