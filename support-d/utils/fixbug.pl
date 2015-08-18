#!/usr/bin/perl

use XML::Simple;
use Data::Dumper;
use Getopt::Long qw(GetOptions);

my %opts;

GetOptions(
    'bug=s' => \$opts{bug},
    'msg=s' => \$opts{msg}
    ) or die "Usage: $0 -bug <bug-id> [-m [edit|<msg>]] <files>\n";


$opts{bug} || die "missing bug";;
my $url = "https://freeswitch.org/jira/si/jira.issueviews:issue-xml/$opts{bug}/$opts{bug}.xml";
my $cmd;
my $prog = `which curl` || `which wget`;
my $auto = 1;

chomp $prog;

$prog || die "missing url fetch program, install curl or wget";

if ($prog =~ /wget/) {
  $cmd = "$prog -O -";
} else {
  $cmd = $prog;
}

my $xml = `$cmd $url 2>/dev/null`;

my $xs= new XML::Simple;
my $r = $xs->XMLin($xml);

my $sum = $r->{channel}->{item}->{summary};

if ($opts{msg} eq "edit") {
  $auto = 0;
  $opts{msg} = undef;
  open T, ">/tmp/$opts{bug}.tmp";
  print T "$opts{bug} #resolve [$sum]\n\n";
  close T;
}

my $args = join(" ", @ARGV);
my $gitcmd;

if ($auto) {
    if ($opts{msg}) {
	$opts{msg} =~ s/%s/$sum/;
	$opts{msg} =~ s/%b/$bug/;
	$gitcmd = "git commit $args -m \"$opts{msg}\"";
    } else {
	$gitcmd = "git commit $args -m \"$opts{bug} #resolve [$sum]\"";
    }
} else {
  $gitcmd = "git commit $args -t /tmp/$opts{bug}.tmp";
}

system $gitcmd;

unlink("/tmp/$opts{bug}.tmp");
