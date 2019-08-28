From owner-snips-users@navya.com  Wed May 26 13:07:56 2004
Return-Path: <owner-snips-users@navya.com>
Received: from navya.com (navya.com [198.173.254.217])
	by fubar.loosenut.com (8.12.10/8.12.10) with ESMTP id i4QK7qwb007096
	for <russell@loosenut.com>; Wed, 26 May 2004 13:07:54 -0700 (PDT)
Received: from navya.com (localhost [127.0.0.1])
	by navya.com (8.12.11/8.12.6) with ESMTP id i4QJoeEi076323
	for <snips-users-outlist@navya.com>; Wed, 26 May 2004 15:50:40 -0400 (EDT)
	(envelope-from owner-snips-users@navya.com)
Received: (from majordom@localhost)
	by navya.com (8.12.11/8.12.6/Submit) id i4QJoeRu076322
	for snips-users-outlist; Wed, 26 May 2004 15:50:40 -0400 (EDT)
	(envelope-from owner-snips-users@navya.com)
X-Authentication-Warning: navya.com: majordom set sender to owner-snips-users@navya.com using -f
Received: from mail.kanetix.com (kanprd2.kanetix.com [216.220.59.175])
	by navya.com (8.12.11/8.12.6) with ESMTP id i4QJodsV076287
	for <nocol-users@navya.com>; Wed, 26 May 2004 15:50:39 -0400 (EDT)
	(envelope-from ean@hedron.org)
Received: from Kingston (unknown [192.168.100.216])
	by mail.kanetix.com (Postfix) with SMTP id EF7F83A274
	for <nocol-users@navya.com>; Wed, 26 May 2004 15:50:35 -0400 (EDT)
Message-ID: <001d01c4435a$b6697a20$d864a8c0@ho.kanetix.com>
From: "Ean Kingston" <ean@hedron.org>
To: <nocol-users@navya.com>
Subject: [snips-users] portmon with ssl support.
Date: Wed, 26 May 2004 15:50:35 -0400
MIME-Version: 1.0
Content-Type: multipart/mixed;
	boundary="----=_NextPart_000_001A_01C44339.2F20EBA0"
X-Priority: 3
X-MSMail-Priority: Normal
X-Mailer: Microsoft Outlook Express 6.00.2800.1409
X-MimeOLE: Produced By Microsoft MimeOLE V6.00.2800.1409
Sender: owner-snips-users@navya.com
Precedence: normal
X-Spam-Checker-Version: SpamAssassin 2.63 (2004-01-11) on fubar.loosenut.com
X-Spam-Status: No, hits=0.0 required=5.0 tests=none autolearn=ham version=2.63
X-Spam-Level: 
Status: RO
Content-Length: 10394
Lines: 367

This is a multi-part message in MIME format.

------=_NextPart_000_001A_01C44339.2F20EBA0
Content-Type: text/plain;
	charset="iso-8859-1"
Content-Transfer-Encoding: 7bit

Folks,

I needed to add https monitoring to our monitoring system at work so I
adapted portmon to work through ssl. It (obviously) needs the openssl
libraries to compile with and it doesn't allow non-ssl connections anymore.

I didn't have a lot of time so I wasn't able to make it an option to the
existing portmon, though I'm hoping to do so in my spare time.

Attached (I hope) are diffs for the three files I modified. These are
differenced from the 1.2beta2 package of Snips.

The files to patch (from the extracted to directory) are:
include/portmon.h
portmon/main.c
portmon/portmon.c

There are no additional variables for ssl handshake timing or validating
server certificates. This just does what portmon did but through ssl.

Hopefully I will have a cleaner implementation that allows for
portmon/portmon+ssl support in one tool at some time in the future. Until
then, these patches might be useful to someone.

I don't have updates to the makefile because I did the compile of my ssl
portmon by hand.

------=_NextPart_000_001A_01C44339.2F20EBA0
Content-Type: application/octet-stream;
	name="main.c.patch"
Content-Transfer-Encoding: quoted-printable
Content-Disposition: attachment;
	filename="main.c.patch"

*** main.c	Sun Jul  8 18:48:28 2001=0A=
--- ssl_main.c	Wed May 26 15:32:27 2004=0A=
***************=0A=
*** 39,44 ****=0A=
--- 39,49 ----=0A=
  #include "portmon.h"=0A=
  #undef _MAIN_=0A=
  =0A=
