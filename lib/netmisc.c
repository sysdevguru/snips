/* $Header: /home/cvsroot/snips/lib/netmisc.c,v 1.1 2008/04/25 23:31:50 tvroon Exp $ */
/*
 * Network utility functions.
 *
 * 	Vikas Aggarwal  (vikas@navya.com)
 */

/*
 * $Log: netmisc.c,v $
 * Revision 1.1  2008/04/25 23:31:50  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/11 03:30:32  vikas
 * Initial revision
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


/*
**
**
**  function to figure ipv6 hostname
**
*/


int get_inet_address6(addr)
  char *addr;

{
	struct addrinfo hints;
	struct addrinfo *res;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;

	int ret_ga = getaddrinfo(addr, NULL, &hints, &res);
	if (ret_ga) {
		//point to go for ipv4 version
		fprintf(stderr, "ping6: %s\n", gai_strerror(ret_ga));
		return (-1);
	}

	if (!res->ai_addr)
		{
			//point to go for ipv4 ping
			return (-1);
		}
	
	return 1;
}

/*
 * Instead of inet_addr() which can only handle IP addresses, this handles
 * hostnames as well...
 * Return -1 on error, 1 if okay.
 */
int get_inet_address(saddr, host)
  struct sockaddr_in *saddr;		/* must be a malloced structure */
  char *host;
{
  register struct hostent *hp;
  struct sockaddr_in sin, *s;

  if (host == NULL || *host == '\0')
    return (-1);

  if ( (s = saddr) == NULL)
    s = &sin;
  bzero((char *)s, sizeof (struct sockaddr_in));

  s->sin_family = AF_INET;

  s->sin_addr.s_addr = (u_long) inet_addr(host);
  if (s->sin_addr.s_addr == -1 || s->sin_addr.s_addr == 0) {
    if ((hp = gethostbyname(host)) == NULL) {
      if (get_inet_address6(host) < 0 )
      	{
      		fprintf(stderr, "%s is unknown host\n", host);
      		return (-1);
      	}
      		//fprintf(stderr, "%s is a IPV6 host\n", host);
      return 1;
    }
#ifdef h_addr		/* in netdb.h */
    bcopy((char *)hp->h_addr, (char *)&(s->sin_addr), hp->h_length);
#else
    bcopy((char *)hp->h_addr_list[0], (char *)&(s->sin_addr), hp->h_length);
#endif
  }
  return 1;	/* OK */
}
