#!/usr/local/bin/perl
#
# $Header: /home/cvsroot/snips/snipsweb/genweb.pl,v 1.1 2000/06/02 21:53:56 vikas Exp $ 
#
#			genweb.pl
# ------------
# This script generates web pages which display snips events (alternative   
# to snipstv). The pages use the REFRESH meta tag - your Web browser must
# support this META parameter (netscape and IE). Can play an alarm sound at
# every cycle when there are critical devices.
#
# This script also reads in an 'updates' file and hide's the event or adds
# a update message to an event. Once the site comes back up, this script
# will remove the status line from the 'updates' file (to prevent any
# confusion when the site goes down next).
#
# It creates 5 output files- one for each level of snips (Critical -> Info)
# and another 'restricted' one for non-staff users (Users.html).
# These files have links to 'snipsweb.cgi' which does additional processing
# for users (with authentication).
#
# Any global 'messages' that are in the snips messages directory are
# also displayed.
#
# The snipsweb.cgi CGI program allows displaying help files, traceroutes,
# old-logs, and also updating the 'updates' file.
#
# INSTALLATION
# ------------
# 1. Most of the configuration options are set in the snipsweb-cfg.pl
#    config file.
# 2. Create a '/snips/etc/updates' file with the owner being the httpd 
#    daemon's owner. (this will be edited by the cgi script)
# 3. Install the snipsweb.cgi CGI script in your CGI directory and set the
#    URL in this file. Install the entire 'gifs' dir under the
#    $baseurl directory.
# 4. If you want to create a separate 'User' view for outsiders, then you
#    can copy the 'Users.html' file to a public web site. Else create
#    a link from Critical.html to 'index.html' so that this is the
#    default page. You can either password protect this URL tree using
#    httpd's access mechanism (.htaccess) or else rely on the $etcdir/$authfile
#    file (see snipsweb.cgi). Either way, you have to create this 'authfile'
#    to list the users who can edit the various snips files, etc.
# 5. Create help files, etc. under $snipsroot/device-help.
#    Note that these help files are HTML files and can contain HTML tags
#    for formatting and clickable links (see details in the CGI script).
# 6. Run this from cron every minute using a crontab entry such as:
#   		* * * * * /snips/bin/genweb.pl >/dev/null 2>&1
#
# AUTHOR
#	Richard Beebe (richard.beebe@yale.edu) 1998
#	Updates: Vikas Aggarwal (vikas@navya.com) 1998
#
#   This script was distributed as part of the SNIPS package.
#
# This script creates five web pages:
#   User.html     -> a critical-only non-updatable page for public use.
#                    We suggest you link index.html to this file so that
#                    it gets displayed by default.
#   Critical.html -> Critical view - allows updating of status
#   Error.html    -> Error view    _   "       "      "    "
#   Warning.html  -> Warning view  _   "       "      "    "
#   Info.html     -> Information view _"       "      "    "
#
# BUGS:
#   If a client station requests a page at the exact time this script
#   is updating that page, they get a 'document contains no data' error.
#   Hitting 'reload' will bring the page up.

#########################################################################

$VERSIONSTR = "1.0";		# version

###
###
# Makefile will edit the $snipsroot.
# Remember to create the $updatesfile (writable by httpd daemon)
$snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS
$etcdir  = "$snipsroot/etc"  unless $etcdir;	# location of config file
$bindir  = "$snipsroot/bin"  unless $bindir;
$datadir = "$snipsroot/data" unless $datadir;
$msgsdir = "$snipsroot/msgs" unless $msgsdir;

push(@INC, $bindir); push(@INC, $etcdir); # add to search paths for 'require'
require  "snipslib.pl";
require  "snipsweb-cfg.pl";	# configuration options for SNIPS web


## The following variables are customized in snipsweb-cfg.pl
# $baseurl is the URL that will be used to access the snips HTML files.
$baseurl = "http://snips.navya.com" unless $baseurl;
$baseurl .= "/" if ($baseurl ne "" && ! ($baseurl =~ /\/$/)); # trailing '/'
# $webdir is the physical path to the $baseurl directory
$webdir  = "/snips/htmldata" unless $webdir;
# The URL (as seen by the browser) to the snipsweb.cgi CGI
$snipsweb_cgi = "/cgi-bin/snipsweb.cgi" unless $snipsweb_cgi;

