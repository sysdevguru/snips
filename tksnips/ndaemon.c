/* #define DEBUG		/* */
/*
 * $Header: /home/cvsroot/snips/tksnips/ndaemon.c,v 1.0 2001/07/09 03:49:04 vikas Exp $
 */

/* ndaemon.c */

/*
 * Intended as a drop-in replacement for SNIPS's netconsole.
 * The code is heavily derived from the netconsole stuff; I've not made
 * much in the way of an attempt to clean up the code, preferring to copy
 * for the sake of future compatibility.
 * 
 * Like netconsole, ndaemon looks through the data directory periodically
 * to see if there have been any updates. However, it then notifies clients
 * connected to it over the network (thus vastly cutting down on the load
 * on the machine, much of which is likely caused by dozens of attempts
 * to repeatedly read the data directory).
 *
 * The monitoring clients can be written in anything, a Tcl/Tk version
 * is supplied (tkSnips).
 *
 * The client can send the following commands:
 *	QUIT  close connection
 *	SEVLEVEL 1-4  max severity level
 * This ndaemon server writes out all events starting with a BEGIN and
 * one event per line.. all fields separated by a '!' (see send_to_clients()).
 *
 * Add -DLEASED_LINES if compiling for LL SNIPS (special hack).
 *
 * AUTHOR:
 *	Lydia Leong (lwl@godlike.com, lwl@sorcery.black-knight.org)
 *	January 1998
 */

/*
 * $Log: ndaemon.c,v $
 * Revision 1.0  2001/07/09 03:49:04  vikas
 * Initial revision
 *
 */

#define _MAIN_
#include "ndaemon.h"
#undef _MAIN_

#ifdef LEASED_LINES		/* Company specific */
# include "lookup.h"
#endif

#ifndef SEVERITY_MAX
# define SEVERITY_MAX 4		/* Info level */
#endif

#ifndef SEVERITY_DEFAULT
# define SEVERITY_DEFAULT 2	/* Error level */
#endif

#ifndef PORT			/* port to listen on,, must match clients */
# define PORT 5005
#endif

static int nclients = 0;
static char *datadir;				/* Dir of data files */
static char *msgsdir;				/* Dir with text msg files */
static DESC *client_descs_head;
static DESC *client_descs_tail;

static void kill_client();

/* --------------------------------------------------------------------------
 * Initialize program, including socket setup. Returns socket fd.
 */

static int ndaemon_init(port)
    int port;
{
    int s;
    struct sockaddr_in server;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("ndaemon FATAL error (socket)");
	exit(1);
    }

#ifdef SO_REUSEADDR_NO
# if defined(SUNOS5) || defined(SVR4)
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, 0, 0) < 0) {
	perror("ndaemon FATAL error (setsockopt)");
	close(s);
	exit(1);
    }
# endif
#endif

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(s, (struct sockaddr *) &server, sizeof(server)) < 0) {
	perror("ndaemon FATAL error (bind)");
	close(s);
	exit(1);
    }

    if (listen(s, 5) < 0) {
	perror("ndaemon FATAL error (listen)");
	close(s);
	exit(1);
    }

    client_descs_head = client_descs_tail = NULL;

    datadir = DATADIR;		/* global variable */

    return s;
}

/* --------------------------------------------------------------------------
 * Report information to clients.
 */

static void text_to_client(d, buf)
    DESC *d;
    char *buf;
{
    char *tstr;

    if (d->output_text == NULL) {
	d->output_text = (char *) malloc(sizeof(char) *
					 (strlen(buf) + 1));
	strcpy(d->output_text, buf);
    } else {
	tstr = (char *) malloc(sizeof(char) *
			       (strlen(d->output_text) +
				strlen(buf) + 2));
	strcpy(tstr, d->output_text);
	strcat(tstr, buf);
	free(d->output_text);
	d->output_text = tstr;
    }
}

