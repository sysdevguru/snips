#ifdef RRDTOOL

/* $Header: /home/cvsroot/snips/lib/rrd_funcs.c,v 1.1 2001/08/05 14:08:49 vikas Exp $ */
/*
 * Store snips events data using RRDtool (http://www.caida.org/Tools)
 * The aggregation  of data stored in the files is hard coded. Remember
 * to match these values in the snipslib.pl perl module if you change
 * any of the following:
 *	RRD_DBDIR
 *	RRA parameters in snips_rrd_create()
 *
 * LOGIC:
 *	Creates subdirectory  RRD_DBDIR/<deviceaddr> and then stores
 *	the RRD data using the subdevice+variable name as the RRD filename.
 *	The 'subdevice' name is usually tacked onto the front of the devicename
 *	with a + at the end, and is extracted from the devicename.
 *
 * BUGS:
 *
 * AUTHOR:
 *	Vikas Aggarwal, vikas@navya.com
 */

/* Copyright 2001, Netplex Technologies, Inc. */

/*
 * $Log: rrd_funcs.c,v $
 * Revision 1.1  2001/08/05 14:08:49  vikas
 * Now checks if the sitename or varname is blank.
 *
 * Revision 1.0  2001/07/08 22:19:38  vikas
 * Lib routines for SNIPS
 *
 *
 */

/* #include "snips.h"	*/
#include "osdefs.h"
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#ifndef NeXT
#  include <unistd.h>			/* for access(), lseek()  */
#endif	/* NeXT */
#include <sys/file.h>
#include <sys/stat.h>

#ifndef RRD_DBDIR
# define RRD_DBDIR "."		/* parent dir for creating RRD data files */
#endif

/*
 * This routine does everything (creates file, updates it, etc).
 *
 * Create the RRD database based on deviceAddr/subdevice+varname.
 * Ignore the sender and 'devicename' for now.
 * Use a single letter hash for dir structure.
 */

snips_rrd(timestamp, devicename, deviceaddr, subdevice, 
	  varname, value, severity, sender)
  time_t timestamp;
  char *devicename, *deviceaddr, *subdevice, *varname, *sender;
  unsigned long value;
  int severity;
{
  register i;
  char dbpath[1024];
  char tdevicename[128], tdeviceaddr[128], tvarname[128], tsubdev[128];
  char *dirname;
  struct stat sbuf;

  cleanup_path(devicename, tdevicename);
  cleanup_path(deviceaddr, tdeviceaddr);
  cleanup_path(subdevice, tsubdev);
  cleanup_path(varname, tvarname);

  /* dirname = tdeviceaddr;		/* dir name is device addr */
  dirname = tdevicename;
  if (*dirname == '\0' || *tvarname == '\0') {
    fprintf(stderr,
	    "ERROR: snips_rrd() empty devicename '%s' or variable '%s'\n",
	    dirname, tvarname);
    return(-1);
  }

  if (*tsubdev)
    sprintf(dbpath,"%s/%c/%s/%s+%s.rrd",
	    RRD_DBDIR, *dirname, dirname, tsubdev, tvarname);
  else
    sprintf(dbpath,"%s/%c/%s/%s.rrd", RRD_DBDIR, *dirname, dirname, tvarname);
    
  if (stat(dbpath, &sbuf) != 0)
  {
    if (errno == ENOENT)
    {
      if (make_dbdir(dirname) != 0)
	return (-1);
      if (snips_rrd_create(dbpath, timestamp, tvarname) != 0)
	return (-1);
    }
    else {
      fprintf(stderr, "Cannot access RRD database %s- ", dbpath);
      perror("");
      return (-1);
    }
  }

  snips_rrd_update(dbpath, timestamp, value, severity);

  return (0);
}	/* snips_rrd  */

/*
 * Delete characters that cannot be in a standard Unix path
 */
cleanup_path(src, dest)
  char *src, *dest;
{
  register i;
  for (i = 0; i < strlen(src) ; ++i)
    switch (src[i]) {
    case ' ': case '\t': case '/': case '\\':
    case '|': case '`':
      dest[i] = '_';	/* delete unusable characters */
      break;
    default:
      if (dest[i] > 32 || dest[i] < 126)
	dest[i] = src[i];
    }
  dest[i] = '\0';
}

