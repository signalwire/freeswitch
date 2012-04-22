# This is an example of sending an event via perlrun from the cli
# Edit to your liking.  perlrun mwi_event.pl
freeswitch::console_log("info", "Perl in da house!!!\n");

$event = new freeswitch::Event("message_waiting");
$event->addHeader("MWI-Messages-Waiting", "yes");
$event->addHeader("MWI-Message-Account", 'sip:1002@10.0.1.100');

#$event->addHeader("Sofia-Profile", 'internal');

$event->fire();