# somewhat browser specific. Comment out if its painful.
# $sound = "${baseurl}chirp.au";	# set in snipsweb-cfg.pl

## Other customizations (in snipsweb-cfg.pl)
$neweventAge = 5 unless $neweventage;	# hilite event if less than this old
$userViewUpdates = 0 unless $userViewUpdates;
$tfontsize = 2 unless $tfontsize;	# table data font size (prefer 2)
# dont generate Info file in table format since its usually too huge
$compactInfo = 1 unless $compactInfo;
$debug = 0 unless $debug;		# local debug level


### Remaining is not really customizable. ###

# url for accessing the various snips images. Dont make it 'images' since
# it usually gets mapped by the httpd daemon.
local ($imageurl) = "${baseurl}gifs/";  # check. Note baseurl has trailing /
$imageurl .= "/" if ($imageurl ne "" && ! ($imageurl =~ /\/$/));

# 'updatesfile' is the full path to the file containing update messages
# Since this file is updated via the web (snipsweb.cgi), this file should
# be owned by 'www' or 'httpd' to allow editing. this must match the
# file location in the snipsweb.cgi script also.
$updatesfile = "$etcdir/updates";		# CHECK this

local (@levels) = ( "User", "Critical", "Error", "Warning", "Info");
local (%ilevels) = ( "User", 1, "Critical", 1, "Error", 2, "Warning", 3,
		     "Info", 4 );
local (@level_color) = ("", "Red", "Blue", "Brown", "Black");
local (@level_imgs) = ("", "${imageurl}redsq.gif", "${imageurl}bluesq.gif",
		       "${imageurl}yellowsq.gif", "${imageurl}greensq.gif");
## These gif images are provided with snips. Put them in your httpd directory
#  tree under $baseurl/images/
local ($emptyimg) = "${imageurl}empty.gif";

@z1 = ('00' .. '59');		# To convert single digit minutes -> 08
($sec, $min, $hour, $mday, $mon, $year,
 $weekday, $yrday, $daylite) = localtime(time);
$mon++;
$year += 1900;	# fix y2k problem
$today = "$z1[$mon]/$z1[$mday]/$year  $hour:$z1[$min]";	# MMDDYY, us centric
# $today = "$year/$z1[$mon]/$z1[$mday]  $hour:$z1[$min]";	# YYMMDD
local($cursecs) = time;	# get Unix timestamp


####
#### END INITIALIZATION
####

