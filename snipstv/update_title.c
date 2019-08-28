/*
 * $Header: /home/cvsroot/snips/snipstv/update_title.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * Update the main title panel. Put the page number and the date.
 * Put the titles of the various columns depending on the value of
 * widescreen.
 *
 *	-vikas@navya.com
 */

/*
 * $Log: update_title.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */

#ifndef lint
  static char rcsid[] = "$Id: update_title.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <time.h>

#include "snipstv.h"		/* includes the curses header files */

void update_title(win)
  WINDOW *win;
{
  extern int COLS;		/* defined in curses.h */
  time_t t = time((time_t *)NULL);

  mvwprintw(win, 0, 0, "%3.2d", pageno);
  if (maxpageno)  wprintw(win, "/%d", maxpageno);
  wBoldon(win);
  mvwprintw(win, 0, COLS/2 - 9, "%s", "SNIPS TextView");
  wBoldoff(win);
  mvwprintw(win, 1, COLS - 7, "wide %d", widescreen);
  mvwprintw(win, 0, COLS - 27, "%26s", ctime(&t) );

  /* put a line first, so that if the HDR wraps around it erases the line */
#if defined(ACS_VLINE) && defined(ACS_HLINE)
  wmove(win, 3, 0);		/* move to the last line (blank) */
# ifdef __NetBSD__
  whline(win, 0, getmaxx(win));	/* steals one char line */
# else
  whline(win, 0, win->_maxx);	/* steals one char line */
# endif
#endif	/* defined ACS_VLINE */
  /* put the event header after a blank line */
  wBoldon(win);
  switch(widescreen)
  {
  case 0:			/* screen is narrow */
    mvwaddstr(win, 2, 0, HDR0); break;
#ifdef HDR1
  case 1:
    mvwaddstr(win, 2, 0, HDR1); break;
#endif
#ifdef HDR2
  case 2:
    mvwaddstr(win, 2, 0, HDR2); break;
#endif
#ifdef HDR3
  case 3:
    mvwaddstr(win, 2, 0, HDR3); break;
#endif
#ifdef HDR4
  case 4:
    mvwaddstr(win, 2, 0, HDR4); break;
#endif
  default:
    mvwaddstr(win, 2, 0, HDR0); break;
  }
  wBoldoff(win);

}
