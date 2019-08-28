/* $Header: /home/cvsroot/snips/radiusmon/radiusmon.c,v 1.2 2008/04/25 23:31:52 tvroon Exp $ */

/*+ radiusmon.c
 *
 * Sends a radius AUTH packet to the remote server, and waits for a reply.
 * Returns 1 if OK, 0 if down.
 *
 * If IGNORE_AUTHCODE is defined, then we dont care about whether the
 * password was valid or not. Else, we want an AUTH_OK response.
 *
 */

/*
 * $Log: radiusmon.c,v $
 * Revision 1.2  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.1  2001/08/05 09:13:42  vikas
 * Added support for using NAS_PORT_TYPE. Needed by some radius servers.
 *
 * Revision 1.0  2001/07/08 22:51:22  vikas
 * For SNIPS
 *
 */

/* Copyright 2001, Netplex Technologies, Inc. info@netplex-tech.com */

#include "radiusmon.h"
#include "md5-local.h"

#ifdef TEST
int debug;
char *prognm;		/* required in libsnips */
#else
extern int debug;	/* Defined in main.c */
#endif

static char *get_response(), *make_radiuspkt();
static int  check_response(), get_inet_address();
void hash_password(char *pwd, int pwdlen, char *result, char *secret, char *vector);

/*+ radiusmon()
 * 	create socket
 * 	create radius packet
 * 	send radius packet
 * 	recieve response
 * 	check response
 */
int radiusmon(host, port, secret, user, pass, timeout, retries, porttype)
  char *host;	/* In dotted decimal addr */
  int  port;
  char *secret, *user, *pass;
  int timeout, retries, porttype;
{
  int sockfd = 0, nretries = retries ? retries : RETRIES;
  char	*querybuf, *respbuf;
  int	querylen, resplen;	/* buffer sizes */
  struct sockaddr_in hostaddr;

  if (port == 0) {
    port = RADIUS_PORT;
  }
  /** setup the socket for the destination **/
  if(!get_inet_address(&hostaddr, host))
    return 0;				/* indicate error */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  hostaddr.sin_family = AF_INET;
  hostaddr.sin_port = htons(port);

  if (debug > 4) {fprintf(stderr, "opened socket\n"); }	/* */
  querybuf = make_radiuspkt(secret, user, pass, &querylen, porttype);

  /* use connect() instead of a sendto() so that we can do a select() later */

  if (connect(sockfd, (struct sockaddr *)&hostaddr, sizeof(hostaddr)) < 0)
  {
    if (debug)  perror("connect");
    close(sockfd);
    return 0;
  }

  do
  {
    /** Send the packet **/
    if (write(sockfd, querybuf, querylen) == -1) {
      if (debug) perror("write");
      close(sockfd);
      return 0;
    }
    if (debug > 4)
      fprintf(stderr,"sent packet to %s port %d\n",
	      (char *)inet_ntoa(hostaddr.sin_addr), port);

    /** Read the response packet **/
    respbuf = get_response(sockfd, /*seq */1,
			   timeout ? timeout : TIMEOUT,
			   &resplen);

  } while (respbuf == NULL && --nretries);

  close(sockfd);

  /** check the response **/
  return (check_response(respbuf));

}	/* radiusmon() */


/*
 
 * Create a radius authentication query packet and send it off.
 * The data consists of a number of attributes:
 *	attribute type (username, password, nasIP, nasPort)
 *	length of attribute + 2
 *	attribute data
 *
 * The password needs to be hashed using the common secret and the 'vector'
 * which is sent along in the packet.
 */
