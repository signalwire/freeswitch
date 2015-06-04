#!/usr/bin/perl

my $myname ;
my $skype_download_url = "http://download.skype.com/linux/skype-4.3.0.37.tar.bz2";
my $skype_download_pkg = "skype-4.3.0.37.tar.bz2";
my $skype_binary_dir = "/usr/local/freeswitch/skypopen/skype-clients-symlinks-dir";
my $skype_download_dir = "/tmp/skype_download";
my $skype_unpacked_dir = "skype-4.3.0.37";
my $skype_share_dir = "/usr/share/skype";
my $freeswitch_modules_config_dir = "/usr/local/freeswitch/conf/autoload_configs";
my $skypopen_sound_driver_dir = "/usr/local/freeswitch/skypopen/skypopen-sound-driver-dir";
my $skype_config_dir = "/usr/local/freeswitch/skypopen/skype-clients-configuration-dir";
my $skype_startup_dir = "/usr/local/freeswitch/skypopen/skype-clients-startup-dir";
my $skype_symlinks_dir = "/usr/local/freeswitch/skypopen/skype-clients-symlinks-dir";
my $skype_clients_to_be_launched = "5";
my $skype_clients_starting_number = "100";
my $multi_skypeusername = "one";
my $skype_username = "your_own_skype_username";
my $skype_password = "your_own_skype_password";
my @skype_username_array;
my @skype_password_array;
my $sure = "nope";

### PRESENTATION ###
system("clear");
printf("\n");
printf("This is the interactive installation helper for Skypopen\n");
printf("(http://wiki.freeswitch.org/wiki/Mod_skypopen_Skype_Endpoint_and_Trunk)\n");
printf("\n");
printf("Especially designed for FreeSWITCH\n");
printf("by Giovanni Maruzzelli\n");
printf("\n");
printf("Please direct all questions or issues to the FreeSWITCH mailing list or Jira\n");
printf("(http://lists.freeswitch.org/mailman/listinfo or https://freeswitch.org/jira)\n");
printf("\n");
printf("\n");
printf("I'll ask you questions, giving default answers in square brackets [] if any\n");
printf("To accept the default, just press Enter\n");
printf("You'll be prompted to confirm your answer at each step\n");
printf("At the end of questions, before I do anything, I'll let you review it all\n");
printf("To abort, press Ctrl-C\n");
printf("\n");
printf("\n");
printf("Let's start asking your name, so you see how the question/answer works\n");
printf("To accept the default, just press Enter\n");
$myname = &promptUser("Enter your name ", "Giovanni");
system("clear");
printf("\n");
printf("OK %s, GREAT! Let's start real questions! (At any time, Ctrl-C to abort)\n", $myname);
printf("\n");
printf("At the end of questions, before I do anything, I'll let you review all your answers, don't worry! :)\n");
printf("\n");
printf("\n");

### ASKING QUESTIONS ###

