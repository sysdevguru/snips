/*+ 	$Header: /home/cvsroot/snips/lib/snips_funcs.c,v 1.4 2008/04/25 23:31:51 tvroon Exp $
 *
 */

/*
 * Miscellanous SNIPS utility routines for the SNIPS library.
 *
 *	Vikas Aggarwal, vikas@navya.com
 *
 */

/*
 * $Log: snips_funcs.c,v $
 * Revision 1.4  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.3  2001/08/22 01:58:17  vikas
 * Now recognizes CONFIGDIR keyword also (crawford.6@sociology.osu.edu)
 *
 * Revision 1.2  2001/08/01 23:21:07  vikas
 * Just added some debug statements.
 *
 * Revision 1.1  2001/07/28 01:48:32  vikas
 * standalone() checks to ensure it does not kill current process.
 *
 * Revision 1.0  2001/07/14 03:17:17  vikas
 * Initial revision
 *
 */

/* Copyright 2000-2001, Netplex Technologies, Inc. */


#include "snips.h"
#include "snips_funcs.h"
#include "eventlog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>				/* signal numbers	*/
#include <sys/file.h>
#include <errno.h>

#define MAX_LEN 256

static char	*configDir = ETCDIR;
static char	*dataDir = DATADIR;
static char	*pidDir = PIDDIR;
static char     jsonDir[MAX_LEN];// = JSONDIR;
static char	*configFile;		/* File with the list of nodes	*/
static char	*dataFile;		/* Names of the data file	*/
static char	 *snips_loghost[SNIPS_HOST_MAX_COUNT];
static void (*old_sigusr1_handler)();	/* store old sigusr1 handler */

/*
 * Called upon startup. Calls 'standalone()', and sets up the interrupt
 * signal handlers.
 *
 * Set the following BEFORE calling this function:
 *	prognm  (set to value of program name or sender)
 *	extnm   (set to any desired -x extension)
 */
int snips_startup()
{
  char *s ;

  if (prognm == NULL)
    prognm = "progname";
  if ((s = (char *)strrchr(prognm, '/')) != NULL)
    prognm = ++s ;				/* delete path from prognm */

  read_global_config();
  /*
   * Create PID file on startup
   */
  s = (char *)get_pidfile();
  if (standalone(s) < 0)    /* Kill prev running process    */
  {
    fprintf(stderr, "%s: Error in standalone...exiting\n", prognm);
    exit (1);
  }

  umask (002);			/* Not world writable atleast */

  /*
   * Signal handlers
   */
#ifdef SVR4				/* When will these guys learn!! */
  bsdsignal (SIGPIPE, SIG_IGN);	/* dont core on writing to pipes */
  bsdsignal (SIGQUIT, snips_done);
  bsdsignal (SIGTERM, snips_done);
  bsdsignal (SIGINT,  snips_done);
  bsdsignal (SIGHUP,  hup_handler);
  old_sigusr1_handler = bsdsignal (SIGUSR1, usr1_handler);
#else
  signal (SIGPIPE, SIG_IGN);		/* dont core on writing to pipes */
  signal (SIGQUIT, snips_done);	/* Delete pid/data file while dying */
  signal (SIGTERM, snips_done);
  signal (SIGINT,  snips_done);
  signal (SIGHUP,  hup_handler);
  old_sigusr1_handler = signal (SIGUSR1, usr1_handler);	/* toggles debug */
#endif

  return 0;
}	/* end snips_startup() */

/*
 * Read global snips config file. This function will grow over time as
 * we add more and more config information in this file.
 */
