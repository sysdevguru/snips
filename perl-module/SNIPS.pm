package SNIPS;

# $Header: /home/cvsroot/snips/perl-module/SNIPS.pm,v 1.1 2001/07/13 05:46:30 vikas Exp $

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;
#require AutoLoader;	# requires one package statement per file

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	     &snips_main  &snips_startup  &snips_reload
	     &read_event &write_event &new_event
	     &pack_event &unpack_event &print_event &event2array
	     &update_event &alter_event &rewrite_event
	     &str2severity &open_datafile &close_datafile
	     &calc_status $libdebug
	     $s_configfile $s_datafile $s_sender $s_pollinterval
	     $E_CRITICAL $E_ERROR $E_WARNING $E_INFO
	     $n_UP $n_DOWN $n_UNKNOWN $n_TEST $n_NODISPLAY $n_OLDDATA
	    );

@EXPORT_OK = qw(
		$autoreload $doreload $dorrd 
		$prognm
	       );

use vars qw (
	     $autoreload $libdebug $doreload $dorrd 
	     $prognm $pollinterval
	     $s_configfile $s_datafile $s_sender $s_pollinterval $extension
	     $E_CRITICAL $E_ERROR $E_WARNING $E_INFO
	     $n_UP $n_DOWN $n_UNKNOWN $n_TEST $n_NODISPLAY $n_OLDDATA
	     $HOSTMON_SERVICE $HOSTMON_PORT
	    );

$VERSION = '1.00';

bootstrap SNIPS $VERSION;

# Preloaded methods go here.
tie $libdebug, 'SNIPS::globals', 'debug';
tie $dorrd, 'SNIPS::globals', 'dorrd';
tie $doreload, 'SNIPS::globals', 'doreload';
tie $autoreload, 'SNIPS::globals', 'autoreload';
tie $s_configfile, 'SNIPS::globals', 'configfile';
tie $s_datafile, 'SNIPS::globals', 'datafile';

###
### Set variables, etc.
###
$s_pollinterval = 300 unless $s_pollinterval ;	# default value in seconds

$E_CRITICAL = 1;
$E_ERROR    = 2;
$E_WARNING  = 3;
$E_INFO     = 4;

$n_UP          = 0x01;
$n_DOWN        = 0x02;
$n_UNKNOWN     = 0x04;
$n_TEST        = 0x08;
$n_NODISPLAY   = 0x10;
$n_OLDDATA     = 0x20;

##  ### ### ### ##

## following are aliases for functions (safer to @EXPORT)
sub snips_main {  return ( SNIPS::main(@_) ); }

sub snips_startup {  return SNIPS::startup(@_); }

sub snips_reload { return SNIPS::reload(@_); }

sub open_datafile { return SNIPS::fopen_datafile(@_); }


##
# Constructor method
sub new {
  my $class = shift;
  my $self = {};		# allocate new memory
  bless($self, $class);
  return $self;
}

## snips_main()
# This is a sample generic main(). You dont have to call this function.
# Requires references to the readconfig() poll() or test() functions,
# e.g.
#
#	SNIPS::main(\&read_conf, \&do_poll, \&do_test)
#

sub main {
  my ($readconf_func, $poll_func, $test_func) = @_;

  $prognm = $0;
  if ( ! defined($readconf_func) ) {
    print STDERR ("FATAL: need valid readconfig() function when calling ",
		  (caller(0))[3], "()\n" );
    exit 1;
  }

  # $test_func = \&do_test  if ( ! defined($test_func) );

  if ( ! defined($poll_func) && !defined($test_func) ) {
    print STDERR ("FATAL: need either poll_function or test_function ",
		  "set when calling ", (caller(0))[3], "()\n" );
    exit 1;
  }

  parse_opts();

  startup();

  &$readconf_func();

  while (1)
  {
    my $starttm = time;

    if (defined ($poll_func)) {
      done() if ( &$poll_func() < 0 ) ;
    }
    else {
      done () if ( SNIPS::poll_devices($test_func) < 0 );
    }

    check_configfile_age()  if ($autoreload);

    if ( $doreload ) {
      reload($readconf_func);
      next;	# dont sleep
    }

    my $polltime = time - $starttm;
    print STDERR "$prognm: polltime = $polltime secs\n"  if ($libdebug);

    if ($polltime < $s_pollinterval) {
      my $sleeptime = $s_pollinterval - $polltime;
      print STDERR "$prognm: Sleeping for $sleeptime secs\n"  if $libdebug;
      $SIG{ALRM} = 'DEFAULT';	# restore in case blocked
      sleep($sleeptime);
    }
    
  } # while(1)
}	# sub main()

