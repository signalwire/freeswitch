#!/usr/bin/perl
#
# Make bulk gateway xml from csv file.
#

open(CSV, "gateways.csv");
my @data = <CSV>;
close(CSV);

foreach my $line (@data) {
  chomp($line);
  my ($gwname, $username, $password) = split(/,/, $line);
  print <<XML;
  <gateway name="$gwname">
  <param name="username" value="$username"/>
  <!--<param name="realm" value="asterlink.com"/>-->
  <!--<param name="from-user" value="cluecon"/>-->
  <!--<param name="from-domain" value="asterlink.com"/>-->
  <param name="password" value="$password"/>
  <!--<param name="extension" value="cluecon"/>-->
  <!--<param name="proxy" value="asterlink.com"/>-->
  <!--<param name="register-proxy" value="mysbc.com"/>-->
  <!--<param name="expire-seconds" value="60"/>-->
  <!--<param name="register" value="false"/>-->
  <!--<param name="register-transport" value="udp"/>-->
  <!--<param name="retry-seconds" value="30"/>-->
  <!--<param name="caller-id-in-from" value="false"/>-->
  <!--<param name="contact-params" value=""/>-->
  <!--<param name="extension-in-contact" value="true"/>-->
  <!--<param name="ping" value="25"/>-->
  <!--<param name="cid-type" value="rpid"/>-->
  <!--<param name="rfc-5626" value="true"/>-->
  <!--<param name="reg-id" value="1"/>-->
  </gateway>
XML

}