int read_global_config()
{
  int lineno = 0;
  char buf[BUFSIZ], fname[256], *cfile;
  extern char *skip_spaces(), *strtok();
  FILE *fp;
  
  //initialize the host array;
  int index;
  for (index = 0; index < SNIPS_HOST_MAX_COUNT; index++) snips_loghost[index] = NULL;
    

  if (! (cfile = (char *)getenv("SNIPS_CONFIG")))
    if (! (cfile = (char *)getenv("SNIPS_CONF")))
      if (! (cfile = (char *)getenv("SNIPSCONFIG")))
	cfile = GLOBAL_CONFIG;
  if ((fp = fopen(cfile, "r")) == NULL)
  {
    cfile = fname;
    sprintf(cfile, "%s/%s", get_etcdir(), "snips.conf");
    if ((fp = fopen(cfile, "r")) == NULL) {
      cfile = "snips.conf";
      if ((fp = fopen(cfile, "r")) == NULL) {
	return (0);		/* cannot access, silently fail */
      }
    }
  }
  fprintf(stderr, "%s- Reading global config file %s\n", prognm, cfile);
  while (fgets(buf, BUFSIZ - 1, fp) != NULL)
  {
    char *line = buf;
    char *tok;

    ++lineno;
    if (*line && *(line + strlen(line) - 1) == '\n')
      *(line + strlen(line) - 1) = '\0';	/* strip trailing newline */
    line = skip_spaces(line);
    if (*line == '\n' || *line == '\0' || *line == '#')
      continue;
    tok = strtok(line, " \t");
    if (! strncasecmp(tok, "DATADIR", strlen("DATADIR")) ) {
      tok = strtok(NULL, " \t");
      if (tok && *tok) {
	if (dataDir != DATADIR) free (dataDir);
	dataDir = Strdup(tok);
      }
    }
    else if ( (! strncasecmp(tok, "ETCDIR", strlen("ETCDIR"))) ||
	      (! strncasecmp(tok, "CONFIGDIR", strlen("CONFIGDIR")))
	    ) {
      tok = strtok(NULL, " \t");
      if (tok && *tok) {
	if (configDir != ETCDIR) free (configDir);
	configDir = Strdup(tok);
      }
    }
    else if (! strncasecmp(tok, "PIDDIR", strlen("PIDDIR")) ) {
      tok = strtok(NULL, " \t");
      if (tok && *tok) {
	if (pidDir != PIDDIR) free (pidDir);
	pidDir = Strdup(tok);
      }
    }
    else if (! strncasecmp(tok, "LOGHOST", strlen("LOGHOST")) ||
	     ! strncasecmp(tok, "SNIPS_LOGHOST", strlen("SNIPS_LOGHOST")) ) {
        index = 0;
    
       //SNIPS-4
       //reading in multiple host entries separated by spaces or tabs
       while ((tok = strtok(NULL, " \t")))
       {
           if (tok && *tok) {
               if (snips_loghost[index]) free (snips_loghost[index]);
               snips_loghost[index] = Strdup(tok);
               index++;
           }
       }
    }
    else {
      fprintf(stderr, "%s [%d]: unknown keyword\n", cfile, lineno);
    }
  }	/* while */
  fclose(fp);

  return (1);
}


/*+ 
 * FUNCTION:
 * 	Increase 'debug' to a value of 3 before resetting it each time
 * it receives a SIGUSR1 signal.
 */
void usr1_handler()
{
  debug = (++debug) % 4;	/* from 0 - 3 */

  if (debug)
    fprintf (stderr, "(debug) %s: recvd USR1, enabling debug= %d\n",
	     prognm, debug);
  else
    fprintf (stderr, " %s: recvd USR1, debug turned OFF\n", prognm);
}    

/*
 * The HUP handler does not invoke any function.. it just sets a
 * flag.
 */
void hup_handler()
{
  if (debug)
    fprintf(stderr, "Recieved HUP signal\n");
  ++do_reload;		/* set global flag */
}


/*
 * function which can be called by an external program to restore the
 * previous signal handler
 */
void snips_restore_sigusr1()
{
#ifdef SVR4
  bsdsignal (SIGUSR1, old_sigusr1_handler);
#else
  signal (SIGUSR1, old_sigusr1_handler);
#endif
}

/*+
 *
 * FUNCTION:
 *
 *	Kill any other process and write the present PID into the
 * PidFile. If cannot write to the file or if the file is locked
 * by another process, it returns a value of -1 (in which case the
 * calling program should EXIT).
 *
 * It assumes that the PidFile is "program.pid", where the pointer
 * to the string "program" is passed to it.
 *
 * (Observe that unless the program name has been stripped of the 
 *  path, the pid file is created in the same directory as the
 *  program).
 *
 *  Method:
 *		See if pid file exists, create if none
 *	        try to lock
 *		if cannot lock 
 *		   kill process
 *		   if (cannot kill)
 *		     return (-1)
 *		   else
 *		     lock file
 *		write pid, return to caller
 *
 *  For some reason, the access() call misbehaved on some platforms. So
 *  we use fopen() to check for a file's existence.
 */

