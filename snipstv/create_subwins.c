/*
 * $Header: /home/cvsroot/snips/snipstv/create_subwins.c,v 1.0 2001/07/09 03:33:52 vikas Exp $
 */

/*
 * The entire screen is divided into the following sub-windows:
 *	Title  = 4 lines (fixed). 2nd and 4th line are blank separators
 *	Events = all the remaining after allocating lines for the rest
 *	MsgTitle = 2 lines fixed. 1st line contains info, 2nd line has the
 *		   word 'Messages'.
 *	Msgs = minimum 2 lines. Displays errors, debugs, etc.
 *	Prompt = 2 lines, 1st is blank separator.
 *
 *		-vikas@navya.com
 */

/*
 * $Log: create_subwins.c,v $
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc.,  info@netplex-tech.com */

#ifndef lint
  static char rcsid[] = "$Id: create_subwins.c,v 1.0 2001/07/09 03:33:52 vikas Exp $";
#endif

#include	"snipstv.h"

WINDOW *
create_subwins()
{
  int i;
  int minsize = 0;
  int currow = 0;
  static WINDOW *wbase;
  extern int COLS, LINES;		/* defined in curses.h */

  for (i = 0; allpanels[i] != NULL; ++i)
  {
    allpanels[i]->rows = allpanels[i]->minrows;	/* reset in case changed */
    minsize += allpanels[i]->minrows;
  }

  if (LINES < minsize)
  {
    fprintf(stderr, "Screen size too small, minimum %d, current %d\n",
	    minsize, LINES);
    endwin();
    exit(1);
  }

  EventPanel.rows = LINES - (minsize - EventPanel.minrows);
  if (EventPanel.rows > 20)	/* give some more rows to MsgPanel */
  {
    EventPanel.rows -= 3;
    MsgPanel.rows += 3;
  }
  else if (EventPanel.rows > 10)
  {
    EventPanel.rows -= 1;
    MsgPanel.rows += 1;
  }
  
  wbase = newwin(0,0,0,0);		/* Create main window	*/
  scrollok (wbase, TRUE);			/* Allow scrolling	*/
  leaveok (wbase, FALSE);			/* leave cursor at curxy */

  for (i = 0; allpanels[i] != NULL; ++i)
  {
    allpanels[i]->win = subwin(wbase, allpanels[i]->rows, COLS, currow, 0);
    currow += allpanels[i]->rows;
  }

  scrollok (MsgPanel.win, TRUE);	/* Message window can scroll	*/
  return(wbase);
}	/* create_subwins */

