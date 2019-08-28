#!/usr/bin/perl

# Simple script to get things for monitoring via SNMP and turn that 
# into snips conf. (for snmpgeneric2)
# 
# Rob Lister <robl@linx.net> 2006
#
# This is designed to discover SNMP elements on linux hosts running snmpd,
# and has some examples of custom OIDs  Modify for your requirements:

use strict;

my $SNMPGET="/usr/bin/snmpget";
my $SNMPWALK="/usr/bin/snmpwalk";
my $SNIPSROOT="/usr/local/snips";

my $HOST=$ARGV[0];

die "no hostname specified" unless ($HOST);

my $HOST_IP=&hostlookup($HOST);

unless ($HOST_IP) {
    die "Unknown host: ", $HOST, "\n";
}

$HOST =~ s/.yourdomain.net//g;

my $COMMUNITY="Your_SNMP_Community";

if ($ARGV[1]) {

print "** Setting community to $ARGV[1]\n";

$COMMUNITY="$ARGV[1]";

}

# You may want to change this to keep copies of the host configs elsewhere:
my $CONFDIR="$SNIPSROOT";

my $OID_SYSDESCR=".1.3.6.1.2.1.1.1.0";

my $OID_PROCS=".1.3.6.1.2.1.25.1.6.0";

my $OID_LOAD1=".1.3.6.1.4.1.2021.10.1.5.1";
my $OID_LOAD5=".1.3.6.1.4.1.2021.10.1.5.2";
my $OID_LOAD15=".1.3.6.1.4.1.2021.10.1.5.3";

my $OID_PROC_INDEX=".1.3.6.1.4.1.2021.2.1.2";
my $OID_PROC=".1.3.6.1.4.1.2021.2.1.5";

my $OID_DISKS_INDEX=".1.3.6.1.4.1.2021.9.1.2";
my $OID_DISKS=".1.3.6.1.4.1.2021.9.1.9";

my $OID_CUSTOM_1=".1.3.6.1.4.1.5000.101.1";
my $OID_CUSTOM_1_NAME="MailQ";

my $OID_LMSENSORS_INDEX=".1.3.6.1.4.1.2021.13.16";

my $OID_CUSTOM_2=".1.3.6.1.4.1.5000.102.1";
my $OID_CUSTOM_2_NAME="FrozenMailQ";

-x $SNMPGET || die("Could not find executable $SNMPGET, exiting");
-x $SNMPWALK || die("Could not find executable $SNMPWALK, exiting");

# my (@CONFIG);

printf ">> Getting SNMP SysDescr from host $HOST ($HOST_IP)...";

my ($s1,$h_sysdescr) = &snmpget("$HOST","$COMMUNITY","$OID_SYSDESCR");

die "snmpget error!" if ($s1 > 0);

printf "Success!\n<< ($h_sysdescr)\n";

print "\nWARN: Not a Linux machine, this might go a bit wrong!\n" unless ($h_sysdescr =~ /linux/i);

print "\n>> Getting stuff:\n";

my $h_numprocs = &get_thing("current number of processes","$HOST","$COMMUNITY","$OID_PROCS");
my $h_load1    = &get_thing("load1 value","$HOST","$COMMUNITY","$OID_LOAD1");
my $h_load5    = &get_thing("load5 value","$HOST","$COMMUNITY","$OID_LOAD5");
my $h_load15   = &get_thing("load15 value","$HOST","$COMMUNITY","$OID_LOAD15");

my $h_mailq    = &get_thing("Exim mailq length (might fail)","$HOST","$COMMUNITY","$OID_CUSTOM_1");
my $h_frozenq  = &get_thing("Exim frozen length (might fail)","$HOST","$COMMUNITY","$OID_CUSTOM_2");


print "\n>> Getting diskinfo...:\n";

my @diskindex = &snmpwalk("$HOST","$COMMUNITY","$OID_DISKS_INDEX");
my (@disk_monitors);

die "failed to get diskindex!" unless (@diskindex);