int standalone(pidfile)
  char *pidfile;		/* path of the pid file */
{
  FILE *pidf ;
  int oldpid = 0, mypid = 0;
  int fd ;
  char hostname[MAXLINE], thishostname[MAXLINE] ;
    
  gethostname(thishostname, sizeof(thishostname) -1) ;
  thishostname[MAXLINE - 1] = '\0' ;
  mypid = getpid();

  if ( (pidf =fopen (pidfile, "r")) != NULL)	/* file exists...	*/
  {
    if (fscanf(pidf, "%d %s", &oldpid, hostname) == EOF)
    {
      fprintf(stderr, "(standalone): fscanf() error parsing old pid ");
      perror(pidfile) ;			/* couldn't read */
      /* return (-1);	/* Lets not return,,, */
      oldpid = 0;
    }
    fclose(pidf);

    if (oldpid)
    {
      if (strcmp(thishostname, hostname) != 0)	/* wrong host */
      {
	fprintf(stderr,
		"(standalone) %s: Program probably running on '%s'\n",
		prognm, hostname) ;
	fprintf(stderr, "Kill and delete '%s' file and restart\n",pidfile);
	return (-1) ;
      }
      else					/* on proper host */
      {
	if (oldpid == mypid)
	{	/* standalone() probably called twice, ignore */
	  fprintf(stderr,
		  "(standalone) %s: Cannot kill self\n", prognm);
	  return (0);
	}

	if (kill (oldpid, SIGKILL) != 0 && errno != ESRCH)
	{
	  fprintf(stderr,
		  "(standalone) %s: Couldn't kill earlier process\n",
		  prognm);
	  perror("signal");
	  return (-1) ;
	}
	else
	  sleep (2) ;		/* Let other process die */
      }
    }	/* if(oldpid) */
  }			/* end if (pidfile could be opened) */

  /*
   * Here only if all other processes have been killed
   */

  if ( (pidf = fopen(pidfile, "w")) == NULL)	/* create file	*/
  {
    fprintf(stderr, "(standalone): fopen ");
    perror(pidfile);	
    return(-1);
  }
  fprintf (pidf,  "%d\n%s\n", mypid, thishostname);	/* Store present pid */
  fflush(pidf);
  fclose(pidf) ;
  fprintf(stderr, "(%s).. locked pid-file, started new process (pid=%d)\n",
	  prognm, mypid);
    
  return (0) ;
}

/*+             snips_done
 * FUNCTION:
 *
 * Delete the PID and the data file. Called just before exiting, typically
 * after recieving some sort of a terminate signal.
 */
void snips_done()
{
  char *s;
 
  closeeventlog() ;
 
  fprintf (stderr, "%s: removing data, pid file.... ", prognm);
  s = (char *)get_pidfile();
  unlink(s);			/* remove the PID file  */
  s = (char *)get_datafile();
  if (s && *s)
    unlink (s);		/* delete the data file */

  fprintf (stderr, "Done\n");
  exit (1);
}

/*+
 * Print a standard help blurb
 */

int snips_help()
{
  fprintf(stderr, "\tSNIPS Version %s\n", (char *)snips_version);
  fprintf(stderr, "\nUSAGE: %s [option...]\n", prognm);
  fprintf(stderr, "\
\t -a              \t auto-reload config file if changed\n\
\t -d              \t debug\n\
\t -f <configfile> \t alternate config file\n\
\t -o <data file>  \t alternate output data file\n\
\t -x <extension>  \t add '-extension' to the program name\n\n\
  The default data filename and the config file name are created from the\n\
  program name by appending -confg and -output extensions.\n\
  Sending a SIGUSR1 toggles debugging, SIGHUP reloads config file\n");
  fprintf(stderr, " Default config file = %s\n", get_configfile());
  fprintf(stderr, " Default data   file = %s\n", get_datafile());
  return 0;
}

/*+
 * Does all the things that snips_main() does on getting a HUP. Calls
 * copy_events_datafile() to copy over the state of all old events.
 * If you dont want to call this generic function, then you can call
 * any function upon detecting do_reload flag is set.
 *
 * You MUST set the value of 'readconfig_func' or pass it a valid
 * function to read the config file in the parameters (else there will
 * be no way to detect which function to call on getting a reload).
 */
