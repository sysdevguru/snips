/*
 * $Header: /home/cvsroot/snips/snipstv/do_filter.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*
 * Creates a new window and reads in the user entered string which is used
 * as the filter. All EVENT structures that match this pattern will then
 * be displayed by the SNIPS TextView program.
 *
 * AUTHOR
 *   Initial version: David Wagner, wagner@phoenix.princeton.edu 1992
 *	(See copyright below)
 *   This version maintained by: Vikas Aggarwal, vikas@navya.com
 *
 */

/*
Copyright 1992 JvNCnet, Princeton University

Permission to use, copy, modify and distribute this software and its
documentation for any purpose is hereby granted without fee, provided
that the above copyright notice appear in all copies and that both
that copyright notice and this permission notice appear in supporting
documentation, and that the name of JvNCnet or Princeton University
not be used in advertising or publicity pertaining to distribution of
the software without specific, written prior permission.  Princeton
University makes no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.

PRINCETON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
DAMAGES WHATSOEVER, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA
OR PROFITS, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * $Log: do_filter.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

#ifndef lint
  static char rcsid[] = "$Id: do_filter.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <string.h>

#include "snips.h"
#include "snipstv.h"

#define MAXLINELEN	256			/* Max length of new pattern */
#define MAXARGS		20			/* Max # of search words */
#define MAXTEXTLEN	1024			/* len of temp pattern */

static char *(compiled[MAXARGS][MAXARGS]);	/* Compiled pattern */
static char *pattern;		/* entered filter string */

/* function prototypes */
void get_filter(WINDOW *w, char pat[], int patlen);
void compile_pattern(char *pattern);

/*
 * Just return the pattern to an external program.
 */
char *get_filterpattern()
{
  return(pattern);
}

/*+	read_filter
 * Creates a new window and reads in a new pattern string for filtering
 * out unwanted information.
 * Compiles that pattern for later use by filter().
 */

void read_filter()
{
  WINDOW *npatwin;				/* Where to get pattern from */

  if (pattern == NULL)
    pattern = (char *)calloc(1, MAXLINELEN);

  npatwin = newwin(0,0,0,0);			/* Create new window */
  touchwin(npatwin);				/* Bring it to the front */
  wstandout(npatwin);				/* Print the title header */
  mvwprintw(npatwin, 0, (int)(COLS/2 - 9), "NEW FILTER PATTERN\n\n");
  wstandend(npatwin);

  if (pattern && *pattern)
  {
    /*
     * There is a pattern compiled already - use it as the default,
     * and let the user edit it as the starting point for a new one.
     */
    wprintw(npatwin, "Use ^U to erase filter\n");
    wprintw(npatwin, "Enter filter (<word> [&] [|] <word>..): %s", pattern);
    wrefresh(npatwin);
    get_filter(npatwin, pattern, strlen(pattern));
  } else {
    /*
     * This is the first call to new_pattern(), so there is no
     * saved pattern available.
     */
    wprintw(npatwin, "Use ^U to erase filter\n");
    wprintw(npatwin, "Enter filter (<word> [&] [|] <word>..): ");
    wrefresh(npatwin);
    get_filter(npatwin, pattern, 0);
  }

  if (*pattern)
    wprintw(npatwin, "\n\nFilter accepted!\n");	/* Notify the user. */

  wrefresh(npatwin);

  compile_pattern(pattern);
  if (debug > 1)
  {
    int i, j;
    wprintw(npatwin, "\n\n(debug)Filter is:\n");
    for (i = 0; compiled[i][0] != NULL ; ++i)
      for (j=0; compiled[i][j] != NULL ; ++j)
	wprintw(npatwin, "pat[%d][%d]= %s\n", i, j, compiled[i][j]);

    wprintw(npatwin, "Hit RETURN to continue ");
    wrefresh(npatwin);
    wgetch(npatwin);
  }
  werase(npatwin);			/* Return to the main display */
}			/* end:   read_filter */

/*
 * Takes the string listed in pat[] as the starting point for a
 * new pattern and allows the user to edit that one to create a new one.
 * The backspace and erase keys are accepted - right now there is no
 * fancy editing.
 *
 * Places the newly entered string into pat[] upon returning.
 */
