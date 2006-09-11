<? 
/*

  This application does not fall under the MPL.  Its 100% freeware
  no code needs to be submitted back to us for this.
  (c)2006 Brian Fertig 

*/
require("classFreeswitch.php");

echo "uuid: $uuid\n";

$fs = new fs_class_api();

$fs->fs_answer();

$fs->fs_play_file("/ram/sr8k.wav");

?>
