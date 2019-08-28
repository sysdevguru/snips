/* $Header: /home/cvsroot/snips/lib/daemon.c,v 1.1 2008/04/25 23:31:50 tvroon Exp $ */

/*
 * Create and fork a daemon process. As posted by andrewg@microlise.co.uk
 * which is a variation of the BSD4.4 daemon.c process
 *
 * Steps are:
 *	-fork
 *	-become a process group leader to avoid recving signals
 *	-disassociate from control TTY
 *	-ensure dont reacquire a control TTY (BSD & SysV have different rules)
 *	-close all open file descriptors
 *	-setup signal handlers to prevent zombie children
 *
 *  Vikas Aggarwal  vikas@navya.com
 */

/*
 * $Log: daemon.c,v $
 * Revision 1.1  2008/04/25 23:31:50  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 22:19:38  vikas
 * Lib routines for SNIPS
 *
 */

#ifndef lint
 static char rcsid[]  = "$RCSfile: daemon.c,v $ $Revision: 1.1 $ $Date: 2008/04/25 23:31:50 $" ;
 static char sccsid[] = "$RCSfile: daemon.c,v $ $Revision: 1.1 $ $Date: 2008/04/25 23:31:50 $" ;
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef SIGCHLD	/* in BSD machines, define a signal handler for the child */
void sig_child()
{
# ifndef NOWAITPID
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0)	/* this is the default */
    ; 	/* wait forever */
# else
#  ifdef WNOHANG
  union wait	status;
  while (wait3((int *)&status, WNOHANG, (struct rusage *) NULL) > 0)
    ;		/* avoid zombie children */
#  else /* WNOHANG */
  int status;
  while (wait(&status) > 0)
    ;
#  endif /* WNOHANG */
# endif /* NOWAITPID */
# ifdef SYSVSIGNALS
  signal(SIGCHLD, sig_child);	/* re-install the signal handler */
# endif

}	/* sig_chld() */

#endif /* SIGCHLD */

/*
 * Daemon() - Create a daemon which does not create further zombie children.
 * Returns -1 on failure, but you can't do much except exit in that case,
 * since we may already have forked. This is based on the BSD version,
 * so the caller is responsible for things like the umask, etc.
 *
 * Believed to work on all Posix systems
 */
int Daemon(pidfile)
  char *pidfile;		/* can be NULL if no PID file */
{
  int fd;

  switch (fork())
  {
  case 0:  break;
  case -1: return -1;
  default: _exit(0);          /* exit the original process */
  }
    
  if (setsid() < 0)               /* become a process group leader & no TTY */
    return -1;

  /*
   * At this point, our daemon does not have a controlling TTY. However, we
   * have to prevent it (and its children) from reacquiring a control TTY.
   *  -Under BSD, if pgpid != 0 (i.e. parent is still alive) then the
   *   children cannot acquire a control TTY.
   *  -Under SysV, if the process is not a process group leader, then the
   *   children cannot acquire a control TTY
   */

  /* dyke out this switch if you want to acquire a control tty in the
   * future - not normally advisable for daemons
   */
  switch (fork())
  {
  case 0:  break;
  case -1: return -1;
  default: _exit(0);
  }
    
/*  chdir("/");	/* Might not want to change directory */
  umask(002);	/* ensure 'o-w' for new files */

  /* close all file descriptors starting from 0 */
  fd = 0;
#ifdef _SC_OPEN_MAX
  while (fd < sysconf(_SC_OPEN_MAX))
#else
    while (fd < getdtablesize())
#endif
      close(fd++);
  if ( (fd = open("/dev/null", O_RDWR) != -1))	/* stdin */
  {
    dup(fd);
    dup(fd);			/* stdout, stderr */
  }

  /*
   * Setup signal handlers to prevent zombie children, The alternative
   * is that the process calls fork2() instead of fork() (fork2 is defined
   * further on in this file).
   */
#if defined(SIGTSTP) || !defined(linux)		/* BSD */
  signal(SIGCHLD, sig_child);		/* have to reap the child */
#else
  signal(SIGCLD, SIG_IGN);		/* SYS V calls SIGCHLD -> SIGCLD */
#endif

  return 0;

}	/* end Daemon() */

/*+ fork2()
 * Like fork, but the new process is immediately orphaned (won't leave a
 * zombie when it exits). Use instead of a fork() if no signal handler
 * is defined.
 * Returns 1 to the parent, not any meaningful pid.
 * The parent cannot wait() for the new process (it's unrelated).
 */

int fork2()
{
  pid_t pid;
  int status;

  if (!(pid = fork()))
  {
    switch (fork())
    {
    case 0:  return 0;
    case -1: _exit(errno);    /* assumes all errnos are <256 */
    default: _exit(0);
    }
  }

  if (pid < 0 || waitpid(pid,&status,0) < 0)
    return -1;

  if (WIFEXITED(status))
    if (WEXITSTATUS(status) == 0)
      return 1;
    else
      errno = WEXITSTATUS(status);
  else
    errno = EINTR;  /* well, sort of :-) */

  return -1;
}
