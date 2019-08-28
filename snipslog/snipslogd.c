/* $Header: /home/cvsroot/snips/snipslog/snipslogd.c,v 1.3 2008/04/25 23:31:52 tvroon Exp $ */

/*
 * Recieve EVENT structures on a UDP port and write or pipe the events
 * to files or programs listed in the config file (similar to syslog).
 * Converts the event structure to ascii using the 'event_to_logstr()'
 * library call.
 *
 * BUGS
 * 	1. Needs to use ntoa and aton functions while converting
 *	   integers to handle mixed endian.
 *	2. Can get buffer overruns if running on a slow machine and
 *	   gets too many packets too fast.
 */

/*
 * $Log: snipslogd.c,v $
 * Revision 1.3  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.2  2002/01/28 23:39:59  vikas
 * Added 'HUP' word in the info message.
 *
 * Revision 1.1  2001/07/28 01:44:26  vikas
 * - Now handles mixed endian (converts network to host endian). Patch
 *   sent in by marya@st.jip.co.jp
 * - Calls standalone() again only if in daemon mode, else was killing
 *   itself if ran in debug mode.
 *
 * Revision 1.0  2001/07/09 03:16:39  vikas
 * For SNIPS v1.0
 *
 */

/* Copyright Netplex Technologies Inc. 2000 */

#define _MAIN_
#include "snipslogd.h"
#undef _MAIN_

#include "snips_funcs.h"
#include "event_utils.h"
#include "eventlog.h"
#include "daemon.h"

typedef struct node_s {
  char		*sender;	/* for comparing EVENT.sender */
  char		*path;		/* Filename in char format */
  FILE		*stream;	/* Opened stream */
  int		isapipe;	/* True if output is a pipe */
  int		loglevel ;   	/* Log level for this stream */
  struct node_s	*next;		/* Linked list */
} node;

static node	*loglist=NULL;
static int	mypid=0;
static char	*configfile, *errfile ;
static int	sighupflag=0, alarmflag=0 ;
static unsigned long  permithosts[50] ;		/* list of hosts */

/* function prototypes */
int authenticate_host(struct sockaddr_in *peer);
void fatal(char *msg);
void readconfig();
int readevent(int fd);
void serve();
void sighuphandler(), sigalrmhandler();
void warning_int(char *format, int num);

int main(ac, av)
  int ac;
  char **av;
{
  extern char	*optarg;
  extern int	optind;
  int	c ;

  errno = 0;
  /* Save the program name */
  if ((prognm = (char *)strrchr (av[0], '/')) == NULL)
    prognm = av[0] ; 			/* no path in program name */
  else
    prognm++ ;                                /* skip leading '/' */

  /* Parse the command line arguments; does no error checking */

  while ((c = getopt(ac, av, "de:f:p:")) != EOF)
    switch(c)
    {
    case 'd':
      debug++;
      break;
    case 'e':
      errfile = optarg ;
      break ;
    case 'f':
      configfile = optarg ;
      break ;
    case '?':
    default:
      fprintf(stderr, "%s: Unknown flag: %c\n", prognm, optarg);
      fprintf(stderr, "Usage: %s [-d] [-f <config file>] ", prognm);
      fprintf(stderr, "[-e <error filename>]\n");
      exit (1);
    }

  snips_startup();
  fprintf(stderr,"%s (info): Loghost set to be \n",
          prognm ) ;
    
    char **hosts = get_snips_loghosts();
    
    while ( *hosts)
    {
        fprintf(stderr, "%s ", hosts);
        hosts++;
    }
    fprintf(stderr,
            "\n"
            ) ;
  fprintf(stderr,"Make sure this logging daemon is running on the above host\n\n");

  if (errfile == NULL)		/* need this before daemon call */
  {
    errfile = (char *)malloc(strlen(PIDDIR) + strlen(prognm) + 32);
    sprintf(errfile, "%s/%s.error", PIDDIR, prognm);
  }
		     
  if (debug)
  {
    fprintf(stderr, "%s: DEBUG  ON, server not daemon-izing\n",prognm);
#ifndef DEBUG
    fprintf(stderr,"%s: WARNING- program NOT compiled with DEBUG option\n",
	    prognm);
#endif
  }
  else		/* Become a daemon */
  {
    int errfd;

    Daemon(NULL);
    mypid = getpid();
    /* close and open the stderr right after the Daemon call */
    close(2);
    if ((errfd = open(errfile, O_CREAT|O_APPEND|O_WRONLY, 0644)) < 0)
      fatal(errfile);
    if (errfd != 2) {
      dup2(errfd, 2);
      close (errfd);
    }

    standalone((char *)get_pidfile());	/* save new pid after forking */

  }	/* if (debug) */

  if (configfile == NULL)
    configfile = (char *)get_configfile();

  if (debug)
    fprintf(stderr, "  configfile= '%s', errfile= '%s'\n\n",
	    configfile, errfile);

  /* Read the configuration file; good idea to do this after daemon() call */
  readconfig();

  /* Set up the signal handler */
  sighupflag = 0;
  (void) signal(SIGHUP, sighuphandler);

    /* And do all the socket stuff - this never returns */
  serve();

  /*NOTREACHED*/
  return(0);
}

