#!/usr/local/bin/perl
#
## $Header: /home/cvsroot/snips/utility/logstats.pl,v 1.2 2003/08/18 06:45:53 russell Exp $
#
# SNIPS log file reporter
#
# Typical usage:
#   logstats.pl  -error  -ignore=10 -file ../logs/warning -v Thruput my-gw
#
# Command line args:
#	-warn	or -crit or -err, only consider events worse than this level
#	-ignore=10	ignore downtimes of less than 10 seconds
#	-f <logfile>	input logfile (else stdin)
#	-v <variable>	only for variable regex listed
#	-r		run report for past 1 week time period only
#	<device> <device> <device>  devicenames to extract report for.
#
# Copyright Netplex Technologies Inc. 1997  info@netplex-tech.com
#
use strict;
use vars qw ( $debug $mindowntime $maxlevel $logfile $var_regex
	      $runtime $stime $etime $info $warn $err $crit
	      $linenum $oklines $badlines $maxlevel $imaxlevel
	      $ncount $device_regex $tot_delta $ndevices $netdown
	      @intlevel %intlevel %month_offset @devices
	      %TOTALDOWN %MAXDOWN %TOTVARVALUE %TOTVARDOWN %DOWN %FLAPS
	      %SPAREUPS 
	    );
$debug = 0;

while ($_ = $ARGV[0], /^-/) {
    shift;
    last if /^--$/;
    /^-ignore\s*=\s*(\d+)/i && do { $mindowntime = $1 ; next; };
    /^-([cewi])/i && do { $maxlevel = $1; next; };
    /^-f/ && do {$logfile = $ARGV[0]; shift; next; };
    /^-v/ && do {$var_regex = $ARGV[0]; shift; next; };
    /^-d/ && do {++$debug ; next; };
    /^-r/ && do {$runtime = time;		# past one week report
		 $etime = localtime($runtime);
		 $stime = localtime($runtime - 604800); };
    /^-/ && do {print STDERR "ERROR unknown option $ARGV[0]\n",
		  " $0 [-info|error|warn|critical] [-ignore=10] \\ \n\t",
		  " [-file=/path/to/file] [-var <variable-regex>] [-r]",
		  " [device device ...]\n\n";
		print STDERR " WHERE\n",
		  "  -ignore\t to skip events shorter than N secs\n",
		  "  -r  \t to run the report for the past one week\n",
		  "      \t (will read logfile from stdin if no -f flag\n",
		  "\n";
		exit 1; };
}
# we are given a list of devices that user is interested in
if ($#ARGV > -1) {
    @devices = @ARGV;
    $device_regex = $ARGV[0]; shift;
    while ($ARGV[0]) {$device_regex .= "|$ARGV[0]"; shift; }
    ($debug > 0) && (print STDERR "+debug DEVICES are @devices, ($device_regex)\n") ;
}
##
## Init variables
# To convert the string severities into numbers...Match only 4 chars
$info = 4; $warn = 3; $err = 2; $crit = 1;
%intlevel = ('I', $info, 'W', $warn, 'E', $err, 'C', $crit);
@intlevel = ("", "Critical", "Error", "Warning", "Info" );
# offset in days of months in the year.
%month_offset = (
    'Jan', 0,    'Feb', 31,   'Mar', 59,  'Apr', 90,
    'May', 120,  'Jun', 151,  'Jul', 181, 'Aug', 212,
    'Sep', 243,  'Oct', 273,  'Nov', 304, 'Dec', 334
		 );
$linenum = 0; $badlines =0;
##
##  END initializing all statics

$maxlevel =~ tr /a-z/A-Z/d;			# all to uppercase
$maxlevel = "CRITICAL" unless $maxlevel;	#default value
$imaxlevel = $intlevel{substr($maxlevel, 0, 1)};	#integer value
if ($imaxlevel >= $info || $imaxlevel < $crit) {
    print "Invalid user level, resetting to CRIT\n";
    $imaxlevel = $crit;
}
$maxlevel = $intlevel[$imaxlevel];

