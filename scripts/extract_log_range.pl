#!/usr/bin/perl
use Time::Local;

my $file = shift;
my $start = shift;
my $stop = shift;


sub parse_date($) {
  my $str = shift;

  if (my ($yr, $mo, $day, $hr, $min, $sec) = $str =~ /(\d{4})\-(\d{2})\-(\d{2}) (\d{2})\:(\d{2})\:(\d{2})/) {
    return timelocal($sec, $min, $hr, $day - 1, $mo - 1, $yr);
  } else {
    die $str;
  }
}

if ($start =~ /\:/) {
  $start = parse_date($start);
}

if ($stop =~ /\:/) {
  $stop = parse_date($stop);
} elsif ($stop =~ /^\+(\d+)/) {
  $stop = $start + $1;
}

open(I, $file);
	       
while (<I>) {
  my $str = $_;
  $epoch = parse_date($str);
  
  if ($epoch > $start) {
    if ($stop && $epoch > $stop) {
      last;
    }
    print;
  }
}

close(I);
