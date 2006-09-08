<? 
include("freeswitch.php");
$session = fs_core_session_locate($uuid);
fs_channel_answer($session);
fs_ivr_play_file2($session, "/ram/sr8k.wav");

?>
