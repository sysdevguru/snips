/*
 * tcpf.c -- TCP to stdio filter
 *
 *   July 1987, Kirk Lougheed
 *
 *   Copyright (c) 1987 by cisco Systems, Inc.
 *
 * This program is designed to read and write from standard input and
 * output to a raw TCP socket.  The specific application is sending data
 * to printers attached to a terminal server's serial or parallel ports,
 * or stuffing a file at a gateway or terminal server.
 * 
 * Syntax:
 * 	tcpf <switches> hostname port-number
 * 
 * 	Switches:
 * 		-d	debugging output
 * 		-e	convert eof in input to ^D on output
 * 		-r      convert LF on input to CRLF on output
 * 		-t[nn]	timeout mode
 * 		-w[nn]	wait for open mode
 * 
 * This program may be used in a shell script.  The following example is
 * of a "print" command that sends text to a PostScript printer.
 * 
 *  /usr/local/bin/lptops -2 -o -ntr $* | /usr/local/bin/tcpf -e chaff 4008 &
 * 
 * In a shared printer environment, multiple systems may be trying to connect
 * to a particular printer.  In this case, you may wish to use the -w switch
 * to cause tcpf to NOT give up immediately if a connection attempt fails.
 * Instead, tcpf will continue to try every second to open connection,
 * until the full timeout (defaults to 5 minutes) has elapsed.
 * 
 * It may also be used to drive our boxes from scripts running on dustbin.
 * The switch, -t ("timeout"), is used to do this.  With -t specified, it no
 * longer gives up when eof in either direction is encountered, but will give
 * up when eof in both directions, or after 5 minutes of inactivity.
 * 
 * To use it to stuff a file at wilma, for instance, use the command line:
 * 
 * 	tcpf -t5 wilma 23 <doit
 * 
 * where the file doit contains, for instance:
 * 
 * 	floozy^M
 * 	ena
 * 	floozy^M
 * 	ping
 * 	ip^Mnit^M100^M^M^M^M
 * 	disa
 * 	exit
 * 
 * This script will log onto wilma, get into enable mode, then send 100 pings
 * aimed at nit.  Output from wilma will be delivered back to the initiator.
 * 
 * Note that the ^M's in this message must be actuall control-M's in the file --
 * most of the parsing uses 015 as the line terminator, and I haven't trained
 * tcpf to do anything exotic like deal with NVT or CRLF issues.  It turns
 * out the exec.c uses 015 or 012 as a line terminator, so the some of the
 * commands can have normal newlines at the end.
 *
 */
 
#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0

#define LF 0xA
#define CR 0xD

/* Accommodate worst case for stupid printers (-r switch) */
#define TCPFBUFSIZ (BUFSIZ * 2)

int debug;                      /* -d switch */
int eofmode;                    /* -e switch */
int timeoutmode;                /* -t switch */
int crlfmode;                   /* -r switch */
int timeoutlen;
int openwait;                   /* -w switch */
int openwaitlen;

struct sockaddr_in sock;
jmp_buf terminate;
unsigned char netbuffer[BUFSIZ];
unsigned char netoutbuffer[TCPFBUFSIZ];
unsigned char ttybuffer[BUFSIZ];
int bytesread, byteswrote;

void crashed();
void transfer();

/*
 * main
 * Parse command line and set up transfer.
 *
 *     tcpf [-d -e -r -t -w] host port
 *
 *      -d              Enable debugging output.
 *      -e              Send CTRL/D over TCP side when EOF received on stdin.
 *                        This makes PostScript printers very happy.
 *      -r              Send a <CR><LF> pair to stream for every <LF> given
 *                      in stdin.
 *      -t[time]        Timeout in seconds.  Default is 5 min.
 *      -w[time]        Wait for Time while opening connection too.
 *      host            Host name, either symbolic or dotted decimal.
 *      port            Decimal TCP port number.  No defaults.
 */