printf("I'm about to download the Skype client for Linux version 2.0.0.72 for OSS\n");
printf("nicely repackaged by Arch Linux with official Skype permission.\n");
printf("I need to create a directory to download and unpack the Skype client\n");
printf("To accept the default, just press Enter\n");
$skype_download_dir = &promptUser("Enter the full path of the Skype download directory ", "$skype_download_dir");
system("clear");
printf("\n");
printf("I'm about to install the Skype client\n");
printf("I would put the binary in $skype_binary_dir and the associated files in $skype_share_dir\n");
printf("Location of associated files is mandatory ($skype_share_dir)\n");
printf("Location of binary is recommended ($skype_binary_dir)\n");
printf("To accept the default, just press Enter\n");
$skype_binary_dir = &promptUser("Enter the directory full path for Skype client binary ", "$skype_binary_dir");
system("clear");
printf("\n");
printf("I'm about to create the FreeSWITCH configuration file for mod_skypopen (skypopen.conf.xml)\n");
printf("I need to know where to put it, eg: where is the FreeSWITCH modules' config dir\n");
printf("To accept the default, just press Enter\n");
$freeswitch_modules_config_dir = &promptUser("Enter the directory full path for FreeSWITCH modules' config files ", "$freeswitch_modules_config_dir");
system("clear");
printf("\n");
printf("I'm about to create the directory where to put our fake sound driver\n");
printf("Location of fake sound driver directory is where you like it more :)\n");
printf("To accept the default, just press Enter\n");
$skypopen_sound_driver_dir = &promptUser("Enter the directory full path for fake sound driver ", "$skypopen_sound_driver_dir");
system("clear");
printf("\n");
printf("I'm about to create the configuration directory needed by the Skype clients\n");
printf("Location of Skype clients configuration directory is where you like it more :)\n");
printf("To accept the default, just press Enter\n");
$skype_config_dir = &promptUser("Enter the directory full path for Skype clients config ", "$skype_config_dir");
system("clear");
printf("\n");
printf("I'm about to create a directory where I'll put the Skype clients startup script\n");
printf("Location of Skype clients startup script directory is where you like it more :)\n");
printf("To accept the default, just press Enter\n");
$skype_startup_dir = &promptUser("Enter the directory full path for Skype clients startup script ", "$skype_startup_dir");
system("clear");
printf("\n");
printf("I'm about to create the directory for symlinks needed by the Skype clients startup script\n");
printf("Location of symlinks directory is where you like it more :)\n");
printf("To accept the default, just press Enter\n");
$skype_symlinks_dir = &promptUser("Enter the directory full path for Skype clients symlinks ", "$skype_symlinks_dir");
system("clear");
printf("\n");
printf("How many Skype clients (channels) do you want to launch?\n");
printf("Each Skype client will be one channel to FreeSWITCH and use approx 70MB of ram\n");
printf("A quad core CPU can very easily support 20 or more Skype clients\n");
printf("Each Skype client allows one concurrent call\n");
printf("Eg: if you plan to have a max of 10 concurrent (outbound and/or inbound) Skype calls then enter 10\n");
printf("To accept the default, just press Enter\n");
$skype_clients_to_be_launched = &promptUser("Enter how many Skype clients will be launched ", "$skype_clients_to_be_launched");
system("clear");
printf("\n");


while(1){
	printf("You want all of the Skype clients to use the same Skype login (skypeusername)?\n");
	printf("eg: you want all of your skypopen channels to be Bob on the Skype network, or you want channel skype01 to be Bob, channel skype02 to be Alice, etc?\n");
	printf("Please answer 'one' for all channels using the same Skype login (you'll be asked just one time for Skype login and password) or 'multi' for being asked for each channel\n");
	printf("\n");
	$multi_skypeusername = &promptUser("Enter 'one' or 'multi' ", "$multi_skypeusername");
	system("clear");
	printf("\n");
	if($multi_skypeusername eq "one" or $multi_skypeusername eq "multi"){
		last;
	}
}


if($multi_skypeusername eq "one"){
	printf("I need the Skype username which will be used by ALL the Skype clients to be launched\n");
	printf("(That's the one-word you registered as login to the Skype network)\n");
	printf("This installer will create the needed files to launch concurrently many (or one) instances of it\n");
	printf("\n");
	printf("NB: DON'T ACCEPT the DEFAULT, write YOUR OWN\n");
	$skype_username = &promptUser("Enter the Skype clients username ", "$skype_username");
	for($count=1; $count <= $skype_clients_to_be_launched ; $count++){
		$skype_username_array[$count] = $skype_username;
	}
	system("clear");
	printf("\n");
	printf("I need the Skype password which will be used by ALL the Skype clients to be launched\n");
	printf("(That's the one-word you registered as password to the Skype network)\n");
	printf("\n");
	printf("NB: DON'T ACCEPT the DEFAULT, write YOUR OWN\n");
	$skype_password = &promptUser("Enter the Skype clients password ", "$skype_password");
	for($count=1; $count <= $skype_clients_to_be_launched ; $count++){
		$skype_password_array[$count] = $skype_password;
	}
	system("clear");
} else {
	for($count=1; $count <= $skype_clients_to_be_launched ; $count++){
		$skype_client_extension = $skype_clients_starting_number + $count ;
		printf("I need the Skype username which will be used by the Skype client for channel 'skype$skype_client_extension'\n");
		printf("(That's the one-word you registered as login to the Skype network)\n");
		printf("\n");
		printf("NB: DON'T ACCEPT the DEFAULT, write YOUR OWN\n");
		$skype_username = &promptUser("Enter the Skype username for channel 'skype$skype_client_extension'", "$skype_username");
		$skype_username_array[$count] = $skype_username;
		system("clear");
		printf("\n");
		printf("I need the Skype password which will be used by the Skype client 'skype$skype_client_extension'\n");
		printf("(That's the one-word you registered as password to the Skype network)\n");
		printf("\n");
		printf("NB: DON'T ACCEPT the DEFAULT, write YOUR OWN\n");
		$skype_password = &promptUser("Enter the Skype password for '$skype_username'", "$skype_password");
		$skype_password_array[$count] = $skype_password;
		system("clear");
	}

}

