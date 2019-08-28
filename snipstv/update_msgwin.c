/*
 * $Header: /home/cvsroot/snips/snipstv/update_msgwin.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $
 */

/*+
 * Update the MSGS window. Fill with msgfiles from the msgs directory.
 *
 * Similar to the update_eventwin.c program (derived from that).
 *
 */

/*
 * $Log: update_msgwin.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */

#ifndef lint
  static char rcsid[] = "$Id: update_msgwin.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>	/* for fstat() */
#include <unistd.h>

#include <sys/param.h>

#include "snipstv.h"

#ifndef MAXPATHLEN
# define MAXPATHLEN 256
#endif

extern int debug;		/* in snips.h, not included here */

int update_msgwin(win)
  WINDOW *win;
{
  int nrows;
  char msg[128];			/* string read from the file */
  char msgfile[MAXPATHLEN];
  static DIR  *dir;
  static FILE *file;
  extern char  *msgsdir;		/* defined in snipstv.h */
  extern bool frozen, silent;

#ifdef __NetBSD__
  nrows = getmaxy(win);
#else
  nrows = win->_maxy;		/* _maxy starts from 0 */
#endif

  if (!dir)
  {
    if (!msgsdir || (dir = opendir(msgsdir)) == NULL )
      return -1;
    waddstr(win, "\n");
  }

  while (nrows)
  {		/* until we write out nrows events */
    if (!file)
    {	/* if no open file */
      struct dirent *d;

      while ((d = readdir(dir)))
      {
	if (*(d->d_name) != '.' && strcmp(d->d_name, "core") != 0)
	{
	  struct stat filestat;

	  sprintf(msgfile, "%s/%s", msgsdir, d->d_name);
	  if (stat(msgfile, &filestat) < 0)
	    continue;
#ifdef S_ISREG
	  if (S_ISREG(filestat.st_mode))
#else
	  if (filestat.st_mode & S_IFMT == S_IFREG)
#endif
	    if ( (file = fopen(msgfile, "r")) != NULL)
	    {
	      if (debug) wprintw(MsgPanel.win,"\nOpened msgfile- %s", msgfile);
	      break;
	    }
	}
      }	/* while readdir() */

      if (!d)
      {
	closedir(dir);
	dir = NULL;
	goto done;
      }

    }	/* endif (!file) */

    if ( fgets(msg, sizeof(msg), file) == NULL )
    {
      fclose(file);
      file = NULL;	/* init so loop opens the next file */
      continue;
    }
    msg[strlen(msg) - 1] = '\0';	/* delete newline */

    if (*msg == '\0')
      continue;

    wprintw(win, "\n%s", msg);
    wclrtoeol(win);
    --nrows ;
  }	/* end while(nrows) */

 done:
  if (nrows)	/* end of data files */
  {
    if (debug > 1)
      wprintw(win, "\nDEBUG= %d, frozen=%s, neverbeep=%s, TERM=%s, LINES=%d",
	      debug, frozen ? "YES" : "NO", silent ? "YES":"NO",
	      ttytype, LINES);
    --nrows;
    wclrtoeol(win);		/* cannot clear till bottom since scrollok */
  }

  return (nrows);		/* 0 if screen is full, +ve if end of data */

}