/*
 * Create path to the db file
 */
make_dbdir(dirname)
  char *dirname;
{
  char dirpath[1024];
  struct stat sbuf;

  sprintf(dirpath, "%s/%c/%s", RRD_DBDIR, *dirname, dirname);
  if (stat(dirpath, &sbuf) == 0)
    return (0);

  if (stat(RRD_DBDIR, &sbuf) != 0 && mkdir(RRD_DBDIR, 0755) != 0)
  {
    perror(RRD_DBDIR);
    return (-1);
  }

  sprintf(dirpath, "%s/%c", RRD_DBDIR, *dirname);
  if (stat(dirpath, &sbuf) != 0 && mkdir(dirpath, 0755) != 0)
  {
    perror(dirpath);
    return (-1);
  }

  sprintf(dirpath, "%s/%c/%s", RRD_DBDIR, *dirname, dirname);
  if (stat(dirpath, &sbuf) != 0 && mkdir(dirpath, 0755) != 0)
  {
    perror(dirpath);
    return (-1);
  }

  return (0);
}	/* make_dbdir() */

snips_rrd_create(dbpath, timestamp, varname)
  char *dbpath, *varname;
  time_t timestamp;
{
  int ac = 0;
  char *(av[16]);
  char ts[32];		/* for timestamp as string */

  bzero (av, sizeof(av));
  sprintf(ts, "%ld", (long)timestamp - 600L);

  /* call rrd library function */
  /* we store the MIN value for storing the severity for 2 days only. This
     way we dont lose any granularity since we dont really want to average
     out severity over multiple days, its meaningless. This way, when we
     plot severity, we call with the MIN CF so that nothing is available
     for the monthly or yearly data.
  */

  av[ac++] = "create";
  av[ac++] = dbpath;
  av[ac++] = "--start";
  av[ac++] = ts;
  av[ac++] = "--step";
  av[ac++] = "600";			/* 10 minute samples (600 secs) */
  av[ac++] = "DS:var1:GAUGE:1800:0:U";	/* always refer to dataset as var1 */
  av[ac++] = "DS:sev:GAUGE:1800:0:U";	/* store severity also */
  av[ac++] = "RRA:AVERAGE:0.5:1:288";	/* 10m avg for 2 days (2*24*60/10) */
/*  av[ac++] = "RRA:MIN:0.5:1:288";	/* 10m min for 2 days (2*24*60/10) */
  av[ac++] = "RRA:AVERAGE:0.5:3:384";	/* 30m avg for 8 days (8*24*60/30) */
/*  av[ac++] = "RRA:MIN:0.5:3:384";	/* 30m min for 8 days (8*24*60/30) */
  av[ac++] = "RRA:AVERAGE:0.5:12:336";	/* 2hr avg for 4 weeks (28*24/2) */
  av[ac++] = "RRA:AVERAGE:0.5:144:370";	/* 1d  avg for 1+ yr (370 days) */

  optind=0;	/* reset gnu optind, very important */
  opterr=0;	/* reset error index */
  rrd_clear_error();
  if (rrd_create(ac, av) < 0)
  {
    fprintf(stderr, "ERROR: rrd_create() %s\n", rrd_get_error());
    return (-1);
  }

  return (0);

}	/* snips_rrd_create() */

snips_rrd_update(dbpath, timestamp, value, severity)
  char *dbpath;
  time_t timestamp;
  unsigned long value;
  int severity;		/* snips severity */
{
  int ac = 0;
  char *(av[16]);
  char tval[128];

  bzero (av, sizeof(av));
  sprintf(tval, "%ld:%lu:%d", (long)timestamp, value, severity); 

  av[ac++] = "update";
  av[ac++] = dbpath;
  av[ac++] = tval;

  optind=0;	/* reset gnu optind, very important */
  opterr=0;	/* reset error index */
  rrd_clear_error();

  if (rrd_update(ac, av) == -1)
  {
    fprintf(stderr, "ERROR: rrd_update() %s- %s\n", dbpath, rrd_get_error());
    return (-1);
  }

  return (0);

}	/* snips_rrd_update() */

#endif /* RRDTOOL */
