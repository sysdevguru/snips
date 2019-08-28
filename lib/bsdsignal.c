/* $Header: /home/cvsroot/snips/lib/bsdsignal.c,v 1.0 2001/07/08 22:19:38 vikas Exp $ */

#if defined(SVR4) && !defined(BSDSIGNAL)

/*
 * Emulate BSD signal routines for SVR4 machines. From page 298 of Stevens'
 * "Advanced Programming in Unix Environment" book.
 *
 *	-Vikas Aggarwal  vikas@navya.com
 */

#include <signal.h>

void (*bsdsignal(signo, func))
     int signo;
     void *func;
{
    struct sigaction	act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);act.sa_flags = 0;
    if (signo == SIGALRM)
    {
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;	/* SunOS */
#endif
    }
    else
    {
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;	/* SVR4, BSD */
#endif
    }

    act.sa_flags |= (SA_SIGINFO|SA_ONSTACK);

    if (sigaction(signo, &act, &oact) < 0) {
	perror ("bsdsignal (sigaction):");
	return(SIG_ERR);	/* return a value only if not void() */
    }
    return(oact.sa_handler);	/* return a value only if not void() */

}


#else	/* SVR4 */
void (*bsdsignal ())		/* dummy function to keep linker happy */
{
    return (void *)0;
}
#endif	/* SVR4 */

