#!/usr/local/bin/perl -w
#
# $Header: /home/cvsroot/snips/utility/notifier.pl,v 1.1 2001/08/17 03:15:33 vikas Exp $
#
# 	notifier.pl  for SNIPS
#
# Search for SET_THIS and set the value of $pager_program.
#
# Run this script every 5-10 minutes from crontab. Adjust $crontime 
# accordingly.
# You can also run this script from snipslogd, so that you get notified as
# soon as a device comes up or goes down. In this mode, it reads stdin for
# eventlog format data, and the time of the event is set to -1 sec. In
# this mode, it notifies for all the events read from stdin (i.e. all
# severities). e.g.
#
#   display_snips_datafile ntpmon-output | grep Critical | notifier.pl -
#
# CAVEATS:
#	- Only looks at CRITICAL events.
#	- Sends one email/page per event instead of grouping them together
#	- Does not notify when device comes back up when run from cron (but
#	  will if run from snipslogd).
#	- If too many mail or pager processes are called, this can generate
#	  a 'Too many open files"  error.
#
# AUTHOR:
#	Adrian Close (adrian@aba.net.au), 1998
#	vikas@navya.com - Added reading data from stdin, cleaned up.
#
use strict;
use vars qw($snipsroot $bindir $etcdir $datadir 
	    $debug $email_program $pager_program $eventage
	    $updatesfile $cfile $OPSMAIL $crontime $repeat_interval
	    %hrlyrepeat %notify %update %severity
	   );

BEGIN {
  $snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS
  push (@INC, "$snipsroot/etc"); push (@INC, "$snipsroot/bin");
  require "snipsperl.conf" ;		# local customizations
}

# require 5.000;
# require "Time::Local";	# in perl v5
require "timelocal.pl";		# convert May 1998 into unix timestamp
use SNIPS;

$datadir = "$snipsroot/data" unless $datadir;
$debug = 0;

# Set locations of extra configuration files. 'updatesfile' is maintained
# by 'websnips'
# $updatesfile = "$etcdir/exceptions";
$updatesfile = "$etcdir/updates";
$cfile = "$etcdir/notifier-confg";

## Locations of helper programs SET_THIS
$pager_program = "/usr/local/bin/sendpage" unless $pager_program; # SET_THIS
$email_program = "/usr/ucb/mail" unless $email_program;	# SET_THIS

-x $pager_program || undef ($pager_program);
-x $email_program or die "Cannot find $email_program.\n";

## the default user who should get email if there is NO config file.
#  remember to put a backslash in front of the '@'
#$OPSMAIL = "ops\@localhost";
$OPSMAIL = q(snips-ops@abc.domain) unless $OPSMAIL;	# SET_THIS

# The notifier should run every $crontime seconds (or less) to catch all
# events and avoid extra pages, email etc. This is the time 'window' used
# for catching new critical events (or repeats).
$crontime = 600;		# seconds	# SET_THIS

# Repeat notification interval
# (notify again for events that are a multiple of $repeat_interval secs old)
# Something like an hour is probably sane (i.e. 3600 secs)
$repeat_interval = 3600;	# seconds

