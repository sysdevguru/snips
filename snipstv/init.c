/*
 * $Header: /home/cvsroot/snips/snipstv/init.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * All initialization stuff.
 *	check_terminal()  See if entry for tty exists in termcap
 *	init_environ()    Extract tty type and set the bold, etc. strings
 *	init_curses()	Go into curses mode
 *	welcome_screen()	Humphh!! :)
 *
 */

/*
 * $Log: init.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc, info@netplex-tech.com */

#ifndef lint
  static char rcsid[] = "$Id: init.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include  "snipstv.h"
/*
 * List common terminals in ok_terminals to avoid doing a lengthy tgetent()
 * call. List unacceptable terminals which might be listed in the /etc/termcap
 * in bad_terminals[]
 *
 */

static char *ok_terminals[]  = {"vt100", "vt200", "xterm", "sun", NULL};
static char *bad_terminals[] = {"ethernet", "network", "dialup", NULL};

/* function prototypes */
int check_terminal(char *ptermtype);

/*+
 * Extract the TERM variable and make sure it is valid. Uses the 'environ'
 * environment pointer. Sets the TERM value after prompting the user.
 *
 */

int init_environ(environ)
  char *environ[];
{
    char termtype[64];
    static char bp[1024] ;		     	/* needed by tgetent, static */
    static char *newvar;			/* has to be static */
    extern char *tgetstr() ;			/* 'termcap' routine */
    
    bzero(termtype, sizeof (termtype)) ;
    strcpy (termtype, (char *)getenv("TERM"));	/* retrieve TERM type	*/
    
    if (check_terminal(termtype) == -1)       	/* Extract terminal type */
      return(-1) ;

    if (newvar) free(newvar);
    newvar = (char *)malloc(strlen(termtype) + 6);
    sprintf (newvar, "TERM=%s", termtype);
    if (putenv(newvar) != 0)			/* add the new string  */
      return (-1) ;
    
    /*
     * Now we extract the various strings. However, except for the bell
     * and the clscr, the rest are NOT required by snipstv since we can
     * only use tputc() for these strings which only works on raw terminals
     * (not on curses WINDOW structures).
     */
    if ( tgetent(bp, termtype) == 1 )
    {
      char *ptr = bp;

      clscr	= tgetstr("cl", &ptr) ;		/* clear screen */
      bellstr	= tgetstr("bl", &ptr) ;
    }
    if (!bellstr)
      bellstr = "\007" ;

    return(0);
}	/* init_environ() */


/*+
 * Check the supplied terminal string against the arrays
 * 'good_term' and 'bad_terminals'. Sets the value of the terminal type.
 * Return 1 if ok, -1 if error.
 */
int check_terminal(ptermtype)
  char *ptermtype ;
{
  register int i ;
  char bp[1024];		/* to extract 'tgetent' entry  */
  char reply[16];
  int tries = 2;

  do
  {
    /* Always prompt  so user can see the terminal type */
    printf("Terminal type [%s]: ", ptermtype);  fflush(stdout);
    fgets(reply, sizeof(reply), stdin);
    reply[strlen(reply) - 1] = '\0';
    if (*reply != '\0')
      strncpy(ptermtype, reply, sizeof(reply));

    for (i = 0; ok_terminals[i] ; ++i )
      if (strcmp (ok_terminals[i], ptermtype) == 0)
	return(1) ;

    for (i = 0; bad_terminals[i] ; ++i )
      if (strcmp (bad_terminals[i], ptermtype) == 0)
      {
	printf("Invalid Terminal type %s\nEnter again:", ptermtype);
	goto doagain;
      }

    fprintf (stderr, "Searching for terminal type '%s'...", ptermtype);
    
    if ( tgetent(bp, ptermtype)  == 1 )	/* entry found */
    {
      fprintf(stderr, "OK\n") ;
      return(1);
    }
    else	/* entry for terminal not found */
    {
      fprintf(stderr, "not found\n") ;
      goto doagain;
    }

  doagain:
    strcpy(ptermtype, "vt100");
  }
  while (--tries);	/* end do-while */

  /* Here if user entered invalid entry twice */
  strcpy(ptermtype, "dumb") ;		/* set terminal type to dumb	*/
  fprintf(stderr, "Setting terminal type to '%s'\n", ptermtype) ;
  sleep (2) ;
  if ( tgetent(bp, ptermtype) <= 0 ) {		/* can't even find dumb */
    fprintf(stderr, "ERROR: tgetent cannot find entry for '%s'\n", ptermtype);
    return (-1) ;				/* error */
  }
  else
    return(1);
}
     
/*
 * Just go into curses mode.
 */

void init_curses()
{
  extern int COLS;			/* defined in curses.h */
  if (in_curses)
  {
    endwin();
    if (clscr) tputs(clscr, 1, outchar);	/* clear the screen */
    fflush(stdout);
  }
  initscr ();			/* into curses mode */
#ifdef	USE_NCURSES
  if (has_color >= 0 && has_colors()) {
    has_color = 1;
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);	/* INFO */
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);	/* WARN */
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);	/* ERROR */
    init_pair(4, COLOR_RED, COLOR_BLACK);	/* CRITICAL */
  }
  else
    has_color = 0;		/* cant leave it negative */
#endif
  noecho();			/* else messes up curses */
  cbreak();			/* don't buffer input */

  if ( COLS < MNCOLS )
  {
    endwin ();
    fprintf (stderr, 
	     "Terminal ('%s') too small (need %d cols, have %d)\n\n", 
	     ttytype, MNCOLS, COLS);
    exit (1);
  }
  if (COLS < 78)
    widescreen = 0;		/* use the smaller format */

  in_curses = 1;

}	/* init_curses */


void welcome_screen()
{
  WINDOW *wscr;
  char *s;

  wscr = newwin(0,0,0,0);
  wstandout(wscr);
  s = "SNIPS TextView\n";
  mvwprintw(wscr, (int)(LINES/2 - 1), (int)(COLS/2) - (int)(strlen(s)/2), s);
  wstandend(wscr);
  s = "        Initializing...";
  mvwaddstr(wscr, (int)(LINES/2 + 1), (int)(COLS/2) - (int)(strlen(s)/2), s);
  wmove(wscr, 0, COLS-1);
#if defined(ACS_VLINE) && defined(ACS_HLINE)
  box(wscr, 0, 0);
#endif

  wrefresh(wscr);
  sleep(1);
}