static char *make_radiuspkt(secret, user, pass, pquerylen, porttype)
  char *secret, *user, *pass;
  int  *pquerylen, porttype;
{
  char		pktvector[32];
  static char	buf[1024];
  char		thishost[256];
  AUTH_HDR	*pkthdr;
  u_char	*pktdata;

  bzero(buf, sizeof(buf));
  /* create the packet header */
  pkthdr = (AUTH_HDR *)buf;
  pkthdr->code = 1;
  pkthdr->id = (getpid() % 256);
  pkthdr->length = 0;		/* fill in length of packet later */
  sprintf(pktvector, "%ld", time(NULL));
  bcopy(pktvector, pkthdr->vector, sizeof(pkthdr->vector));/* unique hash id */

  if (debug > 1)
    fprintf(stderr, " PktHdr: code=%d, id=%d, vector=%s\n",
	    pkthdr->code, pkthdr->id, pktvector);

  /* now create the packet. Remember to update pkthdr->length */
  pktdata = pkthdr->data;		/* point to start of data */

  if (*user) {
    *pktdata++ = PW_USER_NAME;		/* type */
    *pktdata++ = strlen(user) + 2;	/* size */
    bcopy(user, pktdata, strlen(user));
    pktdata += strlen(user);
  }

  if (*pass) {
    int  passLen, hashLen;

    if ( (passLen = strlen(pass)) > AUTH_PW_LEN)
      passLen = AUTH_PW_LEN;

    if (passLen % 16 > 0)
      hashLen = (passLen / 16 + 1) * 16;	/* md5 hash length */
    else
      hashLen = passLen;

    *pktdata++ = PW_PASSWORD;		/* type */
    *pktdata++ = 2 + hashLen;		/* size */
    bcopy(pass, pktdata, passLen);	/* password */
    /* encrypt the password */
    if (debug > 1) fprintf(stderr, " Hashing %s, len=%d, hashlen=%d, %s, %s\n",
			   pass, passLen, hashLen, secret, pkthdr->vector);
    hash_password(pass, passLen, pktdata, secret, pkthdr->vector);
    pktdata += hashLen;
  }

  /* add nasIP and nasPort */
  if (gethostname(thishost, sizeof(thishost)) >= 0)
  {
    UINT4	myip, myport;
    struct sockaddr_in myaddr;

    get_inet_address(&myaddr, thishost);
    myip = myaddr.sin_addr.s_addr;

    /* myip = htonl(myip);		/* dont need to do it */
    *pktdata++ = PW_NAS_ID;
    *pktdata++ = sizeof(myip) + 2;
    bcopy((char *)&myip, pktdata, sizeof(myip));
    pktdata += sizeof(myip);

    myport = htonl(SOURCE_PORT);	/* dummy port number, long not short */
    *pktdata++ = PW_NAS_PORT_ID;
    *pktdata++ = sizeof(myport) + 2;
    bcopy((char *)&myport, pktdata, sizeof(myport));
    pktdata += sizeof(myport);
  }

  if (porttype >= 0)		/* add only if specified by user */
  {
    UINT4  myporttype;

    myporttype = htonl(porttype);
    *pktdata++ = NAS_PORT_TYPE;
    *pktdata++ = sizeof(myporttype) + 2;
    bcopy((char *)&myporttype, pktdata, sizeof(myporttype));
    pktdata += sizeof(myporttype);
  }

  /* now put the packet length in the header */
  *pquerylen = (char *)pktdata - (char *)buf;
  pkthdr->length = htons(*pquerylen);

#ifdef DEBUG
  if (debug > 4) 	/* dump packet to stderr */
    dump_radpkt(pkthdr);
#endif
  return (buf);
  
}	/* make_radiuspkt() */

/*
 * Receives a packet from the radius server.
 */
static char *get_response(sockfd, sequence, timeout, presplen)
  int sockfd;
  int sequence;
  int timeout;
  int *presplen;
{
  int n;
  static char buf[1024];
  fd_set  fdvar;
  struct timeval tval ;

  bzero(buf, sizeof(buf));
  tval.tv_sec = timeout;
  tval.tv_usec = 0;

  /*
   * we loop until timeout to make sure that we dont get back an old
   * response. This is verified by the sequence number we sent...
   */
again:
  FD_ZERO(&fdvar);
  FD_SET(sockfd, &fdvar);
  if ((n = select(sockfd+1, &fdvar, NULL, NULL, &tval)) <= 0)
  {
    if (n < 0 && errno != EINTR)
    {
      perror("select() ");
      return (NULL);
    }
    else	/* read zero bytes or interrupted */
    {
      if (debug > 2)   fprintf(stderr, "select() timed out\n");
      return (NULL);	
    }
  }
  /* we dont do an FD_ISSET() since only one fd was set... */
  if ( (*presplen = read(sockfd, (char *)buf, sizeof(buf)) ) <= 0 )
  {
    if (*presplen < 0 && debug) perror("read");
    return NULL;
  }

  /*
   * Check the sequence number or something to see if this is an old
   * packet getting back to us.
   */
  /* if (check_seq(buf) < 0)
    goto again.
  */
#ifdef DEBUG
  if (debug > 4) {
    fprintf(stderr, "get_response() recieved %d bytes\n", *presplen);
    dump_radpkt(buf);
  }
#endif
  return buf;
}	/* get_response() */

/*
 * We check the response back. Technically we can just ignore the response
 * since getting something back means that the radiusd server is alive.
 * 
 */
static int check_response(rbuf)
  char *rbuf;
{
  AUTH_HDR	*pkthdr;

  if (rbuf == NULL)
    return (0);

  pkthdr = (AUTH_HDR *)rbuf;

  if (pkthdr->id != (getpid() % 256))	/* not sent by us */
  {
    if (debug) 
      fprintf(stderr, "Pkt ID does not match (%d sent, %d returned)\n",
	      getpid() % 256, pkthdr->id);
    return (0);
  }

#ifndef IGNORE_AUTHCODE	/* dont care if we get an OK (perhaps dummy passwd) */
  if (pkthdr->code == PW_AUTHENTICATION_ACK)
#endif
    return (3);	/* all OK */

  if (debug)
    fprintf(stderr, "Password auth failed (returned %d)\n", pkthdr->code);

  return (0);	/* failed */

}	/* check_response() */

