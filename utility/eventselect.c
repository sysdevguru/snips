/*
 * $Header: /home/cvsroot/snips/utility/eventselect.c,v 1.1 2008/04/25 23:31:53 tvroon Exp $
 */

/*
 * Parse the SNIPS raw data files and print out (in ascii) the events that
 * match the parameters specified on the command line.
 *
 * These parameters are:
 *	severity, monitor-name, device, var, from-date, to-date
 *
 * A 'substring' match is performed (i.e. strstr is used, not strcmp)
 * Can specify multiple devices (any will match).
 *
 * Can read from the stdin also.
 *
 */

/*
 * $Log: eventselect.c,v $
 * Revision 1.1  2008/04/25 23:31:53  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 04:12:47  vikas
 * Utility programs for SNIPS
 *
 */

#ifndef lint
static char rcsid[] = "$Id: eventselect.c,v 1.1 2008/04/25 23:31:53 tvroon Exp $";
#endif

#define _MAIN_
#include "snips.h"
#include "snips_funcs.h"
#include "eventlog.h"
#include "event_utils.h"
#undef _MAIN_

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>		/* for read() */
#include <unistd.h>		/* for read() */
#include <time.h>
#include <string.h>
#include <sys/time.h>


static int minseverity = E_INFO;
static int logevents = 0;
static char *(device[32]), *(sender[32]), *(var[32]);	/* ptrs to strings */
static time_t fromsecs, tosecs;
static int ndevice, nvar, nsender;

/* function prototypes */
int eventselect(char *dfile);
int cmpstrlist(char *strlist[], char *str);
int help();

int main(ac, av)
  int ac;
  char **av;
{
  extern int  optind;
  extern char *optarg;
  char c;
  time_t cursecs = time(NULL);

  tosecs = cursecs;		/* init to current time */
  prognm = av[0];

  while ( (c = getopt(ac, av, "dlv:V:s:S:f:t:")) != EOF)
    switch(c)
    {
    case 'd':
      ++debug; break;
    case 'l':
      ++logevents; break;
    case 'v':		/* severity */
      switch(tolower(optarg[0]))
      {
      case 'c': case '1': minseverity = E_CRITICAL; break;
      case 'e': case '2': minseverity = E_ERROR; break;
      case 'w': case '3': minseverity = E_WARNING; break;
      case 'i': case '4': minseverity = E_INFO; break;
      }
      break;
    case 'V':	/* var name */
      var[nvar++] = optarg;  break;
    case 's':	/* sender */
      sender[nsender++] = optarg;  break;
    case 'S':	/* device name/addr */
      device[ndevice++] = optarg;  break;
    case 'f':	/* fromtime */
      if (*optarg == '+')	/* specified in delta from current time */
	fromsecs = cursecs - atoi(++optarg);
      else
	fromsecs = (time_t)get_date(optarg, NULL);
      fromsecs = (fromsecs < 0) ? 0 : fromsecs;
      break;
    case 't':
      if (*optarg == '+')	/* specified in delta from current time */
	tosecs = cursecs - atoi(++optarg);
      else
	tosecs = (time_t)get_date(optarg, NULL);
      break;
    default:
      fprintf(stderr, "%s: Invalid flag %c\n", prognm, optarg);
      help();
      exit(1);
    }
  if (tosecs <= fromsecs)
  {
    fprintf(stderr, "Given end time '%24.24s' ", ctime(&tosecs));
    fprintf(stderr, "is less than start time '%24.24s'\n", ctime(&fromsecs));
    tosecs = cursecs + 3600;
    fprintf(stderr, "Resetting end time to '%24.24s'\n", ctime(&tosecs));
  }

  sender[nsender] = NULL; var[nvar] = NULL; device[ndevice] = NULL;

  if (debug > 1)
  {
    fprintf(stderr, "%s (debug): Selecting severity%s from %24.24s\n",
	    prognm, severity_txt[minseverity], ctime(&fromsecs));
    fprintf(stderr, "\t to %24.24s for %d variables, %d devices, %d sender\n",
	    ctime(&tosecs), nvar, ndevice, nsender);
  }

  if (optind >= ac)
  {
    if (debug)
      fprintf(stderr, "%s: Reading input from stdin\n", prognm);
    eventselect(NULL);		/* standard input */
  }
  else
    for( ; optind < ac ; ++optind)
    {
      if (debug)
	fprintf(stderr, "PROCESSING FILE %s\n", av[optind]);
      eventselect(av[optind]);
    }
  return(0);
}	/* end main() */

int eventselect(dfile)
  char *dfile;		/* data file name */
{
  EVENT nullevt, evt;
  int fd;
  int nread;
  int ver;

  if (dfile == NULL)
    fd = fileno(stdin);
  else if ( (fd= open(dfile, O_RDONLY)) < 0)
  {
    fprintf(stderr, "%s: error opening datafile %s- %s",
	    prognm, dfile, (char *)strerror(errno));
    return(-1);
  }
  if (dfile && (ver = read_dataversion(fd)) != DATA_VERSION)
  {
    fprintf(stderr, "%s: %s data format version mismatch (is %d not %d)\n",
	    prognm, dfile, ver, DATA_VERSION);
    return (-1);
  }

  bzero(&nullevt, sizeof(nullevt));
  while ( (nread = read(fd, &evt, sizeof(evt))) == sizeof(evt))
    if (bcmp(&evt, &nullevt, sizeof(evt)) != 0)	/* not NULL */
    {
      if (evt.severity <= minseverity)
	if (cmpstrlist(device, evt.device.name) == 1 ||
	    cmpstrlist(device, evt.device.addr) == 1)
	  if (cmpstrlist(sender, evt.sender) == 1)
	    if (cmpstrlist(var, evt.var.name) == 1)
	      if (evt.eventtime >= fromsecs && evt.eventtime <= tosecs)
	      {
		if (logevents)
		  eventlog(&evt);
		else
		  printf("%s", event_to_logstr(&evt));
	      }
    }

  if (nread < 0)
    fprintf(stderr, "%s: %s read() error- %s\n", prognm, dfile,
	    (char *)strerror(errno) );

  close(fd);
  return(0);
}		/* eventselect() */

/*
 * See if 'str' exists anywhere in the arry of strings 'strlist'.
 * If array of string is null, returns 1 (OK).
 * Return 1 if ok, 0 if not.
 */
int cmpstrlist(strlist, str)
  char *strlist[];
  char *str;
{
  register char *p;
  register int i;

  if (!strlist || *strlist == NULL || str == NULL || *str == '\0')
    return(1);

  for (p = str; *p; ++p)
    *p = tolower(*p);		/* lowercase the event string */

  for (i = 0; strlist[i] != NULL; ++i)
    if ((char *)strstr(strlist[i], str) != NULL)
      return(1);

  return(0);
}

int help()
{
      fprintf(stderr, "Valid Options are:\n\
\t-d \t enable debug \n\
\t-l \t to log events to snipslogd \n\
\t-v <severity> \t Minimum severity of event Crit/Err/Warn/Info \n\
\t-V <varname> \t Variable name to match \n\
\t-s <sender>  \t Monitor name\n\
\t-S <device name> \t Device name\n\
\t-f <from time> \t e.g. (Tue Jul 22 11:46:06 1997)\n\
\t-t <end time> \t same format as from time\n\
\n\
  The from/to time can also be specified as an offset in seconds from curtime\n\
  such as +300 \n\
  If multiple 'varnames' etc. are specifed, they are 'ORed'\n\
  If both 'varname' and 'devicename' etc. are specifed, they are 'ANDed'\n");
      fprintf(stderr, "\n");
}
