#!/usr/local/bin/perl
#
# $Header: /home/cvsroot/snips/snipsweb/rrdgraph.cgi,v 1.3 2004/04/12 21:43:29 russell Exp $
#
# This CGI calls RRD::graph for the file specified as 'rrdfile'.
# To prevent abuse, searches for rrdfile under $RRD_DBDIR only.
# The GIF is written to stdout, so this should be invoked from an
# HTML program using '<IMG SRC=/cgi-bin/rrdgraph.cgi?parameters>'
#
# Used for plotting SNIPS data.
#
# If called with the 'mode=html' parameter, it generates an HTML
# page (instead of GIF) and calls itself recursively to display all
# the data for the RRD.
#
# If a image cache dir is defined and is writable, a copy of the
# image is saved in this dir and used by RRD::graph --lazy
#
# AUTHOR
#	Vikas Aggarwal, vikas@navya.com
#
##
#  PARAMETERS
#	rrdsubdir	- dir under $RRD_DBDIR containing data file
#	rrdfile  	- rrd file ($RRD_DBDIR/<hash char>/$subdir/$rrdfile)
#	timescale	- one of d, m, y or 'a' (for all)
#	mode		- html or gif or png
#	title		- Title for graph
#	legend		- Legend for graph
#
# Possible combinations are:
#	<subdir=X> <timescale=Z>    recursively call for each file in subdir
#	<subdir=X> <file=Y>    recursively call for each timescale
#	<subdir=X> <file=Y>  <timescale=Z>
#
# INSTALLATION:
#	1. Search for SET_THIS and verify if the variables are correct.
#	2. Create $IMAGE_CACHEDIR and chown to httpd process
#	3. Set @OK_REFERER list to include this host's URLs
#
##
use strict;
use vars qw ( $snipsroot $RRDLIBDIR $RRD_DBDIR
	      $IMAGE_CACHEDIR $RRDIMAGE_URL_PREFIX
	      $rrdgraph_cgi $rrd_realdir $rrdsubdir $rrdfile
	      $title $legend $timescale $mode $comment
	      @OK_REFERER %timestart %timelegend %cgienv
	      $debug
	    );

BEGIN {
  $snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS
  push (@INC, "$snipsroot/etc"); push (@INC, "$snipsroot/bin");
  require "snipsperl.conf" ;		# local customizations
  require "snipsweb-confg";		# Web configuration options
}

$RRDLIBDIR = "/usr/local/rrd/lib" unless $RRDLIBDIR;	# SET_THIS
push(@INC, "$RRDLIBDIR/perl");		# location of  RRDs.pm
use RRDs;

# where are the monitors generating RRD data
$RRD_DBDIR = "$snipsroot/rrddata" unless $RRD_DBDIR;	# SET_THIS

# If the cachedir is defined, then images will be stored in this dir,
# else they will be written directly to stdout.
# This directory must be writable by the httpd user, else you will have
# to generate these image files offline.
$IMAGE_CACHEDIR = "$snipsroot/rrd-images" unless $IMAGE_CACHEDIR;

$RRDIMAGE_URL_PREFIX = "/rrd-images/" unless $RRDIMAGE_URL_PREFIX;

# This file's URL as seen by the browser.
$rrdgraph_cgi =  "/cgi-bin/rrdgraph.cgi" unless $rrdgraph_cgi;

## For security, set OK_REFERER to the URL of snipsweb.cgi. This script
#  will then only run if its been called from snipsweb.cgi. If using
# .htaccess, comment out or set to empty list. See snipsweb-confg.
#
#@OK_REFERER = ( );

#### NO CHANGES BELOW THIS POINT ####

%timestart = ( 'y' => '1y',
	       'm' => '1month',
	       'w' => '7d',
	       'd' => '2d');
#
%timelegend = ( 'y' => '1 year/1 day average',
		'm' => '1 month/2 hr average',
		'w' => '1 week/30 min average',
		'd' => '2 days/10 min average');

## Check how we were called (invoked). Could also be self invoked.
sub check_security {
  if (@OK_REFERER) {
    my $referer = (split('\?', $ENV{'HTTP_REFERER'}))[0];
    $referer = "NoHost" if (! $referer);
    if (grep (/^${referer}$/i, @OK_REFERER) == 0)
    {
      print "Content-type: text/plain\n\n";
      print "ACCESS DENIED from $referer\n";
      print STDERR "ACCESS DENIED from $referer\n";
      exit 1;
    }
  }  # if (@OK_REFERER)
  return (1);		# permitted
}	# sub check_security()

