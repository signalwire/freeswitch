
use XML::Simple;
use Data::Dumper;



my $bug = shift || die "missing bug";;
my $url = "https://freeswitch.org/jira/si/jira.issueviews:issue-xml/${bug}/${bug}.xml";
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

if ($ARGV[0] eq "edit") {
  shift;
  $auto = 0;
  open T, ">/tmp/$bug.tmp";
  print T "#resolve [$sum]\n\n";
  close T;
}

my $args = join(" ", @ARGV);
my $gitcmd;

if ($auto) {
  $gitcmd = "git commit $args -m \"#resolve [$sum]\"";
} else {
  $gitcmd = "git commit $args -t /tmp/$bug.tmp";
}

system $gitcmd;

unlink("/tmp/$bug.tmp");