int snips_reload(newreadconfig)
  int (*newreadconfig)();	/* can be NULL to use default function */
{
  int rc;
  char *odatafile, *ndatafile;

#ifdef SVR4
  bsdsignal (SIGHUP, SIG_IGN);
#else
  signal (SIGHUP, SIG_IGN);
#endif

  do_reload = 0;		/* reset global flag */

  if (newreadconfig == NULL || *newreadconfig == NULL)
    if (readconfig_func == NULL || *readconfig_func == NULL)
    {
      fprintf(stderr,
	      "(%s) Cannot reload, readconfig_func() not set while compiling\n",
	      prognm);
      return (-1);
    }
    
  fprintf(stderr, "(%s) Reloading config file...\n", prognm);

  read_global_config();		/* global config parameters, future */

  /* switch the global datafile name temporarily */
  odatafile = Strdup((char *)get_datafile());
  ndatafile = (char *)malloc(strlen(odatafile) + 10);
  strcpy(ndatafile, odatafile);
  strcat(ndatafile, ".hup");
  set_datafile(ndatafile);

  if (newreadconfig && *newreadconfig)
    rc = (*newreadconfig)();	/* use supplied function */
  else
    rc = (*readconfig_func)();	/* use default function */

  if (rc < 0)
    snips_done();

  /* copy over known event states from old file to new */
  copy_events_datafile(odatafile, ndatafile);

  unlink(odatafile);		/* rename() will overwrite anyway */
  if (rename(ndatafile, odatafile) < 0) {	/* renaming new to old */
    fprintf(stderr, "ERROR- exiting since rename() %s to %s failed ",
	    ndatafile, odatafile);
    perror("");
    snips_done();	/* fatal */
  }

  set_datafile(odatafile);	/* restore datafile name */
  free(odatafile);
  free(ndatafile);
  
#ifdef SVR4
  bsdsignal (SIGHUP, hup_handler);
#else
  signal (SIGHUP, hup_handler);
#endif

  return (1);
}	/* snips_reload() */

/*
 * Given old and new data file descriptors, it searches for all 
 * common data and copies over the existing severity, status etc.
 * from the old datafile. Used when config file reloaded.
 */
int copy_events_datafile(ofile, nfile)
  char *ofile, *nfile;		/* old and new datafiles */
{
  int n, matched = 0;
  int odv, ndv;		/* data versions */
  int ofd, nfd;
  EVENT ov, nv;

  if ( (ofd = open_datafile(ofile, O_RDONLY)) < 0 ) {
    fprintf(stderr, 
	    "(%s) copy_events_datafile() ERROR in open old datafile ", prognm);
    perror(ofile);
    snips_done();
  }
  if ( (nfd = open_datafile(nfile, O_RDWR | O_CREAT)) < 0 ) {
    fprintf(stderr, 
	    "(%s) copy_events_datafile() ERROR in open new datafile ", prognm);
    perror(nfile);
    snips_done();
  }
  if ( (odv = read_dataversion(ofd)) != (ndv = read_dataversion(nfd)) ) {
    fprintf(stderr,
	    "(%s) Data format version mismatch (old=%d new=%d), exiting\n",
	    prognm, odv, ndv);
    snips_done();
  }

  lseek(nfd, (off_t)0, SEEK_SET);	/* rewind new file */
  read_dataversion(nfd);		/* skips past first record */
  while ( (n=read(nfd, (char *)&nv, sizeof(nv))) == sizeof(nv) )
  {
    lseek(ofd, (off_t)0, SEEK_SET);	/* rewind old file */
    read_dataversion(ofd);	      	/* skips past first record */
    while (  (n=read(ofd, (char *)&ov, sizeof(ov))) == sizeof(ov) )
    {
      if (!strcmp(ov.device.name, nv.device.name) &&
	  !strcmp(ov.device.addr, nv.device.addr) &&
	  !strcmp(ov.device.subdev, nv.device.subdev) &&
	  !strcmp(ov.var.name,  nv.var.name) )
      {
	++matched;
	nv.var.value = ov.var.value;	/* copy over value */
	nv.severity = ov.severity;
	nv.loglevel = ov.loglevel;
	/* now extract and set the status flag in the new state */
	ov.state =  ov.state & (n_UP | n_DOWN | n_UNKNOWN);
	SETF_UPDOUN(nv.state, ov.state);

	nv.eventtime = ov.eventtime;
	nv.polltime  = ov.polltime;
	nv.rating = ov.rating;
	nv.id = ov.id;

	lseek(nfd, -(off_t)(sizeof(EVENT)), SEEK_CUR);
	if (write (nfd, (char *)&nv, sizeof (nv)) <= 0)
	{
	  perror("copy_events_datafile write()");
	  return (-1);
	}
	break;	/* from inner while loop */
      }
    }	/* inner while */

  }	/* outer while */

  close_datafile(ofd);
  close_datafile(nfd);

  if (debug)
    fprintf(stderr, "matched %d events from old config\n", matched);

  return (1);
}	/* copy_events_datafile() */

