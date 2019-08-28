/* $Header: /home/cvsroot/snips/include/osdefs.h,v 1.0 2001/07/12 04:30:26 vikas Exp $ */
/*
 * All possible operating system defines.
 */

/*
 * Try and define the OS using the Makefile & uname -s -r 
 * Typically the compiler will set the UPPERCASE define for the
 * operating system, since most compilers dont have the symbols
 * to detect things automatically.
 * What we really want to define or undefine are the following:
 *
 * USE_NCURSES	(include ncurses.h instead of curses.h)
 * BROKENCURSES (have to use cury instead of _cury)
 * INTSIGNALS	(if signal() is not void, but return's integer instead)
 *
 */

#ifndef _osdefs_h
#define _osdefs_h
/*
 * Try and detect the OS automatically. This might work on some systems
 */
#if defined(__FreeBSD__) || defined(FREEBSD4)
# ifndef FREEBSD
#  define FREEBSD
# endif
#endif

#ifdef linux
# ifndef LINUX
#  define LINUX
# endif
#endif


/*
 * Following are UPPERCASE defines for the operating system, which
 * then set the program specific defines like BROKENCURSES, etc.
 */

#if defined(IRIX40) || defined(IRIX51)
# define HAVEGIDTYPE
# ifdef IRIX51
#  define _BSD_SIGNALS
# endif
#endif

#if defined(SUNOS5) || defined(SOLARIS2)
# define NOGETDTABLE
# define HAVEGIDTYPE
# ifndef SYSV
#  define SYSV
# endif
# ifndef POSIX
#  define POSIX
# endif
#endif

#if defined(SVR4) || defined(SVR3) || defined(AUX) || defined(HPUX)
# ifndef POSIX
#  define POSIX
# endif
# ifndef SYSV
#  define SYSV
# endif
#endif

#if defined(SYSV) || defined(SVR4) || defined(__svr4__)
# define index          strchr
# define rindex         strrchr
# define signal         sigset
# define bzero(b,n)     memset(b,0,n)
# define bcmp(a,b,n)    memcmp(a,b,n)
# define bcopy(a,b,n)   memcpy(b,a,n)
#endif

#if defined(SVR3)
# define NOFSYNC
# define NOINITGROUPS
#endif

#ifdef AUX
# define NOFSYNC
# define NOINITGROUPS
# define memmove memcpy
# define _POSIX_SOURCE
#endif

#ifdef HPUX
# define NOGETDTABLE
# define HAVEGIDTYPE
#endif

#if defined(LINUX1) || defined(LINUX2)
# define LINUX		/* see added definitions below */
# define HAVEGIDTYPE
#endif

#if defined(LINUX) || defined(OSF1)
# define USE_NCURSES
#endif

#ifdef SYSV
# define SRANDOM(S)	srand((S))
# define  RANDOM()	 rand()
#else
# define SRANDOM(S)	srandom((S))
# define  RANDOM()	 random()
#endif

#ifdef __NetBSD__
# define BROKEN_CURSES
#endif

#if defined(BSDI) && !defined(BSDI4)
# define BROKEN_CURSES
#endif

/* Most modern OSs already have strerror() */
#ifndef HAVE_STRERROR
# define HAVE_STRERROR 1
#endif

#endif	/* _osdefs_h_ */