### GETTING FINAL APPROVAL ###
printf("\n");
printf("Please check the following values:\n");
printf("\n");
printf("directory for downloading and unpacking Skype client:\n'$skype_download_dir'\n");
printf("directory for Skype client binary:\n'$skype_binary_dir'\n");
printf("directory for FreeSWITCH modules' configs:\n'$freeswitch_modules_config_dir'\n");
printf("directory for fake sound driver:\n'$skypopen_sound_driver_dir'\n");
printf("directory for Skype clients configs:\n'$skype_config_dir'\n");
printf("directory for Skype clients startup script:\n'$skype_startup_dir'\n");
printf("directory for Skype clients symlinks:\n'$skype_symlinks_dir'\n");
printf("how many Skype clients to launch: '$skype_clients_to_be_launched'\n");
if($multi_skypeusername eq "one"){
	printf("Skype login: '$skype_username'\n");
	printf("Skype password: '$skype_password'\n");
}else {
	for($count=1; $count <= $skype_clients_to_be_launched ; $count++){
		$skype_client_extension = $skype_clients_starting_number + $count ;
		printf("channel='skype$skype_client_extension' Skype login='$skype_username_array[$count]' Skype password='$skype_password_array[$count]'\n");
	}
}

$sure = &promptUser("Are you sure you like the values? Write 'sure' for yes ", "$sure");
if($sure ne "sure"){
	printf("No problem, please relaunch the installer and begin again\n");
	exit 0;
}
system("clear");

printf("\n");
printf("GREAT! Please stand back, I'm working...\n");
printf("\n");

#### EXECUTION ###

system("mkdir -p $skype_download_dir");
system("cd $skype_download_dir ; wget -c $skype_download_url");
system("cd $skype_download_dir ; tar -xjf $skype_download_pkg");

system("mkdir -p $skype_binary_dir");
system("cd $skype_download_dir/$skype_unpacked_dir ; cp skype $skype_binary_dir/");

system("mkdir -p $skype_share_dir");
system("cd $skype_download_dir/$skype_unpacked_dir ; cp -a avatars $skype_share_dir/");
system("cd $skype_download_dir/$skype_unpacked_dir ; cp -a sounds $skype_share_dir/");
system("cd $skype_download_dir/$skype_unpacked_dir ; cp -a lang $skype_share_dir/");
system("cd $skype_download_dir/$skype_unpacked_dir ; cp -a icons $skype_share_dir/");


system("mkdir -p $skype_config_dir");
system("mkdir -p $skype_startup_dir");
system("mkdir -p $skype_symlinks_dir");

