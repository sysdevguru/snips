/*
 * $Header: /home/cvsroot/snips/snipstv/update_eventwin.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*+
 * Update the EVENT window. Fill with datafiles from the data directory.
 *
 * Returns:
 *	0 if filled screen
 *	> 0 if end of all datafiles
 *	< 0 if error
 *
 */

/*
 * $Log: update_eventwin.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc.,  info@netplex-tech.com */

#ifndef lint
  static char rcsid[] = "$Id: update_eventwin.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>	/* for fstat() */
#include <unistd.h>
#include <string.h>
#include <curses.h>

#include <sys/param.h>

#include "snips.h"		/* for EVENT structure */
#include "snips_funcs.h"
#include "event_utils.h"
#include "eventlog.h"
#include "snipstv.h"
#include "do_filter.h"

#ifndef MAXPATHLEN
# define MAXPATHLEN 256
#endif

static int prevcritical, curcritical;	/* keep track of total critical */

/* function prototypes */
void Beep();
int printEvent(WINDOW *win, EVENT *pevt);

int update_eventwin(win)
  WINDOW *win;
{
  int bytesread, nrows;
  static int	dataversion;
  static int fd = -1;
  char datafile[MAXPATHLEN];
  static DIR  *dir;
  EVENT evt;

  wmove(win, 0, 0);		/* to beginning of window */
#ifdef __NetBSD__
  nrows = getmaxy(win);
#else
  nrows = (win->_maxy);		/* Dont use +1, leave 1 blank line at bottom */
#endif

  if (!dir)
  {
    if (!datadir) {
      waddstr(MsgPanel.win, "\nData directory is NULL");
      return (-1);
    }
    if ((dir = opendir(datadir)) == NULL) {
      wprintw(MsgPanel.win, "\nCould not open data directory '%s'- %s",
	      datadir, (char *)strerror(errno));
      return -1;
    }
  }

  while (nrows)
  {		/* until we write out nrows events */
    if (fd < 0)
    {	/* if no open file */
      struct dirent *d;

      while ((d = readdir(dir)))
      {
	if (*(d->d_name) != '.' && strcmp(d->d_name, "core") != 0)
	{	/* ignore files beginning with a dot or core files */
	  struct stat filestat;

	  sprintf(datafile, "%s/%s", datadir, d->d_name);
	  if (stat(datafile, &filestat) < 0)
	    continue;		/* next file */
#ifdef S_ISREG
	  if (S_ISREG(filestat.st_mode))	/* check if regular file */
#else
	  if (filestat.st_mode & S_IFMT == S_IFREG)
#endif
	    if ( (fd= open_datafile(datafile, O_RDONLY)) >= 0 )
	    {
	      if (debug)
		wprintw(MsgPanel.win,"\nOpened datafile- %s", datafile);
	      dataversion = read_dataversion(fd);
	      if (dataversion != DATA_VERSION)	/* mismatch */
	      {
		wprintw(MsgPanel.win,"\n%s Data file format mismatch (is %d not %d)",
			datafile, dataversion, DATA_VERSION);
		close_datafile(fd);
		fd = -1;
		continue;
	      }
	      break;	/* out of while(readdir) loop */
	    }	/* if open_datafile() */
	}	/* if (ignore . files) */
      }	/* while readdir() */

      if (!d)	/* end of all the files in the directory */
      {
	closedir(dir);
	dir = NULL;
	if (debug) waddstr(MsgPanel.win,"\nFinished all datafiles");
	goto done;	/* break out of the outer loop */
      }

    }	/* endif (!file) */

    if ( (bytesread = read_event(fd, &evt)) != sizeof(evt) )
    {
      if (bytesread)
	wprintw(MsgPanel.win,"\nError fread(%s)- %s",
		datafile, (char *)strerror(errno));

      close_datafile(fd);
      fd = -1;		/* reset so loop opens the next file */
      continue;
    }

    if (evt.sender[0] == '\0' && evt.device.name[0] == '\0' &&
	evt.device.addr[0] == '\0')
      continue;

    if (printEvent(win, &evt) == 1)
      --nrows ;
  }	/* end while(nrows) */

 done:
  if (nrows)	/* end of data files */
  {
    wclrtoeol(win);		/* clean out previous line */
#ifdef __NetBSD__
    mvwaddstr(win, getcury(win), (int)(COLS/2 - 3), "--X--");	/* */
#else
    mvwaddstr(win, win->_cury, (int)(COLS/2 - 3), "--X--");	/* */
#endif
    wclrtobot(win);		/* clear window till bottom */
    if (!belloff)
      Beep();
    maxpageno = pageno;		/* display approx max pages */
    pageno = 0;		/* displayed in the next pass as 1 */
    prevcritical = curcritical;	/* save prev value */
    curcritical = 0;
  }
  ++pageno;

#if defined(ACS_VLINE) && defined(ACS_HLINE)
/*  box(win, 0, 0);	*/	/* steals one char width all around */
#endif
  return (nrows);		/* 0 if screen is full, +ve if end of data */
}