/*
 * Utility routines
 */

#ifdef DEBUG
dump_radpkt(radpkt)
  AUTH_HDR *radpkt;
{
  char *data;
  int i, j;
  int len;

  data = radpkt->data;
  len = ntohs(radpkt->length);

  fprintf(stderr, "dump_radpkt() Pkt length %d\n", len);
  for (i = 0; i < len && data[i+1] > 0; i += data[i+1])
  {
    struct in_addr ipaddr;
    unsigned long myport;
    unsigned char x = (unsigned char)data[i];

    switch ((int)data[i])
    {
    case PW_USER_NAME:
      fprintf (stderr, "  Username = %.*s\n", data[i+1] - 2, data+i+2);
      break;
    case PW_PASSWORD:
      fprintf(stderr, "  Password = ");
      for (j=i+2; j < i+data[i+1]; ++j)
	if ( (x = (unsigned char)data[j]) < 32 || x > 126)
	  fprintf (stderr, "0x%x ", x);
	else
	  fprintf(stderr, "%c ", x);
      fprintf(stderr,"\n");
      break;
    case PW_NAS_ID:
      bcopy(data+i+2, &(ipaddr.s_addr), sizeof(ipaddr.s_addr));
      fprintf(stderr, "  NasIP = %s\n", (char *)inet_ntoa(ipaddr) );
      break;
    case PW_NAS_PORT_ID:
      bcopy(data+i+2, &myport, sizeof(myport));
      fprintf(stderr, "  NasPort = %ld\n", ntohl(myport));
      break;
    default:
      fprintf(stderr, "  Data type %u = ", x);
      for (j=i+2; j < i+data[i+1]; ++j)
	if ( (x = (unsigned char)data[j]) < 32 || x > 126)
	  fprintf (stderr, "0x%x ", x);
	else
	  fprintf(stderr, "%c ", x);
      fprintf(stderr,"\n");
      break;
    }
  }
}	/* dump_radpkt() */

#endif /* DEBUG */

/* encrypt the password by XORing with the md5 hash of the "secret+vector"
 * combined string.
 */
void hash_password(pwd, pwdlen, result, secret, vector)
  char *pwd;
  int  pwdlen;
  char *result;	/* hashed data  returned in this */
  char *secret;
  char *vector;
{
  int	i, secretlen;
  char	md5buf[AUTH_PW_LEN + AUTH_VECTOR_LEN];
  char	md5result[AUTH_VECTOR_LEN +1];

  bzero(md5buf, sizeof(md5buf));

  secretlen= strlen(secret);
  bcopy(secret, md5buf, secretlen );
  bcopy(vector, md5buf+secretlen, AUTH_VECTOR_LEN);
  md5_calc(md5result, md5buf, AUTH_VECTOR_LEN + secretlen);
  bcopy(pwd, result, pwdlen);
  for (i = 0; i < AUTH_PW_LEN; ++i)
    result[i] ^= md5result[i % AUTH_VECTOR_LEN];
  /* result[pwdlen] = '\0';	*/
}    


/*
 * Instead of inet_addr() which can only handle IP addresses, this handles
 * hostnames as well...
 * Return 0 on error, 1 if okay.
 */
static int get_inet_address(addr, host)
  struct sockaddr_in *addr;		/* must be a malloced structure */
  char *host;
{
  register struct hostent *hp;

  bzero((char *)addr, sizeof (struct sockaddr_in));

  addr->sin_family = AF_INET;

  addr->sin_addr.s_addr = (u_long) inet_addr(host);
  if (addr->sin_addr.s_addr == -1 || addr->sin_addr.s_addr == 0) {
    if ((hp = gethostbyname(host)) == NULL) {
      fprintf(stderr, "%s is unknown host\n", host);
      return 0;
    }
#ifdef h_addr		/* in netdb.h */
    bcopy((char *)hp->h_addr, (char *)&(addr->sin_addr), hp->h_length);
#else
    bcopy((char *)hp->h_addr_list[0], (char *)&(addr->sin_addr), hp->h_length);
#endif
  }
  return 1;
}

#ifdef TEST
main(ac, av)
  int ac;
  char *av[];
{
  int status = 0;
  int porttype = -1;

  debug = 5;

  if (ac < 6) {
    fprintf(stderr,
	    "usage: %s <ipaddr> <port> <secret> <user> <passwd> [porttype]\n",
	    av[0]);
    exit (1);
  }
  if (ac == 7)
    porttype = atoi(av[6]);
  status = radiusmon(av[1], atoi(av[2]), av[3], av[4], av[5], 0, 0, porttype);
  printf("Exit value %d (%s)\n", status, (status == 1 ? "OK" : "FAILED"));
  exit (0);

}
#endif
