<? 
/*

  This application does not fall under the MPL.  Its 100% freeware
  no code needs to be submitted back to us for this.
  (c)2006 Brian Fertig 

*/
require("classFreeswitch.php");

$fs = new fs_class_api;

$fs->fs_answer($session);

$fs->fs_play_file($session, "/ram/sr8k.wav");

?>
