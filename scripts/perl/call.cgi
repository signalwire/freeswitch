#!/usr/bin/perl
use CGI qw(:standard);
use ESL;

my $gateway = "sofia/gateway/my_gateway/";
my $aleg_cid_num = "0987654321";
my $bleg_cid_num = "1234567890";

my $q = new CGI; 
my $c = new ESL::ESLconnection("localhost", "8021", "ClueCon");
print $q->header;

if($q->param()) {
  my $numbera = $q->param('numbera');
  my $numberb = $q->param('numberb');
  my $e = $c->sendRecv("api originate {ignore_early_media=true,origination_caller_id_number=$aleg_cid_num,effective_caller_id_number=$bleg_cid_num}$gateway$numbera &bridge($gateway$numberb)");
  print "API Sent:" . $e->getBody();
  exit;
}

print $q->start_form, $q->h1("Click to Call Example"), br,
  "First Party:", $q->textfield('numbera'), br,
  "Second Party:", $q->textfield('numberb'),br,
  $q->submit, $q->end_form;

