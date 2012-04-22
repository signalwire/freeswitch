# Example Perl script to originate. perlrun
freeswitch::console_log("info", "Perl in da house!!!\n");

$session = new freeswitch::Session("sofia/10.0.1.100/1002") ;
$session->execute("playback", "/sr8k.wav");
$session->hangup();