## Get the CGI variables. Sets global %cgienv
sub get_cgi_variables {
  my $buffer = $ENV{'QUERY_STRING'};
  my @pairs = split(/&/,$buffer);	# Split the the name-value pairs on &
  foreach my $pair (@pairs) {
    my ($name, $value) = split(/=/, $pair);
    #$value =~ tr/+/ /;	# replace '+' with space
    # Now convert any HTML'ized characters into their real world
    # equivalent. e.g. a %20 becomes a space. 
    $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
    $cgienv{$name} = $value;
  }
}	# sub get_cgi_variables


## Graph all files in the sub-directory $rrd_realdir.
#  Force to 'd' timescale. Generates HTML.
#
sub graph_all_subdir {
  my @rrdfiles;

  if (opendir (REALDIR, $rrd_realdir)) {
    @rrdfiles = grep (/.+\.rrd/i, readdir(REALDIR) );
    closedir(REALDIR);
  }
  else {
    print STDERR "Cannot chdir($rrd_realdir) $!\n";
    return;
  }

  $mode = "HTML";		# force to html
  print "Content-type: text/html\n\n";    
  print <<Eof1;
  <HTML> <head> <title>SNIPS Daily Data for $title (all variables) </title></head>
  <body bgcolor="\#ffffff">
  <h2>SNIPS Daily Data for $title <br> <i>(all variables)</i></h2>
  <p>Click on a graph to get detailed historical data on a <b>specific variable</b></p>
Eof1

  my $toc = "<ul>";
  my $content;

  $title = uri_escape($title);		# replace space with %20 for URL
  $legend = uri_escape($legend);

  foreach my $rfile (sort @rrdfiles)
  {
    chomp $rfile;
    my $imgurl = do_graph($rfile, 'd');	# generates file on disk

    my $var = $rfile;
    $var =~ s/\.rrd$//;
    $var =~ tr/[a-zA-Z0-9_.\-\%\+]//cd;	# remove any unwanted characters
    my $legstr = "$legend" . "%20$var";	# tack on filename to legend
    $toc .= "<li><a href=\"#${var}\">${var}</a></li>\n";
    $content .= "  <href name=\"${var}\"><h4>${var}</h4></name>\n" .
          "  <A href=\"" .
          uri_escape( "${rrdgraph_cgi}?rrdsubdir=${rrdsubdir}&rrdfile=${rfile}&title=$title&legend=$legstr&timescale=A&mode=html" ) .
          "\">\n    <IMG SRC=\"$imgurl\">\n   </A>\n  <hr>\n";
  }	# for $rrdfile
  $toc .= "\n</ul><p>&nbsp;</p>\n";
  print $toc, $content, "  </body>\n </HTML>\n";
  exit 0;
}	# graph_all_subdir()

## Call itself recursively with all the timescales for a given $rrdfile
#  Generates html.
sub graph_all_timescales {
  my $var = $rrdfile;
  $var =~ s/\.rrd$//;
  $var =~ tr/[a-zA-Z0-9_.\-\+\%]//cd;	# delete unwanted characters

  $mode = "HTML";	# force to html
  print "Content-type: text/html\n\n";
  print <<Eof1;
  <HTML> <head> <title>SNIPS History for $title ($var)</title></head>
  <body bgcolor="\#ffffff">
    <h2>SNIPS History  for $title &nbsp; &nbsp; ($var)</h2>
Eof1

  $title  =~ s/\s/\%20/g;	# replace space with %20 for URL
  $legend =~ s/\s/\%20/g;

  my $imgurl_d = do_graph($rrdfile, 'd');
  my $imgurl_w = do_graph($rrdfile, 'w');
  my $imgurl_m = do_graph($rrdfile, 'm');
  my $imgurl_y = do_graph($rrdfile, 'y');

  print <<Eof;
    <h4>Daily Statistics</h4>
    <IMG SRC="$imgurl_d"> <hr>
    <h4>Weekly Statistics</h4>
    <IMG SRC="$imgurl_w"> <hr>
    <h4>Monthly Statistics</h4>
    <IMG SRC="$imgurl_m"> <hr>
    <h4>Yearly Statistics</h4>
    <IMG SRC="$imgurl_y">
   </body>
  </HTML>
Eof
  exit 0;
}	# graph_all_timescales