main(argc, argv)
    int argc;
    char *argv[];
{
    char *hostname;
    unsigned short port;
    int conn;

    debug = FALSE;
    eofmode = FALSE;
    timeoutmode = FALSE;
    crlfmode = FALSE;
    argc--; argv++;
    while (argc > 1 && (argv[0][0] == '-')) {
        switch (argv[0][1]) {
            case 'd':
            case 'D':
                debug = TRUE;
                break;
            case 'e':
            case 'E':
                eofmode = TRUE;
                break;
            case 'r':
            case 'R':
                crlfmode = TRUE;
                break;
            case 't':
            case 'T':
                timeoutmode = TRUE;
                timeoutlen = atoi(&argv[0][2]);
                if (timeoutlen == 0)
                  timeoutlen = 300;
                break;
            case 'w':
            case 'W':
                openwait = TRUE;
                openwaitlen = atoi(&argv[0][2]);
                if (openwaitlen <= 0)
                    openwaitlen = 300;
                break;
            default:
                fprintf(stderr, "Usage: tcpf [-d -e -r -t[sec] -w[sec]] host port\n");
                exit(1);
        }
        argc--; argv++;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: tcpf [-d -e -t[sec] -w[sec]] host port\n");
        exit(1);
    }
    hostname = argv[0];
    port = atoi(argv[1]);
    if (!setup_socket(&sock, hostname, port))
        exit(1);
    if (debug) {
        fprintf(stderr, "Connecting to \"%s, %d\"... ",
            inet_ntoa(sock.sin_addr), ntohs(sock.sin_port));
        fflush(stderr);
    }
    conn = setup_connection(&sock);
    if (conn < 0)
        exit(1);
    signal(SIGPIPE, crashed);
    if (setjmp(terminate) == 0)
        transfer(conn);
    close(conn);
    if (debug)
       fprintf(stderr, "\nNet connection closed: %d bytes written, %d read\n",
                                                byteswrote, bytesread);
    exit(0);
}

/*
 * setup_socket
 * Turn a hostname string and port into a sockaddr structure.
 * Returns TRUE or FALSE.
 */

int
setup_socket(soc, host, port)
    struct sockaddr_in *soc;
    char *host;
    unsigned short port;
{
    struct hostent *entry;

    soc->sin_family = AF_INET;
    soc->sin_port = htons(port);
    soc->sin_addr.s_addr = inet_addr(host);
    if (soc->sin_addr.s_addr != -1)
        return(TRUE);
    entry = gethostbyname(host);
    if (entry && entry->h_addr && (entry->h_addrtype == AF_INET)) {
 	bcopy(entry->h_addr,&soc->sin_addr.s_addr, entry->h_length);
/* 	strncpy(&soc->sin_addr.s_addr, entry->h_addr, entry->h_length); */
        return(TRUE);
    }
    fprintf(stderr, "Unknown hostname or address - \"%s\"\n", host);
    return(FALSE);
}

/*
 * crashed
 * Come here if connection failed
 */

void
crashed()
{
    if (debug)
        perror("tcpf lost connection");
    longjmp(terminate, 1);
}

/*
 * setup_connection
 * Open a TCP connection to the specified address
 */

int
setup_connection(soc)
    struct sockaddr_in *soc;
{
    int conn;
    int i;

    conn = socket(AF_INET, SOCK_STREAM, 0);
    if (conn < 0) {
        if (debug)
            perror("socket");
        return(-1);
    }
    if (openwait) {
        for (i=0; i < openwaitlen; i++) {
            if (connect(conn,soc, sizeof(*soc)) >= 0)
                break;
            if (i==0 && debug) {
                fprintf(stderr,"Waiting... ");
                fflush(stderr);
            }
            sleep(1);
            close(conn);
            conn = socket(AF_INET, SOCK_STREAM, 0);
            if (conn < 0) {
                if (debug)
                    perror("socket");
                return(-1);
            }
        }
        if (i >= openwaitlen) {
            if (debug)
                perror("connect");
            return(-1);
        }
    } else if (connect(conn, soc, sizeof(*soc)) < 0) {
        if (debug)
            perror("connect");
        return(-1);
    }
    if (debug)
        fprintf(stderr, "Open\n");
    return(conn);
}

/*
 * transfer
 * Write data from stdin to the specified connection.
 * Read data from connection to stdout.
 */

