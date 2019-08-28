/***************************************************************************
 *
 *           Copyright 1998 by Carnegie Mellon University
 * 
 *                       All Rights Reserved
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of CMU not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * 
 * CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * 
 * Author: Ryan Troll <ryan+@andrew.cmu.edu>
 * 
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef WIN32
#include <memory.h>
#include <winsock2.h>
#else  /* WIN32 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <arpa/inet.h>
#endif /* WIN32 */

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif /* HAVE_SYS_SOCKIO_H */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STRINGS_H
# include <strings.h>
#else /* HAVE_STRINGS_H */
# include <string.h>
#endif /* HAVE_STRINGS_H */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifndef WIN32

#ifdef    HAVE_UTMPX_H
#define UTMPX
# include <utmpx.h>
#else  /* HAVE_UTMPX_H */
#define UTMP
# include <utmp.h>
#endif /* HAVE_UTMPX_H */

#endif /* WIN32 */

#include "uptime.h"

static char rcsid[] = 
"$Id: uptime.c,v 1.5 1998/05/08 20:22:38 ryan Exp $";

/***************************************************************************
 *
 ***************************************************************************/
/* Return this host's uptime */
time_t uptime(void)
{
#ifdef WIN32
  return(0); /* XXXXX */
#else /* WIN32 */

# ifdef HAVE_PROC_UPTIME

  FILE *fp;
  long seconds;

  fp = fopen ("/proc/uptime", "r");
  if (fp != NULL) {
    char buf[BUFSIZ];
    int res;
    fgets(buf, BUFSIZ, fp);
    res = sscanf (buf, "%lf", &seconds);
    if (res == 1)
      uptime = (time_t) seconds;
    fclose (fp);
  }
  return(uptime);
# else /* HAVE_PROC_UPTIME */

  /* Use utmp 
   */
#ifdef UTMPX
  struct utmpx *ut;
  time_t time_now;
  time_t boot_time = 0;

  while ((ut = getutxent()) != NULL) {

    if (ut->ut_type == BOOT_TIME)
      boot_time = ut->ut_tv.tv_sec;
  }

  /* Read through utmp.  Now calculate the uptime. */
  time_now = time(0);
  if (boot_time)
    return(time_now - boot_time);
  return(0);

#else  /* UTMPX */
  struct utmp *ut;
  time_t time_now;
  time_t boot_time = 0;

#ifdef HAVE_GETUTENT

  while ((ut = getutent()) != NULL) {

    /* If BOOT_MSG is defined, use it for the boottime */
#ifdef BOOT_MSG
    if (!strcmp(ut->ut_line, BOOT_MSG))
      boot_time = ut->ut_time;
#endif /* BOOT_MSG */
  }

  /* Read through utmp.  Now calculate the uptime. */
  time_now = time(0);
  if (boot_time)
    return(time_now - boot_time);
  return(0);

#else /* HAVE_GETUTENT */
  fprintf(stderr, "Unable to fetch boottime!\n");
  return(0);
#endif /* HAVE_GETUTENT */

#endif /* UTMPX */

# endif /* HAVE_PROC_UPTIME */
#endif /* WIN32 */
}
