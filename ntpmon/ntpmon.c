/* $Header: /home/cvsroot/snips/ntpmon/ntpmon.c,v 1.3 2008/04/25 23:31:51 tvroon Exp $ */

/*+ ntpmon.c
 *
 * Creates an NTP query packet, and sends it to the remote ntp daemon.
 * Then waits for a reply and analyses it for the stratum value.
 *
 * A C version of the ntpmon PERL monitor sent by mathias@singnet.com.sg
 * (Mathias Koerber).
 *
 */

/*
 * $Log: ntpmon.c,v $
 * Revision 1.3  2008/04/25 23:31:51  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.2  2001/08/13 03:39:38  vikas
 * Was calling bzero() after filling the struct from get_inet_address()
 * (bug reported by m.vevea@seafloor.com)
 *
 * Revision 1.1  2001/08/01 01:19:16  vikas
 * Now sets stratum to 16 instead of -1.
 *
 * Revision 1.0  2001/07/08 22:44:25  vikas
 * For SNIPS
 *
 * Patch to resend the pkt TRIES times (niek@knoware.nl)
 */

/* Copyright 2001, Netplex Technologies Inc., info@netplex-tech.com */

#include <time.h>
#include "netmisc.h"
#include "ntpmon.h"

extern int debug;	/* needed here since not including snips.h */
extern char *prognm;

#ifndef NTP_PORT
# define NTP_PORT 123
#endif

/* function prototypes */
int get_response(int sockfd, int sequence, int timeout);

/*
 * Create a control packet and send it to the ntp deamon.
 * The packet is a ntp_control structure with no data.
 * The return packet is also ntp_control, but the data field contains
 * amongst others the string "stratum=nn" where nn is the stratum number.
 * This number is the one we are interested in.
 * Returns 255 in case of error, else the stratum of the remote clock (1-16)
 */
int ntpmon(host)
  char *host;	/* In dotted decimal addr */
{
  struct ntp_control qpkt;
  struct sockaddr_in hostaddr;
  int pktsize, stratum;
  int sockfd;
  int pktversion = 3;
  int sequence = time(NULL) % 65000;
  int tries;
  char *lcladdr;

  /* Set up the packet */
  int opcode = CTL_OP_READVAR; 
  qpkt.li_vn_mode = PKT_LI_VN_MODE(0, pktversion, MODE_CONTROL);
  qpkt.r_m_e_op = (u_char)opcode & CTL_OP_MASK; 
  qpkt.sequence = htons(sequence);
  qpkt.status = 0;
  qpkt.associd = htons((u_short)0);
  qpkt.offset = 0;
  qpkt.count = 0;

  pktsize = CTL_HEADER_LEN;

  bzero((char *)&hostaddr, sizeof(hostaddr));

  /* setup the socket */
  if(get_inet_address(&hostaddr, host) < 0)
    return 255;				/* indicate error */

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  /*
   * Sometimes, you really want the data to come from a specific IP addr
   * on the machine, i.e. for firewalling purposes
   */
 
  if ( (lcladdr = getenv("SNIPS_LCLADDR")) ||
       (lcladdr = getenv("NOCOL_LCLADDR")) )
  {
    struct sockaddr_in lcl_hostaddr;

    bzero((char *)&lcl_hostaddr, sizeof(lcl_hostaddr));
    lcl_hostaddr.sin_family = AF_INET;
    lcl_hostaddr.sin_addr.s_addr = inet_addr(lcladdr);
    if (bind(sockfd, (struct sockaddr *)&lcl_hostaddr, sizeof(lcl_hostaddr)) < 0) {
      perror("bind LCLADDR");
    }
  }

  hostaddr.sin_family = AF_INET;
  hostaddr.sin_port = htons(NTP_PORT);

  /* we do a connect (instead of a sendto() )so that we can do a select
   * during the read() later on
   */
  if (connect(sockfd, (struct sockaddr *)&hostaddr, sizeof(hostaddr)) < 0)
  {
    if (debug)  perror("connect");
    close(sockfd);
    return 255;
  }

  /* Send the packet. Retry TRIES times upon failure (since UDP). */
  stratum = 255;
  for (tries = 0; tries < TRIES; ++tries)
  {
    if (write(sockfd, &qpkt, pktsize) == -1) {
      if (debug) perror("write");
      break;
    }
 
    /* Read the response packet and analyse it */
    if((stratum = get_response(sockfd, sequence, TIMEOUT)) < 0 )
    {
      if(debug)
	fprintf(stderr, "%s:ntp error for %s \n", prognm, host);
      stratum = 255;	/* indicate error, cannot return -ve */
    }
    else
    {
      if(debug)
	fprintf(stderr,"(debug)%s: Stratum for %s = %d\n",
		prognm, host, stratum);
      break;	/* out of for() */
    }
  }	/* end for() */

  close(sockfd);
  return stratum;
}	/* ntpmon() */


/*
 * Receives a packet from the ntp server, does some checking and returns
 * stratum. Returns -1 for error.
 */
int get_response(sockfd, sequence, timeout)
  int sockfd;
  int sequence;
  int timeout;
{
  struct ntp_control rpkt;
  int n, stratum;
  char *loc;
  fd_set  fdvar;
  struct timeval tval ;

  bzero(&rpkt, sizeof(rpkt));
  tval.tv_sec = timeout;
  tval.tv_usec = 0;

  /*
   * we loop until timeout to make sure that we dont get back an old
   * response. This is verified by the sequence number we sent...
   */
  while (1)
  {
    FD_ZERO(&fdvar);
    FD_SET(sockfd, &fdvar);
    if ((n = select(sockfd+1, &fdvar, NULL, NULL, &tval)) <= 0)
    {
      if (n < 0 && errno != EINTR)
      {
	perror("select() ");
	return (-1);
      }
      else
      {
	if (debug > 2)   fprintf(stderr, "select() timed out\n");
	return (-1);	
      }
    }
    /* we dont do an FD_ISSET() since only one fd was set... */
    if ( (n = read(sockfd, &rpkt, sizeof(rpkt)) ) <= 0 )
    {
      perror("read");
      return -1;
    }

    /*
     * Check opcode and sequence number for a match.
     * Could be old data getting to us.
     */
    if (ntohs(rpkt.sequence) == sequence)
      break;	/* out of while */

    if (debug > 2)
      fprintf(stderr,"ntpmon: Received sequence number %d, wanted %d\n",
	      ntohs(rpkt.sequence), sequence);
  }	/* while(1) */

  /*
   * Check the error code.  If non-zero, return it.
   */
  if (CTL_ISERROR(rpkt.r_m_e_op))
  {
    int errcode;

    errcode = (ntohs(rpkt.status) >> 8) & 0xff;
    if (debug && CTL_ISMORE(rpkt.r_m_e_op)) 
      fprintf(stderr, "Error code %d received \n",errcode);
    
    return (-1);
  }

  *(rpkt.data + ntohs(rpkt.count)) = '\0';	/* put a terminating NULL */

  if (debug > 3)
  {
    /* write(2, rpkt.data, ntohs(rpkt.count));   /* dump pkt to stderr */
  }

  /*
     * Looking for the string :    * "stratum=nn"
     */
  if ( (loc = (char *)strstr(rpkt.data, "stratum=")) == NULL )
    return (-1);

  sscanf(loc + strlen("stratum="), "%d", &stratum);	/* */
  /* *(loc + 10) = '\0';	/* End string after "stratum=nn" */
  /* stratum = atoi(loc + strlen("stratum="));	/* */
  return stratum;

}	/* get_response() */