system("echo \"<configuration name=\\\"skypopen.conf\\\" description=\\\"Skypopen Configuration\\\">\" > $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"<global_settings>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"dialplan\\\" value=\\\"XML\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"context\\\" value=\\\"default\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"destination\\\" value=\\\"5000\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"skype_user\\\" value=\\\"$skype_username\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"report_incoming_chatmessages\\\" value=\\\"false\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"write_silence_when_idle\\\" value=\\\"false\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"  <param name=\\\"setsockopt\\\" value=\\\"true\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"</global_settings>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"<!-- one entry follows per each skypopen interface -->\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"<per_interface_settings>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");


system("echo \"#!/bin/sh\" > $skype_startup_dir/start_skype_clients.sh");
system("echo >> $skype_startup_dir/start_skype_clients.sh");
system("echo >> $skype_startup_dir/start_skype_clients.sh");


for ($count = 1; $count <= $skype_clients_to_be_launched; $count++) {
	$skype_client_extension = $skype_clients_starting_number + $count ;
	$skype_login=$skype_username_array[$count];
	$skype_passwd=$skype_password_array[$count];
	system("ln -s $skype_binary_dir/skype $skype_symlinks_dir/skype$skype_client_extension");
	system("mkdir -p $skype_config_dir/skype$skype_client_extension");
	system("cp -a ../configs/skype-client-configuration-dir-template/skypeclient01/shared.* $skype_config_dir/skype$skype_client_extension");
	system("cp -a ../configs/skype-client-configuration-dir-template/skypeclient01/skypenameA $skype_config_dir/skype$skype_client_extension/$skype_login");

	system("echo \" <interface id=\\\"$count\\\" name=\\\"skype$skype_client_extension\\\">\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
	if($multi_skypeusername ne "one"){
		system("echo \" <param name=\\\"skype_user\\\" value=\\\"$skype_login\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
	}
	system("echo \" <param name=\\\"X11-display\\\" value=\\\":$skype_client_extension\\\"/>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
	system("echo \" </interface>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");

	system("echo \"#start the fake X server on the given port\" >> $skype_startup_dir/start_skype_clients.sh");
	system("echo \"/usr/bin/Xvfb :$skype_client_extension -ac -nolisten tcp -screen 0 640x480x8 &\" >> $skype_startup_dir/start_skype_clients.sh");
	system("echo \"sleep 3\" >> $skype_startup_dir/start_skype_clients.sh");
	system("echo \"# start a Skype client instance that will connect to the X server above, and will login to the Skype network using the 'username password' you send to it on stdin.\" >> $skype_startup_dir/start_skype_clients.sh");
	system("echo \"su root -c \\\"/bin/echo '$skype_login $skype_passwd'| DISPLAY=:$skype_client_extension  $skype_symlinks_dir/skype$skype_client_extension --dbpath=$skype_config_dir/skype$skype_client_extension --pipelogin &\\\"\" >> $skype_startup_dir/start_skype_clients.sh");
	system("echo \"sleep 7\" >> $skype_startup_dir/start_skype_clients.sh");
	system("echo >> $skype_startup_dir/start_skype_clients.sh");
}

system("echo \"</per_interface_settings>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");
system("echo \"</configuration>\" >> $freeswitch_modules_config_dir/skypopen.conf.xml");

system("echo \"exit 0\" >> $skype_startup_dir/start_skype_clients.sh");


printf("\n");
printf("SUCCESS!!!\n");
printf("\n");


#=========================================================================#


#-------------------------------------------------------------------------#
# promptUser, a Perl subroutine to prompt a user for input.
# Copyright 2010 Alvin Alexander, devdaily.com.
# This code is shared here under the 
# Creative Commons Attribution-ShareAlike Unported 3.0 license.
# See http://creativecommons.org/licenses/by-sa/3.0/ for more information.
#-------------------------------------------------------------------------#

# Original at: http://www.devdaily.com/perl/edu/articles/pl010005
# Modified to get confirmations by Giovanni Maruzzelli

#----------------------------(  promptUser  )-----------------------------#
#                                                                         #
#  FUNCTION:	promptUser                                                #
#                                                                         #
#  PURPOSE:	Prompt the user for some type of input, and return the    #
#		input back to the calling program.                        #
#                                                                         #
#  ARGS:	$promptString - what you want to prompt the user with     #
#		$defaultValue - (optional) a default value for the prompt #
#                                                                         #
#-------------------------------------------------------------------------#

sub promptUser {

#-------------------------------------------------------------------#
#  two possible input arguments - $promptString, and $defaultValue  #
#  make the input arguments local variables.                        #
#-------------------------------------------------------------------#

	local($promptString,$defaultValue) = @_;
	local $input;
	local $confirm;
	local $gave;

#-------------------------------------------------------------------#
#  if there is a default value, use the first print statement; if   #
#  no default is provided, print the second string.                 #
#-------------------------------------------------------------------#

	while(1){
		printf("\n");
		if ($defaultValue) {
			print $promptString, "\n[", $defaultValue, "]: ";
		} else {
			print $promptString, ": ";
		}

		$| = 1;               # force a flush after our print
			$input = <STDIN>;         # get the input from STDIN (presumably the keyboard)


#------------------------------------------------------------------#
# remove the newline character from the end of the input the user  #
# gave us.                                                         #
#------------------------------------------------------------------#

			chomp($input);

		$gave = $input ? $input : $defaultValue;
		print("You gave: '$gave'\nIt's OK? Please answer 'Y' for yes or 'N' for not [N]: ");
		$| = 1;               # force a flush after our print
			$confirm = <STDIN>;
		chomp($confirm);
		if($confirm eq "Y" or $confirm eq "y"){
			last;
		}
	}
#-----------------------------------------------------------------#
#  if we had a $default value, and the user gave us input, then   #
#  return the input; if we had a default, and they gave us no     #
#  no input, return the $defaultValue.                            #
#                                                                 # 
#  if we did not have a default value, then just return whatever  #
#  the user gave us.  if they just hit the <enter> key,           #
#  the calling routine will have to deal with that.               #
#-----------------------------------------------------------------#

	if ("$defaultValue") {
		return $input ? $input : $defaultValue;    # return $_ if it has a value
	} else {
		return $input;
	}
}

