/* $Header: /home/cvsroot/snips/lib/snips_main.c,v 1.1 2008/04/25 23:31:51 tvroon Exp $ */

/*
 * Generic snips_main() and  support routines.
 *
 * Requires the following three functions to be set by the calling program:
 *
 *	readconfig_func = (int (*)())readconfig;
 *	polldevices_func  = (int (*)())poll_devices;  # can be null
 *	test_func  = (int (*)())radiusmon;  # set if polldevices_func is null
 *	help_func = (int (*)())help;
 *
 * You can use the set_xxxx_function()   functions to set these variables.
 *
 * If polldevices_func is NULL, then you must set test_func  to a valid
 * test function, because snips_main() calls generic_polldevices() which
 * requires the test_func
 *
 * AUTHOR:    Vikas Aggarwal, vikas@navya.com
 *
 */

/*
 * $Log: snips_main.c,v $
 * Revision 1.1  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/14 03:17:59  vikas
 * Initial revision
 *
 */

/* Copyright 2001 Netplex Technologies, Inc. */

#include "snips.h"
#include "event_utils.h"
#include "eventlog.h"
#include "snips_funcs.h"
#include "snips_main.h"

#include <stdio.h>
#include <signal.h>
#include <sys/file.h>
#include <errno.h>


/*+ snips_main()
 * Parse args, calls readconfig_func() and then calls polldevices_func()
 * forever.
 * If polldevices_func() is not set, then calls generic_poll_devices()
 * which expects test_func() to be set.
 *
 */
void snips_main(ac, av)
  int ac;
  char **av;
{
  extern char *optarg;
  extern int optind;
  register int c ;
  char *p;
  time_t starttm, polltime ;

  prognm = av[0] ;				/* Save the program name */
  pollinterval = 300;				/* let config file override */

  if (help_func == NULL ||  *help_func == NULL)
    set_help_function(NULL);

  while ((c = getopt(ac, av, "adf:o:x:")) != EOF)
    switch (c)
    {
    case 'a':
      autoreload++ ;		/* auto re-read cnfig file if modified */
      break;
    case 'd':
      debug++ ;
      break ;
    case 'f':		/* config file */
      if (! (char *)set_configfile(optarg))
	snips_done();
      break ;
    case 'o':		/* output datafile */
      if (! (char *)set_datafile(optarg))
	snips_done();
      break ;
    case 'x':		/* extension to prognm */
      if( !(extnm = (char *)Strdup(optarg)) ) {
	perror("malloc()");
	snips_done();
      }
      break ;
    case '?':
    default:
      fprintf (stderr, "%s: Unknown flag: %c\n", prognm, optarg);
      (*help_func)();
      snips_done() ;
    }

  switch (ac - optind)
  {
  case 0:					/* default config file */
    break;
  case 1:
      if (! (char *)set_configfile(av[optind]))
	snips_done();
      break ;
  default:
    fprintf (stderr, "%s Error: Too many config files\n\n", prognm);
    (*help_func)();
    snips_done();
  }

  /* ensure these functions are not NULL */
  if (help_func == NULL || *help_func == NULL)
    set_help_function(NULL);
  if (readconfig_func == NULL || *readconfig_func == NULL)
    set_readconfig_function(NULL);
  if (polldevices_func == NULL || *polldevices_func == NULL)
    set_polldevices_function(NULL);	/* sets to generic_poll_devices() */
  if (test_func == NULL || *test_func == NULL)
    set_test_function(NULL);	/* only needed by generic_poll_devices() */

  snips_startup();

  openeventlog() ;			/* Event logging */

  if (debug)
  {
    fprintf(stderr,
	    "(debug) %s: \n\t CONFIGFILE= '%s'\n\t DATAFILE= '%s'\n\t",
	    prognm, get_configfile(), get_datafile()) ;
    
      char **hosts = get_snips_loghosts();
      fprintf(stderr,
              "LOGHOST= "
              ) ;
      
      while ( *hosts)
      {
          fprintf(stderr, "%s ", *hosts);
          hosts++;
      }
      fprintf(stderr,
              "\n"
              ) ;
  }
    
  if ( (*readconfig_func)() == -1)
    snips_done() ;

  /* polldevices_func() makes one complete pass over the list of nodes */
  while (1)			      		/* forever */
  {
    starttm = time((time_t *)NULL) ;	/* time started this cycle */
    if ( (*polldevices_func)() == -1)		/* Polling error */
      break ;

    if (autoreload != 0)
      check_configfile_age();	/* if config file modified, auto reloads */

    if (do_reload)		/* flag set on sighup */
    {
      snips_reload(/*readconfig function*/NULL);
      do_reload = 0;		/* reset  flag */
      continue;
    }

    polltime = time((time_t *)NULL) - starttm; 
    if (debug)
      fprintf(stderr, "%s: Poll time= %ld secs\n", prognm, (long) polltime);
    if (polltime < pollinterval)
    {
      if(debug)
	fprintf(stderr,
		"%s: Sleeping for %u secs.\n",
		prognm, (unsigned)(pollinterval - polltime));
#ifdef SVR4
      bsdsignal(SIGALRM, SIG_DFL);
#else
      signal(SIGALRM, SIG_DFL);		/* in case blocked */
#endif
      sleep ((unsigned)(pollinterval - polltime));
    }

  }	/* while (1) */

  /* HERE ONLY IF ERROR */
  snips_done();

}		/* end snips_main() */