## From URI.pm
#
sub uri_escape {
  my ($uri) = @_;
  my %escapes;

  # Build a char->hex map
  for (0..255) {
    $escapes{chr($_)} = sprintf("%%%02X", $_);
  }
  $uri =~ s/([^;\/?:@&=+\$,A-Za-z0-9\-_.!~*'()])/$escapes{$1}/g; #'; # emacs
  return $uri;
}

sub check_rrdfile {
  $rrdfile =~ s/\.\.//g;	# dont allow any '..' characters in filename
  #
  if ($rrdfile eq "" || (! -f "$rrd_realdir/$rrdfile") ) {
    print "Content-type: text/plain\n\n";
    print "\n\tData file $rrdfile does not exist under $rrdsubdir\n";
    exit 1;
  }
}

####
#### Generate image
####
sub do_graph {
  my ($datafile, $tmscale) = @_;	# one of   d w m y
  my $imgmode = "PNG";
  my $imgfile = "-";		# default generate image to stdout
  my %LABEL = (
	       "Bandwidth",  "0-100 percentage",
	       "PktsPerSec", "Pkts Per Sec",
	       "named-status", "SOA Avail T/F",
	       "ntp", "Stratum",
	       "ICMP-RTT", "msec",
	       "ICMP-ping", "Pkts Rcvd",
	       "WWWport", "Port/page Available T/F",
	       "WWWspeed", "download time in msec",
	       "radius", "Port Available T/F",
	       "Thruput", "Kbps",
              );
  my ($match, $label, $labelfound);

  if ($mode eq "gif") { $imgmode= "GIF"; }

  if ($IMAGE_CACHEDIR ne "")
  {
    my $imgdir = "$IMAGE_CACHEDIR" . "/$rrdsubdir";
    $imgdir =~ s|//|/|g;
    $imgdir =~ s|/$||;
  
    $debug && print STDERR "Checking for image cache dir $imgdir...\n";
    if ( -d $imgdir || mkdir($imgdir, 0755) && -w $imgdir)
    {
      $imgfile = "$imgdir/$datafile";
      $imgfile =~ s/\....$/\-${tmscale}\.gif/;  # replace extension
      $debug && print STDERR "Setting imgfile = $imgfile\n";
    }
    else {
      print STDERR "Cannot create or write dir $imgdir- $!\n";
    }
  }

  if ($imgfile eq "-" && uc($mode) eq "HTML") {
    print "<h3>ERROR- Cannot generate image (please report to webmaster)</h3>\n";
    return;
  }

  foreach $match (keys %LABEL) {
    printf STDERR "examining match $match\n";
    if ($imgfile =~ /$match/) {
      $label = $LABEL{$match};
      $debug && printf STDERR "found label in %s -> %s\n",$match, $label;
      last;
    }
  }

$comment = "(generated " . scalar(localtime) .")";
  ##
  ##
  if ($imgfile eq "-") {
    print "Content-type: image/gif\n";
    print "Cache-Control: max-age=300\n\n";
  }
  # The variable is always stored as 'var1' and the minimum values
  # of the severity are stored as 'sev'.
  my ($averages,$xsize,$ysize) =
    RRDs::graph ($imgfile, "-t", "$title  ($timelegend{$tmscale})",
		 "--imgformat", "$imgmode", "--lazy", "--lower-limit", "0",
		 "-s", "-$timestart{$tmscale}", "-e", "now", 
		 "--vertical-label", "$label",
		 "DEF:var1=$rrd_realdir/${datafile}:var1:AVERAGE", 
		 "DEF:sev=$rrd_realdir/${datafile}:sev:AVERAGE",
		 "CDEF:intsev=sev,UN,UNKN,sev,1.5,GT,sev,2.5,LE,2,sev,3.5,LE,3,4,IF,IF,1,IF,IF",
		 "CDEF:nodata1=var1,UN,INF,UNKN,IF",	# replace unknowns
		 # extract only values at desired severity
		 "CDEF:info=intsev,4,EQ,var1,UNKN,IF",
		 "CDEF:warn=intsev,3,EQ,var1,UNKN,IF",
		 "CDEF:error=intsev,2,EQ,var1,UNKN,IF",
		 "CDEF:crit=intsev,1,EQ,var1,UNKN,IF",
		 # replace non-zero with UNKN
		 "CDEF:infozline=info,UN,UNKN,info,0,EQ,0,UNKN,IF,IF",
		 "CDEF:warnzline=warn,UN,UNKN,warn,0,EQ,0,UNKN,IF,IF",
		 "CDEF:errorzline=error,UN,UNKN,error,0,EQ,0,UNKN,IF,IF",
		 "CDEF:critzline=crit,UN,UNKN,crit,0,EQ,0,UNKN,IF,IF",
		 #
		 "COMMENT:\\s", "COMMENT:$legend\\c",
		 "COMMENT:\\s",
		 "COMMENT:min/avg/max=",
		 "GPRINT:var1:MIN:%1.0lf\\g", "COMMENT:/\\g",
		 "GPRINT:var1:AVERAGE:%1.0lf\\g", "COMMENT:/\\g",
		 "GPRINT:var1:MAX:%1.0lf\\g",
		 "COMMENT:\\c",		# center all the above text
		 "COMMENT:\\s",
		 #
		 "AREA:info#00aa00:OK",
		 "AREA:warn#cccc00:Warning",
		 "AREA:error#ff8c00:Error",
		 "AREA:crit#ff0000:Critical",
		 "AREA:nodata1#d8d8d8:Missing Data",	# gray out all unknown
#		 "LINE2:intsev#ff00ff:severity",
		 "LINE1:var1#0000aa:",		# just a border
		 "LINE3:infozline#00aa00:",	# thick line at zero value
		 "LINE3:warnzline#cccc00:",
		 "LINE3:errorzline#ff8c00:",
		 "LINE3:critzline#ff0000:",
		 #
		 "COMMENT:\\s", "COMMENT:\\s",	# newline
		 "COMMENT:$comment\\r",		# date generated
		 "-i"		# interlace
		);
  
  my ($err) = RRDs::error();
  if ($err) {
    print STDERR "RRD error for $rrd_realdir/$datafile - $err\n";
    exit 1;
  }

  if ($mode eq "HTML") {	# just return url to the generated file
    my $imgurl = $imgfile;
    $debug && print STDERR "URL for $imgfile is $imgurl\n";
    $imgurl =~ s|$IMAGE_CACHEDIR|$RRDIMAGE_URL_PREFIX|;
    $imgurl = uri_escape($imgurl);
    return $imgurl;
  }
  
  exit 0 if ($imgfile eq "-");

  ## here if we have generated the file on disk
  open (F, "< $imgfile") || exit 1;
  print "Content-type: image/gif\n";
  print "Gifsize: ${xsize}x${ysize}\n";
  print "Cache-Control: max-age=300\n\n";
  #print "Averages: ", (join ", ", @$averages);
  print while (<F>);
  close (F);

  exit (0);

}	# sub do_graph()

###
### main()
###

&check_security;
&get_cgi_variables;

$rrdsubdir = $cgienv{'rrdsubdir'};	# without the RRD_DBDIR prefix
$rrdfile = $cgienv{'rrdfile'};
$title =  "$cgienv{'title'}";
$legend =  "$cgienv{'legend'}";
$timescale = $cgienv{'timescale'};
$mode =	uc $cgienv{'mode'};	# html or gif or png

if ("$rrdsubdir" eq "") {
  print STDERR "NULL rrdsubdir not permitted\n";
  exit 1;
}

$rrd_realdir = "$RRD_DBDIR/" . substr($rrdsubdir, 0, 1) . "/$rrdsubdir";
$timescale = 'A' if ($timestart{$timescale} eq "");	# default

if ( (! -d "$rrd_realdir") || (! -r "$rrd_realdir") )
{
  print "Content-type: text/plain\n\n";
  print "\n\tDirectory $rrdsubdir does not exist or not readable\n";
  print STDERR "Directory $rrd_realdir does not exist or not readable\n";
  exit 1;
}

if ($rrdfile eq "") {
  $timescale = 'd';	# force to 'd'
  &graph_all_subdir;
  exit 0;
}

&check_rrdfile;

if ($timescale eq 'A' || $timescale eq 'a') {
  &graph_all_timescales;
  exit 0;
}

if ($mode eq "HTML") {
  # call self with mode = gif
  print "Content-type: text/html\n\n";
  print <<Eof;
  <HTML> <head> <title>$title ($rrdfile)</title></head>
  <body bgcolor="\#ffffff">
    <h2>$title</h2>
    <IMG SRC="${rrdgraph_cgi}?rrdsubdir=${rrdsubdir}&rrdfile=${rrdfile}&title=$title&legend=$legend&timescale=${timescale}&mode=gif">
  </body> </html>
Eof
  exit 0;
}

## here if the mode is 'gif' or 'png' and has to be plotted.
&do_graph($rrdfile, $timescale);