if (defined $logfile && $logfile) {
    open (LOGFILE, "<$logfile") || die("Couldnt open $logfile, $|");
}
else {open (LOGFILE, "cat |"); }	# open stdin

while (<LOGFILE>) {
  ++$linenum;
  if ( /^(\w+\s+\w+\s+\d+\s+\d+:\d+:\d+\s+\d+)\s+\[([^]]+)\]:\s+(SITE|DEVICE)\s+(\S+)\s+(?:(\S+)\s+)?VAR\s+(\S+)\s+(\d+).*LEVEL\s+(\S+)\s+LOGLEVEL\s+(\S+)\s+STATE\s+(\S+)/ )
  {
    my ($e_date, $e_sender, $e_device, $e_addr, $e_varname, $e_varval, $e_level,
	$e_loglevel, $e_nocop) = ($1, $2, $4, $5, $6, $7, $8, $9, $10);
  
    # if any devices given, do only the listed devices.
    if ($device_regex) { next unless $e_device =~ /$device_regex/oi; }
    if ($var_regex)  { next unless $e_varname =~ /$var_regex/oi; }
    
    my $index = "$e_device:$e_sender:$e_varname";
    # if ($debug > 5) { print "+(debug) index=$index\n" };
    my $ilevel = $intlevel{substr($e_level, 0, 1)};
    if ($ilevel < $crit || $ilevel > $info) { # invalid level
      print STDERR "+$linenum Illegal Level ($e_level=$ilevel): $_";
      ++$badlines;
      next ;
    }
    ++$oklines;
    if (!defined($stime)) { $stime = $e_date; }	# starting date of all LOGS
    $etime = $e_date;	# end date for all LOGS, overwritten each time
  
    if ($ilevel <= $imaxlevel) { # user desired worst level, i.e. is DOWN
      # if already down entry, leave alone, dont update. Else...
      if (!defined($DOWN{$index}) || $DOWN{$index} == 0) {
	$DOWN{$index} = &cvt_time($e_date); # store timestamp
	++$FLAPS{$index};	# how many times did this device go down
	$TOTVARVALUE{$index} += $e_varval ; # to calculate average
	if ($debug > 1) { print "+(debug) DOWN{$index} = $DOWN{$index}\n";}
      }
    }
    else {			# device is up
      if ($DOWN{$index} != 0) {	# we have a previous down entry
	my $delta = &cvt_time($e_date) - $DOWN{$index};
	if ($debug > 1) { print "+(debug) $index UP after $delta secs\n";}
	if (defined $mindowntime && $delta <= $mindowntime) {
	  $DOWN{$index} = 0;	#reset
	  # --$FLAPS;		# decrement ?? or let it be...
	  next;
	}
	$TOTALDOWN{$index} +=  $delta;
	$MAXDOWN{$index} = &max($delta, $MAXDOWN{$index});
	$TOTVARDOWN{$e_varname} += $delta; #track stats per variable
	$DOWN{$index} = 0;	# reset this variable
      }
      else { ++$SPAREUPS{$index} ; } # we got an UP when device was not down
    }
    next;
  }	# end if(//)
  # if not matching regular expression above, flag as bad line
  ++$badlines;

}	# end while(<LOGFILE>)

close(LOGFILE);

$tot_delta = &cvt_time($etime); $tot_delta -=  &cvt_time($stime);
if ($tot_delta == 0) {$tot_delta = 1; }

