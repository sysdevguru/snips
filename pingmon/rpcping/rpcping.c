/* $Header: /home/cvsroot/snips/pingmon/rpcping/rpcping.c,v 1.1 2008/04/25 23:31:51 tvroon Exp $ */

/*
 * This program checks to see the status of the RPC portmapper on the
 * specified system. It is useful for Unix hosts since they could be
 * down and still repond to ICMP 'ping's.
 *
 * Exit values:
 *    0 if all ok, non-zero if not.
 *
 * AUTHOR:
 *	Original version:  Viktor Dukhovni
 *	SNIPS version: Vikas Aggarwal, vikas@navya.com
 *
 * This version for SNIPS is derived from a public domain program whose
 * Copyright statement is attached below.
 *
 */

/*
 * $ Original Id: rpcping.c,v 1.1 1994/02/14 18:22:32 viktor Exp $
 * Author: Viktor Dukhovni
 *
 * Copyright (c) 1993, 1994 Lehman Brothers Inc.,
 * Permission is hereby granted to redistribute in source form
 * as long as this copyright notice remains intact.
 *
 * 			 NO WARRANTY
 *  
 * BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
 * FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
 * OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
 * PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
 * OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
 * TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
 * PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
 * REPAIR OR CORRECTION.
 *  
 * IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
 * WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
 * REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
 * INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
 * OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
 * TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
 * YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
 * PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * $Log: rpcping.c,v $
 * Revision 1.1  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/11 03:41:05  vikas
 * Initial revision
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#if defined(__svr4__) || defined(SVR4)
# define bzero(b,n)	memset(b,0,n)
# define bcmp(a,b,n)	memcmp(a,b,n)
# define bcopy(a,b,n)	memcpy(b,a,n)
#endif

static struct timeval tv;
static int timed_out = 0;

#define DEFAULT_TIMEO 15

int main(argc,argv)
     char **argv;
{
    int sock = RPC_ANYSOCK;
    int ch;	/* getopt() returns an int, not a char */
    char *host;
    char *clnt_sperror();
    CLIENT *clnt;
    static struct sockaddr_in addr;
    static char res;
    void alarmed();
    extern int  optind;
    extern char *optarg;

    tv.tv_sec = DEFAULT_TIMEO;
    tv.tv_usec = 0;

    bzero ((char *)&addr, sizeof (addr));

    /*	if (!isatty(0)) {
     *		fclose(stdout);
     *		fclose(stderr);
     *	}
     */

    while((ch = getopt(argc, argv, "t:")) != EOF)
      switch (ch) 
      {
      case 't':
	tv.tv_sec = atoi(optarg);
	break;
      default:
	fprintf(stderr,
		"Usage: rpcping [-t timeout] hostname \n");
	exit(1);
      }	/* end switch() */

    argc -= optind;
    argv += optind;

    if(argc == 0) {
      fprintf(stderr, "No hostname supplied\n");
      exit(1);
    }

    host = argv[0];
    if ( (addr.sin_addr.s_addr = inet_addr(host)) == (unsigned int)-1)
    {
      struct hostent *hp;
      
      if ( (hp = gethostbyname(host)) == NULL )
      {
	fprintf(stderr,"rpcping: Bad hostname '%s'\n", host);
	exit(1);
      }
      bcopy(hp->h_addr, (char *)&addr.sin_addr.s_addr, hp->h_length);
    }

    addr.sin_port = htons(PMAPPORT);
    addr.sin_family = AF_INET;

    (void)signal(SIGALRM,alarmed);
#ifndef SUNOS5
    alarm(tv.tv_sec);
    if (!(clnt=(CLIENT *)clnttcp_create(&addr, PMAPPROG, PMAPVERS, &sock, 0, 0)))
#else
      /*
       * According to sybalsky@eng.ascend.com and kehlet@dt.wdc.com,
       * clnttcp_create() does not time out on Solaris, so should use 
       * clnt_create_timed() instead.
       */
    if (!(clnt=(CLIENT *)clnt_create_timed(host, PMAPPROG, PMAPVERS,
					   "tcp",  &tv)))
#endif
    {
	if (timed_out) {
	  printf("%s: Call to portmapper timed out\n", host);
	  exit(2);
	}
	printf ("%s\n", (char *)clnt_spcreateerror(host));
	exit(1);
    }
    alarm(0);

    clnt_control(clnt, CLSET_TIMEOUT, (char *)&tv);

    bzero((char *)&res, sizeof(res));
    if (clnt_call(clnt, PMAPPROC_NULL, (xdrproc_t) xdr_void, NULL,
    		  (xdrproc_t) xdr_void, NULL, tv) != RPC_SUCCESS)
    {
	printf ("%s\n", clnt_sperror(clnt, "Call to Portmapper failed: "));
	exit(1);
    }
    printf("%s: Portmapper is running\n", host);
    exit(0);
}

void
alarmed()
{
    timed_out = 1;
}
