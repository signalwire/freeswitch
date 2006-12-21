#!/usr/bin/perl

use strict;
use Getopt::Long;
use File::Temp;

my $cn;
my @dns = ();
my $cafile = "root.pem";
my $prefix = "agent";
my $rand = "tls_seed.dat";
my $help = 0;

GetOptions('help' => \$help,
	   'cn=s' => \$cn, 
	   'dns=s' => \@dns, 
	   'ca=s' => \$cafile,
	   'prefix=s' => \$prefix,
           'rand=s' => \$rand);

@dns = split(/,/,join(',',@dns));

if ($help || !$cn || !@dns) {
  print "Usage: make_node_cert -cn <common name>\n".
        "                      -dns <comma separated list of dns names>\n".
        "                     [-ca cafile (default root.pem)]\n".
        "                     [-prefix prefix (default agent)]\n".
	"                     [-rand <random seed file>]\n";
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
basicConstraints=CA:FALSE
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer:always
subjectAltName=$dnsstring
keyUsage=digitalSignature:TRUE,keyEncipherment:TRUE
EOF

system("openssl req -newkey rsa -nodes -keyout ${prefix}key.pem -sha1 -out ${prefix}req.pem -config $filename -rand $rand");

system("openssl x509 -req -in ${prefix}req.pem -sha1 -extensions ext -CA $cafile -CAkey $cafile -out ${prefix}cert.pem -CAcreateserial -extfile $filename");

system("cat ${prefix}cert.pem ${prefix}key.pem >${prefix}.pem");
