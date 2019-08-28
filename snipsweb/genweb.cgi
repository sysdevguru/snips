#!/usr/local/bin/perl
my $versionid = '$Id: genweb.cgi,v 1.4 2003/10/20 13:13:49 russell Exp $ ';#

# ------------
# This program displays SNIPS events in HTML format (for an alternative
# to the snipstv curses program).
#
# It can run as a standalone perl program or as a CGI. It detects the
# mode automatically from the environment.
#
# When run as a perl file, it generates 5 HTML files in $webdir/ for each
# possible 'view':
#	User.html   Critical.html  Error.html  Warning.html Info.html
#
#   User   a critical-only non-updatable page for public use.
#          This can be linked to index.html or copied periodically to
#	   some web dir so that it is displayed by default for public users.
#   Critical     Critical view - contains href links to snipsweb.cgi
#   Error        Error view    _   "       "      "    "
#   Warning      Warning view  _   "       "      "    "
#   Info         Information view _"       "      "    "
#
# Note that even if a public user were to view any of these files, they
# still need to authenticate in snipsweb.cgi before they can edit any
# update file, etc.
#
# When run as a CGI, it generates html output directly on stdout and also
# permits  filtering of the events displayed (calls genweb-filter.cgi to
# set the parameters). Note however, that in CGI mode every user accessing
# this CGI will parse the SNIPS datafiles, so if there are many events and
# many users, this will affect the performance of your system.
#
# CGI parameters that can be passed to this program:
#  view:      see names above
#  refresh:   refresh interval in seconds, your browers should 
#             be able to handle this in the HTTP header, most modern browers
#             (IE, Netscape, Mozilla) do.
#  sound:     define 0 or '' if sound is not desired.
#  sort:      comma separted list of fields. Major sort the first field,
#             within that the 2nd field, etc.
#             sort fields are name, varname, deviceaddr, severe, monitor
#             and varvalue
#  maxrows:   integer specifies the maximum number of entries to put 
#             output in tabular format. Beyond this number the items are 
#             displayed as verbatim output: HTML <pre> tag.
#  namepat:   narrow display to just those device names matching a regexp.
#  varpat:    narrow display to just those variable names matching a regexp.
#  monpat:    narrow display to just those monitors matching a regexp.
#  altprint:  A different table printing format mentioned on the mail archives
#
# Here is an example: 
# http://snips.snips.net/cgi-bin/genweb.cgi?view=Info&sort=name,varname&refresh=120&sound=0&maxrows=70&monpat=^n
#
# which means: 
#
#  I want the Info display sorted by name and within that by variable
#  name.  The refresh rate is 120 seconds (2 minutes), don't play sound
#  on an alert and I only want to see monitors starting with the letter
#  'N' (ntpmon and nsmon)
#
# If I wanted all information about jefe I'd put:
#
# http://snips.snips.net/cgi-bin/genweb.cgi?view=Info&namepat=jefe
#
# This script also reads in an 'updates' file and hide's the event or adds
# a update message to an event. Once the device comes back up, this script
# will remove the status line from the 'updates' file (to prevent any
# confusion when the device goes down next).

# Any global 'messages' that are in the snips messages directory are
# also displayed.
#
# The snipsweb.cgi companion CGI allows displaying device help files, 
# traceroutes, display logs, and also updating the 'updates' file.
#
# INSTALLATION
# ------------
#
# 1. Customize variables in snipsweb-confg
# 2. Create a '$snipsroot/etc/updates' file with the owner being the user who
#    will run this script since this script will delete entries once a device
#    comes back up. If running as a CGI, the owner should be the httpd process.
# 3. Install the snipsweb.cgi CGI script in your CGI directory. If installing
#    this file as a CGI, install both 'genweb.cgi' and 'genweb-filter.cgi'
#    in the CGI dir.
#    Install the entire 'gifs' dir under the $baseurl directory.
# 4. If you want to create a separate 'User' view for outsiders, then you
#    can copy the 'Users.html' file to a public web device. Else create
#    a link from Critical.html to 'index.html' so that this is the
#    default page. You can either password protect this URL tree using
#    httpd's access mechanism (.htaccess) or else rely on the $etcdir/$authfile
#    file (see snipsweb.cgi). Either way, you have to create this authfile
#    to list the users who can edit the various snips files, etc.
# 5. Create device help files, etc. under $snipsroot/device-help/
#    Note that these help files are HTML files and can contain HTML tags
#    for formatting and clickable links (see details in the CGI script).
#
# AUTHOR
#	- Original version: Richard Beebe (richard.beebe@yale.edu) 1998
#	- Updates: Vikas Aggarwal (vikas@navya.com) 1998
#       - Modernized and turned into CGI with sorting and
#         selection: Rocky Bernstein (rocky@panix.com) May 2000
#	- Currently maintained by Vikas Aggarwal (vikas@navya.com)
#
#   This script was distributed as part of the SNIPS package.
#
#########################################################################