static void send_to_clients(v)
    EVENT v;
{
    DESC *d;
    int i;
    static char buf[MAXLINELEN];
    struct tm *tm;

    tm = localtime( (time_t *) &(v.eventtime) );
    /* Use the '!' as an output delimiter for fields. */

    sprintf(buf,
	    "%s!%s!%02d/%02d!%02d:%02d!%s!%s!%lu!%lu!%s!%03o!%d!%s!%d\n",
	    v.device.name, v.device.addr,
	    tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
	    v.sender,
	    v.var.name, v.var.value, v.var.threshold, v.var.units,
	    v.state,
	    v.severity,
#ifdef LEASED_LINES
	    v.var.ticketnum,
	    v.var.ticketstat
#else
	    "X",
	    0
#endif /* LEASED_LINES */
	);

    WALK_DESC_CHAIN(d, i) {
	if (v.severity <= d->sev_level) {
	    text_to_client(d, buf);
	}
    }
}

/* --------------------------------------------------------------------------
 * Output stuff. Replaces display_one_event() in netconsole.
 *
 * Note our exit condition is the opposite of display_one_event() -
 * we return 1 if we're done.
 */

static int output_event(fd)
    int fd;			/* opened file descriptor */
{
    EVENT v;			/* from snips.h */

    if (read(fd, (char *) &v, sizeof(v)) != sizeof(v))
	return 1;		/* end of file */
    else {
	static EVENT null_event; /* all fields are null */
	if ((bcmp(&v, &null_event, sizeof(v)) != 0) &&
	    (v.severity <= SEVERITY_MAX)) {

	    /* We've got a non-null event within our severity levels. */

#ifdef DEBUG
		printf("Device Name: %s\n", v.device.name);
		printf("Device Address: %s\n", v.device.addr);
		printf("Sender: %s\n", v.sender);
		printf("Var Name: %s\n", v.var.name);
		printf("Var Value: %lu\n", v.var.value);
		printf("Var Thresh: %lu\n", v.var.threshold);
		printf("Var Units: %s\n", v.var.units);
#ifdef LEASED_LINES
		printf("Ticket number: %s\n", v.var.ticketnum);
		printf("Ticket status: %d\n", v.var.ticketstat);
#endif /* LEASED_LINES */
		printf("Flags: %03o\n", v.state);
		printf("Condition: %s\n", severity_txt[v.severity]);
#endif /* DEBUG */

		send_to_clients(v);

	}
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * Poll loop. Replaces event_dpy() and fill_window() in netconsole.
 */

static int poll_loop()
{
    DESC *d;
    int i;
    DIR *datadirp;
    int datafd;
    struct dirent *direntry;
    char file[MAXLINELEN];
    struct stat buf;
    int done;

#ifdef DEBUG
    printf("-- BEGINNING NEW CYCLE --\n");
#endif

    WALK_DESC_CHAIN(d, i) {
	text_to_client(d, "BEGIN\n");
    }

    if ((datadirp = opendir(datadir)) == NULL) {

	/* Hmm. This is bad. But we should return and pray that it
	 * goes better next time.
	 */
        fprintf(stderr, "ndaemon error in poll_loop opendir() ");
	perror(datadir);
	return 1;
    }

    while ((direntry = readdir(datadirp)) != NULL) {
	
	/* Walk through the directory, processing each file in turn.
	 * Skip dotfiles, core files, or anything bizarre.
	 */

	if ((*(direntry->d_name) == '.') ||
	    !strcmp(direntry->d_name, "core"))
	    continue;

	sprintf(file, "%s/%s", (char *) datadir, (char *) direntry->d_name);

	if ((datafd = open(file, O_RDONLY)) < 0) {

	    /* Couldn't open the file. Ugly, but not fatal. */
	    fprintf(stderr, "ndaemon error in poll_loop open() ");
	    perror(file);
	    continue;
	}

	fstat(datafd, &buf);
	if ((buf.st_mode & S_IFMT) == S_IFDIR) {
	    /* Skip directories. */
	    close(datafd);
	    continue;
	}

#ifdef DEBUG
	printf("-- READING %s --\n", file);
#endif

	done = 0;
	while (!done) {
	    done = output_event(datafd); /* slurp and display one event */
	}
	close(datafd);
    }

    closedir(datadirp);

#ifdef DEBUG
    fflush(stdout);
#endif

    WALK_DESC_CHAIN(d, i) {
	text_to_client(d, "END\n");
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * Process a client request.
 */

static int handle_command(d, cmd)
    DESC *d;
    char *cmd;
{
    char *argstr;
    int numarg;

    if (strlen(cmd) > 0 && cmd[strlen(cmd) - 1] == '\r')
      cmd[strlen(cmd) - 1] = '\0';

#ifdef DEBUG
    fprintf(stderr, "debug handle_command(%s)\n", cmd);
#endif

    if (!strcasecmp(cmd, "QUIT") || !strcasecmp(cmd, "EXIT")) {
      kill_client(d);
      return 0;
    }

    argstr = (char *) index(cmd, ' ');
    if (!argstr)		/* invalid malformed command */
	return;
    *argstr++ = '\0';

    if (!strcasecmp(cmd, "SEVLEVEL")) {

	/* Change severity level */

	numarg = atoi(argstr);
	if ((numarg < 1) || (numarg > 4)) {
	    /* Invalid severity level */
	    return;
	}
	d->sev_level = numarg;
    }
    else {
	/* Invalid command */
#ifdef DEBUG
        fprintf(stderr, "debug handle_command(%s) invalid command\n", cmd);
#endif
    }
    return 1;
}

/* --------------------------------------------------------------------------
 * Networking routine: Deal with client input.
 */

static int handle_input(d)
    DESC *d;
{
    static char buf[MAXLINELEN];
    int got, cneeded, slen;
    char *startp, *endp, *tstr;

#ifdef DEBUG
    fprintf(stderr, "debug handle_input()\n");
#endif
    got = read(d->descriptor, buf, sizeof(buf));
    if (got <= 0)
	return 0;

    slen = strlen(buf);

    if (d->input_text == NULL) {

	/* Allocate space for the first time */

	d->input_text = (char *) malloc(sizeof(char) * (slen + 1));
	strcpy(d->input_text, buf);

    } else {

	/* Stretch the buffer to accomodate things */

	tstr = (char *) malloc(sizeof(char) *
			       (strlen(d->input_text) + slen + 2));
	strcpy(tstr, d->input_text);
	strcat(tstr, buf);
	free(d->input_text);
	d->input_text = tstr;
    }

    /* Now we walk the string, looking for newlines. Each newline-terminated
     * string is a command.
     */
#ifdef DEBUG
    fprintf(stderr, "handle_input() read %s---\n", d->input_text);
#endif
    slen = strlen(d->input_text);
    startp = d->input_text;
    for (endp = (char *) index(d->input_text, '\n');
	 endp && *endp && (endp <= d->input_text + slen); ) {
	*endp++ = '\0';
	if (handle_command(d, startp) == 0)
	  return 1;		/* closed connection, so return */
	startp = endp;
	endp = (char *) index(startp, '\n');
    }

    /* As we leave this loop, startp should be pointing to the beginning
     * of the string that isn't newline-terminated.
     */

    cneeded = strlen(startp);
    if (cneeded < 1) {
	/* All text has been processed. */
	free(d->input_text);
	d->input_text = NULL;
    } else {
	tstr = (char *) malloc(sizeof(char) * (cneeded + 1));
	strcpy(tstr, startp);
	free(d->input_text);
	d->input_text = tstr;
    }

    return 1;
}

/* --------------------------------------------------------------------------
 * Networking routine: Deal with client output.
 */

static int handle_output(d)
    DESC *d;
{
    int got, total_sent, left_send, to_send, extra_chars;
    char *tmp_text;

    if (d->output_text) {

	total_sent = 0;
	to_send = strlen(d->output_text);
	left_send = to_send;
	extra_chars = 0;

	while (left_send > 0) {
	    got = write(d->descriptor, d->output_text + total_sent, left_send);
	    if (got < 0) {
		/* Hmm, we've hit a problem of some sort. */
		extra_chars = left_send;
		left_send = 0;
	    } else {
		total_sent += got;
		left_send -= got;
	    }
	}

	if (extra_chars == 0) {
	    free(d->output_text);
	    d->output_text = NULL;
	} else {
	    tmp_text = (char *) malloc (sizeof(char) * (extra_chars + 1));
	    strcpy(tmp_text, d->output_text + total_sent);
	    free(d->output_text);
	    d->output_text = tmp_text;
	}
    }

    return 1;
}

/* --------------------------------------------------------------------------
 * Networking routine: Close down a connection.
 */

static void kill_client(d)
    DESC *d;
{
    DESC *curd;

#ifdef DEBUG
    fprintf(stderr, "Killing client\n");
#endif
    if (d->input_text)
	free(d->input_text);
    if (d->output_text)
	free(d->output_text);

    shutdown(d->descriptor, 2);
    close(d->descriptor);

    /* We've got to relink the descriptor list. */

    if (client_descs_head == d) {

	client_descs_head = d->next;
	if (client_descs_tail == d)
	    client_descs_tail = NULL;

    } else {

	/* Find the previous member of the list and relink. */

	for (curd = client_descs_head;
	     (curd != NULL) && (curd->next != d);
	     curd = curd->next)
	    ;
	curd->next = d->next;
	if (client_descs_tail == d)
	    client_descs_tail = curd;
    }

    free(d);
    nclients--;
}

/* --------------------------------------------------------------------------
 * Networking routine: Accept a new connection.
 */

static DESC *new_conn(sockfd)
    int sockfd;
{
    int newsock, addr_len;
    struct sockaddr_in addr;
    DESC *d;
#ifdef SO_LINGER
    struct linger ling;
#endif
    addr_len = sizeof(struct sockaddr);

    if ((newsock = accept(sockfd, (struct sockaddr *) &addr, &addr_len)) < 0)
	return 0;

    /* Make this socket non-blocking. */
    if (fcntl(newsock, F_SETFL, O_NDELAY) < 0) {
	perror("ndaemon error in new_conn (fcntl O_NDELAY)");
    }

#ifdef SO_LINGER
    ling.l_onoff = 0;
    ling.l_linger = 0;		/* linger in secs */
    if (setsockopt(newsock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling)) < 0) {
	perror("ndaemon error in new_conn (setsockopt SO_LINGER)");
    }
#endif
    d = (DESC *) malloc(sizeof(DESC));
    d->descriptor = newsock;
    d->sev_level = SEVERITY_DEFAULT;
    d->input_text = NULL;
    d->output_text = NULL;

    if (client_descs_head == NULL) {
	client_descs_head = d;
    } else {
	client_descs_tail->next = d;
    }
    client_descs_tail = d;
    d->next = NULL;

    nclients++;

    return d;
}

/* --------------------------------------------------------------------------
 * Main event loop, includes network polling.
 */

static void event_loop(sockfd)
    int sockfd;
{
    fd_set input_set, output_set;
    DESC *d, *newd, *nextd;
    int check, found, maxd, i;
    struct timeval timeout;
    long last_time, cur_time;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    maxd = sockfd + 1;

    last_time = time(NULL);

    while (1) {

	FD_ZERO(&input_set);
	FD_ZERO(&output_set);

	FD_SET(sockfd, &input_set);
	WALK_DESC_CHAIN(d, i) {
	    FD_SET(d->descriptor, &input_set);
	    if (d->output_text)
		FD_SET(d->descriptor, &output_set);
	}

	found = select(maxd, &input_set, &output_set,
		       (fd_set *) NULL, &timeout);
	if ((found < 0) && (errno != EINTR)) {
	    perror("ndaemon error (select)");
	    continue;
	}

	if (FD_ISSET(sockfd, &input_set)) {
	    newd = new_conn(sockfd);
	    if (!newd) {
		check = (errno & (errno != EINTR) &&
			 (errno != EMFILE) && (errno != ENFILE));
		if (check)
		    perror("ndaemon error in event_loop (new_conn)");
	    } else if (newd->descriptor >= maxd)
		maxd = newd->descriptor + 1;
	}

	for (d = client_descs_head, i = 0;
	     (d != NULL) && (i < nclients);
	     d = nextd, i++) {

	    nextd = d->next;

	    if (FD_ISSET(d->descriptor, &input_set)) {
		if (!handle_input(d)) {
		    kill_client(d);
		    continue;
		}
	    }
	    if (FD_ISSET(d->descriptor, &output_set)) {
		if (!handle_output(d)) {
		    kill_client(d);
		    continue;
		}
	    }
	}

	cur_time = time(NULL);
	if (cur_time >= last_time + 15) {
	    poll_loop();
	    last_time = cur_time;
	}
    }
}

/* --------------------------------------------------------------------------
 * Main program.
 */

int main(argc, argv, envp)
    int argc;
    char **argv;
    char **envp;
{
    int sockfd;

    fprintf(stderr, "Listening on port %d\n", PORT);

    sockfd = ndaemon_init(PORT);

    event_loop(sockfd);
}
