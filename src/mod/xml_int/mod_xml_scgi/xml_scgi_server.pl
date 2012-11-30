#
# Copyright (c) 2012-2013, Anthony Minessale II
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# * Neither the name of the original author; nor the names of any contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
# 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

### EXAMPLE SERVER SIDE FOR mod_xml_SCGI
### You will need the SCGI module from CPAN

use SCGI;
use CGI;
use IO::Socket;
use Data::Dumper;

my $socket = IO::Socket::INET->new(Listen => 5, ReuseAddr => 1, LocalPort => 8080)
  or die "cannot bind to port 8080: $!";

my $scgi = SCGI->new($socket, blocking => 1);


my $xml = qq#
<document type="freeswitch/xml">
  <section name="dialplan">
   <context name="default">
     <extension name="foo">
       <condition>
         <action application="answer"/>
         <action application="playback" data="/ram/swimp.raw"/>
       </condition>
     </extension>
   </context>

  </section>
</document>
#;

$SIG{CHLD} = "IGNORE";

while (my $request = $scgi->accept) {
  # fork every new req into its own process (optional)
  my $pid = switch_fork();

  if ($pid) {
    $request->close();
    next;
  }

  my $handle = $request->connection;

  $request->read_env;

  # get the body that contains the PARAMS
  read $handle, $body, $request->env->{CONTENT_LENGTH};

  # Dump SCGI HEADERS
  print Dumper $request->env;

  # Create a CGI parser on the PARAMS
  my $cgi = CGI->new($body);
  my %params = $cgi->Vars();
  # might be big output
  print Dumper \%params;


  ### DO something CGI-like here with %params ... I'm just going to return static xml

  # header is not necessary but optional
  #print $handle "Content-Type: text/xml\n\n";

  print $handle $xml;

  exit unless $pid;
}



