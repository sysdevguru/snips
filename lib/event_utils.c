/* $Header: /home/cvsroot/snips/lib/event_utils.c,v 1.3 2008/04/25 23:31:50 tvroon Exp $ */

/*
 * Routines to manipulate the 'event' structure. Also has routines
 * for the perl interface.
 *
 * AUTHOR:
 *	Vikas Aggarwal, vikas@navya.com
 */

/*
 * $Log: event_utils.c,v $
 * Revision 1.3  2008/04/25 23:31:50  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.2  2002/01/30 05:40:17  vikas
 * Updated calc_status() to handle equal thresholds.
 * Also fixed bug where it was returning the wrong threshold in warning.
 *
 * Revision 1.1  2001/08/01 23:27:30  vikas
 * tstr() needed to be static in event2strarray(), was causing problems
 * in the perl xs unpack_event() routine.
 *
 * Revision 1.0  2001/07/08 22:19:38  vikas
 * Lib routines for SNIPS
 *
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */


#include "snips.h"
#include "event_utils.h"
#include "eventlog.h"
#include <stdlib.h>
#include <string.h>

/*
 * Macro to escalate the severity of a device.
 */
#define ESC_SEVERITY(sev,maxs) (sev > maxs) ? (sev - 1) : maxs



/*
 * Convert EVENT structure to formatted ascii string.
 *
 * Changing this function might effect certain report generating
 * functions which depend on this format.
 *
 * SEE ALSO:   event2strarray() function below
 */
char *event_to_logstr(v)
  EVENT  *v ;
{
  register int  i ;
  static char  fmts[BUFSIZ];
  char states[32];
  char datestr[32] ;
  time_t locclock ;
  if (v == NULL)
    return ("\n");

  if (v->severity < E_CRITICAL || v->severity > E_INFO)
  {
    fprintf(stderr, 
	    "(event_to_logstr): Bad severity %d, setting to CRITICAL\n", 
	    v->severity);
    v->severity = E_CRITICAL ;
  }
  if (v->loglevel < E_CRITICAL || v->loglevel > E_INFO)
  {
    fprintf(stderr, 
	    "(event_to_logstr): Bad loglevel %d, setting to CRITICAL\n", 
	    v->loglevel);
    v->loglevel = E_CRITICAL ;
  }

  /*
   * Create a string from the states flag
   */
  *states = '\0';
  for (i=0 ; *(state_txt[i].str) != '\0' ; ++i)
    if (v->state & state_txt[i].v)
      strcat(strcat(states, state_txt[i].str), " ") ;

  if (*states)
    *(states + strlen(states) - 1) = '\0' ; 	/* strip trailing blank */

  locclock = (time_t)v->eventtime;
  sprintf (datestr, "%s", (char *)ctime(&locclock) );
  *(strchr(datestr, '\n')) = '\0' ;		/* delete the newline */

  /*
   * IF YOU CHANGE THIS LINE, CHANGE THE CORRESPONDING CODE IN
   * logstr_to_event()
   */

  if (v->id > 0)
    sprintf(fmts, "%s [%s] %lu: ", datestr, v->sender, v->id);
  else
    sprintf(fmts, "%s [%s]: ", datestr, v->sender);

  if (*(v->device.subdev)) {
	  sprintf(fmts + strlen(fmts),
		  "DEVICE %s%s%s %s VAR %s %ld %ld %s LEVEL %s LOGLEVEL %s STATE %s\n",
		  v->device.subdev, "+", 
		  v->device.name, v->device.addr,
		  v->var.name, v->var.value, v->var.threshold, v->var.units,
		  severity_txt[v->severity], severity_txt[v->loglevel],
		  states) ;
  } else {
	  sprintf(fmts + strlen(fmts),
		  "DEVICE %s%s%s %s VAR %s %ld %ld %s LEVEL %s LOGLEVEL %s STATE %s\n",
		  "", "",
		  v->device.name, v->device.addr,
		  v->var.name, v->var.value, v->var.threshold, v->var.units,
		  severity_txt[v->severity], severity_txt[v->loglevel],
		  states) ;
  }

  return(fmts) ;
}

/*
 * initialize basic fields in the event structure (esp. time)
 */