my $SNIPSVERSION = "1.2";			# version
# Global variables.
use strict;
use vars qw (
	     $baseurl $webdir $snipsweb_cgi $sound $snipsroot $imageurl $etcdir
	     $bindir $datadir $msgsdir $updatesfile @views $view
             %view2severity $newevents $neweventAge
	     @level_color @level_imgs $emptyimg $tfontsize $max_table_rows
	     %updates $thiscgi $refresh $large_refresh $my_url $prognm @z1
	     $debug @row_data $cgimode $filter_cgi $gen_cgi_links
	     $myStyleSheet $numcols $userViewUpdates %escapes
	     $prefmt $prefmt_filesz $totaldatasize
	    );
BEGIN {
  $snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS
  push (@INC, "$snipsroot/etc"); push (@INC, "$snipsroot/bin");
  require  "snipsperl.conf" ;		# local customizations
  require  "snipsweb-confg";	# all WEB configurable options
}

init();
read_status_file($updatesfile);
my @dfiles = get_datafile_names($datadir);
process_parameters();

my $entries = 1;

if (! $cgimode) { gen_html_files(); }	# never returns
else { gen_html_cgimode(); }


#------------------------------------------------------------
sub init {
  # use CGI qw/:standard -debug/;
  use CGI;
  use SNIPS;

  $etcdir  = "$snipsroot/etc"  unless $etcdir;	# location of config file
  $bindir  = "$snipsroot/bin"  unless $bindir;
  $datadir = "$snipsroot/data" unless $datadir;
  $msgsdir = "$snipsroot/msgs" unless $msgsdir;
  
  # push(@INC, $bindir); push(@INC, $etcdir); # add to search paths for 'require'
  
  $baseurl =~ s|/\s*$||;	  # strip trailing '/' in baseurl
  
  # url for accessing the various snips images. Don't make it 'images' since
  # it usually gets mapped by the httpd daemon.
  $imageurl = "${baseurl}/gifs";  # check.
  $imageurl =~ s|/\s*$||;	  # strip trailing '/' in imageurl
  
  # 'updatesfile' is the full path to the file containing update messages
  # Since this file is updated via the web (snipsweb.cgi), this file should
  # be owned by 'www' or 'httpd' to allow editing. this must match the
  # file location in the snipsweb.cgi script also.
  $updatesfile = "$etcdir/updates" unless $updatesfile;	# check this

  # This is the URL for invoking the filter script
  #$filter_cgi = "/cgi-bin/genweb-filter.cgi" unless $filter_cgi;
  
  @views = qw( User Critical Error Warning Info );
  %view2severity = ( User => 1, Critical => 1, Error => 2, Warning => 3,
		     Info => 4 );
  @level_color = ("", "Red", "Blue", "Brown", "Black");
  @level_imgs  = ("", "${imageurl}/redsq.gif", "${imageurl}/bluesq.gif",
		     "${imageurl}/yellowsq.gif", "${imageurl}/greensq.gif");
  
  ## These gif images are provided with snips. Put them in your httpd directory
  #  tree under $baseurl/images/
  $emptyimg = "${imageurl}/empty.gif";
  $newevents = 0;

  if (! defined($cgimode)) {
    if (defined($ENV{'SERVER_NAME'})) { $cgimode = 1;}
    else { $cgimode = 0; $gen_cgi_links = 0; }
  }

  @z1 = ('00' .. '59');	# To convert single digit minutes -> 08

  if ($cgimode) { $thiscgi = new CGI(); }
  else { $thiscgi = new CGI({}); }	# avoid asking for cmd line parameters.

  $my_url = $thiscgi->self_url;
  $my_url =~ s/;/&/g;
  # $my_url =~ s|http://localhost/||;	# cannot have localhost in URL

  $debug = 0 unless $debug;
  $prefmt = 0;		# print HTML, not <pre>
  $prefmt_filesz = 250000;	# switch to prefmt mode after 250k

  # Build a char->hex map for escape_uri()
  for (0..255) { $escapes{chr($_)} = sprintf("%%%02X", $_); }

  ## Instead of setting the font everywhere and increasing the size of
  #  the HTML, just define a stylesheet.
  $myStyleSheet=<<END;
   body {
     background:  white;
   }
  TABLE.data {
    font-family: Arial, Helvetica, sans-serif;
    TH.data {
      font-size: 8pt;
      font-family: Arial, Helvetica, sans-serif;
    }
    TD.data {
      font-size: 8pt;
      font-family: Arial, Helvetica, sans-serif;
    }
  }
  PRE {
    font-size: 10pt;
    background-color: #C0C0C0
    font-family: Lucida-console, Courier, Courier-new, monospace;
  }

END

}	# init()