##### Start REPORT: ####
## Header
printf ("\n%50s\n%50s\n", "SNIPS SUMMARY REPORT", "====================");
printf ("%75s\n%75s\n", "From: $stime", "To: $etime");
print "Severity level: $maxlevel\n";
print "Total Lines processed = $oklines, Unparseable= $badlines\n";
print "All times in d+hh:mm\n";
if ($#devices > -1) {print "Report for devices:  @devices\n"; }
if ($var_regex) {print "Filter on variable name: $var_regex\n"; }
if ($mindowntime) {print "Skipping downtimes less than $mindowntime secs\n"; }

## Per device report
print "\nPer Device Reports\n==================\n\n";
print "    Device       Variable       +--------- Downtime ---------+   Total    Avg.\n",
      "                                Total     Max      Avg.    %age  Downs   Value\n",
      "-"x78, "\n";
for my  $s (sort(keys %DOWN))
{
  next if (defined $mindowntime && $TOTALDOWN{$s} <= $mindowntime);
  my ($device, $sender, $var) = split(':', $s);
  $netdown += $TOTALDOWN{$s};
  ++$ndevices;
  if ($FLAPS{$s} == 0) { $FLAPS{$s} = 1; }	# avoid divide by zero
  my ($avg, $percent) = (&secs2hrs($TOTALDOWN{$s}/$FLAPS{$s}),
			 ($TOTALDOWN{$s}*100)/$tot_delta );
  printf "%-14s  %-11s  %6s  %6s  %6s  %.3s  %5s  %5s\n",
           $device, $var, &secs2hrs($TOTALDOWN{$s}),
           &secs2hrs($MAXDOWN{$s}),
           $avg, $percent, $FLAPS{$s}, int($TOTVARVALUE{$s}/$FLAPS{$s}) ;
}
if ($ndevices > 0) {
    printf ("\n\nTotal downtime (all devices)=%s\nAverage downtime= %s\n",
	    &secs2hrs($netdown), &secs2hrs($netdown/$ndevices));
}

## Reports  based on variables
print "\nPer Variable Report\n===================\n\n";
print "      Variable      TotalDown\n    --------------------------\n";
$ncount = 0;
foreach my $s (sort {$TOTVARDOWN{$b} <=> $TOTVARDOWN{$a}} (keys %TOTVARDOWN))
{
  last if (++$ncount == 5);		# only top 5
  printf "%18s   %s\n", $s, &secs2hrs($TOTVARDOWN{$s});
}

## 10 Worst devices
print "\n10 Worst Device Events\n======================\n\n";
printf "%15s  %10s %s\n",  "Device     ", "Variable  ", "TotalDown";
print "    ------------------------------------\n";
$ncount= 0;
foreach my $s (sort {$TOTALDOWN{$b} <=> $TOTALDOWN{$a}} (keys %TOTALDOWN))
{
  last if (++$ncount == 10);		# only top 10
  printf "%15s %0.0s %-10s %s\n", split(/:/, $s), &secs2hrs($TOTALDOWN{$s});
}

exit 0;

##
## Sub-routines
##
# Convert a date string 'Wed Mar 18 12:48:19 1997'  into secs since 1970
sub cvt_time {
  my ($datestr) = @_;
  my $timestamp = 0;
  if ($datestr =~ /^\S+\s+(\w+)\s+(\d+)\s+(\d\d):(\d\d):(\d\d)\s+(\d\d\d\d)/)
  {
    $timestamp = (($6 - 1970) * 365) + $month_offset{$1} + $2;
    $timestamp *= (24 * 3600);
    $timestamp += ($3 * 3600) + ($4 * 60) + $5;
  }
  return ($timestamp);
}

sub max {
  my ($a, $b) = @_;
  return ($a > $b) ? $a : $b;
}

sub secs2hrs {
  my ($secs) = @_;
  my ($d, $h, $m, $s);
  $d = int($secs / (24*3600));  $secs %= (24 * 3600);
  $h = int($secs / 3600);	$secs %= 3600;
  $m = int($secs / 60);	$secs %= 60;
  $s = $secs ;

  if ($d > 0) { return sprintf("%d+%2.2d:%2.2d", $d, $h, $m) ;}
  else { return sprintf("%2.2d:%2.2d", $h, $m); }
}
