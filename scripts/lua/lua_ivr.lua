-- lua_ivr.lua
--
-- This script is virtually identical to the demo_ivr defined in conf/autoload_configs/ivr.conf.xml
-- It uses the same sound files and mostly the same settings
-- It is intended to be used as an example of how you can use Lua to create dynamic IVRs
--

-- This hash defines the main IVR menu. It is equivalent to the <menu name="demo_ivr" ... > lines in ivr.conf.xml
ivr_def = {
   ["main"] = undef,
   ["name"] = "demo_ivr_lua",
   ["greet_long"] = "phrase:demo_ivr_main_menu",
   ["greet_short"] = "phrase:demo_ivr_main_menu_short",
   ["invalid_sound"] = "ivr/ivr-that_was_an_invalid_entry.wav",
   ["exit_sound"] = "voicemail/vm-goodbye.wav",
   ["confirm_macro"] = "",
   ["confirm_key"] = "",
   ["tts_engine"] = "flite",
   ["tts_voice"] = "rms",
   ["confirm_attempts"] = "3",
   ["inter_digit_timeout"] = "2000",
   ["digit_len"] = "4",
   ["timeout"] = "10000",
   ["max_failures"] = "3",
   ["max_timeouts"] = "2"
}

-- top is an object of class IVRMenu
-- pass in all 16 args to the constructor to define a new IVRMenu object
top = freeswitch.IVRMenu(
   ivr_def["main"],
   ivr_def["name"],
   ivr_def["greet_long"],
   ivr_def["greet_short"],
   ivr_def["invalid_sound"],
   ivr_def["exit_sound"],
   ivr_def["confirm_macro"],
   ivr_def["confirm_key"],
   ivr_def["tts_engine"],
   ivr_def["tts_voice"],
   ivr_def["confirm_attempts"],
   ivr_def["inter_digit_timeout"],
   ivr_def["digit_len"],
   ivr_def["timeout"],
   ivr_def["max_failures"],
   ivr_def["max_timeouts"]
);

-- bindAction args = action, param, digits
-- The following bindAction line is the equivalent of this XML from demo_ivr in ivr.conf.xml
-- <entry action="menu-exec-app" digits="2" param="transfer 9996 XML default"/>
top:bindAction("menu-exec-app", "transfer 9996 XML default", "2");
top:bindAction("menu-exec-app", "transfer 9999 XML default", "3");
top:bindAction("menu-exec-app", "transfer 9991 XML default", "4");
top:bindAction("menu-exec-app", "bridge sofia/${domain}/888@conference.freeswitch.org", "1");
top:bindAction("menu-exec-app", "transfer 1234*256 enum", "5");
top:bindAction("menu-sub", "demo_ivr_submenu","6");
top:bindAction("menu-exec-app", "transfer $1 XML features", "/^(10[01][0-9])$/");
top:bindAction("menu-top", "demo_ivr_lua","9");

-- This hash defines the main IVR sub-menu. It is equivalent to the <menu name="demo_ivr_submenu" ... > lines in ivr.conf.xml
ivr_sub_def = {
   ["main"] = undef,
   ["name"] = "demo_ivr_submenu_lua",
   ["greet_long"] = "phrase:demo_ivr_sub_menu",
   ["greet_short"] = "phrase:demo_ivr_main_sub_menu_short",
   ["invalid_sound"] = "ivr/ivr-that_was_an_invalid_entry.wav",
   ["exit_sound"] = "voicemail/vm-goodbye.wav",
   ["confirm_macro"] = "",
   ["confirm_key"] = "",
   ["tts_engine"] = "flite",
   ["tts_voice"] = "rms",
   ["confirm_attempts"] = "3",
   ["inter_digit_timeout"] = "2000",
   ["digit_len"] = "4",
   ["timeout"] = "15000",
   ["max_failures"] = "3",
   ["max_timeouts"] = "2"
}

-- sub_menu is an object of class IVRMenu
-- pass in all 16 args to the constructor to define a new IVRMenu object
sub_menu = freeswitch.IVRMenu(
   ivr_sub_def["main"],
   ivr_sub_def["name"],
   ivr_sub_def["greet_long"],
   ivr_sub_def["greet_short"],
   ivr_sub_def["invalid_sound"],
   ivr_sub_def["exit_sound"],
   ivr_sub_def["confirm_macro"],
   ivr_sub_def["confirm_key"],
   ivr_sub_def["tts_engine"],
   ivr_sub_def["tts_voice"],
   ivr_sub_def["confirm_attempts"],
   ivr_sub_def["inter_digit_timeout"],
   ivr_sub_def["digit_len"],
   ivr_sub_def["timeout"],
   ivr_sub_def["max_failures"],
   ivr_sub_def["max_timeouts"]
);

-- Bind the action "menu-top" to the * key
sub_menu:bindAction("menu-top","demo_ivr_lua","*");
--sub_menu:execute(session,"demo_ivr_submenu_lua");

-- Run the main menu
top:execute(session, "demo_ivr_lua");
