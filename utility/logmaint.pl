#!/usr/local/bin/perl -w
# $Id: logmaint.pl,v 1.0 2001/07/09 04:12:47 vikas Exp $
#
# daily/weekly maintenance for SNIPS log files.
#
#    - runs 'logstats.pl' on the LOGFILE
#    - move old logs and then sighup the snipslogd process.
#
## Tweak these variables
##
##
use strict;
use vars qw($snipsroot $bindir $etcdir $rundir
	    $OPSMAIL $email_program $logstats $pidfile $logdir $olddir
	    $logfile $keepold $compress $extn
	   );
BEGIN {
  $snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS
  push (@INC, "$snipsroot/etc"); push (@INC, "$snipsroot/bin");
  require "snipsperl.conf" ;		# local customizations
}

$OPSMAIL = q(snips-ops@localhost) unless $OPSMAIL;	# SET_THIS
$email_program = qq(/bin/mail) unless $email_program;	# SET_THIS
$logstats = "$bindir/logstats.pl";	# location of the stats program
$pidfile = "$rundir/snipslogd.pid";

$logdir = "$snipsroot/logs" unless $logdir;
$olddir = "$logdir/old" unless $olddir;
$logfile = "critical";		# which file should logstats process

$keepold = 6;			# number of old logfile revisions
$compress = "gzip" unless $compress;		# compress or gzip
$extn = "gz";

$ENV{PATH} = "$bindir:/bin:/usr/bin:/sbin:/usr/sbin:$ENV{PATH}";

#############

# umask 002 to give write perm to group, 022 for no write to group. Set last
# number to 6 to deny world read (e.g. 026) or 7 to deny world all (e.g. 027)
umask 022;

{
  my @cmdout = `echo A | $compress 2>&1 1>/dev/null`;
  if ($#cmdout >= 0) {
    $compress = "compress";
    $extn = ".Z";
  }
}

-d $logdir || mkdir ($logdir, 0775) || 
    die "Log directory $logdir not found, exiting";
-d $olddir || mkdir ($olddir, 0775) || 
    die "Cannot create old logs dir $olddir, exiting";

chdir $logdir || die "Cannot chdir $logdir";

## Run logstats to send out the reports (run it on LOGFILE)
#
if (-x $logstats && -s $logfile) {
    system("$logstats -f $logfile | $email_program $OPSMAIL");
}

# since we are already in $LOGDIR, compress and move each file into OLD dir
opendir(DIR, ".");
foreach my $f (readdir(DIR))
{
  # print STDERR "processing log file $f\n";
  next if (! -f $f);
  my $j = $keepold - 1;
  while ( $j > 0 ) {
    my $i = $j - 1;
    if (-f "$olddir/$f.$i.$extn") {
      rename "$olddir/$f.$i.$extn", "$olddir/$f.$j.$extn" ;
    }
    --$j;
  }
  
  system ("$compress -c $f > $olddir/$f.0.$extn");
  system ("cp /dev/null $f");
}
closedir(DIR);

##
# Now send signal to snipslogd so that it closes and reopens all
# file descriptors
if (-s $pidfile) {
  my $pid = 0;
  open(F, "< $pidfile");
  while (<F>) { if (/^(\d+)/) { $pid = $1; last; } }
  close (F);
  sleep 2;
  kill 1, $pid;
}

exit 0;
##