int init_event(pv)
  EVENT *pv;
{
  time_t clock ;			/* Careful, don't use 'long'	*/

  bzero (pv, sizeof(EVENT)) ;
  if (prognm)
    strncpy(pv->sender, prognm, sizeof(pv->sender) - 1);

  strncpy(pv->device.name, "NOTSET", sizeof(pv->device.name));
  strncpy(pv->device.addr, "NOTSET", sizeof(pv->device.addr));

  clock = time((time_t *)0);

  strncpy(pv->var.name, "NOTSET", sizeof(pv->var.name));
  strncpy(pv->var.units, "NOTSET", sizeof(pv->var.units));

  pv->eventtime = (u_long)clock;
  pv->polltime = (u_long)clock;

  pv->var.value = 0 ;
  pv->var.threshold = 30 ;		/* threshold not set here */
  pv->state = SETF_UPDOUN (pv->state, n_UNKNOWN); /* Set all to UNKNOWN */
  pv->severity = E_INFO ;
  pv->loglevel = E_INFO;

  pv->rating = 0xff;			/* start with OK */
  /* pv->id = secs since 2000, = time - 946702800 + increasing serial# */
  /* Should calculate in update_event */
  /* or YYMMDD + (hh * 60 + mm) + increasing number */

  return 0;
}	/* init_event() */
  
/*
 *      calc_status()
 * Useful to extract the status and maximum severity in monitors which
 * have 3 thresholds. Given the three thresholds and the value, it returns
 * the 'status, thres, maxseverity'.
 * Handle increasing or decreasing threshold values.
 * Adapted from the perl library version.
 */

int calc_status(val, warnt, errt, critt, crit_is_hi, pthres, pmaxseverity)
  u_long  val, warnt, errt, critt;     	/* the value and 3 thresholds */
  int  crit_is_hi;			/* 1 if increasing thresholds */
  u_long  *pthres;
  int *pmaxseverity;			/* Return values */

{
  int status = 1;		/* assume up */
  
  if (crit_is_hi == -1) {	/* set automatically */
    if (critt >= warnt)   crit_is_hi = 1;	/* higher value is critical */
    else   crit_is_hi = 0;		/* lower value is critical */
  }

  if ( (critt < warnt && crit_is_hi == 1) || (critt > warnt && crit_is_hi == 0) )
    fprintf(stderr,
	    "calc_status() - invalid test, cthres= %d, wthres= %d, test= %s\n",
	    critt, warnt, crit_is_hi == 0 ? "decreasing" : "increasing");

  if ( (crit_is_hi && val >= warnt) || (crit_is_hi == 0 && val <= warnt) ) 
    status = 0;          /* value exceeds/below warning threshold */

  if ( (crit_is_hi && val >= critt) || (crit_is_hi == 0 && val <= critt) ) 
  { 
    if (pthres) *pthres = critt;
    *pmaxseverity = E_CRITICAL;
  }
  else if ( (crit_is_hi && val >= errt) || (crit_is_hi == 0 && val <= errt) )
  { 
    if (pthres) *pthres = errt;
    *pmaxseverity = E_ERROR;
  }
  else if ( (crit_is_hi && val >= warnt ) || (crit_is_hi == 0 && val <= warnt ) )
  { 

    if (pthres) *pthres = warnt;
    *pmaxseverity = E_WARNING;
  }
  else    
  { 

    if (pthres) *pthres = warnt;
    *pmaxseverity = E_INFO;
  } /* status should be UP */

/*  if (status)
  {
  	status = 1;
  	if (pthres) *pthres = warnt;
    	*pmaxseverity = E_INFO;
  }
 */ 
  return status; 

}       /* end calc_status */

/*+
 * Given status (up/down), value and maxSeverity that this event
 * can be escalated to, this function updates the:
 *
 *	severity
 *	loglevel
 *	rating
 * 	time (only updated if severity changes)
 *
 * Finally, if this is a new change in severity, then the event is
 * logged at the 'worst' of current severity and last log-severity.
 */
