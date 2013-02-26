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

$ua = LWP::UserAgent->new(ssl_opts => { verify_hostname => 0, timeout => 3 });

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
    my $code = $res->code;

    if ($code eq '200') {
	my $content = $res->decoded_content;
	$cache{"$number"} = $content;
	print $content;
    } else {
	print "UNKNOWN";
    }
}

untie %cache;
