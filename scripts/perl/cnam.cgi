#!/usr/bin/perl
#
# OpenCNAM front end because they only take 10 digits and can't filter 11 on their side.
#

use Data::Dumper;
use CGI qw/:standard/;
use LWP::UserAgent;
use SDBM_File;
use Fcntl;

my %params = map { $_ => get_data( $_ ) } param;

$ua = LWP::UserAgent->new(ssl_opts => { verify_hostname => 0 });

sub get_data {
    my $name   = shift;
    my @values = param( $name );
    return @values > 1
        ? \@values
        : $values[0];
}

print "Content-Type: text/plain\n\n";

tie (my %cache, 'SDBM_File', 'cnam.dbm', O_RDWR|O_CREAT, 0640) || die $!;

my $number = $params{number};


if($number =~ m/1?\d{10}/) {


    if($number =~ m/^1(\d{10})$/) {
	$number = $1;
    }

    if($cache{"$number"}) {
	print $cache{"$number"};
	untie %cache;
	exit;
    }

    my $url = "https://api.opencnam.com/v1/phone/$number?format=text";


    my $res = $ua->get( $url );

    if ($res->is_success) {
	my $content = $res->decoded_content;
	if ($content =~ m/^Invalid/) {
	    # API shouldn't return this crap.
	    print "UNKNOWN";
	} else {
	    # Cache the entry.
	    $cache{"$number"} = $content;
	    # print the entry.
	    print $content;
	}
    }
}

untie %cache;