/*
 * Check if config file has been modified and at least 60 secs old.
 * If so, set reload flag. Can be a bit of a problem if using RCS, or
 * the config file is in the middle of edits.
 */
int check_configfile_age()
{
  static time_t config_mtime;		/* config file modified */
  struct stat stat_buf;

  if (stat(configFile, &stat_buf) != 0) {
    fprintf(stderr, "(%s) ERROR stat() %s- %s\n",
	    prognm, configFile, (char *)strerror(errno));
    return (-1);
  }
  if (config_mtime == 0) {		/* first time, just initialize */
     config_mtime= stat_buf.st_mtime;
     return (0);
  }
  if (config_mtime < stat_buf.st_mtime &&
      time((time_t *)NULL) - stat_buf.st_mtime > 60)
    do_reload = 1;

  config_mtime= stat_buf.st_mtime;
  return 0;

}	/* check_configfile_age() */

/*  */
char *get_configfile()
{
  static char *s;

  if (configFile)
    return (configFile);
  if (!extnm)
    extnm = "";
  if (!s) 
  {
    char *t;

    if (!prognm || *prognm == '\0') {
      fprintf(stderr,
	      "FATAL get_configfile()- programming error, 'prognm' not set\n");
      exit (1);
    }
    if ((t = (char *)strrchr(prognm, '/')) != NULL)
      ++t ;				/* delete path from prognm */
    else t = prognm;

    s = (char *)malloc (strlen(configDir) + strlen(t) + strlen(extnm) + 12);
    sprintf(s, "%s/%s%s%s-confg", configDir, t, *extnm ? "-" : "", extnm);
  }
  return (s);
}

char *get_datafile()
{
  static char *s;

  if (dataFile)
    return (dataFile);
  if (!extnm)
    extnm = "";
  if (!s) 
  {
    char *t;

    if (!prognm || *prognm == '\0') {
      fprintf(stderr,
	      "FATAL get_datafile()- programming error, 'prognm' not set\n");
      exit (1);
    }
    if ((t = (char *)strrchr(prognm, '/')) != NULL)
      ++t ;				/* delete path from prognm */
    else t = prognm;

    s = (char *)malloc (strlen(dataDir) + strlen(t) + strlen(extnm) + 12);
    sprintf(s, "%s/%s%s%s-output", dataDir, t, *extnm ? "-" : "", extnm);
  }
  return (s);
}

char *get_pidfile()
{
  static char *s;
  if (!extnm)
    extnm = "";
  if (!s)
  {
    char *t;

    if (!prognm || *prognm == '\0') {
      fprintf(stderr,
	      "FATAL get_pidfile()- programming error, 'prognm' not set\n");
      exit (1);
    }
    if ((t = (char *)strrchr(prognm, '/')) != NULL)
      ++t ;				/* delete path from prognm */
    else t = prognm;

    s = (char *)malloc (strlen(pidDir) + strlen(t) + strlen(extnm) + 12);
    sprintf(s, "%s/%s%s%s.pid", pidDir, t, *extnm ? "-" : "", extnm);
  }
  return (s);
}

char **get_snips_loghosts()
{
  static char *snips_defaultHost = "127.0.0.1";
    
  if (snips_loghost[0] == NULL)
  {
    snips_loghost[0] = snips_defaultHost;
  }
    
    return snips_loghost;
}

/* the following two are needed by snipstv which need to access the
 * directories directly
 */
char *get_datadir()
{
  return (dataDir);
}

char *get_etcdir()
{
  return (configDir);
}

char *get_jsondir()
{
	static char path[256];
   strncpy(path, JSONDIR, strlen(JSONDIR));
   path[strlen(JSONDIR)] = '\0';
    printf("%s %X\r\n", path, path);
    return (char *)path;
}

/*
 * set_ routines to specify alternate config and data files instead of
 * the defaults.
 */
char *set_configfile(f)
  char *f;
{
  if (configFile) {
    free(configFile);
    configFile = NULL;
  }
  if (f)
    configFile = (char *)Strdup(f);

  return (configFile);
}
char *set_datafile(f)
  char *f;
{
  if (dataFile) {
    free(dataFile);
    dataFile = NULL;
  }
  if (f)
    dataFile = (char *)Strdup(f);

  return (dataFile);
}


/*	DATA_VERSION UTILITIES
 * Routines to write and extract the data version of the datafile.
 * NOCOL did not use any version numbers.
 * The version number is stored in the first EVENT record, which
 * is a null event. The version is stored in the first 8 bytes of
 * the file:
 *	\000  \0177  'SNIPS'  DATA_VERSION
 */
