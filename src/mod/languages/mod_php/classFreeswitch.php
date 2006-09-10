 <?
/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian Fertig <brian.fertig@convergencetek.com>
 *		IRC: docelmo
 *
 * php_freeswitch.c -- PHP Module Framework
 *
 */
require("freeswitch.php");  // Required for freeswitch driver to be loaded
global $sessn;
class fs_class_api {
    	function fs_class_api() {
		if($sessn = $this->fs_getsessn($uuid)){}
		else{
		   echo "Couldnt get sessn!\n";
		}
	
	}
	
	function fs_start() {
		// only use this function if you plan to start freeswitch in your script.
		fs_core_set_globals();
		fs_core_init("");
		fs_loadable_module_init();
		fs_console_loop();
		fs_core_destroy();
	}	
	
	function fs_log($level_str, $msg) {

		fs_console_log($level_str, $msg);
				
	}
	
	function fs_log_clean($msg) {
		
		fs_console_clean($msg);
		
	}

	function fs_getsessn($uuid){
		
		return fs_core_sessn_locate($uuid);
	
	}

	function fs_answer(){
		
		fs_channel_answer($sessn);
		
	}

	function fs_early_media($sessn){
		
		fs_channel_pre_answer($sessn);
		
	}

	function fs_hangup($cause){
		
		fs_channel_hangup($sessn, $cause);
		
	}

	function fs_set_variable($var, $val){
		
		fs_channel_set_variable($sessn, $var, $val);
		
	}
	
	function fs_get_variable($var){
		
		return fs_channel_get_var($sessn, $var);
		
	}

	function fs_set_channel_state($state){
		
		fs_channel_set_state($sessn, $state);
		
	}
	
	function fs_play_file($file){
		
		return fs_ivr_play_file($sessn, $file, NULL, NULL, NULL, 0);
				
	}
	
	function record_file($file){
		
		return fs_switch_ivr_record_file($sessn, NULL, $file, NULL, NULL, 0);
		
	}
	
	function fs_wait($ms){
		
		return fs_switch_ivr_sleep($sessn, $ms);
		
	}
	
	function fs_get_dtmf_callback($len){
		
		return fs_switch_ivr_collect_digits_callback($sessn, NULL, NULL, $len);
		
	}

	function fs_get_digit_count ($maxdigits, $terminator, $timeout){
		
		return fs_switch_ivr_collect_digits_count($sessn, NULL, NULL, $maxdigits, NULL, $terminator, $timeout);
		
	}

	function fs_x_way($peer_sessn, $dtmf, $sessn_data, $peer_data){
		
		return fs_switch_ivr_multi_threaded_bridge ($sessn, $peer_sessn, $dtmf, $sessn_data, $peer_data);
		
	}

	function fs_dial($data, $timelimit){
		
		return fs_switch_ivr_originate(sessn, NULL, $data, $timelimit, NULL, NULL, NULL, NULL);

	}

	function fs_transfer($exten, $dialplan, $context){
		
		return fs_switch_ivr_sessn_transfer($sessn, $exten, $dialplan, $context);
		
	}

	function fs_speak($ttsName, $voice, $text, $dtmf=NULL){
		
		return fs_switch_ivr_speak_text($sessn, $ttsName, NULL, NULL, $dtmf, $text, NULL, 0);
		
	}



}


?>
