<?

include("freeswitch.php");

fs_core_set_globals();
fs_core_init("");
fs_loadable_module_init();
fs_console_loop();
fs_core_destroy();
?>