int write_dataversion(fd)
  int fd;
{
#if defined(DATA_VERSION) && defined(MAGIC_STRING)
  char magicstring[] = MAGIC_STRING;
  char  *p;
  EVENT v;

  if (debug > 1)
    fprintf(stderr, "Setting datafile format to version %d\n", DATA_VERSION);
  if (DATA_VERSION == 0) return (0);	/* no header in old snips */
  lseek(fd, (off_t)0, SEEK_SET);
  bzero (&v, sizeof(EVENT));
  p = (char *)&v;
  bcopy(magicstring, p, sizeof(magicstring));
  p[sizeof(magicstring)] = DATA_VERSION;
  write (fd, (char *)&v, sizeof(v));
  return (DATA_VERSION);
#else
  if (debug > 1)
    fprintf(stderr, "write_dataversion() - nothing written, old DATA_VERSION\n");
  return (0);
#endif
}

/*
 * If it finds the magic_string at the start of the file, it returns the
 * dataversion else it rewinds the file and returns 0
 * After calling this function, the 'fd' file descriptor is past the
 * first record which is the data version.
 */
int read_dataversion(fd)
  int fd;
{
#if defined(DATA_VERSION) && defined(MAGIC_STRING)
  int ver;
  EVENT v;

  bzero(&v, sizeof (v));
  lseek(fd, (off_t)0, SEEK_SET);
  if (read(fd, &v, sizeof(v)) <= 0) {
    if (debug)
      fprintf(stderr,
	      "debug (%s) read_dataversion()- read failed (non-fatal) %s\n",
	      prognm, (char *)strerror(errno));
    if (debug > 1)
    {
      struct stat stat_buf;
      if (fstat(fd, &stat_buf) < 0)
	fprintf(stderr, "  cannot fstat() file- %s\n", (char *)strerror(errno));
      else
	fprintf(stderr, "  file size = %ld bytes\n", stat_buf.st_size);
    }
    return (0);
  }
  ver = extract_dataversion(&v);
  if (debug > 2)
    fprintf(stderr, "read_dataversion() - version is %d\n", ver);
  if (ver != 0 )
    return (ver);
  else
    lseek(fd, -(off_t)sizeof(EVENT), SEEK_CUR);	/* rewind */
#endif
  if (debug > 2)
    fprintf(stderr, "read_dataversion() - DATA_VERSION undefined or 0\n");
  return (0);
}
/*
 * Extract the data version from an event structure
 */
int extract_dataversion(pv)
  EVENT *pv;
{
  int ver = 0;
#if defined(MAGIC_STRING)
  static char magicstring[] = MAGIC_STRING;
  char *s;

  s = (char *)pv;
  if (bcmp(magicstring, s, sizeof(magicstring)) == 0)
    ver = (int) *(s + sizeof(magicstring));
#endif
  return (ver);
}

/* Just return the dataversion defined in the include file */
int my_dataversion()
{
#ifdef DATA_VERSION
  return (DATA_VERSION);
#else
  return (0);
#endif
}

/*
 * do an open() followed by a read or write dataversion depending on if file
 * created (and we are at start of file).
 */
int open_datafile(filename, flags)
  char *filename;
  int flags;		/* normal open() flags */
{
  int fd;

  fd = open(filename, flags, DATAFILE_MODE);
  if (debug > 2)
    fprintf (stderr, "open_datafile(), mode = %d\n", DATAFILE_MODE);

  if (fd >= 0)
  {
    struct stat stat_buf;
    if (fstat(fd, &stat_buf) >= 0 && stat_buf.st_size == 0)
      write_dataversion(fd);	/* writes and positions after written record */
    else if ((DATAFILE_MODE & O_WRONLY) != O_WRONLY)
      read_dataversion(fd);	/* skips forward one record */
  }
  return (fd);
}

int close_datafile(fd)
  int fd;
{
  return (close(fd));
}

/*
 * Replacement for strerror() since sys_errlist has multiple prototype
 * definitions.
 */
#ifndef HAVE_STRERROR
const char *strerror(int err)
{
#if !defined(__FreeBSD__) && !defined(__bsdi__)
  extern const char *sys_errlist[];
#endif
  extern int sys_nerr;

  if (err < 0 || err >= sys_nerr) return "Unknown error";
  return sys_errlist[err];
}
#endif	/* HAVE_STRERROR */