foreach (@diskindex) {

	my ($e_oid,$e_volume) = split(/=>/,$_);
	print "ERR: failed to get diskinfo! - maybe you need to add some disk lines into snmpd.conf ??" unless ($e_volume);
	next unless ($e_volume);
	$e_oid =~ s/$OID_DISKS_INDEX/$OID_DISKS/;
	my $e_currentuse = &snmpget("$HOST","$COMMUNITY","$e_oid");
	print "<<  +  $e_oid => $e_volume  (Currently $e_currentuse\% full)\n";
	
	if ($e_volume eq '/') { $e_volume = "%Used_root" };
	
	$e_volume =~ s/\//%Used_/;
		
	push @disk_monitors, "$e_oid=>$e_volume";
}

print "\n";

print "\n>> Getting process monitor info...:\n";
my @procindex = &snmpwalk("$HOST","$COMMUNITY","$OID_PROC_INDEX");
my (@proc_monitors);
die "failed to get procindex!" unless (@procindex);


foreach (@procindex) {
	my ($e_oid,$e_procname) = split(/=>/,$_);
	print "ERR: failed to get procinfo! - maybe you need to add some proc lines into snmpd.conf ??" unless ($e_procname);
	$e_oid =~ s/$OID_PROC_INDEX/$OID_PROC/;
	my $e_currentprocs = &snmpget("$HOST","$COMMUNITY","$e_oid");
	print "<<  +  $e_oid => $e_procname  (Currently $e_currentprocs running)\n";
	
	if ($e_currentprocs == 0) {
		print "<<  *  WARN: No $e_procname processes appear to be running! -- skipping\n";
		next;
	}
	
	$e_procname =~ s/$e_procname/proc_$e_procname/;
	
	push @proc_monitors,"$e_oid=>$e_procname";
	
}

&writeconfig;


####


sub writeconfig {
	
print "Writing SNIPS config snippet: $CONFDIR/$HOST.snmp ...";

my $isodate = `date +"%Y-%m-%d %H:%M:%S"`;
chomp $isodate;
my $line;

my (@conf);

$line = &format_conf("$HOST","$HOST_IP","$OID_PROCS","procs","$COMMUNITY","300","400","500","-");
push @conf,"$line";

$line = &format_conf("$HOST","$HOST_IP","$OID_LOAD1","Load1","$COMMUNITY","6","7","8","load100");
push @conf,"$line";

$line = &format_conf("$HOST","$HOST_IP","$OID_LOAD5","Load5","$COMMUNITY","4","5","6","load100");
push @conf,"$line";

$line = &format_conf("$HOST","$HOST_IP","$OID_LOAD15","Load15","$COMMUNITY","4","5","6","load100");
push @conf,"$line";

if ($h_mailq ne '') {
$line = &format_conf("$HOST","$HOST_IP","$OID_CUSTOM_1","$OID_CUSTOM_1_NAME","$COMMUNITY","100","150","200","-");
push @conf,"$line";
}

if ($h_frozenq ne '') {
$line = &format_conf("$HOST","$HOST_IP","$OID_CUSTOM_2","$OID_CUSTOM_2_NAME","$COMMUNITY","100","150","200","-");
push @conf,"$line";
}

foreach (@disk_monitors) {

	my ($e_oid,$e_volume) = split(/=>/,$_);

$line = &format_conf("$HOST","$HOST_IP","$e_oid","$e_volume","$COMMUNITY","77","85","90","-");
push @conf,"$line";
	
}

foreach (@proc_monitors) {

	my ($e_oid,$e_proc) = split(/=>/,$_);

$line = &format_conf("$HOST","$HOST_IP","$e_oid","$e_proc","$COMMUNITY","1","1","0","pcc");
push @conf,"$line";

}

open (CONFG, ">$CONFDIR/$HOST.snmp") || die "unable to open config file: $CONFDIR/$HOST.snmp";

print CONFG "\n#__BEGIN HOSTCONFIG: $HOST\n";
print CONFG "#  Updated: $isodate\n";
print CONFG "#  SysDescr: $h_sysdescr\n\n";

foreach (@conf) {
	
	print CONFG "$_\n";
	
}

print CONFG "\n#__END HOSTCONFIG: $HOST\n\n";

close (CONFG);

printf "...done\n";

	
}


