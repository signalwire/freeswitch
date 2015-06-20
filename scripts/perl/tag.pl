#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long;
use Data::Dumper;


my ($title, $artist, $file, $volume);

GetOptions(
	   "title=s" => \$title,
	   "artist=s" => \$artist,
	   "file=s" => \$file,
	   "volume=s" => \$volume
	  ) or die $@;

if (-f $file) {
  my $tmp = $$;
  if ($volume) {
    system("avconv -i \"$file\" -vcodec copy -af \"volume=$volume\" /tmp/file$tmp.mp4");
    system("mv /tmp/file$tmp.mp4 \"$file\"");
  }
  if ($title && $artist) {
    system("avconv -i \"$file\" -metadata artist=\"$artist\" -metadata title=\"$title\" -codec copy -vcodec copy /tmp/file$tmp.mp4");
    system("mv /tmp/file$tmp.mp4 \"$file\"");
  }

} else {
  print "$file not found.\n";
}

