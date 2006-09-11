<? 
/*

  This application does not fall under the MPL.  Its 100% freeware
  no code needs to be submitted back to us for this.
  (c)2006 Brian Fertig 

*/
require("classFreeswitch.php");

$fs = new fs_class_api($uuid); // $uuid MUST be there for the class to work.

$fs->fs_answer();

//$fs->fs_play_file("/ram/sr8k.wav");

?>