sub format_conf {
	
my ($h_host,$h_ip,$h_oid,$h_var,$h_community,$h_thresh1,$h_thresh2,$h_thresh3,$h_vartype) = @_;

my $t1 = "\t";    my $t2 = "\t";    my $t3 = "\t\t";    my $t4 = "\t";
my $t5 = "\t";    my $t6 = "\t";    my $t7 = "\t";      my $t8 = "\t";

if (length($h_host) < 8)    {$t1 = "\t\t"; }
if (length($h_ip) < 8)      {$t2 = "\t\t"; }
if (length($h_oid) < 8)     {$t3 = "\t\t\t\t"; }

if ((length($h_oid) >= 8) && (length($h_oid) < 16 ))    {$t3 = "\t\t\t"; }
if ((length($h_oid) >= 16) && (length($h_oid) < 24 ))   {$t3 = "\t\t"; }
if (length($h_oid) > 24 )	                        {$t3 = "\t"; }

if (length($h_var) < 8)     {$t4 = "\t\t"; }
if (length($h_community) <8) {$t5 = "\t\t"; }

my $line= $h_host . $t1 . $h_ip . $t2 . $h_oid . $t3 . $h_var . $t4 . $h_community . $t5 . $h_thresh1 . $t6 . $h_thresh2 . $t7 . $h_thresh3 . $t8 . $h_vartype;

return ("$line")

}


sub get_thing {
	
	my ($descr,$host,$community,$oid) = @_;

	printf "<<  +  Getting $descr...: ";
	my $thing = &snmpget("$host","$community","$oid");
	
	printf "failed!\n" unless defined($thing);
	
	print "$thing\n";
	
	return ($thing);
	
}


sub snmpget {

	my ($host,$community,$oid) = @_;
	my ($line);
	my $value="unknown";
	my $status=0;
	
	open (SNMP, "$SNMPGET -On -v2c -c $community $host $oid  |") || die "Could not run \"$SNMPGET\"\n";
	
	while (<SNMP>)	{
		
		chomp;
		$line=$_;
			
		(undef,$value) = split(/ = /,$line,2);
	
	}
	
		$status=1 unless ($value =~ /(integer|string|gauge.*|opaque|counter|oid|timeticks|address.*):/i);
		$status=2 if ($line =~ /Timeout|No Response/i);
	        $status=3 if ($line =~ /no such object/i);
		$status=4 unless ($line);
	
			
	if ($status >0) {
	#	print "ERR: snmpget() failed (status: $status)\n";
		return ($status,undef);
	} else {
		# Remove the keyword i.e, STRING: or Gauge32:
		(undef,$value) = split(/: /,$value,2);
	}

	return ($status,"$value");
}


sub snmpwalk {

	my ($host,$community,$oid) = @_;
	my ($line,$target_oid);
	my $value="unknown";
	my $status=0;
	my (@things);

	open (SNMP, "$SNMPWALK -On -v2c -c $community $host $oid  |") || die "Could not run \"$SNMPWALK\"\n";

	while (<SNMP>)	{

		chomp;
		$line=$_;

		# print "LINE is: $line\n";

		($target_oid,$value) = split(/ = /,$line,2);

		$status=1 unless ($value =~ /(integer|string|gauge|opaque|counter|oid|timeticks|address):/i);
		$status=2 if ($line =~ /Timeout|No Response/i);
		$status=3 if ($line =~ /no such object/i);

		if ($status >0) {
			print "ERR: snmpwalk(): $line\n";
			} else {
				# Remove the keyword i.e, STRING: or Gauge32:
				(undef,$value) = split(/: /,$value,2);
				push @things, "$target_oid=>$value";
			}

		}

		$status=4 unless ($line);

		if ($status >0) {

			return undef;

			} else {

				# return an array of stuff:

				return (@things);
			}

}


sub hostlookup {

	my $host = @_;
	my $get_address = (gethostbyname ($HOST))[4];
	my @ip_numbers = unpack ("C4", $get_address);
	my $ip_address = join (".", @ip_numbers);

	return ($ip_address);

}