void get_filter(w, pat, patlen)
  WINDOW *w;
  char pat[];
  int patlen;
{
  int i;

  i = patlen;

  while ((pat[i]=wgetch(w)) != '\n')		/* Read one char at a time */
  {
    if (i > 0 && (pat[i]=='\b' || pat[i]==erasechar())) 
    {
      wprintw(w, "\b");			/* Backspace over last char */
      wdelch(w);				/* and delete it */
      wrefresh(w);
      i--;
      continue;
    }
    if (i < MAXLINELEN) 			/* Limit line length */
    {
      if (pat[i] == CTRL('u'))		/* erase line using ^u */
      {
	do 
	  wprintw(w, "\b"), wdelch(w) ;	/* backspace to beginning */
	while (--i) ;
	wrefresh(w);
	continue ;			/* get next character */
      }

      if (iscntrl(pat[i]))			/* Ignore ctrl chars */
	continue ;

      wprintw(w, "%c", pat[i++]);
      wrefresh(w);
      continue;
    } /* end: if(i < MAXLINELEN) */
  }	/* end: while() */


  pat[i] = '\0';				/* Don't forget '\0'! */
}


/*
 * The functions 'compile_pattern' and 'filter' allow NETCONSOLE to filter 
 * out only the information that the user desires.  Logical AND and OR 
 * is also incorporated in this feature, allowing input such as:
 *
 * airport | 13:27 & Error (prints status of the 'airport' sites as well
 *                          any errors that occurred at 13:27)
 * 
 * 'compile_pattern' works like this:
 *
 * A new argument is started every time an AND (&) operator is
 * encountered.
 * 
 * A new row of arguments is started every time an OR (|) operator is
 * encountered.
 *
 * So, if the input is: THIS & THAT | YOU & ME | WHATEVER | A & B & C
 * The pointers would be assigned as follows:
 *
 * compiled[0][0] = "THIS"		compiled[0][1] = "THAT"
 * compiled[1][0] = "YOU"		compiled[1][1] = "ME"
 * compiled[2][0] = "WHATEVER"
 * compiled[3][0] = "A"		compiled[3][1] = "B"	compiled[3][2] = "C"
 *
 * 'filter' filters out the desired information by comparing the
 * EVENT data structure against the arguments row by row, argument by
 * argument.
 *
 * Given the data above, 'filter' would first check arguments of
 * row 0.  If THIS (args[0][0]) was not in the data, the whole row can
 * be skipped because we know that (THIS & THAT) will be FALSE. 
 *
 * 'filter' returns 1 if any of the rows of arguments match,
 * otherwise it returns 0.
 *
 */

	
/*+ 
 * FUNCTION: compile_pattern  
 *
 * Copies the string of arguments 'pattern' to a temporary string,
 * eliminating invalid characters and converting valid characters to
 * lowercase.
 *
 * The locations of the arguments within 'pattern' are stored in the
 * global static two-dimensional array of pointers 'compiled[row][col]'.  
 *
 * If an AND (&) is encountered, it is interpreted as a logical AND,
 * and means that the argument to follow is to included in the current
 * row (or group) of arguments.  Therefore, 'col' is increased by
 * 1, while 'row' remains unchanged.
 *
 * If an OR (|) is encountered, it is interpreted as a logical OR,
 * and means that the argument to follow is to be included in a new
 * row (or group) of arguments.  Therefore, 'row' is increased by 1,
 * and 'col' is set to zero (to start at the beginning of the row).
 *
 * Spaces and tabs are ignored, and any other characters are added to
 * the end of the current argument.
 */


