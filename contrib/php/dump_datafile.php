<?PHP
 /**********************************************************************
  * $Id: dump_datafile.php,v 1.2 2003/07/26 01:05:42 russell Exp $
  *
  * Author: Jerry Wilborn <jerry.wilborn@corp.fast.net>
  *         Network Operations Analyst
  *         FASTNET - Internet Solutions
  *         610-266-6700
  *         www.fast.net
  *
  * 2003 24 Jul
  *
  * PHP Code to Read SNIPS Data Files
  *
  *
  * Modification History:
  *  2003 25 Jul - Russell M. Van Tassell <russell@loosenut.com>
  *    - added this header and user-settable variables
  *    - cleaned up code for readability on an 80 column screen
  *    - renamed some long variable names (for readability)
  *    - Added simple comments
  *    - fixed some warnings (against PHP 4.3.2 and SNIPS 1.2b2)
  *
  *********************************************************************/

//
// The data file we'll open...
//$DATA_FILE = "./data/ippingmon-output";
$DATA_FILE = "/usr/local/nocol/data/ippingmon-output";

//
// You should not need to edit anything beyond this line...
//----------------------------------------------------------------------


// Open the data file for reading, assign it the filehandle "fh."
if ( ! $fh = fopen( "${DATA_FILE}", "r" ) ){
  die( "DIE: Could not open file.\n" );
}

// Just read the header in to a junk variable...
$header = string_unpack( $fh, "a*", 372 );

// Prime eventNo so we can increment this later...
$eventNo = 0;

//
// Read through file until EOF...
while ( ! feof( $fh ) ){

  // Increment the event/line number... make sure it exists first...
  ++$eventNo;

  $event[$eventNo]['sender'] = string_unpack( $fh, "a*", 16 );

  // Make sure we were able to unpack a "sender" as a "valid event"
  if ( ! $event[$eventNo]['sender']) {
    unset( $event[$eventNo] );
    continue;
  }


  //
  // Create an array of associative arrays, based on contents of the file
  $event[$eventNo]['device_name']    = string_unpack( $fh, "a*",  64 );
  $event[$eventNo]['device_addr']    = string_unpack( $fh, "a*", 128 );
  $event[$eventNo]['device_subaddr'] = string_unpack( $fh, "a*",  64 );

  $event[$eventNo]['var']            = string_unpack( $fh, "a*",  64 );
  $event[$eventNo]['value']          = string_unpack( $fh,  "L",   4 );
  $event[$eventNo]['threshold']      = string_unpack( $fh,  "L",   4 );
  $event[$eventNo]['var_units']      = string_unpack( $fh, "a*",   8 );

  $event[$eventNo]['severity']       = ord( fread( $fh, 1 ) );
  $event[$eventNo]['loglevel']       = ord( fread( $fh, 1 ) );
  $event[$eventNo]['state']          = ord( fread( $fh, 1 ) );
  $event[$eventNo]['rating']         = ord( fread( $fh, 1 ) );

  $event[$eventNo]['eventtime']      = string_unpack( $fh,  "L",   4 );
  $event[$eventNo]['polltime']       = string_unpack( $fh,  "L",   4 );

  $event[$eventNo]['op']             = string_unpack( $fh,  "L",   4 );
  $event[$eventNo]['id']             = string_unpack( $fh,  "L",   4 );
}

// Format output to be read using a browser...
print( "\n<PRE>\n" );
var_dump( $event );
print( "</PRE>\n" );


//----------------------------------------------------------------------
//

function
string_unpack( $fh, $method, $length )

//
// Unpack the data in to something more user-friendly...
//

{
  return implode( "", unpack( $method, fread( $fh, $length ) ) );
}

//----------------------------------------------------------------------
?>