/*		FUNCTIONS TO CHANGE FUNCTIONS called by snips_main()
 *
 * These 3 functions can be set in the main program before calling
 * snips_main() to call something other than readconfig(), etc.
 *
 * Only readconfig_function() is important for reload(). If you are not
 * using snips_main(), then you probably dont need to call these functions
 * ( except that reload() needs a valid readconfig_func() )
 *
 *	readconfig_func = (int (*)())readconfig;
 */
/* null functions which show error messages */
void set_help_function(f)
  int (*f)();		/* ptr to function */
{
  int snips_help();

  if (f && *f)
    help_func = (int (*)())f;
  else
    help_func = (int (*)())snips_help;
}
void set_readconfig_function(f)
  int (*f)();
{
  int null_readconfig();

  if (f && *f)
    readconfig_func = (int (*)())f;
  else
    readconfig_func = (int (*)())null_readconfig;

  if (debug > 2)	/* */
    fprintf(stderr, "(debug) set_readconfig_function to ptr addr %lx\n",
	    (u_long)(readconfig_func));
}
void set_polldevices_function(f)
  int (*f)();
{
  int generic_polldevices();

  if (f && *f)
    polldevices_func = (int (*)())f;
  else
    polldevices_func = (int (*)())generic_polldevices;

  if (debug > 2)
    fprintf(stderr, "(debug) set_polldevices_function to ptr addr %lx\n",
	    (u_long)(polldevices_func));
}

void set_test_function(f)
  u_long (*f)();
{
  u_long null_test();
  if (f && *f)
    test_func = (u_long (*)())f;
  else
    test_func = (u_long (*)())null_test;

  if (debug > 2)
    fprintf(stderr, "(debug) set_test_function to ptr addr %lx\n",
	    (u_long)(test_func));
}

int null_readconfig()
{
  fprintf(stderr,
	  "%s- FATAL, Set readconfig_func() before calling snips_main() and recompile\n",
	  prognm);
  snips_done();
  return(0);
}

unsigned long
null_test(fd, cfile)
  int fd;
  char *cfile;
{
  fprintf(stderr,
	  "%s- FATAL, Set test_func() if you dont have a custom poll_devices() function and recompile.\n",
	  prognm);
  snips_done();
  return(0);
}

/*
 * Calls the test_func() which should have been set before calling this
 * routine. Reads the threshold from the datafile events so readconfig()
 * should have filled that field. Cannot handle multiple threshold levels
 * (since no clean way to pass it the 3 threshold levels).
 */
int generic_polldevices()
{
  int status, readbytes, fdout, maxseverity;
  unsigned long value;
  char *datafile;
  EVENT v;			    	/* described in snips.h	*/

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

  while ( (readbytes = read_event(fdout, &v)) == sizeof(v) )
  {
   value = (unsigned long)(*test_func)(v.device.name, v.device.addr);
   
   if (strstr(prognm, "radiusmon"))
   {
   	  status = calc_status(value, 2, 2,
			0, -1,
			&(v.var.threshold), &maxseverity);

   }
   else
   status = calc_status(value, v.var.threshold, v.var.threshold,
			v.var.threshold, -1,
			&(v.var.threshold), &maxseverity);
   if (debug)
   {
     fprintf(stderr, "(debug) %s: %s %s %s\n", prognm, v.device.name,
	     v.var.name, status ? "up" : "down");
     fflush(stderr);
   }

   update_event(&v, status, value, v.var.threshold, maxseverity);
   rewrite_event(fdout, &v);

   if (do_reload)		/* recvd SIGHUP */
     break;
  }	/* end of:    while (read..)	*/
    
  /**** Now determine why we broke out of the above loop *****/
  close_datafile(fdout);
  if (readbytes >= 0)			/* reached end of the file */
    return (1);
  else {				/* error in output data file */
    fprintf (stderr, "%s: Insufficient read data in output file", prognm);
    return (-1);
  }

}	/* generic_polldevices() */

