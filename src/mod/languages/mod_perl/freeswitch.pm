package fs_perl;
use Data::Dumper;

sub wazzup() {
  fs_console_log("WOOHOO!\n");
  $session = fs_core_session_locate($SWITCH_ENV{UUID});
  fs_channel_answer($session);
  fs_ivr_play_file($session, "/root/siriusraw.raw", "");
}

1;
