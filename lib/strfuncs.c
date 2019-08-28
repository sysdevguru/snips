/* $Header: /home/cvsroot/snips/lib/strfuncs.c,v 1.0 2001/07/08 22:19:38 vikas Exp $ */

/*+
 * String utility functions for SNIPS.
 *
 */

/*
 * $Log: strfuncs.c,v $
 * Revision 1.0  2001/07/08 22:19:38  vikas
 * Lib routines for SNIPS
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Id: strfuncs.c,v 1.0 2001/07/08 22:19:38 vikas Exp $" ;
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

char *Strdup() , *Strcasestr(), *skip_spaces();
char *raw2newline();

/*+ 
 * FUNCTION:
 * 	Duplicate a string. Can handle a NULL ptr.
 */
char *Strdup(s)
  char *s ;
{
  char *t ;

  if (s == NULL)
  {
    t = (char *)malloc(1);
    *t = '\0';
  }
  else
  {
    t = (char *)malloc(strlen(s) + 1);
    if (t != NULL)
      (char *)strcpy(t, s);
  }

  return (t);
}

/*
 * Our very own strstr() function. Does a case-insensitive match.
 * Finds the first occurrence of the substring 'needle' in the
 * string 'haystack'.
 * Returns
 *	Ptr in 'haystack' to the begining of 'needle'
 *	Ptr to 'haystack' if needle is empty
 *	NULL if needle is not found.
 *
 */
char *Strcasestr(haystack, needle)
  char *haystack;	/* string */
  char *needle; 	/* string to find */
{
  char p, q;
  size_t needlelen;

  if (haystack == NULL)
    return NULL;

  if (needle == NULL || *needle == 0) 
    return (char *)haystack;
    
  p = *needle++;
  needlelen = strlen(needle);

  for ( ; ; ) 
  {
    register char *s1, *s2;
    int n;

    do
    {		/* match first character */
      if ((q = *haystack++) == 0)
	return (NULL);
    } while ( tolower(q) != tolower(p) );

    s1 = haystack; s2 = needle; n = needlelen;

    do
    {
      if (*s1 == 0 || tolower(*s1) != tolower(*s2++))	/* */
	break;
      ++s1;
    } while (--n != 0);

    if (n == 0) {
      --haystack;
      break;		/* found, break out of forever loop */
    }
    if (*s1 == 0) {
      haystack = NULL;
      break;		/* not found, break out of forever loop */
    }

  }	/* end for */
  return ((char *)haystack);
}	/* Strcasestr() */


/*+ 
 * FUNCTION:
 * 	Skip leading spaces.
 */
char *skip_spaces(str)
  char *str ;
{
  if (str == NULL || *str == '\0')
    return (str);
  while (*str == ' '  || *str == '\t')
    ++str ;				/* skip leading spaces */

  return(str) ;
}

/*
 * Takes a string containing one or many sequences of "\r\n" and replaces
 * these two chars with a newline. The original string is modified directly.
 */
char *raw2newline(inbuf)
  char *inbuf;
{
  register char *p, *q;

  if(!inbuf)
    return (char *)NULL;

  p = q = inbuf;

  for (; ;)
  {			/* forever() */
    while (*p && *p != '\\')
      *q++ = *p++;

    if (*p == '\0') {
      *q = '\0';
      return(inbuf);
    }

    switch (*(p+1))
    {
    case 'n':
      *q++ = '\n';
      p = (p+2);	/* skip \ and n */
      break;

    case 'r':
      *q++ = '\r';
      p = (p+2);	/* skip \ and r */
      break;

    default:
      *q++ = *p++;
      break;
    }	/* end switch */

  }	/* end forever () */

}	/* raw2newline() */
