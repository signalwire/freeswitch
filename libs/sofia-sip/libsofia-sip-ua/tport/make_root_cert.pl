#!/usr/bin/perl

use strict;
use Getopt::Long;
use File::Temp;

my $cn;
my @dns = ();
my $prefix = "root";
my $rand = "tls_seed.dat";
my $help = 0;

GetOptions('help' => \$help,
	   'cn=s' => \$cn,
	   'dns=s' => \@dns,
	   'prefix=s' => \$prefix,
           'rand=s' => \$rand);

@dns = split(/,/,join(',',@dns));

if ($help || !$cn || !@dns) {
  print "Usage: make_root_cert -cn <common name>\n".
        "                      -dns <comma separated list of dns names>\n". 
	"                     [-prefix <name prefix>]\n".
	"                     [-rand <random seed file>\n]";
  exit 0;
}

$_ = "DNS:$_" for @dns;

my $dnsstring = join(',', @dns);

my ($fh, $filename) = File::Temp::tempfile(UNLINK => 1);

print $fh <<"EOF";
[ req ]
default_bits		= 1024
prompt                  = no
distinguished_name	= req_dn

[ req_dn ]
commonName		= $cn

[ ext ]
basicConstraints = CA:TRUE
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer:always
subjectAltName=$dnsstring
EOF

system("openssl req -newkey rsa -nodes -keyout ${prefix}key.pem -sha1 -out ${prefix}req.pem -config $filename -rand $rand");

system("openssl x509 -req -in ${prefix}req.pem -sha1 -extensions ext -signkey ${prefix}key.pem -out ${prefix}cert.pem -extfile $filename");

system("cat ${prefix}cert.pem ${prefix}key.pem >${prefix}.pem");

