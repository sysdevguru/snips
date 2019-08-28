#!/usr/local/bin/perl
#
# Program to run from cron to ensure all the desired SNIPS programs
# are running as desired.
#
# INSTALLATION:
# Set the following:
#	$ADMINMAIL = email address to send the restart messages
#	$email_program = program to send mail. Set subject flags if
#			your mailer supports it  on command line.
#	$ps = command to see all processes on your system
#	$cronsecs = secs between running this file from crontab
#
# Vikas Aggarwal, vikas@navya.com June 2001
#
use strict;
no strict 'refs';
use vars qw($snipsroot $bindir $rundir $ADMINMAIL $email_program $ps
	    $snipshost $thishost $logfile $cronsecs $count
	    @warnings 
	   );

BEGIN {
  $snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS
  push (@INC, "$snipsroot/etc"); push (@INC, "$snipsroot/bin");
  require "snipsperl.conf" ;		# local customizations
}

umask 022;

$ADMINMAIL = q(snips-admin@localhost) unless $ADMINMAIL;# SET_THIS
$email_program = q(/bin/mail) unless $email_program;	# SET_THIS
$cronsecs = 300;		# SET_THIS to secs between runs of this prog

unless ($ps) {
  # my $psout = `ps -ef 2>&1 1>/dev/null`;	# get stderr
  $ps = "/bin/ps axw";		# SET_THIS
  my $ostype= `uname -s -r -m`;
  if ($ostype =~ /HP-UX/ || $ostype =~ /SunOS\s+5/ || $ostype =~ /IRIX/) {
    $ps = "/bin/ps -ef";
  }
}

# List all programs that you want to start automatically here.
# You might want to run some monitors (such as another etherload) on another
# host and write to the same NFS shared data directory. If so, define these
# 
$snipshost = "localhost";		# SET_THIS to your `hostname`
@{$snipshost} = qw( snipslogd etherload ippingmon rpcpingmon
		    nsmon ntpmon portmon radiusmon hostmon hostmon-collector
		    tpmon
		  );
@{'backuphost.navya.com'} = qw( etherload.host2 );

##
# Which program should run on which host.
$thishost=`hostname`; chomp $thishost;

$thishost = 'localhost' if (! defined @{$thishost} && defined @{'localhost'});

die "No monitors for $thishost" if ($#{$thishost} < 0);

##
$rundir = "$snipsroot/run" unless $rundir;
$bindir="${snipsroot}/bin" unless $bindir;
$logfile="${rundir}/keepalive.tmp$$";	# dont create under /tmp

$ENV{PATH} = "$bindir:/bin:/usr/bin:/sbin:/usr/sbin:$ENV{PATH}";

@warnings = ();

chdir $bindir || die "cannot chdir $bindir $!";
if (-f $logfile && (stat($logfile))[9] > time - 15) {
  die "another similar process running??\n";
}

END {unlink $logfile;}
open (LOGFILE, "> $logfile") || die "Cannot open $logfile $!";

foreach my $pgm (@{$thishost}) {
  my $pidfile = "$rundir/$pgm.pid" ;
  my $errfile = "$rundir/$pgm.error";

  ## see if the error file changed recently (since last run?).
  # my $pidage = (stat($pidfile))[9];
  my $errage = (stat($errfile))[9];
  if ( $errage && ($errage > time - $cronsecs) ) {
    push @warnings, "CHECK ERROR FILE $errfile (new errors)\n";
  }
  my $PID = undef;
  if (-f $pidfile && open (PID, "< $pidfile")) {
    while (<PID>) { if (/^\d+$/) { $PID = $_; last; } }
    close (PID);
  }
  if ($PID) { next if (kill 0 => $PID); }	# process okay
  else {
    my $pgmname = $pgm;
    $pgmname =~ s|^.*/||;	# strip pathname
    my @psoutput = `$ps 2>&1`;
    next if (grep(/[\s\/]$pgmname\s+/, @psoutput));	# process OK
  }

  ## need to restart the monitor
  print LOGFILE "RESTARTING monitor  $pgm  on $thishost\n";
  if (-s "$errfile") {
    print LOGFILE "  Previous error file for ${pgm} :\n\n";
    open (ERRFILE, "< $errfile");
    while (<ERRFILE>) { print LOGFILE "  ", $_; }
    close ERRFILE;
    print LOGFILE " ------\n";
  }
  system("${bindir}/${pgm} >$errfile 2>&1 &");	# start new process
  ++$count;
}
close (LOGFILE);

if (-s $logfile || $#warnings >= 0) {
  my $subject = q(-s "[keepalive_monitors]");	# if mailer supports it
  open(MAIL, "|$email_program ". $subject . " $ADMINMAIL") ||
    die "Could not open $email_program $!";

  if ($#warnings >= 0) { print MAIL "WARNINGS\n========\n", @warnings, "\n"; }
  if (-s $logfile) {
    print MAIL "$count MONITORS RESTARTED\n====================\n";
    open (LOGFILE, "< $logfile");
    while (<LOGFILE>) { print MAIL; }
    close(LOGFILE);
  }
  close(MAIL);
}

exit 0;

####