void compile_pattern(pattern)
  char *pattern;
{
  static char text[MAXTEXTLEN];		/* Cooked form of pattern */
  char *s, *t;
  int row, col;
  int was_conjunction;			/* Was previous word AND/OR? */

  bzero(compiled, sizeof(compiled));
  /*
   * We don't want to see a conjunction right off the bat,
   * so pretend we just saw one...  then we'll ignore any
   * conjunctions that come before normal arguments
   */
  was_conjunction = 1;

  /* Run through the pattern one character at a time */
  t = text;
  s = pattern;
  row = col = 0;
  while (*s != '\0')
    switch(*s) {
    case ' ': case '\t': 			/* skip white space */
      s++; break;	   

    case '|' :				/* OR */
      /* If the last thing was a conjunction, ignore this one */
      if (! was_conjunction) {		/* Begin a new row */
	*t++ = '\0';
	compiled[row][++col] = NULL ;	/* Terminating null */
	compiled[++row][0] = ++t;
	col = 0;

	was_conjunction = 1;	       /* Remember we saw a conj. */
      }
      s++; break;
	      
    case '&' :				/* AND */
      /* If the last thing was a conjunction, ignore this one */
      if (! was_conjunction) {
	*t++ = '\0';			/* Goto next column */
	compiled[row][++col] = t;

	was_conjunction = 1;		/* Remember we saw a conj. */
      }
      s++; break;

    default:				/* any normal character */
      *t = tolower(*s);			/* Make everything lowercase */

      /*
       * If the last thing was a conjunction, then this must
       * be the start of a normal argument - so save a pointer
       * to the beginning of this word in sargs[][]
       *
       * Then, t will just skip over the rest of the characters
       * in the word
       */
      if (was_conjunction)
	compiled[row][col] = t;
      t++;
      s++;

      /* Remember this wasn't a conjunction */
      was_conjunction = 0;
      break;
    } /* end of switch */

  /* By the way, we're NOT in the while loop anymore */

  *t='\0';				/* Don't forget the NUL terminator */

  /* Deal with the case where the last thing we saw was a conjunction */
  if (was_conjunction)
    compiled[row][col]=NULL;

  /*
   * Add a NULL terminator to our last row and also our list of rows
   * so we know where the end is (remember, the arguments are global)
   */
  compiled[row][++col]=NULL;		/* terminate present row */
  compiled[++row][0]=NULL;		/* end of rows */
}	/* end of compile_pattern */


/*+ 
 * Searches for arguments in the data structure 'v' by their
 * respective groups as stored in 'args[row][argnum]'.
 *
 * Searching begins with the first row, argument by argument.  If an
 * argument is not found in 'v' then searching stops in that row and
 * moves to the next.
 *
 * If all arguments in a row are found in 'v' then this EVENT structure
 * matches are filter condition and we may return TRUE immediately.
 */

int eventfilter(pevt)
  EVENT *pevt;
{
  char line[1024];
  char *p;
  int row, col;

  if (!pattern || *pattern == '\0')	/* Return if no pattern specified, */
    return (1);
  /*
   * Throw everything in v into one big formatted line
   * This make searching a lot easier
   */
  switch(widescreen)
  {
  case 0:
    sprintf(line, FMT0, FIELDS0); break;
#ifdef FIELDS1
  case 1:
    sprintf(line, FMT1, FIELDS1); break;
#endif
#ifdef FIELDS2
  case 2:
    sprintf(line, FMT2, FIELDS2); break;
#endif
#ifdef FIELDS3
  case 3:
    sprintf(line, FMT3, FIELDS3); break;
#endif
#ifdef FIELDS4
  case 4:
    sprintf(line, FMT4, FIELDS4); break;
#endif
  default:
    sprintf(line, FMT0, FIELDS0); break;
  }

  for (p=line; *p != '\0'; p++)		/* Make everything lowercase */
    *p = tolower(*p);

  /* Loop through the rows one by one... */
  for (row=0; compiled[row][0] != NULL; row++)
  {
    /*
     * If all the arguments on this row match line[],
     * then return TRUE right away - this EVENT structure
     * should pass through the filter
     */
    for (col=0; compiled[row][col] != NULL; col++)
      if (strstr(line, compiled[row][col]) == NULL)	/* not matched */
	break;

    if (compiled[row][col] == NULL)	/* end of a row */
    {
      if (debug > 1)
	wprintw(MsgPanel.win, "\n(eventfilter) Matched %s", line);
      return(1);				/* Match found */
    }

    /*
     * There were still arguments left on this row.
     * I guess line[] doesn't match this row - so let's try another!
     */
  }
  return(0);					/* Match not found */

} 	/* eventfilter() */
