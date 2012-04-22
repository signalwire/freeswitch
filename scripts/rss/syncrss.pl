#!/usr/bin/perl

my $home = shift;

$home or die "Usage: $0 <dir>\n";
chdir($home);

my $hash = {
	    'freeswitch.rss' => 'http://www.freeswitch.org/xml.php'
	   };


foreach (keys %{$hash}) {
  my ($dl) = $hash->{$_} =~ /\/([^\/]+)$/;

  my $cmd = "rm -f $_ $dl ; wget $hash->{$_}";
  if ($dl ne $_) {
    $cmd .= "; /bin/mv $dl $_";
  }
  $cmd .= "\n";
  print $cmd;
  system($cmd);
}
