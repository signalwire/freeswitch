#!/usr/bin/perl
#
# randomize-passwords.pl
#
# Randomizes the auth passwords for any file in the file spec given by the user
# Randomizes the vm passwords for the same files
# Creates a backup copy of each file altered; optionally will remove backups
# 
# This program uses only pure Perl modules so it should be portable.
#
# Michael S. Collins
# 2009-11-11
# 
# Freely contributed to the FreeSWITCH project for use as the developers and community see fit

use strict;
use warnings;
use Getopt::Long;
use File::Basename;
use File::Copy;

$|++;

## 'CHARACTERS' contains punctuation marks
use constant CHARACTERS => 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-=+?></.,!@#$%^&*();:';
my $numchars = length(CHARACTERS);

## 'ALPHACHARS' contains upper and lower case letters and digits but no punctuation
use constant ALPHACHARS => 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890';
my $numalphas = length(ALPHACHARS);

my $vmlen = 4;         # Length of VM password
my $authlen = 10;      # Length of auth password
my $filespec;          # File specification 
my $delbak;            # Flag - delete backups (default = keep backups)
my $nopunct;           # Flag - set to true to disable punction marks (i.e. alphanumerics only) in auth passwords

my $opts_ok = GetOptions ("h"         => \&usage,
                          "help"      => \&usage,
                          "vmlen=i"   => \$vmlen,
                          "authlen=i" => \$authlen,
                          "files=s"   => \$filespec,
                          "D"         => \$delbak,
			  "nopunct"   => \$nopunct,
			  );

## Confirm that a file spec was provided
if ( ! $filespec ) {
    warn "\nPlease provide a file specification.\n";
    die "Example: --files=/usr/local/freeswitch/conf/directory/default/1*.xml\n\n";
}

## Collect the files
my @FILELIST = glob($filespec);
if ( ! @FILELIST ) {
    print "\nNo files found matching this spec:\n$filespec\n";
    exit(0);
} else {
    print "\nFound " . @FILELIST . " file(s).\n\n";
}

## Iterate through the list, process each file
foreach my $file ( @FILELIST ) {
    print "Processing file: $file\n";
    my $bakfile = $file . '.bak';

    if ( move($file,$bakfile) ) {
	print "  $file ===> $bakfile\n";
    } else {
	print "  Unable to backup $file to $bakfile. Skipping...\n";
        next;
    }

    ## FILEIN is the backup file, FILEOUT is the updated file
    open(FILEIN ,'<',$bakfile) or die "Could not open $bakfile - aborting operation.\n";
    open(FILEOUT,'>',$file   ) or die "Could not open $file - aborting operation.\n";

    ## Retrieve new passwords from random generators
    my $newauth =  &get_random_chars($authlen);
    my $newvm   =  &get_random_digits($vmlen);
    
    ## Loop through "bak" file, replace passwords, write out to original file
    while(<FILEIN>) {
	## Check for passwords; if found swap
	if ( m/param name="password"/ ) {
	    # Found auth password, swap it
	    s/value="(.*?)"/value="$newauth"/;
            print "    Old/new auth pass: $1 ==> $newauth\n";
	}

	if ( m/param name="vm-password"/ ) {
	    # Found vm password, swap it
	    s/value="(.*?)"/value="$newvm"/;
            print "      Old/new vm pass: $1 ==> $newvm\n";
	}

        print FILEOUT $_;

    } ## while(<FILEIN>)

    close(FILEIN);
    close(FILEOUT);

    ## Clear out the backup file if user asked for it
    if ( $delbak ) {
        print "    Removing $bakfile...\n";
	unlink $bakfile;
    }
    print "    Finished with $file.\n\n";

} ## foreach my $file ( @FILELIST )

exit(0);

## Return random chars for auth password
sub get_random_chars () {
    my $length = shift;
    if ( ! $length ) { $length = $authlen; }
    my $chars;

    if ( $nopunct ) {
	foreach my $i (1 .. $length) {
	    my $nextchar = substr( ALPHACHARS,int(rand $numalphas),1 );
	    $chars .= $nextchar;
	}

    } else {
	foreach my $i (1 .. $length) {
	    my $nextchar = substr( CHARACTERS,int(rand $numchars),1 );
	    $chars .= $nextchar;
	}
    }
    return $chars;
}

## Return only digits for vm password
sub get_random_digits () {
    my $length = shift;
    if ( ! $length ) { $length = $vmlen; }
    my $digits;
    foreach my $i (1 .. $length) {
	my $nextdigit = int(rand 10);
        $digits .= $nextdigit;
    }
    return $digits;

}

sub usage () {
print <<END_USAGE

Randomize passwords for FreeSWITCH directory entries.

Usage:  ./randomize-passwords.pl --files=<file spec> [-D] [--vmlen=<vm pass length>] [--authlen=<auth pass length>]

Options:
  
  -h, --help       Display this help page

  -D               Delete backups (default is to save backups)

  --files          Specify files to process. Use typical file globs. On a standard Linux install it would look like:
                   --files=/usr/local/freeswitch/conf/directory/default/1*.xml

  --vmlen          Set length of voice mail password. (Default is 4 digits)

  --authlen        Set length of auth password. (Default is 10 characters)

  --nopunct        Disable punction marks in auth passwords, i.e. alphanumerics only

Example:
  To randomize all the passwords for a default Linux install, with 6 digit VM passwords, use this command:

  ./randomize-passwords.pl --files=/usr/local/freeswitch/conf/directory/default/1*.xml -D --vmlen=6

END_USAGE
;
exit(0);
}