int update_event(pv, status, value, thres, maxsev)
  EVENT *pv;
  int status;			/* device status */
  u_long value;			/* event value */
  u_long thres;			/* new threshold */
  int maxsev ;		       	/* max severity */
{
  int prevsev;			/* prev severity */
  time_t clock;
    
  clock = time((time_t *) NULL);

  prevsev = pv->severity;		/* save current severity */
  pv->var.value = (u_long)value ;	/* update value.. */
  pv->var.threshold = thres;		/* ..and threshold */
  pv->polltime = (u_long)clock;		/* update poll timestamp */

  if (status == 1)
  {		/* up */
    pv->eventtime = (u_long)clock;		/* update time always */
#ifndef NOKEEPSTATE
    if (!(pv->state & n_UP))		/* recent change of state */
    {
#endif
      pv->state = SETF_UPDOUN(pv->state, n_UP) ;
      pv->loglevel = pv->severity; 	/* log at earlier level */
      pv->severity = E_INFO ;		/* reset severity to INFO */
      eventlog(pv);
#ifndef NOKEEPSTATE
    }
#endif
    pv->loglevel = E_INFO;		/* reset after logging */
  }
  else
  {			/* down */
    pv->severity = ESC_SEVERITY(pv->severity, maxsev);	/* escalate */

#ifndef NOKEEPSTATE
    if (pv->severity != prevsev)		/* change in severity */
    {
#endif
      pv->eventtime = (u_long)clock;
      pv->state = SETF_UPDOUN (pv->state, n_DOWN);
      pv->loglevel = (pv->severity < prevsev) ? pv->severity : prevsev;
      eventlog(pv);
#ifndef NOKEEPSTATE
    }	/* if (change in severity) */
#endif
  }

  if (status == 0)
    pv->rating = (pv->rating) >> 1;		/* insert 0 at leftmost bit */
  else
    pv->rating = ((pv->rating) >> 1) | (~0177);	/* insert 1 at leftmost bit */

#ifdef RRDTOOL
  if (dorrd && !(pv->state & n_OLDDATA))
    snips_rrd(clock, pv->device.name, pv->device.addr, pv->device.subdev,
	      pv->var.name, pv->var.value, pv->severity, pv->sender);
#endif

  return 0;
}	/* update_event() */

/*
 * EVENT utilities
 */
int read_event(fd, pv)
  int fd;
  EVENT *pv;
{
  int readbytes;

  if ( (readbytes = read(fd, (char *)pv, sizeof(EVENT))) != sizeof(EVENT) )
    if (readbytes != 0)	/* not eof */
      perror("read_event() read");

  return (readbytes);
}

int read_n_events(fd, pv, n)
  int fd, n;
  EVENT *pv;	/* array of events */
{
  int readbytes;
  readbytes = read(fd, (char *)pv, n * sizeof(EVENT));
  if (readbytes < 0)
    perror("read_n_events() read");

  return (readbytes);
}

int write_event(fd, pv)
  int fd;
  EVENT *pv;
{
  int writebytes;

  if ((writebytes = write (fd, (char *)pv, sizeof(EVENT))) != sizeof(EVENT))
    perror("write_event() write");
  return (writebytes);
}

int write_n_events(fd, pv, n)
  int fd, n;
  EVENT *pv;	/* array of events */
{
  int writebytes;

  writebytes = write (fd, (char *)pv, n * sizeof(EVENT));
  if ( writebytes != (n * sizeof(EVENT)) )
    perror("write_n_events() write");
  return (writebytes);
}

/*
 * rewind and write event (used to overwrite after update)
 */
int rewrite_event(fd, pv)
  int fd;
  EVENT *pv;
{
  lseek(fd, -(off_t)sizeof(EVENT), SEEK_CUR);
  return (write_event(fd, pv));
}

int rewrite_n_events(fd, pv, n)
  int fd, n;
  EVENT *pv;		/* array of events */
{
  lseek(fd, -(off_t)(n * sizeof(EVENT)), SEEK_CUR);
  return (write_n_events(fd, pv, n));
}


/* To convert the SNIPS structure into a array */
static char *fieldnames[] = {
#define SENDER 0
    "sender",
#define DEVICE_NAME 1
    "device_name",
#define DEVICE_ADDR 2
    "device_addr",
#define DEVICE_SUBDEV 3
    "device_subdev",
#define VAR_NAME 4
    "var_name",
#define VAR_VALUE 5
    "var_value",
#define VAR_THRESHOLD 6
    "var_threshold",
#define VAR_UNITS 7
    "var_units",
#define SEVERITY 8
    "severity",
#define LOGLEVEL 9
    "loglevel",
#define STATE 10
    "state",
#define RATING 11
    "rating",
#define EVENTTIME 12
    "eventtime",
#define POLLTIME 13
    "polltime",
#define OP 14
    "op",
#define ID 15
    "id",
    ""		/* terminating null string */
};

