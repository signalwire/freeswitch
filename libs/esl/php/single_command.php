#!/usr/bin/php
<?php
require_once('ESL.php');

if ($argc > 1) {
    array_shift($argv);
    $command = sprintf('%s', implode(' ', $argv));
    printf("Command to run is: %s\n", $command);
    $esl = new eslConnection('127.0.0.1', '8021', 'ClueCon');
    $e = $esl->api("$command");
    print $e->getBody();
} else {
    printf("ERROR: You Need To Pass A Command\nUsage:\n\t%s <command>", $argv[0]);
}

?>