## Set global variables after parsing command line options.
#
sub parse_opts {
  use Getopt::Std;
  use vars qw($opt_a $opt_d $opt_f $opt_o $opt_x $opt_v);

  getopts("adf:o:vx:");	# sets $opt_x
  if ($opt_a) { ++$autoreload ; } # will reload if configfile modified
  if ($opt_d) { ++$libdebug ; }
  if ($opt_f) { $s_configfile = $opt_f; }
  if ($opt_o) { $s_datafile = $opt_o ; }
  if ($opt_x) { $extension = $opt_x ; }
  if ($opt_v) { $libdebug = 2 ; }	# verbose mode
}

## Sets up signal handlers, kills other running process, sets 
#  default  config and data filenames.
sub startup {

  $prognm = $0;
  if (! $s_sender) {
    $s_sender = $prognm;
    $s_sender =~ s|^.*/||;	# without the directory prefix
  }
  $extension = undef unless $extension;

  my $rc = SNIPS::_startup($prognm, $extension);

  open_eventlog();

#  set_configfile($s_configfile) if ($s_configfile);
#  set_datafile($s_datafile)     if ($s_datafile);

  return $rc;
}	# startup()

## Perl version of the C function. Close all open filehandles BEFORE
#  calling this routine. Needs *reference* to read_conf function.
sub reload {
  my ($readconfig_func) = @_;

  my $ndatafile = $s_datafile . ".hup";	# new datafile name

  print STDERR "Reloading...";
  $doreload = 0;		# reset at start

  if (! defined($readconfig_func)) {
    print STDERR "Cannot reload, no readconfig function set in program\n";
    return -1;
  }
  my $odatafile = $s_datafile;	# save current name
  $s_datafile = $ndatafile;	# temporarily set to new filename
#  set_datafile($s_datafile);

  &$readconfig_func();		# writes to new datafile

  $s_datafile = $odatafile;	# restore
#  set_datafile($s_datafile);
  
  copy_events_datafile($odatafile, $ndatafile);

  unlink $odatafile;
  if (rename ($ndatafile, $odatafile) != 1) {
    print STDERR "failed. FATAL rename() $!\n";
    SNIPS::done();
    exit -1;
  }
  else {
    print STDERR "done\n";
  }

  $doreload = 0;		# reset
}	# reload()

## De-reference list pointer
sub get_eventfields {
  my $fields = SNIPS::_get_eventfields();
  return @$fields;
}

## De-reference list pointer
sub event2array {
  my ($event) = @_;
  my $arr = SNIPS::_event2array($event);
  return @$arr;
}

## Send XS routine a hash reference.
sub pack_event {
  my (%hevent) = @_;

  if (%hevent) {
    return SNIPS::_pack_event(\%hevent) ;
  }
  else { return undef; }
}

## de-reference hash pointer
sub unpack_event {
  my ($event) = @_;

  my $hevent = SNIPS::_unpack_event($event);
  return (%$hevent);
}

## print event structure, useful for debugging
sub print_event {
  my ($event) = @_;

  my @fields = SNIPS::get_eventfields();
  my %event  = SNIPS::unpack_event($event);
  foreach my $f (@fields) {
    print "$f = $event{$f}\n";
  }
}

## generic poll function. Need a reference to the test function in the
#  arg.
#
sub poll_devices {
  my ($test_func) = @_;
  my $deviceno = 0;
  my $event = undef;
  my ($status, $value, $thres, $maxsev);

  print STDERR "inside generic poll_devices\n" if ($libdebug > 2);

  my $fd = open_datafile($s_datafile, "r+");	# open for read + write

  while ( ($event = read_event($fd) ) ) 
  {
    ++$deviceno;		# start with 1, not 0

    # call test function
    # only $value is really needed, rest can be undef.
    # If $thres is undef, update_event() will keep old threshold
    ($status, $value, $thres, $maxsev) = &$test_func(\$event, $deviceno);

    $maxsev = $E_CRITICAL  if (! defined($maxsev));
    if ( ! defined ($status) )
    {
      my %event = unpack_event($event);
      ($status, $thres, $maxsev) = 
	calc_status($value, $event{var_threshold}, $event{var_threshold},
		    $event{var_threshold});
    }
    
    update_event($event, $status, $value, $thres, $maxsev);
    rewrite_event($fd, $event);
    last if ($doreload > 0);

  }	# while (readevent)

  print STDERR "poll_devices(): Processed $deviceno devices\n" if ($libdebug > 1);
  close_datafile($fd);
  return 1;
}	# sub poll_devices()

#### To access the C variables  #####
#
# Cannot use Autoloader if putting another package in the same .pm file.
# The 'tie' function calls the functions. The FETCH and STORE functions
# are in the .xs file. Somehow, the TIESCALAR function is not working in
# the .xs file.
package SNIPS::globals;

