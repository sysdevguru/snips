/*
 * $Header: /home/cvsroot/snips/nsmon/nsmon.c,v 1.1 2008/04/25 23:31:51 tvroon Exp $
 */

/*+ 
 * Make a domain nameserver query and send it off to the server to
 * test if it is up and running.
 *
 * This is a rewrite of the original version that was distributed in NOCOL.
 * The logic is from nslookup.
 */

/* Copyright 2001, Netplex Technologies Inc. */

/*
 * $Log: nsmon.c,v $
 * Revision 1.1  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/11 03:36:41  vikas
 * Initial revision
 *
 */

#include "nsmon.h"
#include "netmisc.h"

/*
 * return values:
 *	integer (defined in nsmon.h)
 */

int nsmon (host, querystr, querytype, timeout, aa_only, debugflag)
  char *host;			/* host to be queried */
  char *querystr;		/* dns query string */
  int  querytype;		/* T_SOA, T_A, T_NS (see nameser.h) */
  int  timeout;			/* in seconds */
  int  aa_only;			/* 1 if auth answer needed */
  int  debugflag;
{
  int n;
  char qbuf[BUFSIZ], ansbuf[BUFSIZ];
  struct sockaddr_in *psockaddr;
  HEADER *hp;				/* struct defined in arpa/nameser.h */
  
  psockaddr = &(_res.nsaddr) ;
  if (get_inet_address(psockaddr, host) < 0)	/* snips library function */
    return(NS_ERROR);

  psockaddr->sin_port = htons(NAMESERVER_PORT);

  /*
   * Create dns query (set RES_INIT so that we dont call res_init()
   */
  _res.options = RES_INIT;		/* don't call res_init() */
  _res.options |= RES_PRIMARY;		/* only query primary server */
  _res.options &= ~RES_USEVC ;		/* use UDP and not TCP */
  if (aa_only) {
    _res.options &= ~RES_RECURSE ;	/* no recursion */
    _res.options &= ~RES_DNSRCH ;	/* don't search up domain tree */
    _res.options |= RES_AAONLY; 	/* only authoritative answers */
  }
  else {
    _res.options |= RES_RECURSE;	/* do recursion since not AA */
    _res.options |= RES_DNSRCH;		/* search up dns tree */
  }
  if (timeout > 0)
     _res.retrans = timeout;		/* timeout in seconds */
  if (debugflag > 1)
  {
    _res.options |= RES_DEBUG;
    fprintf(stderr, "(debug):\t _res.options = %ld\n", _res.options);
    fprintf(stderr, "        \t _res.defdname = %s\n", _res.defdname);
  }

  _res.nscount = 1 ;			/* number of nameservers */
  _res.retry = 2; 

  /*
   * Call res_mkquery() to build a query structure in 'buf'
   * 'QUERY' is  defined in nameser.h.
   */
  n = res_mkquery(QUERY, querystr, C_IN, querytype,
		  (char *)NULL, 0, NULL, (char *)qbuf, sizeof(qbuf));

  if (n < 0) {
    if (debugflag) {
      fprintf(stderr, "ERROR nsmon: res_mkquery() ");
      perror(host);
    }
    return NS_ERROR;
  }

  if (debugflag > 1) {
    fprintf(stderr, "(debug):\t _res.options = %ld\n", _res.options);
    fprintf(stderr, "        \t _res.defdname = %s\n", _res.defdname);
    fflush(stderr);
  }

  n = res_send(qbuf, n, ansbuf, sizeof(ansbuf) ) ;	/* send query */
  
  if (n < 0) {
    if (debugflag) {
      fprintf(stderr, "ERROR nsmon: res_send() ");
      perror(host);
    }
    return NS_ERROR;
  }

  hp = (HEADER *)ansbuf ;
  
  if (hp->rcode != NOERROR) {
    if (debugflag)
      fprintf(stderr, "(debug): got rcode of %s from %s\n",
	      Rcode_Bindings[hp->rcode], host);
    return NS_ERROR;
  }

  if (ntohs(hp->ancount) == 0)	{ /* no error, but no data either */
    if (debugflag)
      fprintf(stderr, "(debug): got rcode NOERROR, but no answer data\n");
    return NO_NSDATA;
  }

  /*
   * The AA_ONLY option is not implemented by most resolvers, so we have
   * to check if the answer is actually what we asked for.
   */
  if (aa_only && !(hp->aa))
  {
    if (debugflag)
      fprintf(stderr, "(debug): Non-Authoritative response for %s from %s\n",
	      host, querystr);
    return NOT_AUTHORITATIVE;
  }
  
  return ALL_OK;		/* Don't check the response, must be okay */

}	/* end nsmon() */

