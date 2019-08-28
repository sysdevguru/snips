/*+ 
** $Header
**/

/*+
 *  To display the help page by reading the file HELPFILE
 */

/*
 * $Log: help_page.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

#ifndef lint
  static char rcsid[] = "$Id: help_page.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include 	<stdio.h>
#include	<string.h>
#include	<errno.h>

#include	"snipstv.h"

#ifndef HELPFILE
# define HELPFILE	"snipstv-help"	/* should be under ETCDIR */
#endif

#ifdef __NetBSD__
#define WFULL(w)	(getcury(w) == getmaxy(w) - 1) ? 1:0
#else
#define WFULL(w)  	(w->_cury == (w->_maxy - 2)) ? 1:0
#endif

/* function prototypes */
int help_title(WINDOW *hwin, int hpage);

int help_page()
{
  extern int errno;
  extern char *helpfile ;			/* In snipstv.h  */
  WINDOW *hwin;
  FILE *f;
  char line[512];
  int i = 1;

  hwin= newwin( 0, 0, 0, 0 );
  touchwin(hwin) ;	/* */

  help_title(hwin, i) ;			/* display fancy title	*/
  if ((f = fopen(helpfile, "r")) == NULL)
  {
    wprintw(hwin,"Cannot open help file %s:\n\t%s\n",
	    helpfile, (char *)strerror(errno));
    wprintw(hwin, "\nHit any key to continue: ");
    wclrtobot(hwin);
    wrefresh(hwin);
    getchar();
    return(1);
  }

  for ( ; ; )			/* display screenfuls of the helpfile	*/
  {
    while (!(WFULL(hwin)) && fgets(line, sizeof(line), f) != NULL)
      wprintw(hwin, "%s", line);	/* keeps the newline */
	
    if (feof(f))
    {
      wprintw(hwin, "\nHit any key to return: ");
      wclrtoeol(hwin);
      wclrtobot(hwin);			/* Remove old trash	*/
      wrefresh(hwin) ;
      getchar() ;
      break ;				/* from outer for loop	*/
    }
    else	/* window is full */
    {
      wprintw (hwin, "\n--More--");
      wclrtoeol(hwin);
      wrefresh(hwin);
      if (getchar() == 'q')
	break ;				/* from outer 'for'	*/
    }
    werase(hwin);
    help_title(hwin, ++i);
  }						/* end:  for		*/
  fclose(f);
  return(0);					/* all OK	*/
}						/* end: help_page	*/


int help_title(hwin, hpage)
  WINDOW *hwin;
  int hpage;
{
  wstandout (hwin);
  mvwprintw (hwin, 0, (int)(COLS/2 - 11),"SNIPS TextView HELP SCREEN (%d)\n\n",
	     hpage);
  wstandend (hwin);

  return(1) ;
}

