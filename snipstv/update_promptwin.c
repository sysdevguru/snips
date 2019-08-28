/*
 * $Header: /home/cvsroot/snips/snipstv/update_promptwin.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * Update the prompt panel (last line).
 */

/* <robl@linx.net> 2004-11-2004 
 * - PROMPTA and PROMPTB actually the wrong way round.
 * We prompt for next screen when there isn't any, and do not
 * prompt when there is. Reversed the variables.
*/

/*
 * $Log: update_promptwin.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */

#ifndef lint
  static char rcsid[] = "$Id: update_promptwin.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <sys/time.h>

#include "snipstv.h"		/* includes the curses header files */

#define PROMPTA "Enter option, or any other key for next screen: "
#define PROMPTB "Enter option, 'q' to quit, 'h' for help: "

void update_promptwin(win)
  WINDOW *win;
{
  
#ifdef __NetBSD__
  if (getcury(EventPanel.win) >= (getmaxy(EventPanel.win) - 1))
#else
  if (EventPanel.win->_cury >= (EventPanel.win->_maxy - 1))
#endif
    mvwprintw(win, 1, 0, "%s", PROMPTA);	/* end of screens */
  else
    mvwprintw(win, 1, 0, "%s", PROMPTB);	/* next screen */

  wclrtoeol(win);
}