/*
 * Opens a UDP logging socket, binds to it, and selects on it
 * until some data appears.  Then processes all data that comes
 * in and logs it into the appropriate file.  It never returns,
 * but dies on error.
 */
void serve()
{
  int			inetfd, nfds,  sockbuffsize, optlen ;
  struct sockaddr_in	sin;
  struct servent	*sp;
  fd_set		readfds;

  /*
     * Open the network logging socket
     */
  if ((inetfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    fatal("serve(): network socket() failed");

  optlen = 0;
  getsockopt(inetfd, /* level */SOL_SOCKET, /* optname */SO_RCVBUF, 
	     (char *)&sockbuffsize, &optlen);
#ifdef DEBUG
  if (debug)
    fprintf(stderr, "(debug) old inetfd RCVBUF size= %d\n", sockbuffsize);
#endif
  if (sockbuffsize < 65534)
  {
    sockbuffsize = 65534 ;
    setsockopt(inetfd, /* level */ SOL_SOCKET, /* optname */SO_RCVBUF, 
	       (char *)&sockbuffsize, sizeof sockbuffsize );
  }
  if (debug)
  {
    optlen = 0 ;
    getsockopt(inetfd, SOL_SOCKET, SO_RCVBUF, (char *)&sockbuffsize, 
	       &optlen);
    fprintf(stderr, 
	    "(debug) inetfd RCVBUF size set to %d\n", sockbuffsize);
  }

  bzero((char *) &sin, sizeof (sin)) ;	/* important */
  sin.sin_family = AF_INET;
  /* Figure out what port number to use */
  if ((sp = getservbyname(SNIPSLOG_SERVICE, "udp")) == NULL)
    sp = getservbyname(NLOG_SERVICE, "udp");
  if (sp == NULL)
    sin.sin_port = htons(SNIPSLOG_PORT);
  else
    sin.sin_port = sp->s_port;

    /* And do the bind() */
  if (bind(inetfd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
    fatal("serve(): couldn't bind to network socket");

  if(debug)
    fprintf(stderr, "  INET socket fd= %d\n", inetfd);

    /* Start waiting for log messages */
  for (;;) {
    if (sighupflag)
    {
      fprintf(stderr,
	      "%s (%d): Closing streams & re-reading config file on HUP signal\n",
	      prognm, mypid);
      readconfig();
      sighupflag = 0;
    }

    /* We're gonna check this after the select, so clear it now... */
    errno = 0;

    FD_ZERO(&readfds);
    FD_SET(inetfd, &readfds);
    nfds = select(FD_SETSIZE, &readfds, (fd_set *) NULL, (fd_set *) NULL,
		  (struct timeval *) NULL);

    if (errno == EINTR)		/* got a SIGHUP */
      continue;
    else if (nfds <= 0)
      fatal("serve(): select() failed");

    if (FD_ISSET(inetfd, &readfds))
      readevent(inetfd);
  }
  /*NOTREACHED*/
}


/*
 * Reads in one EVENT structure and logs it to the appropriate stream,
 * based on the contents of loglist.  Readconfig() must have been
 * called first.  Dies upon error.
 */
int readevent(fd)
  int fd;			/* socket file desc */
{
  register char *r, *s;
  int		n, len ;
  EVENT	v;
  char	from[sizeof(v.sender)+1]; 
  register node	*p;
  struct sockaddr_in	frominet;
  char *senderIPString;
  char fullLogStr[1024];
  len = sizeof (frominet);

    /* Try to read an EVENT structure in */
  signal(SIGALRM, sigalrmhandler);
  alarmflag = 0 ;
  alarm(READ_TIMEOUT*10);

  n = recvfrom(fd, (char *)&v, sizeof(v), 0, 
	       (struct sockaddr *)&frominet, &len );
  if (n != sizeof(v))
  {
    if (alarmflag)
      fprintf(stderr, "%s readevent: socket read timed out\n", prognm);
    else
      fprintf(stderr,"%s readevent: socket read failed (incomplete)-- %s",
	      prognm, (char *)strerror(errno) );

    alarm(0);	/* turn alarm off */
    return(1) ;	/* go back to waiting */
  }
  senderIPString = inet_ntoa(frominet.sin_addr);
  signal(SIGALRM, SIG_IGN);		/* ignore alarm */
  alarm(0) ;

  ntohevent(&v, &v);			/* network to host endian */

  /* make sure that evnetime got some value*/
  if (!v.eventtime)
  	v.eventtime = time(0);
  /**/
  	
#ifdef DEBUG
  if (debug)
    fprintf(stderr, "Recvd packet, sender '%s', loglevel '%d'\n",
	    v.sender, v.loglevel);
#endif

  /*
     * The rest of this should be fast, perhaps done in a forked child ?
     */
  if (! authenticate_host(&frominet))
    return(1) ;	      	/* illegal host */

    /* Make sure things aren't case sensitive */
  for (r=v.sender, s=from; *r; )
    *s++ = tolower(*r++);
  *s = '\0';

  /*
     * Check the linked list and log it to all applicable streams.
     * For speed, check,,,
     * 		log level first
     *		sender (takes longer since it is a string cmp)
     * Log to all streams at priority EVENT.loglevel and higher.
     */
  for (p=loglist; p; p=p->next)
    if ((int)v.loglevel <= p->loglevel)
      if (strcmp(p->sender, "*") == 0 || strstr(from, p->sender))
      {
	/* Stream closed, try opening it.*/
	if (!p->stream)
	{
#ifdef DEBUG
	  if (debug)
	    fprintf(stderr, "Opening stream '%s'\n", p->path);
#endif

	  if (p->isapipe)
	    p->stream = popen(p->path, "w");
	  else
	    p->stream = fopen(p->path, "a");

	  if (! p->stream)   /* if the stream could not be reopened  */
	  {
	    fprintf(stderr, "%s: Could not open stream `%s'- ",
		    prognm, p->path);
	    perror ("");
	    fflush(stderr);
	    continue ;
	  }
	}
        memset(fullLogStr, 0x00, sizeof(fullLogStr));
        v.var.value = v.var.value/1000.0;
		
        char *logStr = event_to_logstr(&v);
        if (logStr) {
            logStr[strlen(logStr) - 1] = '\0';
            sprintf(fullLogStr, "%s SOURCE %s\n", logStr, senderIPString); 
        }
 
	if (fputs((char *)fullLogStr, p->stream) == EOF) /* error */
	{
	  perror(p->path);
	  fflush(stderr) ;
	  p->isapipe ?  fclose(p->stream) : pclose(p->stream);
	  p->stream = NULL ;	/* try reopening next time */
	  continue ;		/* Next for() */
	}
	    
	fflush(p->stream);
	    
	if (p->isapipe)		/* close and reopen every time ? */
	{
	  pclose(p->stream);
	  p->stream = NULL;	/* reset */
	}
	    
	    
      }	/*  end: if (stream closed, try reopening it) */

    return(0);
}	/* end readevent() */


/*+ 
 * Authenticate hosts. Check to see if the sender has an IP address in
 * the list from the config file. The argument is the socket file
 * descriptor which *must* be of the INET type - not a Unix type.
 *
 *	A = MASK & A
 *
 * Return 1 if OKAY, 0 if not
 */
int authenticate_host(peer)
  struct sockaddr_in *peer ;	/* typecast from sockaddr to sockaddr_in  */
{
  struct sockaddr_in  s ;
  int i ;

  if (peer == NULL)
    return (0);

  for (i = 0; permithosts[i] != 0 ; ++i)
  {
    if (permithosts[i] == (unsigned long) peer->sin_addr.s_addr)
      return(1) ;
#ifdef DEBUG
    if (debug > 1)
      fprintf(stderr, " (debug) %lu != %lu\n",
	      peer->sin_addr.s_addr,permithosts[i]);
#endif
  }

  fprintf(stderr, "%s: Permission denied for host %s\n", 
	  prognm, inet_ntoa(peer->sin_addr));
  return (0);
}


/*+
 * Allocates space for a new node, initializes it's fields, and
 * inserts it into the beginning of loglist (since it is easier).
 * Dies upon error.
 */
node  *insert(sender, path)
  char *sender, *path;
{
  node	*new;
  int		i;
  char	*p;

  new = (node *) malloc(sizeof(node));
  if (new == NULL)
    fatal("insert(): out of memory");
  bzero(new, sizeof(*new));

  new->next = loglist;
  new->sender = (char *) malloc(strlen(sender)+1);
  new->path = (char *) malloc(strlen(path)+1);
  if (new->sender == NULL  ||  new->path == NULL)
    fatal("insert(): out of memory using malloc");
  for (p=new->sender; *sender; sender++)
    *p++ = tolower(*sender);
  *p++ = '\0';

  if (*path == '|') /* pipe */
  {
    char *tstr;

    strcpy(new->path, path+1), new->isapipe = 1 ;
    for (tstr = new->path; *tstr ;tstr++)
      if (*tstr == '^')  /* replace all ^ to spaces to permit args -aad */
	*tstr = ' ';
  }
  else
    strcpy(new->path, path);

  loglist = new;	/* point to the new node */
  return(new);
}

/*
 * Reads in a configuration file and opens the log files named there
 * for output.  Also writes to pipes instead of a log file if requested.
 *
 *		PERMITHOSTS   addr  addr  addr
 *		EVENT.sender  min-log-severity  Log-filename
 *		EVENT.sender  min-log-severity	|execute-filename
 *
 * Dies upon failure.
 */
void readconfig()
{
  char	line[1024], *progstr, *sevstr, *filestr, *p, *q;
  FILE	*config;
  int	level, linenum, i;
  node	*new;

  if (loglist)
  {
    register node *p, *q;
    for (p = loglist; p; p=q)
    {
      q = p->next;
      if (p->stream)
      {
	fflush(p->stream);
	p->isapipe ? pclose(p->stream) : fclose(p->stream);
      }
      if (p->sender != NULL)
	free(p->sender);
      if (p->path != NULL)
	free(p->path);
      free(p);
    }
    loglist = NULL;
  }

  if ((config = fopen(configfile, "r")) == NULL)
  {
    fprintf (stderr, "%s: ERROR could not open confile file '%s'- ", 
	     prognm, configfile);
    perror("fopen()");
    snips_done();
    exit (1);
  }

  i = 0;
  for (linenum=0; fgets(line, sizeof(line)-1, config) != NULL; linenum++)
  {
    line[strlen(line)-1] = '\0';	/* Strip off '\n' */

    if ((p=strchr(line, '#')) != NULL)	/* Get rid of comments */
      *p = '\0';

    /* Parse the line into fields */
    if (*line == '\0'  ||  (progstr=strtok(line, " \t")) == NULL)
      continue;
    else if (strncasecmp("PERMITHOST", line, strlen("PERMITHOST")) == 0)
    {
      while ((q = strtok(NULL, " \t")) != NULL)
      {
	permithosts[i++] = inet_addr(q);
	if (debug)
	  fprintf(stderr, "Added %s to permithosts list\n", q);
      } /* end while() */
      continue ;
    }
    else if ((sevstr=strtok(NULL, " \t")) == NULL) {
      warning_int("config line %d: no severity string, ignoring...\n",
		  linenum);
      continue;
    }
    else if ((filestr=strtok(NULL, " \t")) == NULL) {
      warning_int("config line %d: no logfile, ignoring...\n", linenum);
      continue;
    }

    /* Figure out what severity we're at */
    level = -1;
    switch(tolower(sevstr[0])) {
    case 'c': case '1': level = E_CRITICAL; break;
    case 'e': case '2': level = E_ERROR; break;
    case 'w': case '3': level = E_WARNING; break;
    case 'i': case '4': level = E_INFO; break;
    }
    if (level == -1) {
      warning_int("config line %d: bad severity, ignoring...\n", linenum);
      continue;
    }
    /* Create and insert a new node into the linked list */
    new = insert(progstr, filestr);
    new->loglevel  = level ;
  }

  fclose(config);		/* close config file */
  permithosts[i] = 0 ;	/* null the last one */
}

/*
 * Outputs an error message to stderr.  Records the daemon's name and
 * PID, if available.  Prints out the error message corresponding to
 * errno, if errno is set.  Exits with return value 1.
 */
void fatal(msg)
  char *msg;
{
  if (prognm)
    fprintf(stderr, "%s: fatal error. ", prognm);
  if (mypid)
    fprintf(stderr, "(%d) ", mypid);
  if (errno)
    perror(msg);
  else
    fprintf(stderr, "%s\n", msg);

  snips_done();		/* deletes pid file */
  exit(1);
}

/*
 * Outputs a warning message to stderr.  Records the daemon's name and
 * PID, if available.  Prints out the error message corresponding to
 * errno, if errno is set.  Does not exit.
 *
 * Assumes that 'format' is a valid fprintf format string that requires
 * only one argument, the integer 'num'.
 */
void warning_int(format, num)
  char *format;
  int num;
{
  if (prognm)
    fprintf(stderr, "%s: ", prognm);
  if (mypid)
    fprintf(stderr, "(%d) ", mypid);
  fprintf(stderr, format, num);
  if (errno)
    perror((char *) NULL);
  else
    fprintf(stderr, "\n");
}

/*
 * Sets the sighupflag whenever we get a SIGHUP signal.
 */
void sighuphandler()
{
  sighupflag = 1;
}

void sigalrmhandler()
{
  return;
}
