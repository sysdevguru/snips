/*
** $Header: /home/cvsroot/snips/include/snipstv.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $
*/

#ifndef _snipstv_h_
# define _snipstv_h_

#include "osdefs.h"		/* defines NCURSES, etc. */

/* Curses specific */
#include <ctype.h>

#include <ncurses.h>
#include <curses.h>
#include <term.h>
#include <stdlib.h>

/*
 * For 4.2 curses.
 */
#if defined(VMS) || defined(__convex__) || defined(sequent)
# ifndef cbreak
#  define cbreak crmode
# endif
# ifndef nocbreak
#  define nocbreak nocrmode
# endif
#endif

/*
 * NetBSD and BSDI versions below v4.0 have broken curses.
 */
#ifdef BROKEN_CURSES
# define _maxy maxy
# define _cury cury
# define _curx curx
# define _begy begy
# define _begx begx
# define bool int
# include <termios.h>
#endif	/* BROKEN_CURSES */


/*
 * Usually defined in ttychars.h.
 */
#ifndef ESC             /* ESCAPE character */
#define ESC 033
#endif

#ifndef RUBOUT
#define RUBOUT        	'\177'
#endif

#ifndef erasechar
#define erasechar()	RUBOUT
#endif

#ifdef CTRL             /* Some implementations do a: c & 037 (assume int) */
#undef CTRL             /* and some do 'c' & 037, so use our version... */
#endif  /* CTRL */
#define CTRL(c)         (c & 037)	/* treat as an integer */

#ifndef max
# define max(a, b)	((a) > (b) ? (a) : (b))
#endif	/* endif max */


#ifndef HELPFILE
# define HELPFILE 	"snipstv-help"	/* shld be in ETCDIR */
#endif

#if __STDC__
int outchar (int c);
#else
int outchar();			/* required by tputs() */
#endif

#ifdef A_BOLD
# define wBoldon(w)	wattron(w, A_BOLD)
# define wBoldoff(w)	wattroff(w, A_BOLD)
#else
# define wBoldon(w)	{ }
# define wBoldoff(w)	{ }
#endif

#define PAUSE		15			/* wait secs for poll()	*/

/*
 *	Global definitions
 *
 * helpfile is ETCDIR/HELPFILE
 * msgsdir  is datadir/../msgs
 */
#define MNCOLS		70		/* depends on widescreen value */

#ifdef _MAIN_
# define EXTERN
#else
# define EXTERN extern
#endif
EXTERN char *datadir ;				/* Dir of data files */
EXTERN char *msgsdir ;				/* Dir with text msg files */
EXTERN char *helpfile ;				/* help file */
EXTERN char *bolds;				/* Bold sequence */
EXTERN char *bolde;				/* Bold end sequence */
EXTERN char *ulines;				/* underline start */
EXTERN char *ulinee;				/* underline end */
EXTERN char *clscr;			    	/* Clear screen sequence */
EXTERN char *bellstr;				/* Bell sequence */

EXTERN int widescreen;				/* wide display (132 cols) */
EXTERN int displevel;				/* display level */
EXTERN int pageno;
EXTERN int maxpageno;
EXTERN int has_color;				/* color terminal */
EXTERN bool in_curses;
EXTERN bool frozen ;				/* indefinite poll wait	*/
EXTERN bool belloff;				/* toggle bell off or on */
EXTERN bool silent;				/* Command line never beep */

#ifdef EXTERN
# undef EXTERN
#endif

/*
 * The screen is made up of several subwindows. This struct is
 * essentially a list of all the windows on the main SNIPSTV screen.
 */

typedef struct _panel {
  WINDOW  *win;
  void    *update_func;
  int	  minrows;	/* minimum rows */
  int	  rows;
} PANEL;

#ifdef _MAIN_
int update_title(), update_msgstitle(), update_eventwin(),
update_msgwin(), update_promptwin();

PANEL TitlePanel = {NULL, update_title, 4, 0};		/* 2 blank lines */
PANEL EventPanel = {NULL, update_eventwin, 2, 0};	/* min size is 2 */
PANEL MsgTitlePanel = {NULL, update_msgstitle, 2, 0};	/* 1 blank line */
PANEL MsgPanel = {NULL, update_msgwin, 2, 0};		/* min size is 2 */
PANEL PromptPanel = {NULL, update_promptwin, 2, 0};	/* 1 blank line above*/
PANEL *(allpanels[]) = {
  &TitlePanel, &EventPanel, &MsgTitlePanel, &MsgPanel, &PromptPanel, NULL};
#else
extern PANEL TitlePanel, MsgTitlePanel, EventPanel, MsgPanel, PromptPanel;
extern PANEL *allpanels[];
#endif	/* MAIN */

char *rating2str();
struct tm *evtm;		/* tm structure needed in define's below */
char *devicename;		/* needed in the define's below */

/*+  Display strings and formats */

/*
 * The header names and the formats are coded here. They depend on the 'event'
 * structure and should be modified with care. Make sure that you match the
 * sizes with the size of the various fields in 'snips.h'. Leave space for
 * the 'condition' string added at the end.
 *
 * You can have upto 5 formats defined here (0-4)
 *
 */
#define NUMHDRS 4		/* max number of print formats */


/* 123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789. */
#define HDR3 \
 "      Device             Address          Date  Time  +- Sender -+  +-- Variable --+ +-Value-+  +-Thres-+   Units   Flags  State "
/*+++++++++|++++++++++  +++++++++|++++++++ ++/++ ++:++  +++++++++|++  +++++++++|++++++ +++++++++  +++++++++  ++++++++  +++  END */
#define FMT3 	"%20.20s  %-18.18s %02d/%02d %02d:%02d  %-12.12s  %16.16s %9lu  %9lu  %8.8s  %03o  "	/* Condition added at end */
#define FIELDS3 devicename, pevt->device.addr, \
        evtm->tm_mon + 1, evtm->tm_mday, evtm->tm_hour, evtm->tm_min, \
	pevt->sender, pevt->var.name, \
	pevt->var.value, pevt->var.threshold, pevt->var.units, pevt->state

# define HDR2 \
 "            Device           Address             Rating  +-Variable-+ State "
/*123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789. */
# define FMT2	"%26.26s  %-18.18s _%8.8s %-12.12s "
# define FIELDS2  devicename, pevt->device.addr, \
	rating2str(pevt->rating), pevt->var.name

#define HDR1 \
 "              Device             +-Variable-+  -Value-+  -Units-+  State "
/*123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789. */
#define FMT1	"%31.31s  %12.12s  %8lu  %8.8s  "
#define FIELDS1	devicename, \
	pevt->var.name, pevt->var.value, pevt->var.units

#define HDR0 \
 "        Device      Address      Rating Time   +-Variable-+  -Value-+  State"
/*123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789. */
#define FMT0	"%15.15s  %-15.15s _%4.4s %02d:%02d  %12.12s  %8lu  "
#define FIELDS0	devicename, pevt->device.addr,\
        rating2str(pevt->rating),\
	evtm->tm_hour, evtm->tm_min, pevt->var.name, pevt->var.value

#endif	/* ! __snipstv_h */