+ #ifdef WITH_SSL=0A=
+ #include <openssl/ssl.h>=0A=
+ SSL_CTX *ctx;=0A=
+ #endif=0A=
+ =0A=
  static int	rtimeout =3D RTIMEOUT;		/* select timeout in seconds */=0A=
  static int	simulconnects =3D MAXSIMULCONNECTS;=0A=
  struct _harray	*hostlist;			/* global ptr to list */=0A=
***************=0A=
*** 50,55 ****=0A=
--- 55,66 ----=0A=
  {=0A=
    hostlist =3D NULL;=0A=
    set_functions();=0A=
+ #ifdef WITH_SSL=0A=
+   SSL_load_error_strings();=0A=
+   SSL_library_init();=0A=
+   ctx =3D SSL_CTX_new(SSLv23_client_method());=0A=
+   if ( NULL =3D=3D ctx ) perror ("SSL failed to create context.\n");=0A=
+ #endif=0A=
    snips_main(ac, av);=0A=
  }=0A=
    =0A=

------=_NextPart_000_001A_01C44339.2F20EBA0
Content-Type: application/octet-stream;
	name="portmon.h.patch"
Content-Transfer-Encoding: quoted-printable
Content-Disposition: attachment;
	filename="portmon.h.patch"

*** portmon.h	Sun Jul  8 18:04:27 2001=0A=
--- ssl_portmon.h	Wed May 26 15:34:08 2004=0A=
***************=0A=
*** 22,27 ****=0A=
--- 22,28 ----=0A=
  #include <signal.h>=0A=
  #include <netdb.h>=0A=
  =0A=
+ #define WITH_SSL=0A=
  =0A=
  #define VARUNITS	"secs"=0A=
  #define POLLINTERVAL  	(60*15)		/* seconds between passes */=0A=

------=_NextPart_000_001A_01C44339.2F20EBA0
Content-Type: application/octet-stream;
	name="portmon.c.patch"
Content-Transfer-Encoding: quoted-printable
Content-Disposition: attachment;
	filename="portmon.c.patch"

*** portmon.c	Sun Jul  8 18:48:28 2001=0A=
--- ssl_portmon.c	Wed May 26 15:32:37 2004=0A=
***************=0A=
*** 30,36 ****=0A=
  =0A=
  /* Copyright 2001, Netplex Technologies Inc. */=0A=
  =0A=
- =0A=
  /* =0C */=0A=
  #ifndef lint=0A=
  static char rcsid[] =3D "$Id: portmon.c,v 1.0 2001/07/08 22:48:28 =
vikas Exp $" ;=0A=
--- 30,35 ----=0A=
***************=0A=
*** 38,43 ****=0A=
--- 37,47 ----=0A=
  =0A=
  #include "portmon.h"=0A=
  =0A=
+ #ifdef WITH_SSL=0A=
+ #include <openssl/ssl.h>=0A=
+ extern SSL_CTX *ctx;=0A=
+ #endif=0A=
+ =0A=
  extern char	*skip_spaces() ;		/* in libsnips */=0A=
  extern char	*Strcasestr();			/* like strstr() */=0A=
  =0A=
***************=0A=
*** 53,59 ****=0A=
--- 57,67 ----=0A=
  {=0A=
    int i, nleft;=0A=
    int maxfd =3D 0;=0A=
+   int rv =3D 0;=0A=
    int sockarray[MAXSIMULCONNECTS];=0A=
+ #ifdef WITH_SSL=0A=
+   SSL *ssl[MAXSIMULCONNECTS];=0A=
+ #endif=0A=
    time_t elapsedsecs =3D 0;=0A=
    fd_set rset, wset;=0A=
  =0A=
***************=0A=
*** 80,85 ****=0A=
--- 88,99 ----=0A=
        return (-1);	/* fatal error */=0A=
      }=0A=
  =0A=