##
#  Read configuration file
#
#	device:addr:variable  \t repeat \t <notify-action>
#  where 
#	notify-action = address:medium:min-age  (medium = 'mail' or 'page')
#	repeat = 0 or 1 (hourly page or not)
#	
#  e.g.
#	nfshost:12.1.2.3:ICMP-ping	0	adrian@abc.com:mail:3600,617383939:page:0
#
#
sub readconf {
  my $lineno = 0;

  if (! open (CONFIG, "< $cfile")) {
    print STDERR "Unable to open $cfile.\n" if ($debug);

    $hrlyrepeat{'default'} = 1;	# for unlisted devices, do repeat notification
    $notify{'default'} = [ "$OPSMAIL" . ":email" . ":0" ];
    return;
  }

  while (<CONFIG>)
  {
    ++$lineno;
    chomp;
    next if ( /^\s*\#/ || /^\s*$/ );	# comments & blank lines

    # device:addr[:var]  repeat  to:medium:minage
    if (/^\s*(\S+)\s+([0-1])\s+(.+)/)
    {
      $hrlyrepeat{$1} = $2;		# 0 or 1
      $notify{$1} = [ split (/\,/, $3) ] ;	# hash of array values
      next;
    }

    print STDERR "Unknown config line [$lineno]: $_\n";
  }	# while config
  close (CONFIG);

}	# sub readconf()

##
# Read updates file (earlier called exceptions) for special comments on a
# device. This file is maintained by websnips and its syntax is:
#
#	devicename:deviceaddr:variable \t status
# If the status begins with a (H), then the device is hidden/ignored.
#
sub readupdates { 		
  if (open (UPDATESFILE, $updatesfile))
  {
    while (<UPDATESFILE>)
    {
      chomp;
      next if (/^\s*\#/);	# skip comments
      next if (/^\s*$/);	# skip blank lines
      if (/^(\S+)\s+(.+)/) { $update{$1} = $2 ;	next; } # Build $update array
      print STDERR "Cannot parse updates line $_\n";
    }
    close (UPDATESFILE);
  }

} #&readupdates


##
# Process Current Events
#

sub processevents {

  # get all data files from SNIPS data directory
  # (extract only those files that have '-output' as prefix)

  opendir(DATADIR, "$datadir") || die "\nCannot open input directory $datadir";
  my @datafiles = sort(grep(/.+-output/i, readdir(DATADIR)));
  closedir(DATADIR);

  foreach my $datafile (@datafiles)
  {
    my ($junk, $cmonth, $cyear, $event);
    my $datafd = open_datafile("$datadir/$datafile", "r") ||
      die "Cannot open $datadir/$datafile";
    $debug && print STDERR "Processing file: $datafile\n";

    # Get current month and year
    ($junk,$junk,$junk,$junk, $cmonth,$cyear, $junk,$junk,$junk) = 
      localtime(time);
    my $cursecs = time;		# Epoch seconds (seconds since 1/1/1970)

    while ( ($event = read_event($datafd)) )
    {
      my %event = unpack_event($event);
      next if ($event{severity} != $E_CRITICAL);	# not critical

      # we check for device:addr:var and then for device:addr generic index
      my $id  = "$event{device_name}:$event{device_addr}"; # no variable name
      my $idv = "${id}:$event{var_name}";	# with varname
		
      # event is "hidden" , so ignore
      if ( (defined($update{$idv}) && $update{$idv} =~  m/^\(H\)/) ||
	   (defined($update{$id})  && $update{$id} =~ m/^\(H\)/) )
      {
	print  STDERR "${idv}: Hidden!\n" if ($debug > 1);
	next;
      }

      $eventage = $cursecs - $event{eventtime};	# age of event
      if ($eventage < 0) {
	$cursecs = time;	# update, event probably just happened
	$eventage = $cursecs - $event{eventtime};	# age of event
	print STDERR "Event $idv  age is negative ($eventage), check.\n" 
	  if ($eventage < 0);	# still negative, probably clock mismatch?
      }
      print STDERR "Event $idv is $eventage secs old.\n" if ($debug > 1);
      my @tm = localtime($event{eventtime});
      my $subject = "[SNIPS] $event{device_name} $event{device_addr}";
      my $message = "$event{device_name} $event{device_addr} $event{var_name} critical";
      $message .= sprintf "  since %02d:%02d on %02d/%02d\n",
	          $tm[2], $tm[1], $tm[4]+1, $tm[3];

      if    ( defined($notify{$idv}) ) { &Notify($idv, $subject, $message); }
      elsif ( defined($notify{$id}) ) { &Notify($id, $subject, $message); }
      elsif ( defined($notify{'default'}) ) { &Notify('default', $subject, $message); }
      else  { print STDERR "No action for event $idv, skipping\n" if $debug; }

    }	# while (&readevent)

    close_datafile($datafd);

  }	# foreach ($datafile)

}	# sub processevents()


##
# Process addresses to be notified for current event's DEVICE:ADDRESS
# combination and send via appropriate medium. Check the minimum age
# of the notifier.
# If minimum age is set to -1 in config file, it only matches the event
# when run from stdin.
#
sub Notify {
  my ($id, $subject, $message) = @_;

  print STDERR "Notify: Processing $id\n" if ($debug > 1);
  # Process each address
  foreach my $notify (@{ $notify{$id} })
  {
    print STDERR "  Action = $notify\n" if ($debug > 2);
    my $donotify = 0;	# assume dont need to page or mail
    my ($address, $medium, $minage) = split /:/, $notify;
    $address =~ s/^\s*(.*?)\s*$/$1/;	# trim white space
    $medium  =~ s/^\s*(.*?)\s*$/$1/;	# trim white space

    if ($minage == -1 && $eventage == -1)	# see if we need to notify
    {
      $donotify = 1;
    }
    else
    {
      $minage = int($minage);
      $minage *= 60;	# convert to seconds
      if ($debug > 2) {
	printf STDERR ("event age = $eventage, $address wants between %d and %d\n",
	  ($minage - $crontime), ($minage + $crontime + 1));
      }

      # Check the age of the event and if hourly repeat is set. We have
      # to consider granularity of how often this program is run to avoid
      # excessive notifications.
      # 	- If minage (+- crontime), then notify
      #		- If multiple of an hour, and hrlyrepeat, then notify

      if ($eventage > ($minage - $crontime) && 
	  $eventage <= ($minage + $crontime + 1) )
      { $donotify = 1; }
      elsif ( (($eventage - $minage) / $repeat_interval) >= 1 &&
	      (($eventage - $minage) % $repeat_interval) <= $crontime )
      {
	# notify only if config permits repeat-notifications
	if ( ( defined($hrlyrepeat{$id}) && $hrlyrepeat{$id} == 1) ||
	     (!defined($hrlyrepeat{$id}) && $hrlyrepeat{'default'} == 1) )
	{
	  $donotify = 1;
	}
      }
    }

    next if ($donotify == 0);

    $debug && print STDERR "Notifying: $address by $medium about $subject\n";
    if ($medium =~ /sms/i || $medium =~ /page/i)  {
      &PageNotify($id, $address, $message) if (defined($pager_program));
    }
    elsif ($medium =~ /mail/i) {	#email or mail
      &EmailNotify($id, $address, $message, $subject);
    }
    else {
      print STDERR "Unknown notify medium- $medium\n";
    }

  }	# foreach
}

#
# Send  page
sub PageNotify {
  my ($id, $address, $message) = @_;

  print STDERR "   Paging $address\n" if ($debug > 1);
  open(XPIPE, "|$pager_program $address") ||
    die "Could not open $pager_program\n"; 
  print XPIPE "$message";
  if (defined($update{$id}) && $update{$id} ne "") {
    print XPIPE " - $update{$id}.";
  }
  close(XPIPE);
}

sub EmailNotify {		# Send email notification of fault
  my ($id, $address, $message, $subject) = @_;

  print STDERR "   Emailing $address\n" if ($debug > 1);

  if ($email_program =~ /ucb/) {
    open(XPIPE, "|$email_program -s \"SNIPS notification: $subject\" $address") ||
      die "Cannot open $email_program.\n";
  }
  else {	# not ucb/mail so cannot take -s command line option
    open(XPIPE, "|$email_program $address") || die "Cannot open $email_program.\n";
  }
  print XPIPE "$message";
  if (defined($update{$id}) && $update{$id} ne "") {
    print XPIPE "\n\t UPDATE: $update{$id}\n";
  }
  close(XPIPE);
}

##
#  Read the log line from the stdin.  Extract the device, variable and 
#  decide who to send this to based on the config file. This is
#  used when notifier.pl is run by snipslogd
sub process_stdin {
  my $eventstr;
  while (<>)
  {
    chomp;
    $eventstr = $_;
    if (/[(\S+)]:\s+SITE|DEVICE\s+(\S+)\s+(\S+)\s+.*\s*VAR\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+LEVEL/)
    {
      my ($sender, $devicename, $deviceaddr, $varname, $varval) = ($1, $2, $3, $4, $5);
      # we check for device:addr:var and then for device:addr generic index
      my $id  = "${devicename}:${deviceaddr}"; # no variable name
      my $idv = "${id}:${varname}";	# with varname
		
      # event is "hidden" , so ignore
      if ( (defined($update{$idv}) && $update{$idv} =~  m/^\(H\)/) ||
	   (defined($update{$id}) &&  $update{$id} =~ m/^\(H\)/) )
      {
	print  STDERR "${idv}: Hidden!\n" if ($debug > 1);
	next;
      }

      # set eventage to special value to indicate stdin data so that we
      # always notify and dont work off of the age.
      $eventage = -1;
      print STDERR "Event $idv is $eventage secs old.\n" if ($debug > 1);

      my $subject = "[snips] $devicename $deviceaddr ($sender)";
      if    ( defined($notify{$idv}) ) { &Notify($idv, $subject, $eventstr); }
      elsif ( defined($notify{$id}) ) { &Notify($id, $subject, $eventstr); }
      elsif ( defined($notify{'default'}) ) { &Notify('default', $subject, $eventstr); }
      else  { print STDERR "No action for event $idv, skipping\n" if $debug; }
      next;
    }
    print STDERR "UNKNOWN format of input line:\n  $_\n";
  }	# while <>
}  # process_stdin

##
## Main program
##
&readconf;
&readupdates;
if ($#ARGV >= 0 && $ARGV[0] eq '-') {
  $debug && print STDERR "Processing stdin\n";
  &process_stdin;
  exit 0;
}
&processevents;		# process all data files in data directory

