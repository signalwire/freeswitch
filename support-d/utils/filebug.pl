#!/usr/bin/perl
#use strict;
use Getopt::Long qw(GetOptions);
use Term::ReadKey;
use JIRA::REST;
use Data::Dumper;

my $editor = $ENV{"EDITOR"} || $ENV{"VISUAL"} || `which emacs` || `which vi`;
my $default_versions = "1.7 1.6";
my $default_components = "freeswitch-core";



sub getpass {
  ReadMode( "noecho");
  print "Password: ";
  chomp (my $pwd = <>);
  ReadMode ("original");
  return $pwd;
}

sub getfield {
    my $prompt = shift;
    my $default = shift;

    print $prompt . ($default ? "[$default]: " : "");
    chomp (my $data = <>);
    if (!$data) {
	$data = $default;
    }
    return $data;
}

sub get_text {
    my $text = shift;

    my @chars = ("A".."Z", "a".."z");
    my $string;
    $string .= $chars[rand @chars] for 1..8;
    
    if ($text) {
	open O, ">/tmp/TEXT.$string";
	print O $text;
	close O;
    }

    system("$editor /tmp/TEXT.$string");
    my $newtext = `cat /tmp/TEXT.$string`;
    unlink("/tmp/TEXT.$string");
    return $newtext;
}

my %opts;

my $hashtxt = `git log -1 --oneline 2>/dev/null`;
my ($hash) = split(" ", $hashtxt);

GetOptions(
    'project=s' => \$opts{project},
    'summary=s' => \$opts{summary},
    'desc=s' => \$opts{desc},
    'components=s' => \$opts{components},
    'hash=s' => \$opts{hash},
    'user=s' => \$opts{user},
    'pass=s' => \$opts{pass},
    'type=s' => \$opts{type},
    'versions=s' => \$opts{versions},
    'noedit' => \$opts{noedit},
    'askall' => \$opts{askall},
    'debug' => \$opts{debug},
    ) or die "Usage: $0 -summary <summary> -desc <desc> [-debug] ....\n";


$opts{project} or $opts{project} = "FS";

if ($opts{versions}) {
    $opts{versions_array} = [map {{name => $_}} split(" ", $opts{versions})];
} else {
    $opts{versions_array} = [map {{name => $_}} ($default_versions)];
    $opts{versions} = $default_versions;;
}

if ($opts{components}) {
    $opts{components_array} = [map {{name => $_}} split(" ", $opts{components})];
} else {
    $opts{components_array} = [map {{name => $_}} ($default_components)];
    $opts{components} = $default_components;
}

if (!$opts{user}) {
  $opts{user} = getfield("User: ");
}

if (!$opts{pass} && !$opts{debug}) {
  $opts{pass} = getpass();
  print "\n";
}

my $jira;
my $issue;

if (!$opts{debug}) {
    $jira = JIRA::REST->new('https://freeswitch.org/jira', $opts{user}, $opts{pass}) or die "login incorrect:";
    $issue = $jira->GET("/issue/FS-7985") or die "login incorrect:";
}

#print $issue->{key};
#exit;

if (!$opts{type}) {
  $opts{type} = "Bug";
}

if (!$opts{hash}) {
    $opts{hash} = $hash;

    if (!$opts{hash}) {
	$opts{hash} = "N/A";
    }
}

if ($opts{askall}) {
    $opts{project} = getfield("Project: ", $opts{project});
    $opts{type} = getfield("Type: ", $opts{type});
    $opts{versions} = getfield("Versions: ", $opts{versions});
    $opts{versions_array} = [map {{name => $_}} split(" ", $opts{versions})];
    $opts{summary} = getfield("Summary: ", $opts{summary});
    $opts{components} = getfield("Components: ", $opts{components});
    $opts{components_array} = [map {{name => $_}} split(" ", $opts{components})];
    $opts{hash} = getfield("GIT Hash: ", $opts{hash});

    if ($opts{noedit}) {
	$opts{desc} = getfield("Description: ", $opts{desc});
    } else {
	$opts{desc} = get_text($opts{desc});
    }
}

if (!$opts{desc}) {
    if ($opts{noedit}) {
	$opts{desc} = getfield("Description: ", $opts{desc});
    } else {
	$opts{desc} = get_text($opts{desc});
    }

    if (!$opts{desc}) {
	die "missing desc:";
    }
}

if (!$opts{summary}) {
    $opts{summary} = getfield("Summary: ", $opts{summary});
    if (!$opts{summary}) {
	die "Summary is mandatory.";
    }
}

my $input = { 
    fields => {
	project   => { key => $opts{project} },
	issuetype => { name => $opts{type} },
	summary   => $opts{summary},
	description => $opts{desc},
	customfield_10024 => $opts{hash},
	customfield_10025 => $opts{hash},
	components => $opts{components_array},
	versions => $opts{versions_array}
    },
};

if ($opts{debug}) {
    print Dumper \%opts;
    print Dumper $input;
} else {
    $issue = $jira->POST('/issue', undef, $input) or die "Issue was not created:";
    print "Issue Posted: " . $issue->{key};
}

