#!/usr/bin/perl

use XML::Simple;
use Data::Dumper;
use Getopt::Long qw(GetOptions);

my %opts;

GetOptions(
    'bug=s' => \$opts{bug},
    'msg=s' => \$opts{msg},
    'debug' => \$opts{debug},
    'append=s' => \$opts{append},
    'comment=s' => \$opts{comment},
    'author=s' => \$opts{author}
    ) or die "Usage: $0 -bug <bug-id> [-m [edit|<msg>]] [-append <msg>] [-debug] <files>\n";


$opts{bug} or $opts{bug} = shift;

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
if ($opts{debug}) {
    print "URL $url\n";
    print $xml;
}

my $xs= new XML::Simple;
my $r = $xs->XMLin($xml);

my $sum = $r->{channel}->{item}->{summary};
$sum =~ s/\"/\\"/g;

if ($opts{msg} eq "edit") {
  $auto = 0;
  $opts{msg} = undef;
  open T, ">/tmp/$opts{bug}.tmp";
  print T "$opts{bug} #resolve [$sum]\n\n---Cut this line to confirm commit.....";
  close T;
}

my $args = join(" ", @ARGV);
my $gitcmd;

if ($opts{append}) {
    $opts{append} = " " . $opts{append};
}

if ($opts{comment}) {
    $opts{append} .= " #comment " . $opts{comment};
}

if ($auto) {
    if ($opts{msg}) {
	$opts{msg} =~ s/%s/$sum/;
	$opts{msg} =~ s/%b/$opts{bug}/;
	$gitcmd = "git commit $args -m \"$opts{msg}$opts{append}\"";
    } else {
	$gitcmd = "git commit $args -m \"$opts{bug} #resolve [$sum]$opts{append}\"";
    }
} else {
  $gitcmd = "git commit $args -t /tmp/$opts{bug}.tmp";
}

if ($opts{author}) {
    $gitcmd .= " --author \"$opts{author}\"";
}

if ($opts{debug}) {
    print "CMD: $gitcmd\n";
} else {
    system $gitcmd;
}

unlink("/tmp/$opts{bug}.tmp");
