#!/usr/bin/perl


my $start = 8;
my $i = $start;
my $step = "4";
my $wr = 0;

  printf "%s %0.4d START    $list[4]\n", " " x $i, $i;

while(<>) {
  my $sub = 0;
  my $indent = 0;

  next unless /ERR/;

  @list = split;

  if ($list[9] eq "ACQUIRED") {

    if ($list[7] eq "Read") {
      $mark = "READLOCK ";
      $i += $step;
      $indent = $i;
    } else {
      $mark = "WRITELOCK";
      $wr = 1;
      $indent = 0;
    }

  } elsif($list[9] eq "CLEARED") {
    if ($wr && $i <= $start) {
      $mark = "WRCLEARED";
      $indent = 0;
    } else {
      $sub = $step;
      $mark = "CLEARED  ";
      $indent = $i;
    }
  } elsif($list[9] eq "FAIL") {
    $mark = "FAIL     ";
    $indent = $i;
  }

  printf "%s %0.4d $mark $list[4]\n", " " x $indent, $indent;

  if ($sub) {
    $i -= $sub;
    $sub = 0;
    print "\n";
  }


}