# Read in the status file. Entries in this file will get a status
# message if not in User view. Global %updates set.
sub read_status_file {
  my ($updatesfile)=@_;
  if (!open (INPUT, "<$updatesfile")) {
    ooops("Can't open $updatesfile for reading: $!");
  }
  while (<INPUT>) {
    chomp;
    next if (/^\s*\#/ || /^\s*$/);   # skip comments & blank lines
    my ($addr, $update) = split(/\t/);
    $updates{$addr} = $update;
  }
  close (INPUT);
}	# read_status_file

#------------------------------------------------------------
## Get all log files from SNIPS data directory (extract only those
# files that have '-output' as prefix). Also calculate the total data
# size.
sub get_datafile_names {
  opendir(DATADIR, $datadir) 
    || ooops("\nCannot open input directory $datadir: $!");
  
  my @dfiles = sort grep(/.+-output/i, readdir(DATADIR));
  closedir(DATADIR);
  foreach my $f (@dfiles) {
    my @s = stat("$datadir/$f");
    $totaldatasize += $s[7];
  }
  print STDERR "Total datafile size = $totaldatasize\n" if $debug;
  return @dfiles;
}

#------------------------------------------------------------
##
sub process_parameters {
  $view=$thiscgi->param('view');
  if ( (!defined($view)) || ($view =~ /^\s*$/) ) {$view = 'User'};

  # Did we specify a sound to play? If so, what sound? 
  my $s=$thiscgi->param('sound');
  $sound=$s if defined($s) && $s ne '' && $s ne 'no' ;

  my $d=$thiscgi->param('debug');
  $debug=1 if defined($d) && $d ;
  
  # Did we specify a refresh rate? If so what values?
  my $r=$thiscgi->param('refresh');
  if (defined($r) && $r =~ /\d+/) {
    $refresh = $r;
  }
  $refresh = 30 if ($refresh < 30);	# minimum
  # Did we specify compact Info format? 
  my $m=$thiscgi->param('maxrows');
  $max_table_rows=$m if defined($m);
  
}	# process_parameters()

#------------------------------------------------------------
## Print HTTP header (in CGI mode)
sub print_http_header {
  my ($my_refresh) = @_;
  $my_refresh = $large_refresh if ($#row_data > $max_table_rows);

  print $thiscgi->header(-Refresh=>"$my_refresh; URL=$my_url",
			 -expires=>'+30s');
}

#------------------------------------------------------------
# Print html start and form buttons to select the severity.
# $view must be set before calling this.
sub print_html_prologue {
  my ($my_refresh) = @_;

  my $action = $views[$view2severity{$view}];

  # Date stuff -- replace with Perl module Parse::Date?
  my ($sec, $min, $hour, $mday, $mon, $year, $weekday, $yrday, $daylite) 
    = localtime(time);
  $mon++;
  $year += 1900;	# fix y2k problem

  my $today = "$z1[$mon]/$z1[$mday]/$year  $hour:$z1[$min]";  # MMDDYY, US-centric
  #my $today = "$year/$z1[$mon]/$z1[$mday]  $hour:$z1[$min]";  # YYMMDD
  my $cursecs = time;	# get Unix timestamp
  my $refresh_url = $cgimode ? "$my_url" : "${baseurl}/${view}.html" ;

  print $thiscgi->start_html(-title=>"SNIPS - $action view", 
		       -head=>[
			       "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"$my_refresh;URL=$refresh_url\">",
##			       "<META HTTP-EQUIV=\"PRAGMA\" CONTENT=\"NO-CACHE\">",
			       "<META NAME=\"keywords\" CONTENT=\"snips,nocol,nms\">"
			      ],
		       -style=>{-code=>$myStyleSheet},
                       -bgcolor=>"#FFFFFF",
                       -link=>"#003366",
                       -vlink=>"#003366",
                       -alink=>"#003366"
                      ), "\n";
  print $thiscgi->comment("Generated by $versionid"), "\n";
  # print $thiscgi->comment("FUNCTION ------- print_html_prologue"), "\n";

  print <<EOT;
    <!-- title banner -->
    <TABLE class="header" cellpadding=2 cellspacing=0 border=0>
     <tr><td height="6"> &#160 </td></tr>	<!-- vertical space -->
     <tr><td bgcolor="#003366">
	  <font face="arial,helvetica" size=5 color="#FFFF00">
          <b>&nbsp;S&nbsp;N&nbsp;I&nbsp;P&nbsp;S&nbsp;</b></font>
	  <font face="arial,helvetica" size=3 color="#FFFFFF">
          <b>&nbsp;(System &amp; Network Integrated Polling Software)&nbsp;</b>
	  </font></td>
     </tr>
     <tr><td height="6"> &#160 </td></tr>	<!-- vertical space -->
    </TABLE>

    <P>
    <!-- last update date -->
    <TABLE width="100%" cellpadding=0 cellspacing=0 border=0>
     <tr><td width="50%" align=left> &nbsp;
          <b>Current view: <FONT color=$level_color[$view2severity{$view}]>$action</FONT></b> </td>
	<td align=right>
	 <FONT size="-1"><i>Last update: $today &nbsp;</i> </FONT>
        </td> </tr>
     <tr><td></td>
    <SCRIPT language="JavaScript">
	<!-- Begin
	updateTime = $cursecs;	// get unix timestamp
	now = new Date();
	age = ((now.getTime() / 1000) - updateTime) / 60;  // in minutes
        if (age > 0 && age < 1) { age = 0; }
        else { age = parseInt(age); }
	if ( age > 15 ) {
	  document.write('<td bgcolor=yellow align=right>');
          document.write('This data is <b>OUTDATED</b>&nbsp; &nbsp;</td>');
	}
	else if (age < 0)
        {
	  document.write('<td align=right nowrap><font size="-1">(is your clock out of sync by ' + age + 'min ?)</font></td>');
        }
	else
        {
	  document.write('<td align=right><font size="-1">(updated ');
	  document.write(age + ' min ago)&nbsp;</font></td>');
	}
	onError = null;
	// End -->
    </SCRIPT>
     </tr> <!-- gap -->
     <!-- <tr><td height="6"></td></tr>	-->
    </TABLE>
    <P></P>
EOT

  ## now print the buttons for the form
  my $restore_url = $cgimode ? "$my_url" : "${baseurl}/${view}.html";
  $restore_url = uri_escape($restore_url);

  if ($view ne "User") {
    print <<EOT1;
   <!--	--- buttons for other views --- -->
   <TABLE border=0 cellpadding=0 cellspacing=5>
      <TR>
EOT1
    my ($path, $query) = split (/\?/, $my_url);
    foreach my $v (@views) {
      next if ($v eq "User");
      if ($gen_cgi_links)
      {
	my $newq;
	($newq=$query) =~ s/view=[A-Za-z]+//;
	$newq .= '&' if $newq;
	$newq .="view=$v";
	print "<TD valign=middle><FORM action=\"$path\" method=\"get\">
	<input type=submit name=view value=\"$v\">\n";
	foreach  (split /&/, $newq) {
	  my ($key, @val) = split /=/;
	  print $thiscgi->hidden(-name => $key, -value=>\@val);
	}
        print "\n  </FORM></TD>\n";
      }
      else {	# not cgi mode
	print "       <TD valign=middle>
               <FORM action=\"${baseurl}/${v}.html\" method=\"get\">
                <input type=submit name=view value=\"$v\">
               </FORM> </TD>\n";
      }
    }	# foreach @views

    # this button invokes the genweb-filter.cgi script for sorting/selecting
    if ($filter_cgi ne "")
    {
      print "  <TD valign=middle>
                 <FORM action=\"${filter_cgi}\" method=\"get\">
	         <input type=submit value=\"Filter\">\n";
      foreach ( qw(view refresh sound sort maxrows namepat varpat monpat
		   filepat altprint))
      { 
	my $p =$thiscgi->param($_);
	next if !defined($p) || $p eq '' || $p eq 'no';
	print $thiscgi->hidden(-name => $_, -value=>$thiscgi->param($_));
      }
      print "
             <input type=hidden name=noncgiurl value=\"${baseurl}/${view}.html\">
            </FORM></TD>\n";
    }

    print "
      <TD width=50>&nbsp;</TD>
      <TD valign=middle><FORM action=\"$snipsweb_cgi\" method=\"post\">
        <input type=submit name=command value=\"Help\">
        <input type=hidden name=restoreurl value=\"$restore_url\"></FORM>
    ";
    print "</TD>
      </TR>
   </TABLE>
   <P><font face=\"arial,helvetica\">
    <i>Select a device name to update or troubleshoot it </i>
   </font></P>\n";
    print "<P align=center> <b>Filter (CGI) Mode</b></P>\n" if ($cgimode);
  }	# if ($view != "user")
  else {	# User mode, no useful buttons

    print <<EOT1a;
   <P align="right">
   <FORM action="$snipsweb_cgi" method="get">
       <input type=submit name=command value="UserHelp">
       <input type=hidden name=restoreurl value="$restore_url">
   </FORM> </P>
EOT1a
  }	# if-else ($view != "user")

}  # print_html_prologue()

#------------------------------------------------------------
## print out the header for the columns that will be collected in
# @row_data.
# If data rows are going to be in tabular form, we print out the
# table header too.
sub print_column_headers {
  print $thiscgi->comment("   --- print_column_headers   ---"), "\n";
  my @fields = ('', '#', 'Status', 'Device Name', 'Address', 
		'Variable / Value', 'Down At', 'Monitor');
  
  push @fields, 'Updates' 
    if ($view ne "User" || $userViewUpdates);
  
  $numcols = ($#fields + 1) * 2;
  
  print $thiscgi->comment('     --- main data table --'), "\n";
  if ($max_table_rows == -1 || $prefmt == 1)  {	
    # Print status in a compact format
    print "     <PRE> <b>\n";
    printf "%4s %1.1s  %14s %15s  %21s  %11s  %-12s %s\n",
        '#', 'S', 'Device Name ', 'Address   ',
	'Variable / Value', 'Down At  ', 'Monitor', 'Updates';
    print "   </b>\n\n";
  } else {
    # Print status in a table format
  print <<EOT2;
    <TABLE class="data" cellpadding=0 cellspacing=0 border=0>
       <!-- thin blank line -->
  <TR><td colspan=$numcols bgcolor="#000000">
        <img src="$emptyimg" alt="."></td></TR>
       <!-- table header row -->
  <TR bgcolor="#FFFFFF">
EOT2
  
  foreach my $field (@fields) {
    $field =~ s@\/@<br>&nbsp;\/@g ; # split words onto two separate lines
    print <<EOT2a;
      <td nowrap align=center class="data"> &nbsp; <b>$field</b>  &nbsp; </td>
      <td bgcolor="#AAAAAA" width=1>
	  <img src="$emptyimg" alt="."></td>  <!-- thin vertical divider -->
EOT2a
  }	# foreach $field
  
  print <<EOT2b;
    </TR>
      <!-- thin black line -->
  <TR><td colspan=$numcols bgcolor="#000000">
	<img src="$emptyimg"></td></TR>
EOT2b

  } # endif $view eq Info
  print $thiscgi->comment('  --- end print_column_headers()  ---'), "\n";

}  # sub print_column_headers


#------------------------------------------------------------
## Read in all the data for the given file and store in @row_data
#  Filename filtering and event filtering is also done here.
sub get_row_data {
  my ($dfile) = @_;
  my $namepat   = $thiscgi->param('namepat');
  my $varpat    = $thiscgi->param('varpat');
  my $monpat    = $thiscgi->param('monpat');
  my $filepat   = $thiscgi->param('filepat');
  my $event;

  my $file=$dfile;
  $file =~ s/-output//;
  return if ($filepat ne "" && $file !~ /$filepat/);

  my $datafd = open_datafile("$datadir/$dfile", "r");
  if (!defined($datafd)) {
    logerr("cannot open $dfile for reading");
    return;
  }

  # process lines of data file.
  while ( ($event = read_event($datafd)) ) {

    my %ev = unpack_event($event);
    next if ($ev{device_name} eq "" && $ev{device_addr} eq "");

    $ev{file}=$file;	# store the filename also
    
    my $update = $updates{"$ev{device_name}:$ev{device_addr}:$ev{var_name}"};
    #if ($update eq "") {$update = $updates{"$ev{device_name}:$ev{device_addr}"}; }
    #if ($update eq "") {$update = $updates{"$ev{device_name}"; }
    
    # If device is no longer critical, remove its status information
    if (($ev{severity} > 1) && $update) {
      &remove_updates_entry($ev{device_name}, $ev{device_addr}, $ev{var_name});
    }
    
    # Are we interested in this event?
    next if (defined($namepat) && $ev{device_name} !~ /$namepat/);
    next if (defined($varpat)  && $ev{var_name}  !~ /$varpat/);
    next if (defined($monpat)  && $ev{sender}   !~ /$monpat/);
    
    if ( (! $view) || $ev{severity} <= $view2severity{$view}) {
      # Yep, we want this event.
      select_sound($ev{severity});
      push @row_data, \%ev;	# stores the entire assoc array
      ++$entries ;
    }
  }	# while ( readevent() )

} # sub get_row_data


#------------------------------------------------------------
## Sort the entire @row_data  list of events.
sub sort_rows {
  my $sort = $thiscgi->param('sort');
  $sort = "deviceaddr" if (! defined($sort));	# default by address

  my @ordering = split(/,/, $sort);
  while (@ordering) {
    my $order = pop @ordering;
    if ($order eq 'name' || $order eq 'devicename') {
      @row_data = sort { $a->{device_name} cmp $b->{device_name} } @row_data;
    } elsif ($order eq 'severe') {
      @row_data = sort { $a->{severity} cmp $b->{severity} } @row_data;
    } elsif ($order eq 'varname') {
      @row_data = sort { $a->{var_name} cmp $b->{var_name} } @row_data;
    } elsif ($order eq 'deviceaddr') {
      @row_data = sort { $a->{device_addr} cmp $b->{device_addr} } @row_data;
    } elsif ($order eq 'varvalue') {
      @row_data = sort { $a->{var_value} cmp $b->{var_value} } @row_data;
    } elsif ($order eq 'monitor') {
      @row_data = sort { $a->{sender} cmp $b->{sender} } @row_data;
    }
  }	# while()

}  # sub sort_rows()

#------------------------------------------------------------
## write data collected from the xxxmon programs.
#
sub print_row_data {

  print $thiscgi->comment("--- Start of real data rows --"), "\n";

#  my $np = $thiscgi->param('altprint');
#  my $print_routine = defined($np) && $np ne 'no' ? 
#      \&print_row_new : \&print_row;

  # Dump out processed rows...
  my $i=1;
  foreach my $ev (@row_data) {
#    &$print_routine($ev, $i++);  
    print_row($ev, $i++);
  }
}

## From URI.pm
sub uri_escape {
  my ($uri) = @_;
  $uri =~ s/([^;\/?:@&=+\$,A-Za-z0-9\-_.!~*'()])/$escapes{$1}/g;  #'; # emacs
  return $uri;
}

#------------------------------------------------------------
## Write out one row of data.

#  Alternates the row colors. Also, if the event is new (less than
#  $neweventAge minutes old), it sets the button background to yellow.
#  If $view is 'Info', then does not write out in table format so that
#  the size of the data file is small.
#
sub print_row {
  my ($ev, $entry_count) = @_;
  my $ifnewbg = "";	# new background if new event
  my $ADMINMODE = ($view eq "User") ? 0 : 1 ;
  my @rowcolor = ("#FFFFcc", "#D8D8D8");	# alternating row colors
  my $action   = $views[$view2severity{$view}];

  my( $update );
  if ( ! $ev->{device_subdev} ){
    $update =
       ($updates{"$ev->{device_name}:$ev->{device_addr}:$ev->{var_name}"} 
       or '');
  } else {
    $update =
       ($updates{"$ev->{device_subdev}+$ev->{device_name}:$ev->{device_addr}:$ev->{var_name}"} 
       or '');
  }
  $update = "OLD DATA" if ($ev->{state} & $n_OLDDATA);

  #if ($update eq "") {$update = $updates{"$ev->{device_name}:$ev->{device_addr}"}; }
  #if ($update eq "") {$update = $updates{"$ev->{device_name}"}; }

  # hide if Critical display
  return if $update =~ /^\(H\)/ && $action eq 'Critical';

  my $restore_url = $cgimode ? "$my_url" : "${baseurl}/${view}.html";
  my $deviceHREF = "<A HREF=\"$snipsweb_cgi?displaylevel=$action";
  $deviceHREF = $deviceHREF .
    "&devicename=$ev->{device_name}&deviceaddr=$ev->{device_addr}" .
      "&subdevice=$ev->{device_subdev}&variable=$ev->{var_name}" .
	"&sender=$ev->{sender}" .
	  "&command=Updates&restoreurl=$restore_url\">";
  # $deviceHREF = uri_escape($deviceHREF);
  #$deviceHREF =~ s/\+/$escapes{'+'}/g;	# + has special meaning in URLs?
  if ($max_table_rows == -1 || $prefmt == 1)
  {
    # need to put the href in front of the devicename, but we dont want
    # the devicename to be prepended with underlined blanks. i.e. convert
    #   '<a href="xx">   device</a>'  INTO  '  <a href="xx">device</a>'
    my $device = sprintf "%-14.14s", $ev->{device_name}; # printing size
    $device =~ s|(\S+)|$deviceHREF$1</a>| ;
    my @tm = localtime($ev->{eventtime});
    printf "%4d %1.1s  %s %-15.15s  %12.12s= %8lu  %02d/%02d %02d:%02d %-12s %s\n",
      $entry_count, $views[$ev->{severity}], $device,
      $ev->{device_addr}, $ev->{var_name}, $ev->{var_value},
      $tm[4]+1, $tm[3], $tm[2], $tm[1], $ev->{sender}, $update;
    return;
  }

  ## see if this is a recent event (less than $neweventAge minutes old)
  if ($ev->{loglevel} != $E_INFO &&  time - $ev->{eventtime} < $neweventAge * 60)
  {
      ++$newevents;			# total displayed in Messages
      $ifnewbg = "bgcolor=yellow";	# background of the little button
  }

  my $tdstart = "<td nowrap align=\"left\" class=\"data\"> \&nbsp\; ";
  my $tdend = "</td>\n    <td bgcolor=\"\#AAAAAA\" width=1>";
  $tdend .= "<img src=\"${emptyimg}\" alt=\"&nbsp;\"></td>\n";

  ## begin the row of data
  # 	ser-no  severity  devicename  address  variable+value
  print " <TR bgcolor=\"$rowcolor[$entry_count % 2]\">\n";
  print "   <td $ifnewbg>";
  if ($ADMINMODE) { print "$deviceHREF"; }
  print "<img src=\"$level_imgs[$ev->{severity}]\" alt=\"\" border=\"0\">";
  if ($ADMINMODE) { print "</a>"; }
  print $tdend;

  my $devicename = $ev->{device_name};
  if ($ev->{device_subdev} ne "") {
    $devicename = "$ev->{device_subdev}" . "+${devicename}";
  }
  if ($ADMINMODE) {
    $deviceHREF =~ s/command\=Updates/command\=DeviceHelp/;  # change command
    $devicename = "$deviceHREF" . "$devicename" . "</a>";
  }
  my ($x, $min, $hr, $mday, $mon, $yr, $x,$x,$x) = localtime($ev->{eventtime});
  ++$mon;
  print <<EoRow ;
    $tdstart $entry_count $tdend
    $tdstart $views[$ev->{severity}]  $tdend
    $tdstart $devicename $tdend
    $tdstart $ev->{device_addr} $tdend
    <td nowrap align=right class="data"> 
       &nbsp; $ev->{var_name}= $ev->{var_value} &nbsp; $tdend
    <td nowrap align=right $ifnewbg class="data"> $mon/$mday $hr:$min $tdend
     $tdstart $ev->{sender} $tdend
EoRow

  if ($ADMINMODE || $userViewUpdates) {
    print "     $tdstart $update $tdend"; 
  }
  print "</TR>\n";	# end of row
}	# print_row()

#------------------------------------------------------------
## Print out the file contents from the MSGSDIR if any.
sub print_msgs {
   ## Now write out the messages from the message dir. Perhaps should be
   #  in a separate frame.
  # print $thiscgi->comment("FUNCTION --- print_msgs"), "\n";
   if (opendir(MSGSDIR, "$msgsdir")) {
     my @msgfiles = grep (!/^\./, readdir(MSGSDIR) );
     if ($#msgfiles >= 0) {
       print "<center><hr noshade width=\"100\%\"><h3>Messages</h3></center>\n"; }
     foreach my $mfile (@msgfiles)
     {
       if (open(F, "< $msgsdir/$mfile"))
       {
	 # print STDERR "Opened $mfile\n" if $debug;
	 print "<P>";
	 while (<F>) { print ; }
	 close (F);
       }
     }				# foreach()
     closedir(MSGSDIR);
   }				# if opendir()
}

#------------------------------------------------------------
#
sub show_config {
  my($query) = @_;
  my(@values,$key);

  print "<hr width=\"20\%\" align=\"left\"><br>
          <b>Current filter settings for this view</b>\n
          <p><font size=\"-1\">";
  foreach $key ($query->param) {
    print "<STRONG>$key</STRONG> -> ";
    @values = $query->param($key);
    print join(", ",@values),"<BR>\n";
  }
  print "</font></p>\n";
}

#------------------------------------------------------------
##
sub print_footer {
   ## simple footer
  # print $thiscgi->comment("FUNCTION --- print_footer"), "\n";
  print $thiscgi->p("<hr width=\"20\%\" align=\"left\">"), "\n";
  print "<font size=\"1\">
<a href=\"http://www.netplex-tech.com/software/snips\">SNIPS- v$SNIPSVERSION</a>";
   print "\n </BODY>\n</HTML>\n";
}

##------------------------------------------------------------
# This routine deletes an entry from the updates file if it is
# no longer critical.
#
# If you've added a status for a device that's been down and that
# device  comes  up,  we  want  to  remove that status to prevent
# confusion should that device go down at a later date.

sub remove_updates_entry {
  my ($devicename, $deviceaddr, $subdevice, $varname) = @_;

  open (SFILE, "< $updatesfile") || return; 

  my  (@list) = <SFILE>;	# slurp into memory
  close (SFILE);
  # if we're not successful in opening it this time, we'll just
  # wait until the next time through.
  open (SFILE, ">> $updatesfile") || return ;
  foreach (1..3) {	# try locking the file three times...
    if (flock(SFILE, 2)) {
      seek(SFILE, 0, 0);
      truncate(SFILE, 0);
      last;
    }
    if ($_ == 3) {
      print STDERR "Could not flock $updatesfile, cannot remove entry $devicename\n";
      return;
    }
    sleep 1;
  }
  
  foreach (@list)
  {
    # skip comments and blank lines
    if (/^\s*\#/ || /^\s*$/) { print SFILE; next; }
    print "$devicename:$deviceaddr:$varname<br>\n" if $debug;
    next if /^$devicename\:$deviceaddr\:$varname/; # dont write out, delete
    print "not removed<P>\n" if $debug;
    
    print SFILE;

  }		# foreach
  close (SFILE);

}	# remove_entry()


# Decide which, if any, sound file should be played.
# My set global $sound.
sub select_sound {
  my ($severity) = @_;
  return if !defined ($sound) || !$sound ;

  if ($severity == $view2severity{"Critical"}) { 
    $sound = "critical.wav";
  } elsif (($severity == $view2severity{"Error"}) 
      && ($sound ne "critical.wav")) {
    $sound = "error.wav";
  } elsif (($severity == $view2severity{"Warning"}) &&
	   (($sound ne "critical.wav") && ($sound ne "error.wav"))) {
    $sound = "warning.wav";
  }
}

#------------------------------------------------------------
# log error to syslog and print to HTML.
sub logerr {
  my($msg) = shift;

  logger($msg);
  print $thiscgi->comment("error: $msg"), "\n";
}

#------------------------------------------------------------
sub ooops {
  my ($msg) = @_;
  my $action   = $views[$view2severity{$view}];
  print_http_header(300);	# refresh to 5 minutes
  print $thiscgi->start_html(-title=>"SNIPS - $action view", 
                       -type =>'text/css',
                       -bgcolor=>"#FFFFFF",
                      ), "\n";
  print $thiscgi->comment("Generated by $versionid"), "\n";
  print $thiscgi->h2($msg);
  print_footer();
  exit 1;
}

#------------------------------------------------------------
## A 'main' for cgi mode.
sub gen_html_cgimode {
  foreach my $dfile (@dfiles) {
    get_row_data($dfile);
  }
  if ($#row_data > $max_table_rows) {
    $prefmt = 1;
    $refresh = $large_refresh  if ($large_refresh > $refresh);
  }
  sort_rows();
  print_http_header($refresh);
  print_html_prologue($refresh);
  print_column_headers();
  print_row_data();
  
  if ($prefmt) { print "\n</PRE>\n"; }
  else {
    # Print a helpful message if there's nothing wrong
    if ($entries == 1) {
      my $msg = "No devices to be displayed at this level!";
      $msg = "No monitors running?" if (! @dfiles);
      print "<TR><TD colspan=$numcols align=center bgcolor=\"\#CCCC99\">\n";
      print "<br><H3>$msg</H3>";
      print "</TD></TR>\n";
    }
    print "<TR><TD  height=5></TD></TR>\n"; # vertical space
    print "</TABLE>\n";
  }
  
  print_msgs();			# stuff from msgs directory
  
  if ($newevents > 0) {
    if ($sound) {
      print "
<EMBED src=\"$sound\" 
       TYPE=\"audio/x-wav\"
       autostart=true hidden=true loop=FALSE>\n";
      #print "<NO EMBED><bgsound=\"$sound\" loop=1></NO EMBED>\n"; # for IE
    }
    print "<P>$newevents new events (less than $neweventAge minutes old)</P>\n";
  }
  
  show_config($thiscgi);	# display parameters
  print_footer();	# closing stuff

  exit 0;
}	# sub gen_html_cgimode()

#------------------------------------------------------------
## This is an alternative 'main' which generates the HTML files in
#  the $webdir so that they can be directly sent by httpd instead
#  of invoking this genweb as a CGI every time. It looks at the previous
#  size of the generated HTML files to determine if it should create
#  <PRE> data output instead of tables.
#  It sets the global variable $view & $prefmt in order to generate all
#  the 5 output files in one pass.
sub gen_html_files {

  my %fh;
  my %i = ( User => 1, Critical => 1, Error => 1, Warning => 1, Info => 1 );
  my %prefmt = (User => 0, Critical => 0, Error => 0, Warning => 0, Info => 1);
  my %newevents = ( User => 0, Critical => 0, Error => 0, Warning => 0, Info => 0 );

  foreach $view (@views)
  {
    # use * notation for indirect filehandles, since strict subs complains
    # if done any other way.
    local *FH;
    my $myrefresh = $refresh;
    my $ofile = "$webdir/${view}.html";
    if (-f $ofile) {
      my @s = stat($ofile);
      if ($s[7] > $prefmt_filesz) {	# if prev file larger
	$prefmt{$view} = 1;
	$myrefresh = 2*$refresh;
      }
      else { $prefmt{$view} = 0; }
    }
    open (FH, ">$ofile") ||
      die "Cannot open output file $webdir/${view}.html $!";
    print STDERR "Opened $ofile\n" if $debug;
    select(FH);
    $prefmt = $prefmt{$view};
    print_html_prologue($myrefresh);
    print_column_headers();
    $fh{$view} = *FH;	# store the open filehandle
  }
  foreach my $dfile (@dfiles)
  {
    @row_data = ();	# initialize to null list
    undef $view ;	# so get_row_data gets all the data

    $entries = 1;	# reset
    if ($totaldatasize < 5000000) {
      print STDERR "Reading all files in one pass\n" if $debug;
      foreach (@dfiles) { get_row_data($_); }
    }
    else {
      print STDERR "Doing file $dfile\n" if $debug;
      get_row_data($dfile);
    }
    sort_rows(@row_data);
    # print STDERR "Read $entries events from $dfile\n";
    foreach my $ev (@row_data)
    {
      foreach $view (@views)
      {
	if ($ev->{severity} <= $view2severity{$view})
	{
	  select $fh{$view};	# default for print statements
	  $prefmt = $prefmt{$view};
	  print_row($ev, $i{$view});
	  ++$i{$view};
	  $newevents{$view} += $newevents;
	  $newevents = 0;
	}
      }	   # foreach  @views
    }	# foreach @row_data

    last if ($totaldatasize < 5000000);
  } # foreach @dfiles

  foreach $view (@views)  {	  # write out the closing HTML for each file
    select $fh{$view};
    if ($prefmt{$view} == 1) {
      print "\n</PRE>\n";
    }
    else {
      # Print a helpful message if there's nothing wrong
      if ($i{$view} == 1) {
	my $msg = "No Devices at this level!";
	$msg = "No monitors running?" if (! @dfiles);
	print "<TR><TD colspan=$numcols align=center bgcolor=\"\#CCCC99\">\n";
	print "<br><H3>$msg</H3>";
	print "</TD></TR>\n";
      }
    }
  
    print "<TR><TD  height=5></TD></TR>\n"; # vertical space
    print "</TABLE>\n";

    print_msgs();		# stuff from msgs directory

    if ($newevents{$view} > 0) {
      if ($sound) {
      print "
<EMBED src=\"$sound\" 
       TYPE=\"audio/x-wav\"
       autostart=true hidden=true loop=FALSE>\n";
    #print "<NO EMBED><bgsound=\"$sound\" loop=1></NO EMBED>\n"; # for IE
    }
      print "<P>$newevents{$view} new events (less than $neweventAge minutes old)</P>\n";
    }

    print_footer();	# closing stuff

  }	# foreach @views

  exit 0;

}   # gen_html_files()

