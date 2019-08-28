/*
 * $Header: /home/cvsroot/snips/snipstv/main.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * The main for the SNIPS TextView. A simple curses based display for the
 * data stored by the various monitoring agents.
 *
 * CAVEATS:
 * Currently ignores the machine endian. Hence, if the data directory
 * is mounted via NFS from a differing endian machine, then the integer and
 * long values will all be wrong (e.g. if NFS mounting the data dir from
 * a Sparc onto a Intel Linux/FreeBSD system).
 *
 * AUTHOR:
 *	Vikas Aggarwal    vikas@navya.com
 *
 */

/*
 * $Log: main.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001 Netplex Technologies, Inc.  info@netplex-tech.com */

#ifndef lint
  static char rcsid[] = "$Id: main.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define _MAIN_
# include  "snips.h"		/* just for the E_CRITICAL etc. defines */
# include  "snipstv.h"		/* application specific	*/
#undef _MAIN_

#include "snips_funcs.h"
#include "create_subwins.h"
#include "main.h"
#include "init.h"
#include "parse_user_input.h"

static WINDOW *basewin = NULL;	/* base or root window i.e. the screen */

/* function prototypes */
void update_subwins();
int help();

int main(ac, av, environ)
  int ac;
  char *av[];
  char *environ[];		/* shell user environment, TERMinal etc. */
{
  char c;
  extern char *optarg;
  extern int  optind;

  prognm = av[0];

#ifdef  _SNIPSTV_VERSION_
  fprintf (stderr, "SNIPS TextView version %s\n\n", snipstv_version);
  fflush (stderr);
#endif

#ifdef DATA_VERSION
  if (debug)
    fprintf (stderr, "Data Format version %d\n", DATA_VERSION);
#endif

  pageno = 1;
  belloff = 1;
  widescreen = 0;	/* 80 cols. Use 2 for 132 cols */

  while ( (c = getopt(ac, av, "bdewl:qsC")) != EOF)
    switch (c)
    {
#ifdef NCURSES
    case 'C':
      has_color = -1;		/* disable */
      break;
#endif
    case 'q':
    case 'b':
	belloff = 1 ; 	break;
    case 'd': debug++ ;	break;
    case 'w':
    case 'e':		/* default is 1. Set to 2 for 132 column */
      widescreen = ++widescreen % NUMHDRS;
      break;
    case 'l':
      sscanf(optarg, "%d", &displevel);
      break;
    case 's': silent = 1 ; break;		/* never beep */
    case '?':
    default:
      fprintf(stderr, "%s- unknown flag '%c'\n", prognm, c);
      help();
    }

  if ( (ac - optind) == 1)
    datadir = av[optind] ;			/* alternate data dir */
  else if ( (ac - optind) > 1)
    help();

  /* Init the various directory locations, etc. */
  if (!datadir || *datadir == '\0')
    if ((datadir = (char *)get_datadir()) == NULL)
    {
      fprintf(stderr, "Sorry, cannot find data directory, exiting\n");
      exit (1);
    }


  helpfile = (char *)malloc( strlen((char *)get_etcdir()) + strlen(HELPFILE));
  sprintf(helpfile, "%s/%s", get_etcdir(), HELPFILE) ;

  msgsdir = (char *)malloc( strlen(datadir) + 16);
  sprintf(msgsdir, "%s/../msgs", datadir);

  displevel = E_CRITICAL;

  /* Setup to catch ALL signals except SIGUSR1/SIGUSR2 for security */
  /*  for (i = 0 ; i < SIGUSR1 ; ++i )
    signal(i, finish); */

#ifdef SIGWINCH
  signal(SIGWINCH, winresize);
#endif

  init_environ(environ);
  init_curses();
  welcome_screen();
  basewin = (WINDOW *)create_subwins();
  while (1)
  {
    update_subwins();		/* update the data on the sub windows */

    /* force the cursor to the last line after the prompt */
#ifdef __NetBSD__
    wmove(basewin, getcury(PromptPanel.win) + getbegy(PromptPanel.win),
	  getcurx(PromptPanel.win) + getbegx(PromptPanel.win));
#else
    wmove(basewin, PromptPanel.win->_cury + PromptPanel.win->_begy,
	  PromptPanel.win->_curx + PromptPanel.win->_begx);
#endif
    touchwin(basewin);		/* force an update since its foolproof */
    wrefresh(basewin);

    parse_user_input();
  }
  /* never reached */

}	/* end main() */

void update_subwins()
{
  register int i;
  int (*func)();

  for (i = 0; allpanels[i] != NULL ; ++i)
  {
    func = allpanels[i]->update_func;
    func(allpanels[i]->win);	/* call the update function  */
  }
}

/*
 * Simple function to permit refereshing the statically defined basewin
 */
void refresh_screen()
{
  touchwin(basewin);
  wrefresh(basewin);
  /* wrefresh(curscr);	*/
}

int help()
{
  fprintf(stderr, "\nUSAGE: %s  <options>  [data-directory]\n", prognm);
#ifdef NCURSES
  fprintf(stderr, "\n\t-C\tdisable color terminal");
#endif
  fprintf(stderr, "\n\t-d\tdebug \n\t-w\twidescreen");
  fprintf(stderr, "\n\t-b\tbell-off \n\t-s\tsilent \n");
  exit (1);
}

/*
 * Quit on getting any signal.
 */
void finish()
{
  if (in_curses)
    endwin();
  /* fprintf(stderr, "Exited\n");	*/
  exit(1);
}

/*
 * Upon getting a window resize (typically in xterms).
 * Note that after getting this signal, the system blocks all further
 * occurences of this signal until this function returns. Hence dont
 * jump off to a wierd function that never returns from here...
 *
 * We also call this function when we have changed the minimum sizes of
 * the subwindows and we want to recreate the basewindow.
 */
void winresize()
{
#ifdef TIOCGSIZE
  static struct ttysize win;
  if (ioctl (0, TIOCGSIZE, &win) == 0)
  {
    if (win.ts_lines > 0)
      LINES = win.ts_lines;
    if (win.ts_cols > 0)
      COLS = win.ts_cols;
  }
#else
# ifdef TIOCGWINSZ
  static struct winsize win;	/* 4.3 BSD */

  if (ioctl(1, TIOCGWINSZ, (char *) &win) == 0 )
  {
    if (win.ws_row > 0)
      LINES = win.ws_row ;
    if (win.ws_col > 0)
      COLS = win.ws_col ;
  }
# endif		/* TIOCGWINSZ */
#endif	/* TIOCGSIZE */

  winreset();
  /* we go back to displaying whatever file at wherever location */

}	/* winresize() */

/*+
 * Just restart and create windows all over again.
 */
void winreset()
{
  init_curses();
  basewin = (WINDOW *)create_subwins();
  wrefresh(curscr);		/* redraw entire screen */
}

/*
 * Function required by tputs(). Cannot be a macro.
 */
int outchar (c)
  int c;
{
  return fputc (c, stdout);
}

void dotest(arrdevices, arrvalue, ndevices)
  void          **arrdevices;
  void          *arrvalue;
  int           ndevices;
{
  return;
}