/*
 * print out the EVENT line using selected 'widescreen' format.
 * Return 1 if printed out,  or 0 if not printed out.
 */
int printEvent(win, pevt)
  WINDOW *win;
  EVENT *pevt;
{
  char state[16];
  char tstr[128];
  extern bool belloff, silent;
  extern int displevel, widescreen;
  extern struct tm *evtm;
  extern char *devicename;	/* devicename is used in the 'define's */

  if ((int)pevt->severity > displevel || pevt->state & n_NODISPLAY)
    return 0;

  if (eventfilter(pevt) == 0)
    return 0;

  /* struct tm *evtm is defined in snipstv.h for the define statements */
  evtm = localtime((time_t *)&(pevt->eventtime));

  /* devicename is used in the 'define' statements */
  if ( *(pevt->device.subdev) ) {	/* name = device.subdev+device.name */
    strncpy(tstr, pevt->device.subdev, sizeof (tstr) - 1);
    strncat(tstr, "+", sizeof (tstr) - 1 - strlen(tstr));
    strncat(tstr, pevt->device.name, sizeof (tstr) - 1 - strlen(tstr));
    devicename = tstr;
  }
  else
    devicename = pevt->device.name;
	
#ifdef	USE_NCURSES
  if (has_colors()) {
    switch (pevt->severity)
    {
    case E_CRITICAL:
      wattron (win, COLOR_PAIR(4) | A_BOLD);
      break;
    case E_ERROR:
      wattron (win, COLOR_PAIR(3) | A_BOLD);
      break;
    case E_WARNING:
      wattron (win, COLOR_PAIR(2) | A_BOLD);
      break;
    case E_INFO:
      wattron (win, COLOR_PAIR(1));
      break;
    }
  }
#endif

  switch (widescreen)
  {
  case 0:
    wprintw (win, FMT0, FIELDS0); break;
#ifdef FIELDS1
  case 1:
    wprintw (win, FMT1, FIELDS1); break;
#endif
#ifdef FIELDS2
  case 2:
    wprintw (win, FMT2, FIELDS2); break;
#endif
#ifdef FIELDS3
  case 3:
    wprintw (win, FMT3, FIELDS3); break;
#endif
#ifdef FIELDS4
  case 4:
    wprintw (win, FMT4, FIELDS4); break;
#endif
  default:
    wprintw (win, FMT0, FIELDS0); break;
  }

  if (pevt->state & n_OLDDATA)
    sprintf(state, "Old%-4.4s", severity_txt[pevt->severity]);
  else if (pevt->state & n_UNKNOWN)
    sprintf(state, "Unkn%-4.4s", severity_txt[pevt->severity]);
  else if (pevt->state & n_TEST)
    sprintf(state, "Test%-4.4s", severity_txt[pevt->severity]);
  else
    sprintf(state, "%-8.8s", severity_txt[pevt->severity]);

  switch (pevt->severity)
  {
  case E_CRITICAL:
    ++curcritical ;		/* Increase number	*/
    if (belloff && curcritical > prevcritical)
    {
      if (!silent)
	belloff = 0;		/* Force bell on */
    }
#ifdef USE_NCURSES		    
    if (!has_colors())
#endif
    wstandout (win);
    waddstr (win, state);
#ifdef USE_NCURSES
    if (has_colors())
       wattroff (win, COLOR_PAIR(4) | A_BOLD);
    else
#endif
    wstandend (win);
    break;

  case E_ERROR:
#ifdef USE_NCURSES
    if (!has_colors())
#endif
    wBoldon(win);	/* */
    waddstr(win, state);
#ifdef USE_NCURSES
    if (has_colors())
       wattroff (win, COLOR_PAIR(3) | A_BOLD);
    else
#endif
    wBoldoff(win);	/* */
    break;

  case E_WARNING:
    waddstr(win, state);
#ifdef USE_NCURSES
    if (has_colors())
       wattroff (win, COLOR_PAIR(2) | A_BOLD);
#endif
    break;

  case E_INFO:
  default:
    waddstr(win, state);
#ifdef USE_NCURSES
    if (has_colors())
       wattroff (win, COLOR_PAIR(1));
#endif
    break;
  }	/* switch() */

  waddch(win, '\n') ;	/* add a newline, automatically does wclrtoeol() */
  return(1) ;
}	/* update_eventwin() */

/*+ 
 * 'beep' on the terminal.
 */

void Beep()
{
  extern bool silent;
  extern char *bellstr;

  if (!silent)
    tputs(bellstr, 1, outchar) ;	/* */
}


/*
 * convert unsigned integer into a string
 */
char *rating2str(n)
  unsigned char n;
{
  register int i;
  static char s[16];

  i = sizeof(n) * 8;
  for (s[i] = '\0'; i != 0; n>>=1)
    if ( n & 01 )
      s[--i] = '1';
    else
      s[--i] = '0';

  return (s);
}