void
transfer(net)
    int net;
{
    int tib, nib, count;
    unsigned char *tibptr, *nibptr, *stiptr, *stoptr;
    int nready, readmask, writemask;
    int stdinmask, stdoutmask, netinmask, netoutmask;
    struct timeval timeout;
    int neteof, stdeof, nonblock;

    bytesread = byteswrote = 0;
    tib = nib = 0;
    neteof = stdeof = FALSE;
    stdinmask = (1 << fileno(stdin));
    stdoutmask = (1 << fileno(stdout));
    netinmask = (1 << net);
    netoutmask = (1 << net);
    timeout.tv_sec = timeoutlen;
    timeout.tv_usec = 0;
    nonblock = 1;
    ioctl(net, FIONBIO, &nonblock);
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    while (TRUE) {
        if ((tib == 0) && (nib == 0) && (neteof == TRUE) && (stdeof == TRUE))
            return;
        readmask = 0;
	if (tib == 0)
	    readmask |= (stdeof ? 0 : stdinmask);
	if (nib == 0)
	    readmask |= (neteof ? 0 : netinmask);
        writemask = 0;
        if (tib)
           writemask |= netoutmask;
        if (nib)
           writemask |= stdoutmask;
        nready = select(8*sizeof(int), &readmask, &writemask, NULL, 
                        (timeoutmode ? &timeout : NULL));
	if (debug)
	    printf("\nnready %d, rmask %4x, wmask %4x",
		   nready, readmask, writemask);
        if (nready <= 0)
            /* exit if timeout or eror */
            return;

        /*
         * First we try reading from the net
         */
        if ((nib == 0) && (readmask & netinmask) && (neteof == FALSE)) {
            nibptr = netbuffer;
            nib = read(net, nibptr, BUFSIZ);
            if (debug)
                fprintf(stderr,"\nRead %d bytes from net", nib);
            if ((nib == 0) || (nib == -1)) {
                nib = 0;
                neteof = TRUE;
            }
            else {
                bytesread += nib;
                if (eofmode && (netbuffer[nib-1] == '\004')) {
                    netbuffer[--nib] = '\000';
                    neteof = TRUE;
                }
            }
        }

        /*
         * If we have input from the net, try writing it to standard out.
         */
        if ((nib > 0) && (writemask & stdoutmask)) {
            count = write(fileno(stdout), nibptr, nib);
            if (debug)
                fprintf(stderr,"\nWrote %d bytes to stdout", count);
            if ((count == nib) || (count < 0))
                nib = 0;
            else {
                nibptr += count;
                nib -= count;
            }
        }

        /*
         * Now read from standard input.  If we read EOF and the user
         * wants us to pass it through, send a CTRL/D.  This is very
         * useful when dealing with PostScript printers that require
         * the last character of a job be CTRL/D.
         */
        if ((tib == 0) && (readmask & stdinmask) && (stdeof == FALSE)) {
            tibptr = ttybuffer;
            tib = read(fileno(stdin), tibptr, BUFSIZ);
            if (debug)
                fprintf(stderr,"\nRead %d bytes from stdin", tib);
            if ((tib == 0) || (tib == -1)) {
                if ((tib == 0) && (eofmode == TRUE) && byteswrote) {
                    if (debug)
                        fprintf(stderr, " -- sending EOF character");
                    tib = 1;
                    ttybuffer[0] = '\004';
                }
                else
                    tib = 0;
                stdeof = TRUE;
            }
            /*
             * Convert <LF> to <CR><LF>. Preexisting <CR><LF> pairs not hurt,
             * as <CR><CR><LF> has same effect. Uses this slow loop, rather
             * than "memccpy", because the memory routines do not exist on
             * all Unixes.
             */
            if (crlfmode) {
                for (stiptr = ttybuffer, stoptr = netoutbuffer;
                     ((stiptr - ttybuffer) < tib);) {
                    if (*stiptr == LF) {
                        *stoptr++ = CR;
                    }
                    *stoptr++ = *stiptr++;
                }
                tib = (stoptr - netoutbuffer);
                tibptr = netoutbuffer;
            }
        }

        /*
         * If we have input from standard input, try writing to the net
         */
        if ((tib > 0) && (writemask & netoutmask)) {
            errno = 0;
            count = write(net, tibptr, tib);
            if (debug)
                fprintf(stderr,"\nWrote %d bytes to net", count);
            if (count < 0) {
                if ((errno != ENOBUFS) && (errno != EWOULDBLOCK))
                    longjmp(terminate, -1);
                count = 0;
            }
            byteswrote += count;
            tibptr += count;
            tib -= count;
        }

        /*
         * If we hit EOF on either side, maybe set EOF on other side.
         */
        if ((stdeof == TRUE) && ((readmask & netinmask) == 0)&&(!timeoutmode))
            neteof = TRUE;
        if ((neteof == TRUE) && ((readmask & stdinmask) == 0)&&(!timeoutmode))
            stdeof = TRUE;
    }
}