#ifdef TEST

/* This is already provided through netmisc.h */
#if 0
get_inet_address(saddr, host)
  struct sockaddr_in *saddr;		/* must be a malloced structure */
  char *host;
{
  register struct hostent *hp;
  struct sockaddr_in sin, *s;

  if ( (s = saddr) == NULL)
    s = &sin;
  bzero((char *)s, sizeof (struct sockaddr_in));

  s->sin_family = AF_INET;

  s->sin_addr.s_addr = (u_long) inet_addr(host);
  if (s->sin_addr.s_addr == -1 || s->sin_addr.s_addr == 0) {
    if ((hp = gethostbyname(host)) == NULL) {
      fprintf(stderr, "%s is unknown host\n", host);
      return (-1);
    }
#ifdef h_addr		/* in netdb.h */
    bcopy((char *)hp->h_addr, (char *)&(s->sin_addr), hp->h_length);
#else
    bcopy((char *)hp->h_addr_list[0], (char *)&(s->sin_addr), hp->h_length);
#endif
  }
  return 1;	/* OK */
}
#endif

usage()
{
  fprintf(stderr,
	  "Usage: nstest [ -d ] [-A (AA only)] [ -T query type ] [ -t timeout (secs) ] \n");
  fprintf(stderr, "\t<nameserver>  <request>  [ <nameserver> <request> ]...\n");
  fprintf(stderr, "  e.g.\t nstest -A -T 6 ns1.navya.com navya.com\n");
  fprintf(stderr, "  type T_SOA=6, T_A=1, T_ANY=255\n");
  exit (1);
}

main(argc, argv)
  int argc;
  char *argv[];
{
  int type, timeout, ch;
  int aaonly = 0, debug = 0;
  extern char *optarg;
  extern int optind;

  type = T_SOA; timeout = 15;

  while ((ch = getopt(argc, argv, "AdT:t:")) != EOF)
  {
    switch (ch)
    {
      case 'A':
        aaonly = 1; break;
      case 'd':
        ++debug ; break;
      case 'T':
        type = atoi(optarg); break;
      case 't':
        timeout = atoi(optarg); break;
      default:
	usage();
	break;
    }
  }	/* while () */

  argc -= optind;
  if (argc < 1) {
    fprintf(stderr, "nstest: no servers/requests were specified\n");
    usage();
    return 1;
  }
  for (argv += optind; argc >=2; argc -=2, argv +=2)
  {
    int ret = nsmon(argv[0], argv[1], type, timeout, aaonly, debug);

    printf("Server: %s  Request: %s ", argv[0], argv[1]);
    switch (ret)
    {
    case ALL_OK: printf("is OK\n"); break;
    case NS_ERROR: printf("gave ERROR\n"); break;
    case NOT_AUTHORITATIVE: printf("gave non-authoritative answer\n"); break;
    case NO_NSDATA: printf("is OK but no data ??\n"); break;
    default: printf("unknown return code\n"); break;
    }
  }
  if (argc) 
    fprintf(stderr, "nstest: Missing request\n");
  return 0;
}

#endif /* TEST */