/*+ event2strarray()
 * Converts the event structure into an array of character strings.
 * (mainly for using by perl interface to unpack() an EVENT structure).
 * If called with a null event, fills in the names of the fields in the
 * char array.
 *
 * SEE ALSO -  event2logstr()
 */
char **event2strarray(pv)
  EVENT *pv;
{
  static char *ea[ID + 1];
  static char tstr[ID + 1][64];		/* temp string storage */

  if (pv == NULL)
    return (fieldnames);

  bzero (ea, sizeof(ea));
  bzero (tstr, sizeof(tstr));

  ea[SENDER] = pv->sender;
  ea[DEVICE_NAME] = pv->device.name;
  ea[DEVICE_ADDR] = pv->device.addr;
  ea[DEVICE_SUBDEV] = pv->device.subdev;

  ea[VAR_NAME] = pv->var.name;
  ea[VAR_VALUE] = &(tstr[VAR_VALUE][0]);
  sprintf(ea[VAR_VALUE], "%lu", pv->var.value);
  ea[VAR_THRESHOLD] = &(tstr[VAR_THRESHOLD][0]);
  sprintf(ea[VAR_THRESHOLD], "%lu", pv->var.threshold);
  ea[VAR_UNITS] = pv->var.units;

  ea[SEVERITY] = &(tstr[SEVERITY][0]);
  sprintf(ea[SEVERITY], "%d", (int)pv->severity);
  ea[LOGLEVEL] = &(tstr[LOGLEVEL][0]);
  sprintf(ea[LOGLEVEL], "%d", (int)pv->loglevel);
  ea[STATE] = &(tstr[STATE][0]);
  sprintf(ea[STATE], "%d", (int)pv->state);

  ea[RATING] = &(tstr[RATING][0]);
  sprintf(ea[RATING], "%d", (int)pv->rating);

  ea[EVENTTIME] = &(tstr[EVENTTIME][0]);
  sprintf(ea[EVENTTIME], "%lu", (u_long)(pv->eventtime));
  ea[POLLTIME] = &(tstr[POLLTIME][0]);
  sprintf(ea[POLLTIME], "%lu", (u_long)(pv->polltime));

  ea[OP] = &(tstr[OP][0]);
  sprintf(ea[OP], "%lu", (u_long)pv->op);
  ea[ID] = &(tstr[ID][0]);
  sprintf(ea[ID], "%lu", (u_long)pv->id);

#ifdef DEBUG
  if (debug > 2)
  {
    int i;
    for (i = 0; i <= ID; ++i)
      fprintf(stderr, "(debug) event2strarray() %s = %s\n",
	      fieldnames[i], ea[i]);
  }
#endif

  return (ea);		/* return event as a char array */

}	/* event2strarray() */

/*
 * Convert an array of strings into an 'event' structure.
 * The array of strings is in the same order as 'fieldnames' above.
 * This routine is mainly for perl functionality (pack %event).
 */
EVENT *strarray2event(sa)
  char *sa[];
{
  static EVENT v;

#define DOSTRCPY(A,B) strncpy(A, sa[B], sizeof(A))

  DOSTRCPY(v.sender, SENDER);
  DOSTRCPY(v.device.name, DEVICE_NAME);
  DOSTRCPY(v.device.addr, DEVICE_ADDR);
  DOSTRCPY(v.device.subdev, DEVICE_SUBDEV);

  DOSTRCPY(v.var.name, VAR_NAME);
  DOSTRCPY(v.var.units, VAR_UNITS);
  v.var.value = strtoul(sa[VAR_VALUE], (char **)NULL, 10);
  v.var.threshold = strtoul(sa[VAR_THRESHOLD], (char **)NULL, 10);

  v.severity = atoi(sa[SEVERITY]);
  v.loglevel = atoi(sa[LOGLEVEL]);
  v.state = atoi(sa[STATE]);
  v.rating = atoi(sa[RATING]);

  v.eventtime = strtoul(sa[EVENTTIME], (char **)NULL, 10);
  v.polltime = strtoul(sa[POLLTIME], (char **)NULL, 10);
  v.op = strtoul(sa[OP], (char **)NULL, 10);
  v.id = strtoul(sa[ID], (char **)NULL, 10);

#undef DOSTRCPY
  return (&v);
}