+ #ifdef WITH_SSL=0A=
+     ssl[i] =3D SSL_new(ctx);=0A=
+     if ( debug ) fprintf (stderr, " (debug) SSL_new(), ssl[%d] =3D =
%x\n", i, ssl[i]);=0A=
+     if ( NULL =3D=3D ssl[i] ) perror("Failed to create SSL handle.\n");=0A=
+ #endif=0A=
+ =0A=
      /* NBconnect() will return zero/positive if it gets an immediate=0A=
       * connection, else it will set errno.=0A=
       */=0A=
***************=0A=
*** 97,102 ****=0A=
--- 111,120 ----=0A=
  	FD_SET(sockarray[i], &rset);=0A=
  	FD_SET(sockarray[i], &wset);=0A=
  	if (sockarray[i] > maxfd) maxfd =3D sockarray[i];=0A=
+ #ifdef WITH_SSL=0A=
+         if ( 0 =3D=3D SSL_set_fd(ssl[i], (int) sockarray[i]) ) =
perror("Failed to tie SSL to socket.\n");=0A=
+         SSL_set_connect_state(ssl[i]);=0A=
+ #endif=0A=
      }=0A=
  =0A=
    }	/* for (i =3D 1..nhosts) */=0A=
***************=0A=
*** 114,120 ****=0A=
      {=0A=
        for (i =3D 0; i < nhosts; ++i)=0A=
  	close(sockarray[i]);=0A=
!       return 0;=0A=
      }=0A=
      tval.tv_sec =3D rtimeout - elapsedsecs;=0A=
      tval.tv_usec =3D 0;=0A=
--- 132,139 ----=0A=
      {=0A=
        for (i =3D 0; i < nhosts; ++i)=0A=
  	close(sockarray[i]);=0A=
!       rv =3D 0;=0A=
!       break; /* while loop */=0A=
      }=0A=
      tval.tv_sec =3D rtimeout - elapsedsecs;=0A=
      tval.tv_usec =3D 0;=0A=
