/*+ $Header: /home/cvsroot/snips/snipstv/parse_user_input.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $ */


/*+ 
 * Parse the user input and call corresponding function.
 */

/*
 * $Log: parse_user_input.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies, Inc. info@netplex-tech.com */

#ifndef lint
  static char rcsid[] = "$Id: parse_user_input.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <ctype.h>
#include <sys/time.h>

#include "snips.h"
#include "snipstv.h"
#include "main.h"
#include "update_msgtitle.h"
#include "help_page.h"
#include "do_filter.h"
 
/* function prototypes */
int poll_input();

int parse_user_input()
{
  char response;		/* user's character input */
  extern int widescreen, displevel;
  extern bool belloff, frozen;

  switch (poll_input())
  {
  case 'b':					/* turn beep/bell off */
    if (!belloff)
    {
      belloff = 1;
      waddstr(MsgPanel.win, "\nACK, bell off");
      refresh_screen();
    }
    break ;

  case 'c': 					/* Contract event win	*/
    if (EventPanel.rows > 2)
    {
      ++MsgPanel.minrows;
      --EventPanel.minrows;
      winreset();
    }
    else
    {
      wprintw(MsgPanel.win, "\nEvent window size is minimum");
      refresh_screen();
    }
    break ;

  case 'd':					/* toggle debug mode	*/
    debug =  (++debug) % 3;
    wprintw(MsgPanel.win, "\nDebug mode %s", debug ? "ON" : "OFF");
    refresh_screen();
    break ;

  case 'w':
  case 'e':					/* rotate event views */
  case 'v':
    widescreen = (++widescreen) % NUMHDRS;
    winreset();
    refresh_screen();
    break ;

  case CTRL('s'): case CTRL('q'):
  case 'f':				/* Freeze display */
    frozen = !frozen ;				/* Invert the bit */
    update_msgstitle(MsgTitlePanel.win);	/* To indicate frozen */
    refresh_screen();
    parse_user_input(poll_input());		/* To redisplay present	*/
    break ;

  case 'l':					/* new display level	*/
    wprintw(MsgPanel.win, "\nEnter new level [%d-%d]", E_CRITICAL, E_INFO);
    refresh_screen();
    response = poll_input() - '0';	/* character to number	*/
    if (response >= E_CRITICAL && response <= E_INFO )
      displevel = response ;
    wprintw(MsgPanel.win, " %d OK", displevel);
    refresh_screen();
    break ;

  case 'q':					/* quit	*/
    (void)finish ();
    break ;

  case '?': case 'h':			/* Help page */
    help_page();
    refresh_screen();
    break;
	
  case CTRL('l'):
  case 'r':					/* redraw screen */
    wrefresh (curscr);
    parse_user_input (poll_input());
    break ;

  case 's':					/* search filter */
  case '/': case '|':
    read_filter();
    update_msgstitle(MsgTitlePanel.win);	/* indicate filter in effect */
    refresh_screen();
    break ;

  case 'x':					/* Expand window */
    if (MsgPanel.rows > 2)
    {
      --MsgPanel.minrows;
      ++EventPanel.minrows;
      winreset();
    }
    else
    {
      wprintw(MsgPanel.win, "\nEvent window size is maximum");
      refresh_screen();
    }
    break;

  default:					/* return for next scr	*/
    break ;
  }					    	/* End switch()		*/

  return (1);	

}	/* parse_user_input() */

/*
 * Do a select() on the stdin for PAUSE seconds and return a 0 or the
 * user input.
 */

#ifndef PAUSE
# define PAUSE 15		/* time to wait for user input */
#endif

int poll_input ()
{
  extern bool frozen ;			/* Defined in netconsole.h */
  struct timeval tm;
  char	reply;
  int	read_mask = 1 ;			/* For 'select' routine */

  tm.tv_sec = PAUSE;			/* init poll delay */
  tm.tv_usec = 0;

  if (frozen)
    reply = select (2, (fd_set *)&read_mask, (fd_set *)0, (fd_set *)0, NULL);
  else
    reply = select (2, (fd_set *)&read_mask, (fd_set *)0,(fd_set *)0, &tm);

  if (!(reply && read_mask))			/* no input, timed out */
    return(0); 
  else
    return (getchar()) ;			/* return the input */
}
