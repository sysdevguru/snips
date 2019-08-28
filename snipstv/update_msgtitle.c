/*
 * $Header: /home/cvsroot/snips/snipstv/update_msgtitle.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * Update the title for the MESSAGES panel. This is actually on 2 lines
 * The first line displays information like Display Level, filter string,
 * etc.
 *
 */

/*
 * $Log: update_msgtitle.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc.  info@netplex-tech.com */

#ifndef lint
  static char rcsid[] = "$Id: update_msgtitle.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>

#include "snips.h"		/* for EVENT severity definitions */
#include "snipstv.h"		/* includes the curses header files */
#include "do_filter.h"

void update_msgstitle(win)
  WINDOW *win;
{
  char *pat;
  extern bool frozen;
  extern int COLS, displevel;

  wmove(win, 0, 0) ;
  if (frozen)                                 /* on first line of win */
  {	             	                      /* in reverse video     */
    wstandout (win);
    waddstr(win, "SCREEN FROZEN") ;
    wstandend (win);
  }
  else
    wclrtoeol (win) ;                   /* erase line */

  /*
   * display the filter pattern after the 'screen locked' message (about
   * 20 chars long). The show_display_level should overwrite in case this
   * gets too long to display.
   */
  pat = (char *)get_filterpattern();
  if (pat && *pat)
    mvwprintw(win, 0, max((int)(COLS/4),  20), "FILTER ON: %.*s..",
	      (int)(COLS/2), pat);

  wclrtoeol (win);                     /* erase rest of line */
  /* now the display level */
  mvwprintw (win, 0, COLS - 26,
	     "  Display Level: %-8.8s", severity_txt[displevel]);
  wBoldon(win);
  mvwaddstr (win, 1, (int)(COLS/2 - 4), "MESSAGES");
  wBoldoff(win);
}