# Read in the status file. Entries in this file will get a status message
# if not in User view.
if (open (INPUT, $updatesfile)) {
   while (<INPUT>)
   {
     chop;
     next if (/^\s*\#/ || /^\s*$/);   # skip comments & blank lines
     ($addr, $update) = split(/\t/);
     $updates{$addr} = $update;
    }
   close (INPUT);
}


# get all log files from SNIPS data directory
# (extract only those files that have '-output' as prefix)
opendir(DATADIR, "$datadir") || die "\nCannot open input directory $datadir";
local(@dfiles) = sort(grep(/.+-output/i, readdir(DATADIR)));
closedir(DATADIR);


##
## generate the 'prologue' (open each file and write it out) one at a time
##
foreach $lvl (@levels) { 
  print STDERR "Writing ${baseurl}${lvl}.html\n" if $debug;
  # we refresh the screen every minute unless we're in 'info' view because
  # info view can be so large that it takes longer than a minute to peruse
  # it
  local ($refresh) = ( $lvl eq "Info" ? 300 : 60 );
  local ($thispage) = "${baseurl}${lvl}.html";
  local ($ADMINMODE) = ($lvl eq "User") ? 0 : 1; # No href links for userPage
  $cnt{$lvl} = 1;	# serial number per view

  open (OUTPUT, ">$webdir/${lvl}.html") or die 
    "Unable to open output file ($webdir/${lvl}.html) $!";
  select OUTPUT;		# default for print statements

  &print_html_prologue($thispage, $lvl, $refresh);
  
  ## now write out the header for the rows
  @fields = ('', '#', 'Status', 'Device Name', 'Address',
	     'Variable / Value', 'Down At');
  if ($ADMINMODE || $userViewUpdates) { @fields = (@fields, 'Updates'); }
  $numcols = ($#fields + 1) * 2;

  if ($compactInfo && ($lvl eq "Info"))  {	# no table, compact format
    print "     <!-- ### main data table ### -->\n";
    print "     <PRE> <b>\n";
    printf "%3s %s %14s %15s %20s %s %s\n",
	    ' # ', 'S', 'Device Name ', '  Address',
	    'Variable/Value ', '  Down At', '  Updates';
    print "   </b>\n\n";
  }
  else {	# fancy table
    print <<EOT2;
    <!-- ### main data table ### -->
    <TABLE cellpadding=0 cellspacing=0 border=0>
       <!-- thin blank line -->
     <TR><td colspan=$numcols bgcolor="#000000"><img src="$emptyimg"></td></TR>
       <!-- table header row -->
     <TR bgcolor="#FFFFFF">
EOT2
  
    foreach $field (@fields)
    {
      $field =~ s@\/@<br>&nbsp;\/@g ; # split words onto two separate lines
      print <<EOT2a;
      <td nowrap align=center><font face="arial,helvetica" size="$tfontsize"> &nbsp;
       <b>$field</b>  &nbsp;     </font></td>
      <td bgcolor="#AAAAAA" width=1><img src=\"$emptyimg\"></td>  <!-- thin vertical divider -->
EOT2a
    }	# foreach $field
    print "</TR>\n";
  
    print <<EOT2b;
    </TR>
      <!-- thin black line -->
    <TR><td colspan=$numcols bgcolor="#000000"><img src="$emptyimg"></td></TR>

    <!-- ### Start of real data rows ### -->
EOT2b

 } # endif $lvl eq Info

  close (OUTPUT);
  open ($lvl, ">>$webdir/${lvl}.html");	# open new filehandle for each file
}				# end foreach($lvl)

############## End writing out the prologue

## write out all the html files together so that we dont have to make
#  4 passes over each data file. The files have already been reopened with
#  new file handles.

foreach $dfile (@dfiles)	# process each data file one at a time
{
  open (INPUT, "< $datadir/$dfile") || die "cannot open $dfile";
  &read_dataversion(INPUT);	# skip past first record
  # print STDERR "Opened $dfile\n" if $debug;
  # process log file line by line
  $i = 1;		# index required by snipslib readevent()
  while (&readevent (INPUT, $i)) {
    ## clean up the sitename, etc.
    # $sitename{$i} =~ tr/a-zA-Z0-9._\-\(\)//cd;
    $sitename{$i} =~ tr/\000//d;
    $siteaddr{$i} =~ tr/\000//d;
    $varname{$i} =~ tr/\000//d;
       
    $update = $updates{"$sitename{$i}:$siteaddr{$i}:$varname{$i}"};
    #if ($update eq "") {$update = $updates{"$sitename{$i}:$siteaddr{i}"}; }
    #if ($update eq "") {$update = $updates{"$sitename{$i}"}; }
       
    # If device is no longer critical, remove its status information
    if (($severity{$i} > 1) && $update) {
      &remove_updates_entry($sitename{$i}, $siteaddr{$i}, $varname{$i});
    }

    foreach $lvl (@levels) 	# write out the row to each file in turn
    {
      if ( $severity{$i} <= $ilevels{$lvl} )
      {
	if (defined ($sound) && $sound ne "") {
	  # set the appropriate warning sound
	  if ($severity{$i} == $ilevels{"Critical"}) { 
	    $sound = "critical.wav";
	  }
	  if (($severity{$i} == $ilevels{"Error"}) && ($sound ne "critical.wav")) {
	    $sound = "error.wav";
	  }
	  if (($severity{$i} == $ilevels{"Warning"}) &&
	      (($sound ne "critical.wav") && ($sound ne "error.wav"))) {
	    $sound = "warning.wav";
	  }
	}	# if defined $sound
	select $lvl;		# default for print statements
	&print_row($i, $lvl);
	++$cnt{$lvl} ;
      }
    } 
    # $i++;	# need not update
  }				# end of processing one log file
  close INPUT;
}	# end foreach($dfile), process next log file

################### finally write out the footer
foreach $lvl (@levels)
{
  select($lvl);
  if ($compactInfo && ($lvl eq "Info")) {
    print "\n</PRE>\n";
  }
  else {
    # Print a helpful message if there's nothing wrong
    if ($cnt{$lvl} == 1) {
      print "<TR><TD colspan=$numcols align=center bgcolor=\"\#CCCC99\">\n";
      print "<br><H3>The Network is <U>healthy</U>!</H3>";
      print "</TD></TR>\n";
    }
   
    print "<TR><TD  height=5></TD></TR>\n"; # vertical space
    print "</TABLE>\n";
  }

  &write_msgs;		# stuff from msgs directory
  if ($newevents{$lvl} > 0) {
    if ($sound) {
      print "<EMBED src=\"$sound\" autostart=true hidden=true loop=FALSE>\n";
      #print "<NO EMBED><bgsound=\"$sound\" loop=1></NO EMBED>\n"; # for IE
    }
    print "<P>$newevents{$lvl} new events (less than $neweventAge minutes old)</P>\n";
   }
   
  &write_footer;	# closing stuff
  close($lvl);			# close output HTML file
}	#end of foreach($lvl) loop for the footer

exit 0;


#------------------------------------------------------------
# Print header and the form buttons to select the severity.
sub print_html_prologue {
  local ($thispage, $lvl, $refresh) = @_;

  local ($action) = $levels[$ilevels{$lvl}];
  local ($id) = '$Id: genweb.pl,v 1.1 2000/06/02 21:53:56 vikas Exp $';#'

  $id =~ s/\$//g;	# cleanup
  print <<EOT;
<HTML>
  <HEAD>
    <META HTTP-EQUIV="REFRESH" CONTENT="$refresh;URL=$thispage">
    <META HTTP-EQUIV="PRAGMA" CONTENT="no-cache">
    <TITLE>SNIPS - $action view</TITLE>
    <!-- link rel="stylesheet" type="text/css" href="fonts_ep.css" -->
    <!-- Generated by $id -->
  </HEAD>
  <BODY bgcolor="#FFFFFF" link="#003366" vlink="#003366" alink="#003366">

    <TABLE cellpadding=2 cellspacing=0 border=0>  <!-- title banner -->
     <tr><td height="6"> &#160 </td></tr>	<!-- vertical space -->
     <tr><td bgcolor="#003366">
	  <font class="header" face="arial,helvetica" size=4 color="#FFFFFF">
          <b>&nbsp;&nbsp; SNIPS (System and Network Integrated Polling Software)&nbsp;&nbsp;</b>
	  </font></td>
     </tr>
     <tr><td height="6"> &#160 </td></tr>	<!-- vertical space -->
    </TABLE>
    <P>
    <TABLE width="100%" cellpadding=0 cellspacing=0 border=0>
     <tr><td width=50% align=left> &nbsp;
          <b>Current view: <FONT color=$level_color[$ilevels{$lvl}]>$action</FONT></b>
	 </td>
	<td align=right>
	 <FONT size="-1"><i>Last update: $today &nbsp;</i> </FONT>
        </td> </tr>
     <tr><td></td>
    <SCRIPT language="JavaScript">
	<!-- Begin
	updateTime = $cursecs;	// get unix timestamp
	now = new Date();
	age = ((now.getTime() / 1000) - updateTime) / 60;
        if (age > 0 && age < 1) { age = 0; }
        else { age = parseInt(age); }
	if ( age > 15 ) {
	  document.write('<td bgcolor=yellow align=right>');
          document.write('This data is <b>OUTDATED</b></td>');
	}
	else if (age < 0)
        {
	  document.write('<td align=right nowrap><font size="-1">(is your clock out of sync by ' + age + 'min ?)</font></td>');
        }
	else
        {
	  document.write('<td align=right><font size="-1">(updated ');
	  document.write(age + ' min ago)</font></td>');
	}
	onError = null;
	// End -->
    </SCRIPT>
     </tr>
     <!-- <tr><td height="6"></td></tr>	<!-- gap -->
    </TABLE>
    <P></P>

EOT
  
  if ($ADMINMODE) {
    print <<EOT1;
   <!--	### buttons for other views ### -->
   <TABLE border = 0 cellpadding=0 cellspacing=5>
      <TR>
      <TD valign=middle><b>New view: &nbsp; &nbsp;</b></TD>
      <TD valign=middle><FORM action="${baseurl}Critical.html" method="get">
	<input type=submit name=command value="Critical"></FORM></TD>
      <TD valign=middle><FORM action="${baseurl}Error.html" method="get">
        <input type=submit name=command value="Error"></FORM></TD>
      <TD valign=middle><FORM action="${baseurl}Warning.html" method="get">
        <input type=submit name=command value="Warning"></FORM></TD>
      <TD valign=middle><FORM action="${baseurl}Info.html" method="get">
        <input type=submit name=command value="Info"></FORM></TD>
      <TD width=50>&nbsp;</TD>
      <TD valign=middle><FORM action="$snipsweb_cgi" method="post">
        <input type=submit name=command value="Help">
        <input type=hidden name=return value="$thispage"></FORM></TD>
      </TR>
   </TABLE>
   <font face="arial,helvetica" size="$tfontsize">
   <i>Select a device name to update or troubleshoot it </i>
   </font> <P>
EOT1
  }
  else {
    print <<EOT1a;
   <P align="right">
   <FORM action="$snipsweb_cgi" method="post">
       <input type=submit name=command value="UserHelp">
       <input type=hidden name=return value="$thispage">
   </FORM> </P>
EOT1a
  }
}	# print_html_prologue()

#------------------------------------------------------------
## Write out one row of data.
#  Alternates the row colors. Also, if the event is new (less than 5 minutes
#  old), it sets the button background to yellow.
#  If $lvl is 'Info', then does not write out in table format so that the
#  size of the data file is small.
#
sub print_row {
  local ($i, $lvl) = @_;
  local ($ifnewbg) = "";	# new background if new event

  local	$cnt = $cnt{$lvl};	# the row count
  local (@rowcolor) = ("#FFFFcc", "#D8D8D8");	# alternating row colors
  local ($thispage) = "${baseurl}${lvl}.html";
  local ($action) = $levels[$ilevels{$lvl}];
  local ($ADMINMODE) = ($lvl eq "User") ? 0 : 1;  # No href links for userPage

  # the data is already clean since the new snipslib.pl uses 'A' to unpack
  # which strips out all the nulls.
  # &clean_data($i);	# delete unwanted characters (not needed anymore)

  local($update) = $updates{"$sitename{$i}:$siteaddr{$i}:$varname{$i}"};
  #if ($update eq "") {$update = $updates{"$sitename{$i}:$siteaddr{i}"}; }
  #if ($update eq "") {$update = $updates{"$sitename{$i}"}; }

  # hide if Critical display
  return if $update =~ /^\(H\)/ && $action eq 'Critical';

  local ($siteHREF) = "<A HREF=\"$snipsweb_cgi?displaylevel=$action";
  $siteHREF .= "&sitename=$sitename{$i}&siteaddr=$siteaddr{$i}";
  $siteHREF .= "&variable=$varname{$i}&sender=$sender{$i}";
  $siteHREF .= "&command=Updates&return=$thispage\">";

  if ($compactInfo && ($lvl eq "Info")) {
    # need to put the href in front of the sitename, but we dont want
    # the sitename to be prepended with underlined blanks. i.e. convert 
    #   '<a href="xx">   site</a>'  INTO  '  <a href="xx">site</a>'
    local ($site) = sprintf "%14.14s", $sitename{$i}; # printing size
    $site =~ s|(\S+)|$siteHREF$1| ;
    $site .= "</a>";
    printf "%3d %1.1s  %s %-15.15s  %12.12s= %-8lu %02d/%02d %02d:%02d %s\n",
	    $cnt, $levels[$severity{$i}], $site,
	    $siteaddr{$i}, $varname{$i}, $varvalue{$i}, 
	    $mon{$i},$day{$i},$hour{$i},$z1[$min{$i}], $update;
    return;
  }

  ## see if this is a recent event (less than 5 minutes old)
  if ($mon == $mon{$i} && $mday == $day{$i} &&
      (($hour * 60) + $min) - (($hour{$i} * 60) + $min{$i}) < $neweventAge)
  {
    ++$newevents{$lvl};			# total displayed in Messages
    $ifnewbg = "bgcolor=yellow";	# background of the little button
  }


  local ($tdstart) = "<td nowrap align=\"left\"> <font face=\"arial,helvetica\" size=\"$tfontsize\"> \&nbsp\;";
  local ($tdend) = "</font> </td>\n   <td bgcolor=\"\#AAAAAA\" width=1>";
  $tdend .= "<img src=\"${emptyimg}\" alt=\"&nbsp;\"></td>\n";

  ## begin the row of data
  # 	ser-no  severity  sitename  address  variable+value
  print "<TR bgcolor=\"$rowcolor[$cnt % 2]\">\n";
  print "<td $ifnewbg><font>";
  if ($ADMINMODE) { print "$siteHREF"; }
  print "<img src=\"$level_imgs[$severity{$i}]\" alt=\"\" border=\"0\">";
  if ($ADMINMODE) { print "</a>"; }
  print $tdend;

  local ($siteName) = $sitename{$i};
  if ($ADMINMODE) {
    $siteHREF =~ s/command\=Updates/command\=SiteHelp/;  # change the command
    $siteName = "$siteHREF" . "$sitename{$i}" . "</a>";
  }

  print <<EoRow ;
    $tdstart $cnt $tdend
    $tdstart $levels[$severity{$i}]  $tdend
    $tdstart $siteName $tdend
    $tdstart $siteaddr{$i} $tdend

    <td nowrap align=right><font face="arial,helvetica" size="$tfontsize">
    &nbsp; $varname{$i}= $varvalue{$i} &nbsp;
    $tdend

    <td nowrap align=right $ifnewbg><font face="arial,helvetica" size="$tfontsize">
    $mon{$i}/$day{$i} $hour{$i}:$z1[$min{$i}]  $tdend
EoRow

  if ($ADMINMODE || $userViewUpdates) {print "$tdstart $update $tdend"; }
  print "</tr>";	# end of row
}	# printline()

#------------------------------------------------------------
## Print out the file contents from the MSGSDIR if any.
sub write_msgs {
   ## Now write out the messages from the message dir. Perhaps should be
   #  in a separate frame.
   if (opendir(MSGSDIR, "$msgsdir")) {
     @msgfiles = grep (!/^\./, readdir(MSGSDIR) );
     if ($#msgfiles >= 0) {
       print "<center><hr noshade width=\"100\%\"><h3>Messages</h3></center>\n"; }
     foreach $mfile (@msgfiles)
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
##
sub write_footer {
   ## simple footer
   print "<p><hr width=\"20\%\" shade align=\"left\">\n";
   print "<font size=\"-2\"><a href=\"http://www.netplex-tech.com/software/snips\">SNIPS- v$VERSIONSTR</a>";
   print "</body></html>\n";

}

##------------------------------------------------------------
# This routine deletes an entry from the updates file if it is
# no longer critical.
#
# If you've added a status for a device that's been down and that
# device  comes  up,  we  want  to  remove that status to prevent
# confusion should that device go down at a later date.

sub remove_updates_entry {
  local ($sitename, $ipaddr, $varname) = @_;

  open (SFILE, "< $updatesfile") || return; 

  local (@list) = <SFILE>;	# slurp into memory
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
      print STDERR "Could not flock $updatesfile, cannot remove entry $sitename\n";
      return;
    }
    sleep 1;
  }
  
  foreach (@list)
  {
    # skip comments and blank lines
    if (/^\s*\#/ || /^\s*$/) { print SFILE; next; }
    print "$sitename:$ipaddr:$varname<br>\n" if $verbose;
    next if /^$sitename\:$ipaddr\:$varname/; # dont write out, delete
    print "not removed<P>\n" if $verbose;
    
    print SFILE;

  }		# foreach

  close (SFILE);

}	# remove_entry()
