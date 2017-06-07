#!/usr/bin/perl

use XML::Simple;
use Data::Dumper;
use Getopt::Long qw(GetOptions);
use Term::ReadKey;
use JIRA::REST;

my %opts;

sub getpass {
  ReadMode( "noecho");
  print "Password: ";
  chomp (my $pwd = <STDIN>);
  ReadMode ("original");
  return $pwd;
}

sub getfield {
    my $prompt = shift;
    my $default = shift;

    print $prompt . ($default ? "[$default]: " : "");
    chomp (my $data = <STDIN>);
    
    if (!$data) {
	$data = $default;
    }

    return $data;
}

GetOptions(
    'bug=s' => \$opts{bug},
    'msg=s' => \$opts{msg},
    'user=s' => \$opts{user},
    'pass=s' => \$opts{pass},
    'debug' => \$opts{debug},
    'noresolve' => \$opts{noresolve},
    'append=s' => \$opts{append},
    'comment=s' => \$opts{comment},
    'versions=s' => \$opts{versions},
    'author=s' => \$opts{author},
    'auth' => \$opts{auth}
    ) or die "Usage: $0 -bug <bug-id> [--auth] [-m [edit|<msg>]] [--append <msg>] [--debug] <files>\n";


$opts{bug} or $opts{bug} = shift;

if ($opts{versions}) {
    $opts{auth} = 1;
    $opts{versions_array} = [map {{name => $_}} split(" ", $opts{versions})];
}

my $url = "https://freeswitch.org/jira/si/jira.issueviews:issue-xml/$opts{bug}/$opts{bug}.xml";
my $cmd;
my $prog = `which curl` || `which wget`;
my $auto = 1;
my $post = " \#resolve";
my $component;
my $summary;

chomp $prog;

if ($opts{auth}) {
    if (!$opts{user}) {
	$opts{user} = getfield("User: ");
    }
    
    if (!$opts{pass}) {
	$opts{pass} = getpass();
	print "\n";
    }

    $jira = JIRA::REST->new('https://freeswitch.org/jira', $opts{user}, $opts{pass}) or die "login incorrect:";
    $issue = $jira->GET("/issue/FS-7985") or die "login incorrect:";

    my $issue = $jira->GET("/issue/" . $opts{bug});

    $component = join(",", map {$_->{name}} @{$issue->{fields}->{components}});
    $summary = $issue->{fields}->{summary};


    if ($opts{versions_array}) {
	$input = {

	    update => {
		fixVersions => [
		    {set => $opts{versions_array}}
		    ]
	    }
	};
	
	$jira->PUT("/issue/" . $opts{bug}, undef, $input);
    }

} else {
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
    $summary = $r->{channel}->{item}->{summary};
    $summary =~ s/\"/\\"/g;

    $component = $r->{channel}->{item}->{component};

    if(ref($component) eq 'ARRAY') {
	$component = join(",", @{$component});
    }

    $component =~ s/\"/\\"/g;
}

if ($opts{noresolve}) {
    $post = "";
}


if ($opts{msg} eq "edit") {
  $auto = 0;
  $opts{msg} = undef;
  open T, ">/tmp/$opts{bug}.tmp";
  print T "$opts{bug}${post} [$summary]\n\n---Cut this line to confirm commit.....";
  close T;
}

my $args = join(" ", @ARGV);
my $gitcmd;

if ($opts{append}) {
    $opts{append} = " -- " . $opts{append};
}

if ($opts{comment}) {
    $opts{append} .= " #comment " . $opts{comment};
}

if ($auto) {
    if ($opts{msg}) {
	$opts{msg} =~ s/%s/$summary/;
	$opts{msg} =~ s/%b/$opts{bug}/;
	$opts{msg} =~ s/%c/$component/;
	$gitcmd = "git commit $args -m \"$opts{msg}$opts{append}\"";
    } else {
	$gitcmd = "git commit $args -m \"$opts{bug}: [$component] ${summary}$opts{append}${post}\"";
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
