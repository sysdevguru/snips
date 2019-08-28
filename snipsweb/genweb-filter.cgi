#!/usr/bin/perl
#
# CGI to display GUI to invoke genweb.cgi with appropriate parameters.
# This cgi is typically invoked from genweb.cgi. It sets the various
# parameters and calls genweb.cgi again.
#
# AUTHOR:  Rocky@panix.com, June 2000
#
#
#
my $vcid = '$Id: genweb-filter.cgi,v 1.2 2001/08/22 03:34:23 vikas Exp $ ';

use vars qw ( $debug $refresh $large_refresh $genweb_cgi
	      $snipsroot $etcdir $max_table_rows
	    );
$snipsroot = "/usr/local/snips";	# SET_THIS
$etcdir = "$snipsroot/etc";
push (@INC, $etcdir);

require  "snipsperl.conf";
require  "snipsweb-confg";		# in etcdir
use CGI;	# also requires Base64.pm

my @monitors = qw( etherload hostmon ippingmon nsmon ntpmon portmon 
		   rpcpingmon trapmon ) ;
my @sortfields = qw(name varname deviceaddr varvalue monitor severe);

$debug = 0;	# FIX FIX
$query = new CGI;
$debug= $query->param('debug') if $query->param('debug');
my $a=$query->param('Action');

## user clicked on submit button, create URL  to genweb.cgi with parameters
#  tacked on and redirect.
if (defined($a) &&  $a eq 'Submit') {
  my $url=make_url($query);
  print $query->redirect($url);
  exit 0;
}
elsif (defined($a) && $a =~ /^Non-filter/i) {
  my $url = $query->param('noncgiurl');
  if (defined($url) && $url ne "") {
    print $query->redirect($url);
    exit 0;
  }
}

# this script was invoked by external script.
set_defaults($query);
print $query->header;
print $query->start_html("SNIPS Filter Configurator");
print "<H1> SNIPS Filter Configurator</H1>\n";
&print_form($query);
show_config($query) if $debug;
print $query->end_html;

###
### Subroutines
###

sub print_form {
  my($query) = @_;
  
  print $query->start_form;

  print "<P><EM>Which view?</EM><BR>\n",
        $query->radio_group(
			    -name=>'view',
			    -value=>['Critical', 'Error', 'Warning',
				     'Info'],
			    -default=>'Critical');
  
  print "<P><EM>Which monitors do you want?</EM><BR>\n";
  print $query->checkbox_group(
			       -name=>'monpat',
			       -value=> \@monitors,
#			       -linebreak=>'yes',
			       -defaults=> \@monitors);
  
  print "<P><EM>Sound?</EM><BR>\n",
        $query->radio_group(
			    -name=>'sound', 
			    -value => ['yes', 'no'],
			    -default=>'no' );

#  print "<P><EM>Alternate Print Style?</EM><BR>\n",
#        $query->radio_group(
#			    -name=>'altprint', 
#			    -value => ['yes', 'no'],
#			    -default=>'no' );

  print "<p>Refresh interval (in seconds)? \n",
        $query->textfield(
			  -name=>'refresh',
			  -default=>$refresh,
			  -size=>5,
			  -maxlength=>10
			 ),
	" (overridden if events more than $max_table_rows)\n";


  print "<p>Sort order: ",
        $query->popup_menu(
			   -name=>'sort',
			   -value=>\@sortfields,
			   -default=>'name'),

        "\n";

  print "<p>file pattern? \n",
    $query->textfield(-name=>'filepat',
		      -default=>'',
		      -size=>15,
		      -maxlength=>20
		     ), "\n";


  print "<P>", $query->reset, "&nbsp; &nbsp;",
#        $query->submit('Action', 'Toggle'), "&nbsp; &nbsp;\n",
        $query->submit('Action', 'Submit');
  my $url = $query->param('noncgiurl');
  if (defined($url) && $url ne "") {
    print "&nbsp; &nbsp; &nbsp; &nbsp;\n",
           $query->submit('Action', 'Non-Filter mode'),
          $query->hidden(-name=>'noncgiurl', -value=> q($url)),
         "\n";
  }
  print $query->endform, "<HR>\n";
}

sub make_url {
  my($query) = @_;
  my(@values,$key);
  my $param;
  my $url=$genweb_cgi;
  
  foreach $key ($query->param) {
    next if $key eq 'Action';
    @values = $query->param($key);
    my $sep=',';
    if ( $key eq 'monpat' ) {
      next if $#values == $#monitors;
      $sep='|';
    }
    next if (@values==1) && $values[0] eq 'no';
    my $value .= "$key=" . join($sep, @values);
    next if $value eq '';
    if ($param) {
      $param .= "&$value";
    } else {
      $param = $value;
    }
  }
  $url .= "?" . $param if $param;
  return $url;

}

# Sets default values for form.
sub set_defaults {
  my($query) = @_;

  my $values = $query->param('monpat');
  if ( (! defined($values)) || ($values =~ /^\s*$/) ) {
    $query->param(-name=>'monpat', -value=>\@monitors);
  }
  else {
    my @values = split /|/, $values;
    $query->param(-name=>'monpat', -value=>\@values);
  }
}

sub show_config {
  my($query) = @_;

  print "<H2>Here are the current settings in this form</H2>\n";
  # print $query->dump, "<p> _________________<p>\n";
  foreach my $key ($query->param) {
    print "<STRONG>$key</STRONG> -> ";
    my @values = $query->param($key);
   print join(", ",@values),"<BR>\n";
  }
  print "<p>", make_url($query);
}
