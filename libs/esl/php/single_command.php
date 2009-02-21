#!/usr/bin/php
<?php
require_once('ESL.php');

if ($argc > 1) {
    array_shift($argv);
    $command = sprintf('%s', implode(' ', $argv));
    printf("Command to run is: %s\n", $command);

    $sock = new ESLconnection('localhost', '8021', 'ClueCon');
    $res = $sock->api($command);
    printf("%s\n", $res->getBody());
} else {
    printf("ERROR: You Need To Pass A Command\nUsage:\n\t%s <command>", $argv[0]);
}

?>