***************=0A=
*** 129,135 ****=0A=
        if (n < 0 && errno !=3D EINTR)=0A=
        {=0A=
  	perror("select()");=0A=
! 	return (-1);	/* fatal ?? */=0A=
        }=0A=
        else=0A=
        {=0A=
--- 148,155 ----=0A=
        if (n < 0 && errno !=3D EINTR)=0A=
        {=0A=
  	perror("select()");=0A=
! 	rv =3D -1;	/* fatal ?? */=0A=
!         break; /* while loop */=0A=
        }=0A=
        else=0A=
        {=0A=
***************=0A=
*** 177,186 ****=0A=
--- 197,214 ----=0A=
  	  hv[i]->elapsedsecs =3D elapsedsecs;	/* store elapsed time */=0A=
  =0A=
  	  if (FD_ISSET(sockarray[i], &ws))=0A=
+ #ifdef WITH_SSL=0A=
+ 	    send_hoststring(ssl[i], sockarray[i], hv[i]);=0A=
+ #else=0A=
  	    send_hoststring(sockarray[i], hv[i]);=0A=
+ #endif=0A=
  =0A=
  	  if (FD_ISSET(sockarray[i], &rs) || hv[i]->responselist =3D=3D NULL)=0A=
+ #ifdef WITH_SSL=0A=
+ 	    if ( process_host(ssl[i], sockarray[i], hv[i]) =3D=3D 1 )=0A=
+ #else=0A=
  	    if ( process_host(sockarray[i], hv[i]) =3D=3D 1 )=0A=
+ #endif=0A=
  	    {=0A=
  	      FD_CLR(sockarray[i], &rset); FD_CLR(sockarray[i], &wset);=0A=
  	      close(sockarray[i]);	/* ensure this is closed */=0A=
***************=0A=
*** 205,214 ****=0A=
    if (nleft && debug > 1)=0A=
      fprintf(stderr, " %d devices unprocessed (no response)\n", nleft);=0A=
  =0A=
!   for (i =3D 0; i < nhosts; ++i)		/* close any open sockets */=0A=
      close(sockarray[i]);=0A=
  =0A=
!   return (0);=0A=
  =0A=
  }	/* check_ports() */=0A=
  =0A=
--- 233,247 ----=0A=
    if (nleft && debug > 1)=0A=
      fprintf(stderr, " %d devices unprocessed (no response)\n", nleft);=0A=
  =0A=
!   for (i =3D 0; i < nhosts; ++i) {	/* close any open sockets */=0A=
! #ifdef WITH_SSL=0A=
!      if (debug) fprintf(stderr,"SSL_free ssl[%d] =3D %x\n", i, ssl[i]);=0A=
!      SSL_free(ssl[i]);=0A=
! #endif=0A=
      close(sockarray[i]);=0A=
+   } /* end for */=0A=
  =0A=
!   return (rv);=0A=
  =0A=
  }	/* check_ports() */=0A=
  =0A=
***************=0A=
*** 215,227 ****=0A=
  /*+=0A=
   * Send a string to a host=0A=
   */=0A=
  send_hoststring(sock, h)=0A=
    int sock;		/* connected socket */=0A=
    struct _harray *h;=0A=
  {=0A=
    int n;=0A=
  =0A=
!   if (debug)=0A=
      fprintf(stderr, " (debug) send_hoststring('%s:%d')\n", h->hname, =
h->port);=0A=
  =0A=
    /* if socket is non-blocking, undo this flag */=0A=
--- 248,265 ----=0A=
  /*+=0A=
   * Send a string to a host=0A=
   */=0A=
+ #ifdef WITH_SSL=0A=
+ send_hoststring(ssl, sock, h)=0A=
+    SSL *ssl;=0A=
+ #else=0A=
  send_hoststring(sock, h)=0A=
+ #endif=0A=
    int sock;		/* connected socket */=0A=
    struct _harray *h;=0A=
  {=0A=
    int n;=0A=
  =0A=
!   if (debug > 1)=0A=
      fprintf(stderr, " (debug) send_hoststring('%s:%d')\n", h->hname, =
h->port);=0A=
  =0A=
    /* if socket is non-blocking, undo this flag */=0A=
***************=0A=
*** 234,240 ****=0A=
--- 272,282 ----=0A=
      return 1;=0A=
  =0A=
    /* send our string */=0A=
+ #ifdef WITH_SSL=0A=
+   if ( (n =3D SSL_write(ssl, h->wptr, strlen(h->wptr))) < 0)=0A=
+ #else=0A=
    if ( (n =3D write(sock, h->wptr, strlen(h->wptr))) < 0)=0A=
+ #endif=0A=
    {=0A=
      if (errno =3D=3D EWOULDBLOCK)=0A=
        return 0;=0A=
***************=0A=
*** 276,282 ****=0A=
--- 318,329 ----=0A=
   * 	Checks the port for the structure _harray passed to it.=0A=
   * Return value is '1' if finished processing, '0' if yet to finish.=0A=
   */=0A=
+ #ifdef WITH_SSL=0A=
+ process_host(ssl, sock, h)=0A=
+   SSL *ssl;=0A=
+ #else=0A=
  process_host(sock, h)=0A=
+ #endif=0A=
    int sock;		/* connected socket */=0A=
    struct _harray *h;=0A=
  {=0A=
***************=0A=
*** 303,309 ****=0A=
--- 350,360 ----=0A=
        h->testseverity =3D E_INFO;=0A=
        h->status =3D 1;		  	/* device up */=0A=
        if (h->quitstr && *(h->quitstr))=0A=
+ #ifdef WITH_SSL=0A=
+ 	SSL_write(ssl, h->quitstr, strlen(h->quitstr));=0A=
+ #else=0A=
  	write(sock, h->quitstr, strlen(h->quitstr));=0A=
+ #endif=0A=
        close (sock);=0A=
        return(1);	/* done testing */=0A=
      }=0A=
***************=0A=
*** 310,316 ****=0A=
--- 361,371 ----=0A=
  =0A=
    /* now fill any remaining read buffer space we have */=0A=
    buflen =3D h->readbuf + READBUFLEN - h->rptr;	/* amount we can read*/=0A=
+ #ifdef WITH_SSL=0A=
+   n =3D SSL_read(ssl, h->rptr, buflen - 1);=0A=
+ #else=0A=
    n =3D read(sock, h->rptr, buflen - 1);=0A=
+ #endif=0A=
  #ifdef DEBUG  	/* */=0A=
    if (debug > 2)=0A=
      fprintf(stderr, "  read %d bytes from %s:%d\n", n, h->hname, =
h->port);=0A=
***************=0A=
*** 389,395 ****=0A=
--- 444,455 ----=0A=
      h->status =3D 0;=0A=
  =0A=
    if (h->quitstr && *(h->quitstr))=0A=
+ #ifdef WITH_SSL=0A=
+     SSL_write(ssl, h->quitstr, strlen(h->quitstr));=0A=
+ #else=0A=
      write(sock, h->quitstr, strlen(h->quitstr));=0A=
+ #endif=0A=
+ =0A=
    close(sock);=0A=
  =0A=
    return(1);	/* done processing */=0A=

------=_NextPart_000_001A_01C44339.2F20EBA0--

