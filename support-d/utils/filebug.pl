#!/usr/bin/perl
#use strict;

use Getopt::Long qw(GetOptions);
use Term::ReadKey;
use JIRA::REST;
use Data::Dumper;

sub getpass {
  ReadMode( "noecho");
  print "Password: ";
  chomp (my $pwd = <>);
  ReadMode ("original");
  return $pwd;
}

sub getuser {
  print "User: ";
  chomp (my $usr = <>);
  return $usr;
}

sub get_text {
  my @chars = ("A".."Z", "a".."z");
  my $string;
  $string .= $chars[rand @chars] for 1..8;

  my $editor = $ENV{"EDITOR"} || $ENV{"VISUAL"} || `which emacs` || `which vi`;

  system("$editor /tmp/TEXT.$string");
  my $text = `cat /tmp/TEXT.$string`;
  unlink("/tmp/TEXT.$string");
  return $text;
}

my %opts;

my $hashtxt = `git log -1 --oneline 2>/dev/null`;
my ($hash) = split(" ", $hashtxt);

GetOptions(
	   'summary=s' => \$opts{summary},
	   'desc=s' => \$opts{desc},
	   'components=s' => \$opts{components},
	   'hash=s' => \$opts{hash},
	   'user=s' => \$opts{user},
	   'pass=s' => \$opts{pass},
	   'type=s' => \$opts{type},
	  ) or die "Usage: $0 -summary <summary> -desc <desc> ....\n";


if ($opts{components}) {
  $opts{components_array} = [map {{name => $_}} split(" ", $opts{components})];
} else {
  $opts{components_array} = [map {{name => $_}} qw(freeswitch-core)];
}


#print Dumper \%opts;
#exit;

if (!$opts{user}) {
  $opts{user} = getuser();
}

if (!$opts{pass}) {
  $opts{pass} = getpass();
}

my $jira = JIRA::REST->new('https://freeswitch.org/jira', $opts{user}, $opts{pass}) or die "login incorrect:";
my $issue = $jira->GET("/issue/FS-7985") or die "login incorrect:";
#print $issue->{key};
#exit;

if (!$opts{type}) {
  $opts{type} = "Bug";
}

if (!$opts{summary}) {
  die "missing summary:";
}

if (!$opts{desc}) {
  $opts{desc} = get_text();

  if (!$opts{desc}) {
    die "missing desc:";
  }
}

if (!$opts{hash}) {
  $opts{hash} = $hash;

  if (!$opts{hash}) {
    $opts{hash} = "N/A";
  }
}



my $issue = $jira->POST('/issue', undef, 
			{ 
			    fields => {
				project   => { key => 'FS' },
				issuetype => { name => $opts{type} },
				summary   => $opts{summary},
				description => $opts{desc},
				customfield_10024 => $opts{hash},
				customfield_10025 => $opts{hash},
				components => $opts{components_array}
			    },
			}) or die "Issue was not created:";


print "Issue Posted: " . $issue->{key};


__END__

my $jira = JIRA::REST->new('https://freeswitch.org/jira', $user, $pass);

#$issue = $jira->GET("/issue/FS-7985");
#print Dumper $issue;