sub TIESCALAR {
  my $class = shift;
  my $var = shift;	# debug or dorrd or...
  # print "TIESCALAR, blessing $var in class $class\n";
  return bless \$var, $class;
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

# Documentation for SNIPS.pm

=head1 NAME

snips - Perl extension for SNIPS (System & Network Integrated Polling Software)

=head1 SYNOPSIS

  use SNIPS;

=head1 DESCRIPTION

SNIPS.pm is a perl API for SNIPS
(http://www.netplex-tech.com/software/snips) to write system and network
monitors in Perl.

The library reads, writes and generally works with an packed 'EVENT'
C structure (described in snips.h). Most of the following routines
work on this packed structure directly for performance, but the 
individual fields of the packed structure can be accessed 
using 'unpack_event()' and 'pack_event()'.

=head2 API FUNCTIONS

In order to invoke the API functions listed below, you should specify the
complete package name, i.e.

    SNIPS::function()

Alternatively, some functions are exported into the calling file's namespace
as a convenience. These are aliases for the main(), reload() etc. functions
below, but can be called without specifying the complete package name:

	     snips_main()
	     snips_startup()
	     snips_reload()
	     open_datafile()

=over 4

=item main(\E<38>readconfig, \E<38>dopoll, \E<38>dotest)

This is a generic main, and it will call the user defined functions dopoll()
or if undefined, it will invoke a generic dopoll().

Note the '\' before the '&' to pass function references to main().

The readconfig() function should ideally write out an event for each device
to be monitored to the output file.
If dopoll() is defined, then this function must do one complete pass of
monitoring all the devices. main() will call dopoll() in an endless loop and
check for any HUP signal between polls to see if the readconfig() function
should be called to reload the new config file.

If dopoll() is NOT defined, but dotest() is defined instead, then a generic
dopoll() function invokes the dotest() function
with the EVENT (read from the datafile) and the event-number 
(starting with 1, not 0) as input parameters.
The function should return a list with the following fields:

   ($status, $value, $thres, $max_severity) = &dotest(EVENT *ev, count)

The complete sequence that is performed by snips_main() is listed below.
Instead of using snips_main(), a monitor can have its own (similar) 
calling routine. (The following is in pseudocode and no parameters
are listed for the functions):

    parse_opts()
    startup()
    sub readconfig()
    {
	    open_datafile()
	    new_event()
	    foreach device in configfile
	    {
	    	alter_event()  # change devicename, addr, varname
	    	write_event()  # to data file
	    }
	    close_datafile()
    }
    while (1)
    {
      sub dopoll()
      {
	    open_datafile()
	    while (read_event())
	    {
		    sub dotest()
		    {
			    %event = unpack_event()  # if needed
			    ### test function here ###
			    calc_status()	# if needed
		    }
		    # set %event fields directly and call pack_event()
		    # OR call update_event()
		    update_event()
		    rewrite_event()
	    } # end while
	    close_datafile()
	    if ($doreload)
	       reload()
	    sleep()
      } # end dopoll

    }  # end while

=item parse_opts()

This reads the 'standard' command line options:

	-a	autoreload (if config file has changed)
	-d	debug
	-x ext	Extension to add onto program name (see below)
	-f configfile
	-o datafile

The 'startup()' function (below) creates a default config/data filename
based on the program name. If an extension is specified using C<-x>, then
this is added to the program name before creating the default config/data
filenames. This allows running multiple copies of the program with each
reading different config files. The other alternative is to create a
symbolic link to the program with a different name and invoking that.

=item startup(I<char *progname [,char *extension]>)

This function extracts the program name from the command line and sets up
the various signal handlers. It sets the default config and data file names
(based on the program name). It kills any previously running process and
writes the PID in the pidfile. Every monitor MUST call this routine.

The program name (typically $0 in perl) is provided as an argument. The
optional 'extension' can be used to append a string to the program name
(useful for running multiple copies of the program).

=item standalone(char *progname [,char *extension])

This utility function has the same arguments as I<startup()> but it only
ensures that there is no other program running with the same name. If it
finds a pid in the pidfile, it will attempt to kill that function, then
writes the pid of the current process in the pidfile.

=item reload(\E<38>readconfig)

On getting a HUP signal, the 'doreload' flag is set to a non-zero value. On
detecting this flag, you should call reload() with the reference to the
readconfig() function as a parameter.

This function will then call readconfig() (which should write out the initial
datafile by re-reading the config file), and then copy over current monitored 
state information from the current datafile to the new datafile. It resets
the reload flag before returning.

=item EVENT * new_event()

This returns a packed binary event structure with its fields initialized
(i.e. it calls init_event() after mallocing memory for the event structure).

=item init_event(EVENT * event)

This zeroes out the binary event structure and fills it in with the
current date and time.

=item alter_event(EVENT *ev, sender, devicename, deviceaddr, subdevice, varname, varunits)

This is a utility routine which allows you to set the fields in the EVENT
structure without having to unpack() and then pack(). ALL the fields are
character strings, and any of the fields can be 'undef' to leave it
unchanged. To delete a field, set it to the empty string "".

=item update_event(EVENT *ev, status, value, thres, severity)

This updates the event 'ev' with the specified values. 'status' is 0 or 1,
'value' is the measured/monitored value, 'thres' is the new threshold that
has been exceeded, 'severity'  is the new severity of the event. 'thres' can
be 'undef' if it is unchanged (i.e. not working with 3 thresholds).

=item calc_status(int value, int warn_thres, error_thres, crit_thres)

If the monitor has 3 separate thresholds for the variable being monitored,
calc_status() is a utility routine to calculate which threshold the value
exceeds. It returns:

	 ($status, $threshold, $maxseverity) = calc_status(...)

This is typically called to get the parameters needed for update_event()


=item int fd = open_datafile(char *filename, char *mode)

This opens a new output datafile with the mode specified using 
"w" "r" "r+" "w+" "a+" based on reading, writing, read and write, etc.
(see the fopen() C library call). It returns an integer file descriptor that
should be used for the various event file functions listed below.

=item close_datafile(int fd)

close the data file opened using open_datafile().

=item EVENT * read_event(int fd)

Read one packed event  from the datafile.

=item write_event(int fd, EVENT *v)

Write one packed event to the datafile. Note that if you want to read and
then update a particular event, then you should call rewrite_event() instead,
because the read_event() will have advanced the file pointer for the 
write_event() so you will end up overwriting some other event.

=item rewrite_event(int fd, EVENT *v)

Rewinds the file one event, and then writes out the event passed to it.

Write one packed event to the datafile.

=item EVENT * pack_event(HASH %event)

This converts a event hash ( returned by unpack_event() ) into a binary
event structure needed by write_event().

=item HASH * unpack_event(EVENT *v)

This converts a packed event structure into a hash, with the elements of
the hash accessed using the field names such as 'sender', 'device_name',
'device_addr', etc. This gives access to the various fields of the event
structure.

	%event = unpack_event($event);
	$event{sender} = "pingmon";
	$event{device_name} = "gw.navya.com";
	$event = pack_event(%event);

The list of field names can be extracted using I<get_eventfields()>. These
names are:
	sender
	device_name device_addr device_subdev
	var_name var_value var_units
	severity loglevel
	eventtime  polltime
        state rating op id

=item str2severity(char *severity)

Given a severity string such as Critical, Error, Warning, Info, returns an
integer indicating the severity.

=item get_eventfields()

This utility function returns a list of the field names of the 
EVENT structure
(which are used in the hash returned by 'unpack_event()').

=item ARRAY event2array(EVENT *v)

This converts an event into an array (the field names and their order can be
obtained from get_eventfields(). This is an alternative to using unpack_event()
if you want to extract the various fields in an event structure as an array.

Typical usage would be:

	@fields = get_eventfields();
	@event = event2array($event);
	my $i = 0;
	foreach my $f (@fields) { $$f = $i; ++$i; }
	print "$event[$sender] $event[$var_name\n";

=back

=head2 VARIABLES

=over 4

=item $s_configfile  $s_datafile

These are the configfile and datafile names, and the names are 
automatically created by the startup() function (unless provided on
the command line). You must call the functions set_configfile() and
set_datafile() to change the names in the C library.

=item $s_pollinterval

C<$pollinterval> should be set only if you are calling the snips_main()
function to set the sleep interval between polls.

=item $E_INFO $E_WARN $E_ERROR $E_CRITICAL

These are constants for the various severity levels and correspond to integer
values 4 thru 1.

=item $libdebug $dorrd $autoreload $doreload

These are NOT exported, so in order to access these variables you
have to specify the package name (e.g. C<$SNIPS::libdebug>).
C<$libdebug> will enable debugging (higher than one is more verbose),
C<$dorrd> will enable generation of RRDtool graph data,
C<$autoreload> is set when the user specifies C<-a> on the command line
and the function 'check_configfile_age()' should be called if this is set
(which actually sets the C<$doreload> flag).
C<$doreload> is set when the function recieves a HUP signal and the
main function should call 'reload()' when this is set.

=back

=head1 AUTHOR

Vikas Aggarwal (vikas@navya.com)

=head1 SEE ALSO

snips(8) perl(1).

=cut
